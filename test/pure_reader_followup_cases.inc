#include "subscriber_intervention.h"
#include "release_reference_reader.h"
#include "character_membership_reader.h"
#include <functional>

namespace pure_followup_fixture {
using definition_scope_fixture::Fixture;
namespace live=mt2mp::game::live_host_state;
namespace live_host_state=mt2mp::game::live_host_state;
namespace subscriber_intervention=mt2mp::game::subscriber_intervention;
namespace release_references=mt2mp::game::release_references;
namespace character_membership_reader=mt2mp::game::character_membership_reader;
namespace character_definition_identity=mt2mp::game::character_definition_identity;
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
struct Runtime {
    Fixture& fixture;uintptr_t image=0,mutation_address=0,throw_address=0;unsigned mutation_hits=0,mutation_occurrence=2;
    std::function<void()> mutation;
    struct Memory {Runtime& runtime;
        bool copy(uintptr_t address,void* out,size_t bytes,std::string& error){auto& r=runtime;
            if(address==r.throw_address)throw std::runtime_error("original pure reader exception");
            if(address==r.mutation_address&&++r.mutation_hits==r.mutation_occurrence)r.mutation();
            if(!mt2mp::game::root_saved_building::bytes(reinterpret_cast<void*>(address),0,out,bytes,error))return false;
            const auto* value=static_cast<const unsigned char*>(out);r.fixture.events.push_back({address,{value,value+bytes}});return true;
        }
    } memory;
    Runtime(Fixture& value,uintptr_t image_value):fixture(value),image(image_value),memory{*this}{}
};
#include "pure_release_string_bodies.inc"
template<class Read,class Copy>bool subscriber_candidate(Read& read,Copy& copy,Runtime& r,subscriber_intervention::State& values,subscriber_intervention::Owners& owners,std::string& error){
#include "pure_subscriber_call.inc"
}
template<class Read,class Text>bool release_baseline(Read& read,Text& text,Runtime& r,uintptr_t state,release_references::Types& types,release_references::Types& checked,std::map<uintptr_t,std::string>& out,std::string& e){
    struct World {uintptr_t state;};const World world{state};
#include "pure_release_baseline.inc"
    return true;
}
template<class Read,class Text>bool release_candidate(Read& read,Text& text,Runtime& r,uintptr_t state,release_references::Types& types,release_references::Types& checked,std::map<uintptr_t,std::string>& out,std::string& e){
    struct World {uintptr_t state;};const World world{state};
#include "pure_release_call.inc"
    return true;
}
template<class Find>uint32_t lookup(Find& find,uint64_t uid,uintptr_t& pointer){return find(uid,pointer);}
template<class Read,class Find>bool users_candidate(Read& at,Find& original,uintptr_t image,const character_definition_identity::Census& current,character_membership_reader::Users& current_users,std::string& error){
    struct Owner {uintptr_t image;};const Owner owner{image};struct Api {Find& find;};Api api{original};
#include "pure_users_find.inc"
#include "pure_users_call.inc"
}
}

void pure_reader_followup_cases(){
    using namespace pure_followup_fixture;Fixture fixture;t::totals={};t::Scope capture(t::Part::HostCapture);
    const auto base=uintptr_t(fixture.owned.memory);
    {
        const auto image=base-0x13df600,manager=base+0x2000,data=base+0x3000;Runtime runtime(fixture,image);
        auto setup=[&]{fixture.reset();runtime.mutation_address=runtime.throw_address=0;runtime.mutation_hits=0;runtime.mutation={};
            fixture.put(image+0x13df690,manager);fixture.put(manager,image+0x1725f40);fixture.put(manager+0x40,data);fixture.put(manager+0x48,int32_t(2));fixture.put(manager+0x4c,int32_t(2));
            for(size_t n=0;n<2;++n){const auto object=base+0x4000+n*0x1000;fixture.put(data+n*8,object);fixture.put(object,image+0x16e75f0);fixture.put(object+0x20,int32_t(n+1));}};
        auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& error){return at(runtime,object,offset,value,error);};
        auto copy=[&](uintptr_t address,void* out,size_t bytes,std::string& error){return runtime.memory.copy(address,out,bytes,error);};
        subscriber_intervention::State baseline,scoped;subscriber_intervention::Owners before,after;std::string error;
        setup();auto start=count(t::Part::HostMemoryVirtualQuery);
        need(subscriber_intervention::read_all(read,copy,image,baseline,before,error),"unscoped actual Subscriber complete reader");
        const auto trace=fixture.events;const auto baseline_queries=count(t::Part::HostMemoryVirtualQuery)-start;
        need(trace.size()==24&&baseline_queries==24,"Subscriber baseline has24 live reads/copies and24 OS queries");
        setup();start=count(t::Part::HostMemoryVirtualQuery);need(subscriber_candidate(read,copy,runtime,scoped,after,error),"extracted Subscriber pure extent");
        const auto scoped_queries=count(t::Part::HostMemoryVirtualQuery)-start;
        need(fixture.events==trace&&baseline==scoped&&before==after&&scoped_queries==1&&!t::page_read_scope,"Subscriber24 reads, complete values and owners stay exact with one metadata query");
        fixture.put(base+0x4000+0x50,uint32_t(7));need(subscriber_candidate(read,copy,runtime,scoped,after,error)&&scoped[0].value.warnings==7,"next Subscriber extent reads fresh warning value");
        auto mutate=[&]{runtime.mutation_address=manager+0x40;runtime.mutation_hits=0;runtime.mutation=[&]{fixture.put(manager+0x40,data+8);};};
        setup();mutate();need(!subscriber_intervention::read_all(read,copy,image,baseline,before,error),"baseline Subscriber final manager guard rejects change");const auto rejected=fixture.events;const auto cause=error;
        setup();mutate();need(!subscriber_candidate(read,copy,runtime,scoped,after,error)&&fixture.events==rejected&&error==cause&&!t::page_read_scope,"Subscriber scoped final guard retains same current reads and original cause");
        setup();runtime.throw_address=manager+0x40;bool caught=false;
        try{subscriber_candidate(read,copy,runtime,scoped,after,error);}catch(const std::runtime_error& e){caught=std::string(e.what())=="original pure reader exception";}
        need(caught&&!t::page_read_scope,"Subscriber original exception closes only local metadata extent");
        std::cout<<"Subscriber pure reader value reads="<<trace.size()<<" baseline OS queries="<<baseline_queries<<" scoped OS queries="<<scoped_queries<<'\n';
    }
    {
        Runtime runtime(fixture,fixture.image);auto setup=[&]{fixture.reset();runtime.mutation_address=runtime.throw_address=0;runtime.mutation_hits=0;runtime.mutation={};
            for(size_t n=0;n<2;++n)for(size_t field=0;field<2;++field){const auto object=n?fixture.second:fixture.first;const auto native=object+(field?0x248:0xb0);const auto data=base+0xb000+(n*2+field)*64;
                fixture.put(native,data);fixture.put(native+8,uint64_t(3));const char text[4]={'A',char('0'+n),char('0'+field),0};std::memcpy(reinterpret_cast<void*>(data),text,sizeof text);}};
        auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& error){return at(runtime,object,offset,value,error);};
        auto text=[&](uintptr_t address,std::string& value,std::string& error){return string(runtime,address,value,error);};
        release_references::Types baseline,baseline_checked,scoped,scoped_checked;std::map<uintptr_t,std::string> before,after;std::string error;
        setup();auto start=count(t::Part::HostMemoryVirtualQuery);need(release_baseline(read,text,runtime,fixture.state,baseline,baseline_checked,before,error),"unscoped actual Release double base-type reader");
        const auto trace=fixture.events;const auto baseline_queries=count(t::Part::HostMemoryVirtualQuery)-start;
        need(trace.size()==188&&baseline_queries==188,"Release baseline has188 live definition/string reads and188 OS queries");
        setup();start=count(t::Part::HostMemoryVirtualQuery);need(release_candidate(read,text,runtime,fixture.state,scoped,scoped_checked,after,error),"extracted Release pure double-read extent");const auto scoped_queries=count(t::Part::HostMemoryVirtualQuery)-start;
        need(fixture.events==trace&&baseline==scoped&&baseline_checked==scoped_checked&&before==after&&scoped_queries<=3&&!t::page_read_scope,"Release188 reads, both full passes and canonical keys preserved with bounded metadata queries");
        fixture.put(base+0xb000,char('Z'));after.clear();need(release_candidate(read,text,runtime,fixture.state,scoped,scoped_checked,after,error)&&scoped.rows[0].name[0]=='Z',"next Release extent reads current native name bytes");
        auto mutate=[&]{runtime.mutation_address=base+0xb000;runtime.mutation_hits=0;runtime.mutation=[&]{fixture.put(base+0xb000,char('X'));};};
        setup();mutate();before.clear();need(!release_baseline(read,text,runtime,fixture.state,baseline,baseline_checked,before,error),"Release baseline double-read guard rejects changed text");const auto rejected=fixture.events;const auto cause=error;
        setup();mutate();after.clear();need(!release_candidate(read,text,runtime,fixture.state,scoped,scoped_checked,after,error)&&fixture.events==rejected&&error==cause&&!t::page_read_scope,"Release scoped double-read guard retains fresh text and exact original cause");
        setup();runtime.throw_address=base+0xb000;bool caught=false;after.clear();try{release_candidate(read,text,runtime,fixture.state,scoped,scoped_checked,after,error);}catch(const std::runtime_error& e){caught=std::string(e.what())=="original pure reader exception";}
        need(caught&&!t::page_read_scope,"Release original text-read exception closes metadata extent");
        std::cout<<"Release pure reader value reads="<<trace.size()<<" baseline OS queries="<<baseline_queries<<" scoped OS queries="<<scoped_queries<<'\n';
    }
    {
        Runtime runtime(fixture,fixture.image);const auto data=base+0x6000,actor=base+0x7000,addresspoint=base+0xa000;
        auto setup=[&]{fixture.reset();runtime.mutation_address=runtime.throw_address=0;runtime.mutation_hits=0;runtime.mutation={};
            for(size_t n=0;n<2;++n){const auto object=n?fixture.second:fixture.first;fixture.put(object+0x4c0,runtime.image+0x1747ae0);fixture.put(object+0x4c8,n?uintptr_t(0):data);fixture.put(object+0x4d0,int32_t(n?0:1));fixture.put(object+0x4d4,int32_t(n?0:1));}
            fixture.put(data,actor);fixture.put(actor,addresspoint);fixture.put(actor+0x18,uint64_t(0x300000001));fixture.put(addresspoint+0x438,runtime.image+character_membership_reader::callbacks[1]);};
        auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& error){return at(runtime,object,offset,value,error);};
        character_definition_identity::Census census;std::string error;setup();need(character_definition_identity::read(read,runtime.image,fixture.state,fixture.manager,census,error),"current definitions for users fixture");
        unsigned finds=0;auto original=[&](uint64_t uid,uintptr_t& pointer)->uint32_t{++finds;pointer=uid==0x300000001?actor:0;return 0;};
        auto baseline_find=[&](uint64_t uid,uintptr_t& pointer,std::string&)->bool{return original(uid,pointer)==0;};
        character_membership_reader::Users baseline,scoped;fixture.events.clear();auto start=count(t::Part::HostMemoryVirtualQuery);
        need(character_membership_reader::users(read,baseline_find,runtime.image,census,baseline,error),"unscoped actual current user reader");const auto trace=fixture.events;const auto baseline_queries=count(t::Part::HostMemoryVirtualQuery)-start;
        need(trace.size()==19&&baseline_queries==19&&finds==1,"users baseline retains19 reads and one original Find");
        setup();finds=0;start=count(t::Part::HostMemoryVirtualQuery);need(users_candidate(read,original,runtime.image,census,scoped,error),"extracted users scope and actual Find boundary");const auto scoped_queries=count(t::Part::HostMemoryVirtualQuery)-start;
        need(fixture.events==trace&&baseline==scoped&&finds==1&&scoped_queries==2&&!t::page_read_scope,"users retain19 reads and one Find while metadata reloads after Find");
        setup();auto changed=[&](uint64_t uid,uintptr_t& pointer)->uint32_t{fixture.put(fixture.first+0x4d0,int32_t(0));return original(uid,pointer);};
        auto changed_baseline=[&](uint64_t uid,uintptr_t& pointer,std::string&)->bool{return changed(uid,pointer)==0;};
        need(!character_membership_reader::users(read,changed_baseline,runtime.image,census,baseline,error),"baseline users final array guard rejects callback change");const auto rejected=fixture.events;const auto cause=error;
        setup();need(!users_candidate(read,changed,runtime.image,census,scoped,error)&&fixture.events==rejected&&error==cause&&!t::page_read_scope,"users final array reread and cause survive callback barrier");
        setup();bool protected_page=false;DWORD protection=0;auto protect=[&](uint64_t uid,uintptr_t& pointer)->uint32_t{
            MEMORY_BASIC_INFORMATION info{};const auto before=count(t::Part::HostMemoryVirtualQuery);
            need(t::virtual_query(reinterpret_cast<void*>(fixture.first),&info,sizeof info)==sizeof info&&count(t::Part::HostMemoryVirtualQuery)==before+1,"Find entry cannot reuse caller metadata");
            protected_page=VirtualProtect(reinterpret_cast<void*>(fixture.first),4096,PAGE_NOACCESS,&protection)!=0;need(protected_page,"protect only owned user-array page inside original callback");return original(uid,pointer);};
        bool okay=false;uint64_t barriers=0;{t::Scope take(t::Part::Take);const auto before=count(t::Part::OriginalNestedReset);okay=users_candidate(read,protect,runtime.image,census,scoped,error);barriers=count(t::Part::OriginalNestedReset)-before;}
        DWORD replaced=0;need(protected_page&&VirtualProtect(reinterpret_cast<void*>(fixture.first),4096,protection,&replaced)!=0,"restore only owned user-array page after callback");
        need(!okay&&error.find("unreadable")!=std::string::npos&&barriers==2&&!t::page_read_scope,"Find invalidates metadata on entry and return before actual final read");
        setup();auto failed=[&](uint64_t,uintptr_t&)->uint32_t{return 123;};need(!users_candidate(read,failed,runtime.image,census,scoped,error)&&error=="Character complete census current FindUID failed SEH=123"&&!t::page_read_scope,"original Find failure remains visible after scope closes");
        auto throws=[&](uint64_t,uintptr_t&)->uint32_t{throw std::runtime_error("original Find exception");};bool caught=false;
        try{users_candidate(read,throws,runtime.image,census,scoped,error);}catch(const std::runtime_error& e){caught=std::string(e.what())=="original Find exception";}
        need(caught&&!t::page_read_scope,"original Find exception unwinds both callback and reader metadata scopes");
        std::cout<<"users reader value reads="<<trace.size()<<" original Find calls=1 baseline OS queries="<<baseline_queries<<" scoped OS queries="<<scoped_queries<<'\n';
    }
    t::totals={};
}
