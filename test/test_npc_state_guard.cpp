#include "live_host_state/day_rollover/economy/state_difference.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <cstring>
namespace economy=mt2mp::game::live_host_state::day::economy;
namespace quest_state=mt2mp::subscriber_quest_state;
using namespace economy;
using Pin=mt2mp::game::live_host_state::Pin;
namespace {
struct QuestOwner {std::string key;uintptr_t npc=1;};
std::vector<QuestOwner> native_owners;
std::vector<quest_state::Npc> native_rows;
unsigned checks=0,reads=0,counters=0;
bool read_error=false,current_error=false,corrupt_post=false,empty_post=false,counter_error=false;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
void need(bool value,const std::string& e){if(!value)throw std::runtime_error(e);++checks;}
namespace npc_probe::action_runtime {bool state_owners(const auto&,const auto&,std::vector<QuestOwner>& out,std::string& e){if(read_error)return fail(e,"original owner read failed");out=native_owners;return true;}}
bool quest_npcs(const Pin&,const std::vector<QuestOwner>&,std::vector<quest_state::Npc>& out,std::string&){++reads;out=native_rows;if(reads==2){if(corrupt_post)out[0].completed.today+=1;if(empty_post)out.clear();}return true;}
bool store_npc_counters(const Pin&,const QuestOwner&,const quest_state::Npc&,const quest_state::Npc& target,const std::function<bool(std::string&)>& guard,std::string& e){++counters;if(!guard(e))return false;if(counter_error)return fail(e,"original DailyCount CAS failed");const auto pose=native_rows[0].pose;native_rows[0]=target;native_rows[0].pose=pose;return true;}
struct Memory {bool copy(uintptr_t,void*,size_t,std::string& e){return fail(e,"unexpected transform memory read");}} memory;
template<class T>bool at(uintptr_t,size_t,T&,std::string& e){return fail(e,"unexpected transform field read");}
bool compare_fixed_i32(uintptr_t,int32_t,int32_t,std::string& e){return fail(e,"unexpected level CAS");}
struct Fence {bool read_current_fence(const Pin&,std::string&){return true;}};
void npc_transform(uintptr_t,uintptr_t,const mt2mp::game::subscriber_motion::Pose&){throw std::runtime_error("unexpected original transform");}
struct NativePort {
 const Owners* scalar_before_=nullptr;Fence fence_;
 bool current(const Owners&,uintptr_t,bool,std::string& e){return !current_error||fail(e,"original current owner failed");}
 bool store_npc_state(const Owners&,size_t,const quest_state::Npc&,const quest_state::Npc&,std::string&);
 bool store_npc_pose(const Owners&,size_t,const quest_state::Npc&,const quest_state::Npc&,std::string&);
};
#include "npc_state_guard.inc"
quest_state::Npc row(){quest_state::Npc n;n.key="action:2e770ada-0000-4000-8000-000000000000";n.level=11;n.pose.region=67;n.pose.orientation={0,0,0,1};n.pose.scale={1,1,1};return n;}
void reset(){native_rows={row()};native_owners={{row().key,1}};reads=counters=0;read_error=current_error=corrupt_post=empty_post=counter_error=false;}
}
int main(){try{
 NativePort port;Owners before;before.state.npcs={row()};const auto old=row();auto target=old;target.accepted.today=1;std::string e;
 auto prior=[&](auto mutate,const std::string& cause){reset();mutate();e.clear();need(!port.store_npc_state(before,0,old,target,e)&&!counters&&e.find(cause)!=std::string::npos,cause+": "+e);};
 reset();native_rows[0].pose.position[0]=8;e.clear();need(port.store_npc_state(before,0,old,target,e)&&native_rows[0].pose.position[0]==8&&counters==1,"counter phase rejected or rewrote flushed pose: "+e);
 prior([]{native_rows[0].accepted.today=4;},"accepted.today actual=4 required=0");
 prior([]{native_rows[0].highest_cache[0]=4;},"highest_cache actual=[4,0] required=[0,0]");
 prior([]{native_owners[0].key="foreign";},"npc.owner.key");prior([]{native_owners.clear();},"owners.count=0");prior([]{native_rows.clear();},"actual.count=0");
 prior([]{read_error=true;},"original owner read failed");prior([]{current_error=true;},"original current owner failed");
 reset();corrupt_post=true;e.clear();need(!port.store_npc_state(before,0,old,target,e)&&counters==1&&e.find("full readback differs; no replay: ")!=std::string::npos&&e.find("completed.today actual=1 required=0")!=std::string::npos,"post difference lost: "+e);
 reset();empty_post=true;e.clear();need(!port.store_npc_state(before,0,old,target,e)&&counters==1&&e.find("actual.count=0")!=std::string::npos,"post count lost: "+e);
 reset();counter_error=true;e.clear();need(!port.store_npc_state(before,0,old,target,e)&&e=="original DailyCount CAS failed"&&counters==1,"upstream cause overwritten");
 reset();e.clear();need(port.store_npc_state(before,0,old,target,e)&&counters==1&&reads==2,"successful full body regressed: "+e);
 reset();auto foreign=target;foreign.key="foreign";need(!port.store_npc_state(before,0,old,foreign,e)&&!reads&&e.find("npc.key")!=std::string::npos,"entry key not diagnosed");
 reset();foreign=target;foreign.type=1;need(!port.store_npc_state(before,0,old,foreign,e)&&!reads&&e.find(".type actual=0 required=1")!=std::string::npos,"entry type not diagnosed");
 reset();foreign=target;foreign.pose.region=68;need(!port.store_npc_pose(before,0,old,foreign,e)&&counters==0&&e.find("pose.region actual=67 required=68")!=std::string::npos,"transform Region not diagnosed");
 std::cout<<"PASS "<<checks<<" actual store_npc_state body cases\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
