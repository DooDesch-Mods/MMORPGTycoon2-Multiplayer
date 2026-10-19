#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
SIZE_T observed_query(LPCVOID,PMEMORY_BASIC_INFORMATION,SIZE_T);
#define VirtualQuery observed_query
#include "live_host_state/memory_query_timing.h"
#undef VirtualQuery
#include <iostream>
#include <stdexcept>
#include <thread>

namespace t=mt2mp::game::live_host_state::apply_timing;
int checks=0;
void need(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
constexpr t::Part categories[]={t::Part::QueryPlacement,t::Part::QueryOpenWitness,t::Part::QueryEngine,
    t::Part::QuerySavedFields,t::Part::QueryWorldProp,t::Part::QueryOther};
struct Clock {
    using time_point=std::chrono::steady_clock::time_point;
    static inline uint64_t calls=0;
    static time_point now(){++calls;SetLastError(987);return time_point(std::chrono::nanoseconds(calls*100));}
};
struct Fake {
    static inline int calls=0;
    static inline DWORD incoming=0,result_error=0;
    static inline SIZE_T result=0;
    static inline bool throws=false;
    static inline LPCVOID address=nullptr;
    static inline PMEMORY_BASIC_INFORMATION output=nullptr;
    static inline SIZE_T size=0;
    static SIZE_T query(LPCVOID p,PMEMORY_BASIC_INFORMATION out,SIZE_T n){
        ++calls;incoming=GetLastError();address=p;output=out;size=n;
        if(throws)throw std::runtime_error("original query exception");
        if(out)out->State=MEM_COMMIT;
        SetLastError(result_error);return result;
    }
};
uint64_t count(t::Part part){return t::totals[size_t(part)].count;}
void partition(){
    uint64_t calls=0,ns=0;
    for(auto part:categories){calls+=count(part);ns+=t::totals[size_t(part)].total_ns;}
    need(calls==count(t::Part::MemoryVirtualQuery),"adapter call counts partition aggregate exactly");
    need(ns==t::totals[size_t(t::Part::MemoryVirtualQuery)].total_ns,"adapter elapsed times partition aggregate exactly");
}
#include "query_prefix_cases.inc"
#include "query_span_cases.inc"
#include "query_scope_cases.inc"
#include "query_phase_cases.inc"
#include "npc_find_scope_cases.inc"
#include "open_witness_scope_cases.inc"
#include "npc_saved_metadata_cases.inc"
#include "world_prop_attribution_cases.inc"
#include "character_definition_scope_cases.inc"
#include "pure_reader_followup_cases.inc"
#include "region_identity_scope_cases.inc"
#include "dungeon_roster_scope_cases.inc"
#include "party_pool_scope_cases.inc"
#include "shared_census_scope_cases.inc"
#include "general_project_identity_cases.inc"
#include "composed_character_census_cases.inc"
void host_capture_query_cases(void*& owned){
    t::totals={};Clock::calls=0;Fake::calls=0;
    need(t::host_capture_depth==0&&t::host_query_part==t::Part::HostQueryOther,"Host query attribution initially inactive");
    MEMORY_BASIC_INFORMATION info{};const auto address=reinterpret_cast<LPCVOID>(uintptr_t(0x123456));
    constexpr std::pair<t::Part,t::Part> stages[]={
        {t::Part::HostCaptureManagement,t::Part::HostQueryManagement},
        {t::Part::HostCaptureManagementFinish,t::Part::HostQueryManagementFinish},
        {t::Part::HostCaptureEconomy,t::Part::HostQueryEconomy},
        {t::Part::HostCaptureParty,t::Part::HostQueryParty},
        {t::Part::HostCaptureRelease,t::Part::HostQueryRelease}};
    {
        t::Scope capture(t::Part::HostCapture);
        for(const auto& [stage,category]:stages){
            const auto before=count(category),all=count(t::Part::HostMemoryVirtualQuery);
            t::Scope measured(stage);Fake::result=17;Fake::result_error=66;SetLastError(55);
            need(t::detail::query<Fake::query,Clock>(address,&info,sizeof info,t::Part::QuerySavedFields)==17,"Host adapter preserves exact original result");
            need(Fake::incoming==55&&GetLastError()==66&&Fake::address==address&&Fake::output==&info&&Fake::size==sizeof info,"Host timer preserves query arguments and LastError");
            need(count(category)==before+1&&count(t::Part::HostMemoryVirtualQuery)==all+1,"one completed Host query has one stage and one total");
            need(t::totals[size_t(category)].total_ns==100,"Host stage preserves exact measured nanoseconds");
        }
        need(t::host_query_part==t::Part::HostQueryOther,"Host child restores surrounding stage");
        Fake::result=0;Fake::result_error=ERROR_INVALID_PARAMETER;SetLastError(77);
        need(t::detail::query<Fake::query,Clock>(address,&info,3,t::Part::QueryPlacement)==0&&GetLastError()==ERROR_INVALID_PARAMETER,"Host OS query failure remains visible");
        need(count(t::Part::HostQueryOther)==1&&count(t::Part::MemoryVirtualQuery)==0,"Host-only queries do not alter Client Take counters");
        {t::Scope management(t::Part::HostCaptureManagement);
            {t::Scope release(t::Part::HostCaptureRelease);need(t::host_query_part==t::Part::HostQueryRelease,"nested Host stage selects its own category");}
            need(t::host_query_part==t::Part::HostQueryManagement,"nested Host stage restores parent category");}
        const auto completed=count(t::Part::HostMemoryVirtualQuery);Fake::throws=true;bool caught=false;
        try{t::detail::query<Fake::query,Clock>(address,&info,sizeof info);}
        catch(const std::runtime_error& error){caught=std::string(error.what())=="original query exception";}Fake::throws=false;
        need(caught&&count(t::Part::HostMemoryVirtualQuery)==completed,"Host original exception has no false completed-query receipt");
        uint64_t worker_queries=99;size_t worker_depth=99;const auto parent=count(t::Part::HostMemoryVirtualQuery);
        std::thread worker([&]{worker_depth=t::host_capture_depth;{t::Scope own(t::Part::HostCapture);Fake::result=9;t::detail::query<Fake::query,Clock>(address,&info,sizeof info);}worker_queries=count(t::Part::HostMemoryVirtualQuery);});worker.join();
        need(worker_depth==0&&worker_queries==1&&count(t::Part::HostMemoryVirtualQuery)==parent,"Host capture attribution stays thread-local");
        owned=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);need(owned!=nullptr,"Host owned test page");
        const auto before=count(t::Part::HostMemoryVirtualQuery);
        {t::PageReadScope pages;
            SetLastError(321);need(t::virtual_query(owned,&info,sizeof info,t::Part::QueryPlacement)==sizeof info,"Host actual OS miss remains measured");
            need(t::virtual_query(owned,&info,sizeof info,t::Part::QuerySavedFields)==sizeof info&&GetLastError()==321,"Host existing metadata hit preserves result and LastError");
            need(count(t::Part::HostMemoryVirtualQuery)==before+1,"Host metadata hit is not counted as a second OS query");}
        need(t::virtual_query(owned,&info,sizeof info)==sizeof info&&count(t::Part::HostMemoryVirtualQuery)==before+2,"Host scope exit still performs fresh OS query");
        need(VirtualFree(owned,0,MEM_RELEASE)!=0,"release Host owned test page");owned=nullptr;
    }
    need(t::host_capture_depth==0&&t::host_query_part==t::Part::HostQueryOther,"Host capture restores depth and stage");
    try{t::Scope outer(t::Part::HostCapture);t::Scope inner(t::Part::HostCaptureEconomy);throw std::runtime_error("Host scope unwind");}
    catch(const std::runtime_error& error){need(std::string(error.what())=="Host scope unwind","Host scope preserves exception");}
    need(t::host_capture_depth==0&&t::host_query_part==t::Part::HostQueryOther,"Host capture attribution unwinds on exception");
    for(const char* name:{"host_memory_virtual_query","host_query_management","host_query_management_finish","host_query_economy","host_query_party","host_query_release","host_query_other"})
        need(t::snapshot().contains(name),"Host query attribution exported through existing snapshot");
    t::totals={};Clock::calls=0;Fake::calls=0;Fake::result=0;Fake::result_error=0;Fake::throws=false;
}
int main(){void* owned=nullptr;try{
    world_prop_attribution_cases();
    npc_saved_metadata_cases();
    open_witness_scope_cases();
    prefix_cases();
    npc_find_scope_cases();
    phase_cases();
    scope_cases();
    span_cases();
    character_definition_scope_cases();
    pure_reader_followup_cases();
    region_identity_scope_cases();
    dungeon_roster_scope_cases();
    party_pool_scope_cases();
    shared_census_scope_cases();
    project_identity_fixture::checks();
    composed_character_fixture::checks();
    host_capture_query_cases(owned);
    MEMORY_BASIC_INFORMATION info{};
    const auto address=reinterpret_cast<LPCVOID>(uintptr_t(0x123456));
    Fake::result=17;Fake::result_error=44;SetLastError(33);
    need(t::detail::query<Fake::query,Clock>(address,&info,sizeof info,t::Part::QueryEngine)==17,"outside apply original result");
    need(Fake::calls==1&&Fake::incoming==33&&GetLastError()==44,"outside apply LastError");
    need(Clock::calls==0&&count(t::Part::MemoryVirtualQuery)==0,"outside apply has no timer or category record");
    {
        t::Scope take(t::Part::Take);
        for(auto part:categories){
            const auto original_calls=Fake::calls;const auto previous_clock=Clock::calls;
            const auto before=count(part);Fake::result=sizeof info;Fake::result_error=66;SetLastError(55);
            need(t::detail::query<Fake::query,Clock>(address,&info,sizeof info,part)==sizeof info,"original query result preserved");
            need(Fake::calls==original_calls+1&&Fake::address==address&&Fake::output==&info&&Fake::size==sizeof info,"one original with exact arguments");
            need(Fake::incoming==55&&GetLastError()==66,"timer does not alter original LastError inputs or output");
            need(Clock::calls==previous_clock+2&&count(part)==before+1,"one timer feeds one adapter entry");
            need(t::totals[size_t(part)].total_ns==100,"exact measured nanoseconds retained");
            Fake::result=0;Fake::result_error=ERROR_INVALID_PARAMETER;SetLastError(77);
            need(t::detail::query<Fake::query,Clock>(address,&info,3,part)==0&&GetLastError()==ERROR_INVALID_PARAMETER,"query failure not suppressed");
            need(count(part)==before+2,"failed OS query included in category");partition();
        }
        const auto before=count(t::Part::QueryOther);Fake::result=9;Fake::result_error=88;
        need(t::detail::query<Fake::query,Clock>(address,&info,sizeof info)==9&&count(t::Part::QueryOther)==before+1,"unchanged callers use Other category");
        const auto total=count(t::Part::MemoryVirtualQuery);Fake::throws=true;bool caught=false;
        try{t::detail::query<Fake::query,Clock>(address,&info,sizeof info,t::Part::QueryPlacement);}
        catch(const std::runtime_error& e){caught=std::string(e.what())=="original query exception";}Fake::throws=false;
        need(caught&&count(t::Part::MemoryVirtualQuery)==total,"original exception remains visible without false completed-query receipt");
        owned=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);need(owned!=nullptr,"owned test page");
        auto before_os=count(t::Part::MemoryVirtualQuery),before_placement=count(t::Part::QueryPlacement),before_engine=count(t::Part::QueryEngine);
        {t::PageReadScope read;
            SetLastError(321);need(t::virtual_query(owned,&info,sizeof info,t::Part::QueryPlacement)==sizeof info,"actual OS query on cache miss");
            need(t::virtual_query(owned,&info,sizeof info,t::Part::QueryEngine)==sizeof info,"second category can use same metadata cache");
            need(count(t::Part::MemoryVirtualQuery)==before_os+1&&count(t::Part::QueryPlacement)==before_placement+1&&count(t::Part::QueryEngine)==before_engine,"cache hit is not an OS query or category event");
            need(GetLastError()==321,"cached query preserves LastError");
        }
        need(t::virtual_query(owned,&info,sizeof info,t::Part::QuerySavedFields)==sizeof info,"outside read scope remains fresh");
        need(count(t::Part::MemoryVirtualQuery)==before_os+2,"scope exit discards cache");partition();
        const auto snapshot=t::snapshot();
        for(const char* name:{"query_placement","query_open_witness","query_engine","query_saved_fields","query_world_prop","query_other"})
            need(snapshot.contains(name)&&snapshot.at(name).contains("count")&&snapshot.at(name).contains("total_ns")&&snapshot.at(name).contains("max_ns"),"existing diagnostic snapshot exports category");
        uint64_t thread_count=0;const auto parent=count(t::Part::MemoryVirtualQuery);
        std::thread worker([&]{t::Scope own_take(t::Part::Take);MEMORY_BASIC_INFORMATION value{};
            t::virtual_query(owned,&value,sizeof value,t::Part::QueryWorldProp);thread_count=count(t::Part::MemoryVirtualQuery);});worker.join();
        need(thread_count==1&&count(t::Part::MemoryVirtualQuery)==parent,"foreign thread counters remain isolated");
    }
    need(t::quest_store_depth==0,"Quest attribution initially inactive");
    try{t::Scope outer(t::Part::QuestStores);
        need(t::quest_store_depth==1,"Quest phase activates attribution");
        {t::Scope nested(t::Part::QuestStores);need(t::quest_store_depth==2,"nested Quest phase retains nesting");}
        need(t::quest_store_depth==1,"nested Quest phase restores outer attribution");
        size_t worker_depth=99;std::thread worker([&]{worker_depth=t::quest_store_depth;});worker.join();
        need(worker_depth==0,"Quest attribution is thread-local");
        throw std::runtime_error("retained timing test exception");
    }catch(const std::runtime_error& e){need(std::string(e.what())=="retained timing test exception","original exception preserved");}
    need(t::quest_store_depth==0,"Quest phase unwinds on exception");
    for(const char* name:{"quest_store_toon_read","quest_store_subscriber_guard","quest_store_fresh_link_guard","quest_store_scalar_qualified","quest_store_nonqualified","quest_store_canonical_owners","quest_store_admit_history","quest_store_restore_pause"})
        need(t::snapshot().contains(name),"Quest attribution exported through existing snapshot");
    need(t::take_depth==0,"take depth restored");need(VirtualFree(owned,0,MEM_RELEASE)!=0,"release only owned test page");owned=nullptr;
    std::cout<<checks<<" query attribution checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"failure after "<<checks<<": "<<e.what()<<'\n';if(owned)VirtualFree(owned,0,MEM_RELEASE);return 1;}}
