namespace pure_scenery_field_fixture {
using Memory=world_prop_attribution_fixture::Memory;
template<class Port>bool saved(bool batch,Port&& port){
    return
#include "saved_root_field_scope.inc"
    ;
}
template<class Port>bool final_region(Port&& port){
#include "saved_root_final_field_scope.inc"
}
namespace host {using ProcessMemory=Memory;}
inline uintptr_t readable=0;
inline unsigned original_calls=0,manager_reads=0;
inline bool fail_code=false,changed_manager=false;
inline t::PageReadScope* expected_parent=nullptr;
bool read_fields(Memory& memory,unsigned amount,std::string& error){
    for(unsigned i=0;i<amount;++i){uintptr_t value=0;if(!memory.copy(readable,&value,sizeof value,error))return false;}
    return true;
}
bool code(Memory& memory,uintptr_t,std::string& error){if(fail_code){error="original catalogue body guard failure";return false;}return read_fields(memory,4,error);}
bool manager(Memory& memory,uintptr_t,uintptr_t& out,std::string& error){++manager_reads;out=33+(changed_manager&&manager_reads>1?1:0);return read_fields(memory,1,error);}
bool text(Memory& memory,uintptr_t pointer,std::string& out,std::string& error){if(!read_fields(memory,2,error))return false;out=pointer==101?"cactus":"p_ext_treecactus_01.vmb";return true;}
uint32_t lookup(uintptr_t,uintptr_t,int32_t category,int32_t variant,uintptr_t& directory,uintptr_t& filename,uint64_t& roundtrip){
    ++original_calls;
    need(t::page_read_scope==expected_parent&&!t::PageReadScope::reader_active(),"original allocating catalogue lookup must stay outside both new field scopes");
    for(unsigned i=0;i<3;++i){MEMORY_BASIC_INFORMATION information{};need(t::virtual_query(reinterpret_cast<void*>(readable),&information,sizeof information,t::Part::QueryWorldProp)!=0,"original lookup fixture page query failed");}
    directory=101;filename=102;roundtrip=uint64_t(uint32_t(category))|(uint64_t(uint32_t(variant))<<32);return 0;
}
struct NativeCatalog {
    Memory& memory_;uintptr_t image_;uint32_t thread_=GetCurrentThreadId();
    bool identify(int32_t,int32_t,std::string&,std::string&,std::string&);
};
#include "catalog_field_scope_body.inc"
}
void pure_scenery_field_cases(){
    namespace f=world_prop_attribution_fixture;
    namespace p=pure_scenery_field_fixture;
    auto* allocation=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x1a3b000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    need(allocation!=nullptr,"allocate only the owned native-shaped World field fixture");
    struct Release {void* owned;~Release(){if(owned&&!VirtualFree(owned,0,MEM_RELEASE))std::terminate();}} release{allocation};
    const auto image=reinterpret_cast<uintptr_t>(allocation);
    const auto mode=image+0x1000,game=image+0x2000,map=image+0x3000,selection=image+0x4000;
    auto put=[&](uintptr_t address,auto value){std::memcpy(reinterpret_cast<void*>(address),&value,sizeof value);};
    put(mode,image+0x16e6730);put(game,image+0x1743740);put(map,image+0x1742860);put(game+0x28,mode);
    put(mode+0x2c4,int32_t(4));put(selection,image+0x17205d0);put(image+0x13df630,selection);put(map+0x1c0,image+0x174ad20);
    f::Memory memory;memory.field_mode=true;f::Origin origin;origin.success=true;
    origin.value.game=reinterpret_cast<void*>(game);origin.value.mode=reinterpret_cast<void*>(mode);origin.value.map=reinterpret_cast<void*>(map);
    origin.value.state=reinterpret_cast<void*>(image+0x5000);origin.value.world=reinterpret_cast<void*>(image+0x6000);
    origin.value.world_epoch=7;origin.value.generation=8;origin.value.bytes=123;origin.value.uuid="current";origin.value.sha256=std::string(64,'a');
    f::Reader reader{memory,origin};reader.image_=image;f::WorldState before,after;std::string error;
    t::totals={};t::Scope measured(t::Part::HostCapture);
    const auto query_start=count(t::Part::HostMemoryVirtualQuery);
    need(reader.baseline_current(before,true,error),"original complete World fields fixture failed");
    const auto trace=memory.field_reads;const auto baseline_queries=count(t::Part::HostMemoryVirtualQuery)-query_start;
    need(trace.size()==12&&baseline_queries==12,"original complete World reader checks all12 native fields through OS");
    memory.field_reads.clear();const auto optimized_start=count(t::Part::HostMemoryVirtualQuery);
    need(reader.current(after,true,error),"actual pure World fields reader failed");
    need(memory.field_reads==trace&&before.root==after.root&&before.selection==after.selection&&before.saves==after.saves&&before.edits==after.edits,"actual pure scope retains exact field values and read order");
    need(count(t::Part::HostMemoryVirtualQuery)-optimized_start==1&&!t::page_read_scope,"actual complete World field scope reduces12 OS queries to1 and ends on return");
    world_context_cases(reader,memory,origin,before,error);
    put(mode+0x30c,int32_t(1));error.clear();need(!reader.current(after,true,error)&&error=="info: remove_wait_save_edit_or_graph","next invocation freshly reads save state instead of caching native values");put(mode+0x30c,int32_t(0));
    for(const auto& entry:trace){
        memory.failed_field=entry.first;memory.field_reads.clear();error.clear();need(!reader.baseline_current(before,true,error),"original field failure did not reject");
        const auto failure=error;const auto failed_trace=memory.field_reads;memory.field_reads.clear();error.clear();
        need(!reader.current(after,true,error)&&error==failure&&memory.field_reads==failed_trace&&!t::page_read_scope,"pure World fields preserve exact first failing field and original cause");
    }
    memory.failed_field=0;memory.throw_field=game+0x28;bool caught=false;
    try{reader.current(after,true,error);}catch(const std::runtime_error& cause){caught=std::string(cause.what())=="original field read exception";}
    need(caught&&!t::page_read_scope&&!reader.world_busy_,"original field exception restores both actual World reader scopes");memory.throw_field=0;
    {t::PageReadScope original(t::PageBoundary::OriginalCall);origin.expected_parent=t::page_read_scope;
        const auto start=count(t::Part::HostMemoryVirtualQuery);
        need(reader.current(after,true,error)&&t::page_read_scope==origin.expected_parent&&!t::PageReadScope::reader_active(),"actual field Reader scope restores enclosing native OriginalCall");
        need(count(t::Part::HostMemoryVirtualQuery)-start==1,"non-null OriginalCall still opens a fresh pure Reader extent");
    }origin.expected_parent=nullptr;
    p::readable=mode;
    auto member=[&]{need(t::PageReadScope::reader_active(),"concrete admitted Scenery callback must have its own pure Reader scope");return p::read_fields(memory,20,error);};
    auto start=count(t::Part::HostMemoryVirtualQuery);need(p::saved(true,member)&&!t::page_read_scope&&count(t::Part::HostMemoryVirtualQuery)-start==1,"actual admitted saved Scenery call reuses page metadata for20 fresh field reads");
    start=count(t::Part::HostMemoryVirtualQuery);need(p::final_region(member)&&!t::page_read_scope&&count(t::Part::HostMemoryVirtualQuery)-start==1,"actual final Region adapter scopes only complete pinned read callback");
    start=count(t::Part::HostMemoryVirtualQuery);need(p::saved(false,[&]{need(!t::page_read_scope,"unbatched generic callbacks must stay unscoped");return p::read_fields(memory,20,error);})&&count(t::Part::HostMemoryVirtualQuery)-start==20,"unadmitted generic saved callbacks retain original page boundaries");
    {t::PageReadScope original(t::PageBoundary::OriginalCall);auto* parent=t::page_read_scope;
        need(p::saved(true,member)&&t::page_read_scope==parent,"actual saved field callback restores native parent");
        memory.failed_field=mode;error.clear();need(!p::final_region(member)&&error=="original native field read failure"&&t::page_read_scope==parent,"actual final Scenery field callback preserves failure and restores parent");memory.failed_field=0;
        caught=false;try{p::saved(true,[&]()->bool{throw std::runtime_error("original member callback exception");});}catch(const std::runtime_error& cause){caught=std::string(cause.what())=="original member callback exception";}
        need(caught&&t::page_read_scope==parent,"actual saved Scenery callback scope unwinds original exception");
    }
    p::NativeCatalog catalog{memory,image};std::string type,variant;
    p::original_calls=p::manager_reads=0;start=count(t::Part::HostMemoryVirtualQuery);
    need(catalog.identify(8,2,type,variant,error)&&type=="cactus"&&variant=="p_ext_treecactus_01.vmb","actual catalogue method preserves exact native type/variant and roundtrip");
    need(p::original_calls==1&&p::manager_reads==2&&!t::page_read_scope&&count(t::Part::HostMemoryVirtualQuery)-start==5,"actual catalogue has two independent field scopes and leaves all3 original allocating-call queries unscoped");
    {t::PageReadScope original(t::PageBoundary::OriginalCall);p::expected_parent=t::page_read_scope;p::original_calls=p::manager_reads=0;
        need(catalog.identify(8,2,type,variant,error)&&p::original_calls==1&&t::page_read_scope==p::expected_parent,"actual catalogue restores the original native boundary around its single allocating call");
    }p::expected_parent=nullptr;
    p::fail_code=true;p::original_calls=p::manager_reads=0;error.clear();
    need(!catalog.identify(8,2,type,variant,error)&&error=="original catalogue body guard failure"&&p::original_calls==0&&!t::page_read_scope,"catalogue body failure does not call original or leak metadata scope");p::fail_code=false;
    p::changed_manager=true;p::original_calls=p::manager_reads=0;error.clear();
    need(!catalog.identify(8,2,type,variant,error)&&error=="scenery_catalog_actual_manager_changed"&&p::original_calls==1&&!t::page_read_scope,"independent post-call fields freshly detect changed native catalogue manager");p::changed_manager=false;
    t::totals={};
}
