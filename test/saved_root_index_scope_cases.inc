#include "object_identity/remove_runtime/copy/saved_scenery_batch.h"
namespace saved_root_index_fixture {
template<class Read>
bool candidate(uintptr_t native_image,uintptr_t saved_map,uintptr_t root_shard,
    mt2mp::game::region_saved_identity::Index& result,Read& at,std::string& error){
#include "saved_root_index_capture.inc"
    return capture_index(native_image,saved_map,root_shard,result,at,error);
}
}

void saved_root_index_scope_cases(){
    namespace index=mt2mp::game::region_saved_identity;
    region_scope_fixture::Fixture fixture;std::string error;index::Index before,after;
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return fixture.read(object,offset,value,cause);};
    auto baseline=[&](index::Index& out){return index::capture(fixture.image,fixture.map,fixture.root,out,read,error);};
    auto candidate=[&](index::Index& out){return saved_root_index_fixture::candidate(fixture.image,fixture.map,fixture.root,out,read,error);};
    mt2mp::build_actions::private_native::host::SavedSceneryBatch effect;
    auto revision=t::original_call_revision;
    need(effect.admit(fixture.image,fixture.map,fixture.root,revision,error),"batch admits current actual native boundary revision");
    {t::PageReadScope original(t::PageBoundary::OriginalCall);
        need(t::original_call_revision==revision+1&&!t::PageReadScope::reader_active(),"actual OriginalCall advances the native boundary and suspends page reuse");
    }
    error.clear();need(!effect.admit(fixture.image,fixture.map,fixture.root,t::original_call_revision,error)&&
        error=="saved Scenery batch native boundary or owner changed","actual native boundary invalidates retained saved membership");
    need(!t::page_read_scope,"native boundary regression restores current reader");error.clear();
    t::totals={};t::Scope capture(t::Part::HostCapture);
    auto start=count(t::Part::HostMemoryVirtualQuery);
    need(baseline(before)&&baseline(after)&&before==after,"historical saved Root before/after index validates all121 Regions");
    const auto original_trace=fixture.events;const auto original_queries=count(t::Part::HostMemoryVirtualQuery)-start;
    need(original_trace.size()==1002&&original_queries==1002,"unscoped saved Root scans query each field before reading it");
    fixture.reset();error.clear();start=count(t::Part::HostMemoryVirtualQuery);
    need(candidate(before)&&!t::page_read_scope&&candidate(after)&&!t::page_read_scope&&before==after,
         "actual saved Root callback closes its first extent before the second scan");
    need(fixture.events==original_trace,"actual saved Root callback preserves every field value and native read order");
    need(count(t::Part::HostMemoryVirtualQuery)-start==2,"actual saved Root callback uses one OS query per complete owned allocation scan");

    // The second invocation must read changed values even at the same addresses.
    fixture.put(fixture.region(120)+0x834,int32_t(999));error.clear();
    need(candidate(after)&&after!=before&&after.saved.back().first==999,
         "saved Root scan has no retained Region ID or index value cache");

    for(unsigned kind=0;kind<5;++kind){
        auto corrupt=[&]{
            if(kind==0)fixture.put(fixture.region(120)+0x834,int32_t(0));
            if(kind==1)fixture.put(fixture.region(120)+0x70,fixture.root+8);
            if(kind==2)fixture.put(fixture.data+120*8,fixture.region(0));
            if(kind==3)fixture.put(fixture.owned,fixture.region(120)+8);
            if(kind==4)fixture.mutation_address=fixture.map+0x240;
        };
        fixture.reset();error.clear();corrupt();need(!baseline(before),"historical saved Root owner corruption fails");
        const auto cause=error;const auto trace=fixture.events;
        fixture.reset();error.clear();corrupt();
        need(!candidate(after)&&error==cause&&fixture.events==trace&&!t::page_read_scope,
             "actual saved Root scan preserves original failure cause and first failing field");
    }
    fixture.reset();error.clear();fixture.throw_address=fixture.region(60)+0x834;bool caught=false;
    try{candidate(after);}catch(const std::runtime_error& cause){caught=std::string(cause.what())=="original region read exception";}
    need(caught&&!t::page_read_scope,"actual saved Root scan unwinds its page metadata on the original exception");

    fixture.reset();error.clear();
    {t::PageReadScope parent;auto* enclosing=t::page_read_scope;
        start=count(t::Part::HostMemoryVirtualQuery);
        need(candidate(before)&&t::page_read_scope==enclosing,"saved Root scan restores the enclosing reader");
        need(candidate(after)&&t::page_read_scope==enclosing&&count(t::Part::HostMemoryVirtualQuery)-start==2,
             "separate scans cannot borrow cached page metadata across their intervening native work");
        {t::PageReadScope original(t::PageBoundary::OriginalCall);
            need(candidate(after)&&!t::PageReadScope::reader_active(),"scan reentry restores the enclosing original-call boundary");
        }
    }
    need(!t::page_read_scope,"no saved Root scan metadata survives callback return");

    fixture.reset();error.clear();need(candidate(before),"saved Root first scan before owned page protection change");
    DWORD previous=0,replaced=0;
    const bool protected_page=VirtualProtect(fixture.memory,0x80000,PAGE_NOACCESS,&previous)!=0;
    need(protected_page,"protect only the saved Root fixture allocation between scans");
    const bool readable=candidate(after);const auto unreadable=error;
    const bool restored=VirtualProtect(fixture.memory,0x80000,previous,&replaced)!=0;
    need(restored,"restore only the saved Root fixture allocation");
    need(!readable&&unreadable.find("unreadable")!=std::string::npos&&!t::page_read_scope,
         "second complete scan freshly observes native page protection changes");
    std::cout<<"Saved Root scan exact value reads="<<original_trace.size()<<" OS queries="<<original_queries<<"->2 per before/after pair\n";
    t::totals={};
}
