#include "live_host_state/memory_query_timing.h"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>
namespace timing=mt2mp::game::live_host_state::apply_timing;
namespace live_host_state=mt2mp::game::live_host_state;
int checks=0;
void need(bool yes,const char* label){++checks;if(!yes)throw std::runtime_error(label);}
bool fail(std::string& e,const char* label){e=label;return false;}
struct ProcessMemory {bool copy(std::uintptr_t,void*,std::size_t,std::string&);};
#include "reader_copy.inc"
ProcessMemory memory;
unsigned char* allocation=nullptr;
bool injected_failure=false,injected_exception=false,mutate_bytes=false;
uint64_t queries(){return timing::totals[size_t(timing::Part::MemoryVirtualQuery)].count;}
bool scan(std::string& e){
    if(injected_exception)throw std::runtime_error("original reader exception");
    if(injected_failure)return fail(e,"original owner read failure");
    for(size_t i=0;i<1000;++i){unsigned char value=0;if(mutate_bytes&&i==500)allocation[0]=72;
        if(!memory.copy(uintptr_t(allocation),&value,1,e))return false;
        if(value!=(mutate_bytes&&i>=500?72:allocation[0]))return fail(e,"reader returned stale value");}
    return true;
}
namespace development {
struct NativePort {};
struct StartingpointSnapshot {int value=0;};
namespace detail {
bool observe(NativePort&,nlohmann::json& out,std::string& e,StartingpointSnapshot*,bool,bool=false,const std::string& = "startingpoint");
bool observe(NativePort&,nlohmann::json& out,std::string& e,StartingpointSnapshot*,bool,bool,const std::string&){const bool okay=scan(e);out=okay;return okay;}
}
#include "development_entries.inc"
}
namespace weapon {
namespace action_session {enum class Role {Host,Client};}
struct Lease {uintptr_t toon=0,subscriber=0;struct {action_session::Role role=action_session::Role::Client;} context;};
struct Plan {int value=0;};
struct Call {};
uintptr_t image=1;
bool owner(uintptr_t toon,Lease& lease,bool& owned,std::string& e){owned=true;lease.toon=toon;lease.subscriber=2;return scan(e);}
struct {decltype(&owner) owner=weapon::owner;} callbacks;
std::atomic<bool> ready=true;
bool verify_hook(std::string& e){return scan(e);}
bool read_plan(ProcessMemory&,uintptr_t,uintptr_t,Plan&,std::string& e){return scan(e);}
bool read_attachment(ProcessMemory&,const Plan&,bool& same,std::string& e){same=true;return scan(e);}
struct Native {
#include "weapon_reader_entries.inc"
};
}
#include "tutorial_outer_metadata_cases.inc"
#include "day_reader_scope_cases.inc"
int main(){try{
    allocation=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));need(allocation!=nullptr,"private test allocation");allocation[0]=31;
    timing::Scope measured(timing::Part::Take);std::string error;
    SetLastError(123);auto start=queries();need(scan(error),"uncached actual protected reader");need(queries()-start==1000,"baseline performs1000 real metadata queries");
    auto run=[&](int which)->bool{nlohmann::json out;development::StartingpointSnapshot snapshot;weapon::Native calls;weapon::Plan plan;weapon::Lease lease;bool owned=false,same=false;
        switch(which){case 0:return development::read(out,error);case 1:return development::read_startingpoint(snapshot,out,error,true,false);case 2:return development::read_research("bridges",snapshot,out,error,true,false);case 3:return calls.pin(1,lease,owned,error);case 4:return calls.plan(1,plan,error);default:return calls.matches(plan,same,error);}};
    for(int entry=0;entry<6;++entry){
        start=queries();SetLastError(123);need(run(entry),"actual scoped entry succeeds");need(queries()-start<=2,"actual entry reduces repeated metadata queries");need(GetLastError()==123,"incoming LastError preserved");need(timing::page_read_scope==nullptr,"scope ends before caller can mutate");
        mutate_bytes=true;allocation[0]=31;need(run(entry)&&allocation[0]==72,"live changed bytes still observed");mutate_bytes=false;
        injected_failure=true;error.clear();need(!run(entry)&&error=="original owner read failure","original failure not concealed");injected_failure=false;need(timing::page_read_scope==nullptr,"failed entry leaves no cache");
        injected_exception=true;bool caught=false;try{run(entry);}catch(const std::runtime_error& x){caught=std::string(x.what())=="original reader exception";}injected_exception=false;need(caught&&timing::page_read_scope==nullptr,"original exception preserved and cache unwound");
        DWORD previous=0;need(VirtualProtect(allocation,4096,PAGE_NOACCESS,&previous)!=0,"test page protection changed");error.clear();need(!run(entry)&&error=="error: native placement UI page unreadable","next entry rechecks changed page metadata");DWORD ignored=0;need(VirtualProtect(allocation,4096,previous,&ignored)!=0,"restore owned test page");
    }
    {timing::PageReadScope outer;unsigned char value=0;need(memory.copy(uintptr_t(allocation),&value,1,error),"outer reader prime");const auto before=queries();nlohmann::json out;need(development::read(out,error),"nested actual reader");need(memory.copy(uintptr_t(allocation),&value,1,error)&&queries()>before+1,"nested entry invalidates caller cache on return");}
    outer_metadata_cases();
    day_reader_scope_cases();
    need(timing::page_read_scope==nullptr,"no retained global cache");need(VirtualFree(allocation,0,MEM_RELEASE)!=0,"release only private test allocation");allocation=nullptr;
    std::cout<<checks<<" actual reader-scope checks passed; uncached1000 queries, scoped<=2 per entry\n";return 0;
}catch(const std::exception& x){std::cerr<<"failure after "<<checks<<": "<<x.what()<<'\n';if(allocation)VirtualFree(allocation,0,MEM_RELEASE);return 1;}}
