#include "live_host_state/memory_query_timing.h"
#include "tutorial_replication/state.h"
#include "tutorial_development.h"
#include "placement_ui_probe/ui_probe.h"
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
namespace timing=mt2mp::game::live_host_state::apply_timing;
namespace live_host_state=mt2mp::game::live_host_state;
using mt2mp::game::tutorial_replication::Cursor;
using mt2mp::game::tutorial_replication::State;
namespace startingpoint=mt2mp::game::tutorial_replication::startingpoint;
namespace action_session {enum class Role {Host,Client};struct Context {Role role=Role::Client;};struct Binding {};}
using World=mt2mp::game::building_mixed_identity::opened_file::CurrentWorld;
unsigned checks=0;uint64_t copies=0;uintptr_t blocked=0;bool throw_read=false;
void need(bool value,const std::string& message){++checks;if(!value)throw std::runtime_error(message);}
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
namespace actual_placement {
class ProcessMemory {public:bool copy(std::uintptr_t,void*,std::size_t,std::string&);};
#include "placement_copy.inc"
}
struct Memory {
    actual_placement::ProcessMemory actual;
    bool copy(uintptr_t p,void* out,size_t n,std::string& e){
        ++copies;if(throw_read)throw std::runtime_error("original tutorial read exception");
        if(p==blocked)return fail(e,"original tutorial read failure");
        return actual.copy(p,out,n,e);
    }
} memory;
namespace actual_observer {
#include "observer_copy.inc"
namespace live_host_state=mt2mp::game::live_host_state;
struct Port {
#include "observer_member.inc"
};
}
uintptr_t base=0,image=0x140000000;World world;
int origin_calls=0;bool changed_origin=false,observer_failure=false,quarantine=false;
template<class T>void put(uintptr_t p,const T& value){std::memcpy(reinterpret_cast<void*>(p),&value,sizeof value);}
template<class T>bool at(uintptr_t p,size_t offset,T& value,std::string& e){return memory.copy(p+offset,&value,sizeof value,e);}
#include "tutorial_strings.inc"
void string_at(uintptr_t object,uintptr_t storage,const std::string& text){put(object,storage);put(object+8,uint64_t(text.size()));std::memcpy(reinterpret_cast<void*>(storage),text.c_str(),text.size()+1);}
bool supported(const std::string& id){return id=="initial_tutorial"||id=="leveltwo_tutorial";}
bool same_world(const World& a,const World& b){return a.world==b.world&&a.generation==b.generation;}
bool origin(const action_session::Context&,const action_session::Binding&,World& out,std::string& e){
    ++origin_calls;uintptr_t magic=0;if(!at(base,0,magic,e))return false;
    out=world;if(changed_origin&&origin_calls>1)++out.generation;return true;
}
struct {std::set<uintptr_t> sequences,quarantined;} load;
struct NativeCursor {uintptr_t instance=0,sequence=0,steps=0;Cursor value;};
namespace tutorial_observer {
bool read(nlohmann::json& out,std::string& e){
    if(observer_failure)return fail(e,"original tutorial observer failure");
    actual_observer::Port port;timing::PageReadScope pages;uintptr_t value=0;
    if(!port.copy(base,&value,sizeof value,e))return false;
    out={{"initial_tutorial_complete",false},{"leveltwo_tutorial_complete",false}};return true;
}
}
namespace tutorial_development {
using StartingpointSnapshot=mt2mp::game::tutorial_development::StartingpointSnapshot;
bool read_startingpoint(StartingpointSnapshot& out,nlohmann::json&,std::string&,bool,bool){out.origin=world;out.row=startingpoint::Row{};return true;}
}
struct {World origin;uintptr_t tech=0,record=0;std::optional<startingpoint::CommitBugs> bugs;} commit_receipt;
namespace display_native {
bool capture(const action_session::Context&,const action_session::Binding&,const tutorial_development::StartingpointSnapshot&,std::optional<mt2mp::game::tutorial_replication::staff_display::State>&,std::string&,bool){return true;}
}
namespace lifecycle_native {bool decorate(const World&,const action_session::Context&,State&,std::string&){return true;}}
bool research_capture(const action_session::Context&,const action_session::Binding&,const World&,State&,std::string&){return true;}
namespace message_progress {
struct Binding {std::optional<mt2mp::game::tutorial_replication::MessageProgress> values;};
bool read(Memory&,uintptr_t,uintptr_t,int32_t,Binding&,std::string&){return true;}
}
namespace mt2mp::game::tutorial_replication {
bool valid(const State&,std::string&){return true;}
Start command_policy(const std::string& command,bool){return command=="safe"?Start::Native:Start::Reject;}
namespace startingpoint {bool accepts_complete_tech(const std::string&,int32_t,const std::string&,const std::optional<Row>&){return false;}}
}
using mt2mp::game::tutorial_replication::Start;
using mt2mp::game::tutorial_replication::command_policy;
#include "tutorial_readers.inc"
#include "baseline_tutorial_readers.inc"
uint64_t queries(){return timing::totals[size_t(timing::Part::MemoryVirtualQuery)].count;}
void setup(){
    std::memset(reinterpret_cast<void*>(base),0,0x40000);world={};
    world.state=reinterpret_cast<void*>(base);world.world=reinterpret_cast<void*>(base+0x100);world.generation=1;
    put(base,uintptr_t(17));put(base+0x1e8,base+0x1000);put(base+0x1020,base+0x2000);put(base+0x1028,int32_t(1));put(base+0x2000,base+0x3000);
    string_at(base+0x3018,base+0x9000,"initial_tutorial");string_at(base+0x3060,base+0x9100,"step106");
    put(base+0x3058,base+0x4000);put(base+0x4020,base+0x5000);put(base+0x4028,int32_t(107));put(base+0x4050,int32_t(107));
    put(base+0x3048,base+0x6000);put(base+0x3050,int32_t(107));
    for(size_t i=0;i<107;++i){const auto step=base+0x10000+i*0x100;put(base+0x5000+i*8,step);put(step,image+0x16fda00);
        const auto name="step"+std::to_string(i);string_at(step+0x20,base+0x20000+i*64,name);string_at(base+0x6000+i*32,base+0x20000+i*64,name);}
    load.sequences={base+0x4000};load.quarantined.clear();origin_calls=0;changed_origin=observer_failure=false;blocked=0;throw_read=false;
}
int main(){void* allocation=nullptr;try{
    allocation=VirtualAlloc(nullptr,0x40000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);need(allocation!=nullptr,"owned tutorial fixture");base=reinterpret_cast<uintptr_t>(allocation);setup();
    timing::Scope take(timing::Part::Take);action_session::Context context;action_session::Binding binding;std::string e;
    State old_state,current;std::vector<NativeCursor> old_cursors,cursors;auto before=queries();auto reads=copies;
    need(baseline_native_state(context,binding,old_state,old_cursors,e),e);const auto baseline_queries=queries()-before,baseline_copies=copies-reads;
    origin_calls=0;before=queries();reads=copies;need(native_state(context,binding,current,cursors,e),e);
    need(current==old_state&&current.active.size()==1&&current.active[0].history.size()==107&&cursors[0].value==old_cursors[0].value,"full original native_state output unchanged");
    need(copies-reads==baseline_copies&&baseline_queries>300&&queries()-before<10,"complete live reads retained while native_state metadata queries collapse");
    std::cout<<"native_state OS queries "<<baseline_queries<<" -> "<<queries()-before<<", live reads="<<baseline_copies<<'\n';
    auto target=current.active[0];before=queries();reads=copies;need(baseline_interval(cursors[0],target,std::nullopt,e),e);const auto interval_queries=queries()-before,interval_copies=copies-reads;
    before=queries();reads=copies;need(interval(cursors[0],target,std::nullopt,e),e);
    need(copies-reads==interval_copies&&interval_queries>400&&queries()-before<5,"all107 step/history guards retained inside interval scope");
    std::cout<<"interval OS queries "<<interval_queries<<" -> "<<queries()-before<<", live reads="<<interval_copies<<'\n';
    target.history.push_back("foreign");need(!interval(cursors[0],target,std::nullopt,e)&&e=="Host tutorial history ID absent from local sequence","foreign history guard preserved");
    target=current.active[0];target.next=106;need(!interval(cursors[0],target,std::nullopt,e)&&e=="tutorial backward/count/completion transition differs","backward cursor guard preserved");
    put(base+0x1028,int32_t(65));need(!native_state(context,binding,current,cursors,e)&&e=="tutorial scenario count outside0..64","scenario count guard preserved");setup();
    blocked=base+0x2000;need(!native_state(context,binding,current,cursors,e)&&e=="original tutorial read failure","original read error retained");setup();
    load.quarantined.insert(base+0x4000);need(!native_state(context,binding,current,cursors,e)&&e=="Client tutorial constructor remains unconfirmed","joined quarantine guard preserved");setup();
    changed_origin=true;e.clear();need(!native_state(context,binding,current,cursors,e)&&e=="tutorial origin changed during capture","fresh second Origin read retained");setup();
    observer_failure=true;need(!native_state(context,binding,current,cursors,e)&&e=="original tutorial observer failure","upstream observer failure retained");setup();
    throw_read=true;bool caught=false;try{native_state(context,binding,current,cursors,e);}catch(const std::runtime_error& x){caught=std::string(x.what())=="original tutorial read exception";}
    need(caught&&timing::page_read_scope==nullptr,"read exception remains visible and scope closes");setup();
    actual_observer::Port observer;uintptr_t value=0;before=queries();for(int i=0;i<1000;++i)need(observer.copy(base,&value,sizeof value,e),e);need(queries()-before==1000,"actual observer adapter reports uncached OS calls");
    before=queries();{timing::PageReadScope reader;for(int i=0;i<1000;++i)need(observer.copy(base,&value,sizeof value,e),e);put(base,uintptr_t(99));need(observer.copy(base,&value,sizeof value,e)&&value==99,"observer still copies changed live bytes");}
    need(queries()-before==1,"actual observer adapter reuses only page metadata");
    DWORD old=0;need(VirtualProtect(allocation,4096,PAGE_NOACCESS,&old)!=0,"change protection after reader exit");
    need(!observer.copy(base,&value,sizeof value,e)&&e=="tutorial native memory not readable","next observer rejects fresh protection change with original cause");
    need(VirtualFree(allocation,0,MEM_RELEASE)!=0,"release owned fixture");allocation=nullptr;std::cout<<checks<<" tutorial reader checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"failure: "<<e.what()<<'\n';if(allocation&&!VirtualFree(allocation,0,MEM_RELEASE))std::cerr<<"owned fixture release failed\n";return 1;}}
