#include "live_host_state/memory_query_timing.h"
#include "character_catalogue_timing.h"
#include "subscriber_lifecycle/record_graph.h"
#include <iostream>
#include <stdexcept>
#include <vector>
namespace subscriber_lifecycle=mt2mp::subscriber_lifecycle;
namespace live_host_state=mt2mp::game::live_host_state;
namespace character_catalogue_timing=mt2mp::game::character_catalogue_timing;
namespace t=live_host_state::apply_timing;
namespace {
void need(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
struct Read {uintptr_t address;std::vector<unsigned char> bytes;bool operator==(const Read&)const=default;};
struct Memory final:subscriber_lifecycle::Memory {
    std::vector<Read> reads;
    bool read(uintptr_t address,void* out,size_t size,std::string& error)override{
        MEMORY_BASIC_INFORMATION info{};
        if(!t::virtual_query(reinterpret_cast<void*>(address),&info,sizeof info,t::Part::QueryWorldProp)||
           info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD))||
           address<uintptr_t(info.BaseAddress)||size>info.RegionSize||
           address-uintptr_t(info.BaseAddress)>info.RegionSize-size){error="record fixture unreadable";return false;}
        std::memcpy(out,reinterpret_cast<void*>(address),size);
        const auto* bytes=static_cast<unsigned char*>(out);reads.push_back({address,{bytes,bytes+size}});return true;
    }
};
struct Captured {Memory& memory;subscriber_lifecycle::Graph graph;};
using Inspect=Captured;
namespace baseline {
#include "unscoped_record_inspect.inc"
}
namespace membership {
#include "membership_record_inspect.inc"
}
namespace actor {
#include "actor_record_inspect.inc"
}
struct Fixture {
    void* allocation=VirtualAlloc(nullptr,65536,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    uintptr_t base=uintptr_t(allocation),context=base+0x9000,tokens=base+0x5000;
    Fixture(){need(allocation!=nullptr,"owned record allocation failed");
        put(base+0x50,base+0x8000);put(base+0x58,int32_t(100));put(base+0x5c,int32_t(100));
        for(size_t i=0;i<100;++i){const auto child=base+0x100+i*0x80;
            put(base+0x8000+i*8,child);put(child+0x38,tokens+i*0x30);put(child+0x40,int32_t(1));put(child+0x44,int32_t(1));
            put(tokens+i*0x30,int32_t(4));put(tokens+i*0x30+0x28,uint32_t(i));}
        put(context,base+0x9100);
    }
    ~Fixture(){if(allocation&&!VirtualFree(allocation,0,MEM_RELEASE))std::cerr<<"owned record release failed "<<GetLastError()<<'\n';}
    template<class T>void put(uintptr_t address,const T& value){std::memcpy(reinterpret_cast<void*>(address),&value,sizeof value);}
};
nlohmann::json value(const subscriber_lifecycle::Graph& graph){
    auto out=nlohmann::json::array();for(const auto& row:graph.records){auto tokens=nlohmann::json::array();
        for(const auto& token:row.tokens)tokens.push_back({token.type,token.bits,token.text});
        out.push_back({row.parent,row.child_index,row.label.type,row.label.bits,row.label.text,tokens});}
    for(const auto& link:graph.private_links)out.push_back({link.saved_link_id,link.object,link.has_inline_definition});return out;
}
uint64_t queries(){return t::totals[size_t(t::Part::HostMemoryVirtualQuery)].count;}
using Callback=bool(*)(void*,const void*,const void*,std::string&);
void run(Callback candidate,const char* name){
    Fixture f;Memory memory;Captured result{memory,{}};std::string error;
    t::Scope measured(t::Part::HostCapture);
    const auto call=[&](Callback callback){return callback(&result,reinterpret_cast<void*>(f.base),reinterpret_cast<void*>(f.context),error);};
    uint64_t unscoped=0,scoped=0;std::vector<Read> expected_reads;nlohmann::json expected;
    {
        t::PageReadScope original(t::PageBoundary::OriginalCall);auto* parent=t::page_read_scope;
        auto start=queries();if(!call(baseline::inspect))throw std::runtime_error("baseline original graph read failed: "+error);unscoped=queries()-start;
        expected_reads=memory.reads;expected=value(result.graph);memory.reads.clear();start=queries();
        need(call(candidate),"production inspect graph read failed");scoped=queries()-start;
        need(value(result.graph)==expected&&memory.reads==expected_reads,"scoped reader changed native bytes/read order/output");
        need(t::page_read_scope==parent&&!t::PageReadScope::reader_active(),"inspect failed to restore original-call boundary");
        need(unscoped>1000&&scoped==1,"metadata cache did not reduce actual OS queries to one homogeneous region");
        f.put(f.tokens+0x28,uint32_t(777));memory.reads.clear();
        need(call(candidate)&&result.graph.records[1].tokens[0].bits==777,"next inspection reused stale token bytes");
        f.put(f.tokens,int32_t(5));memory.reads.clear();
        need(!call(baseline::inspect),"baseline invalid token was accepted");const auto invalid_reads=memory.reads;const auto cause=error;
        memory.reads.clear();need(!call(candidate)&&error==cause&&memory.reads==invalid_reads,"scoped invalid input changed original rejection/read sequence");
        f.put(f.tokens,int32_t(4));need(call(candidate),"reader failed to recover after token repair");
        DWORD previous=0;need(VirtualProtect(f.allocation,65536,PAGE_NOACCESS,&previous)!=0,"owned page protection failed");
        const bool accepted=call(candidate);DWORD ignored=0;
        need(VirtualProtect(f.allocation,65536,previous,&ignored)!=0,"owned page restore failed");
        need(!accepted&&error=="record fixture unreadable","new inspect reused protection from prior original callback");
        need(call(candidate),"new inspect failed after page restore");
    }
    need(!t::page_read_scope,"inspection leaked metadata outside original scope");
    std::cout<<name<<": "<<expected_reads.size()<<" unchanged value reads, OS queries "<<unscoped<<" -> "<<scoped<<'\n';
}
}
#include "character_region_owners_cases.inc"
#include "character_prop_owners_cases.inc"
#include "scenery_identity_field_cases.inc"
#include "character_catalogue_manifest_cases.inc"
#include "management_record_scope_cases.inc"
#include "world_zone_scope_cases.inc"
#include "dungeon_census_scope_cases.inc"
int main()try{run(membership::inspect,"character definition");run(actor::inspect,"character actor");region_owners_cases();prop_owners_fixture::run();scenery_identity_field_fixture::run();catalogue_manifest_fixture::run();project_progress_fixture::run();gm_record_fixture::run();world_zone_scope_fixture::run();dungeon_census_scope_fixture::run();return 0;}
catch(const std::exception& problem){std::cerr<<problem.what()<<'\n';return 1;}
