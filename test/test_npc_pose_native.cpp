#include <algorithm>
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
unsigned checks=0,reads=0,counters=0,transforms=0;bool flush_counter=false,spawn_drift=false,region_drift=false,follows_drift=false;
bool read_error=false,current_error=false,corrupt_post=false,empty_post=false,counter_error=false;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
void need(bool value,const std::string& e){if(!value)throw std::runtime_error(e);++checks;}
namespace npc_probe::action_runtime {bool state_owners(const auto&,const auto&,std::vector<QuestOwner>& out,std::string& e){if(read_error)return fail(e,"original owner read failed");out=native_owners;return true;}}
bool quest_npcs(const Pin&,const std::vector<QuestOwner>&,std::vector<quest_state::Npc>& out,std::string&){++reads;out=native_rows;if(reads==2){if(corrupt_post)out[0].completed.today+=1;if(empty_post)out.clear();}return true;}
bool quest_npc(const Pin& pin,const QuestOwner& owner,quest_state::Npc& out,std::string& e){
 std::vector<quest_state::Npc> rows;if(!quest_npcs(pin,native_owners,rows,e))return false;
 const auto found=std::find_if(native_owners.begin(),native_owners.end(),[&](const auto& item){return item.key==owner.key;});
 const auto index=size_t(found-native_owners.begin());
 if(found==native_owners.end()||index>=rows.size())return fail(e,"selected NPC value read failed actual.count="+std::to_string(rows.size()));
 out=rows[index];return true;
}
bool store_npc_counters(const Pin&,const QuestOwner&,const quest_state::Npc&,const quest_state::Npc& target,const std::function<bool(std::string&)>& guard,std::string& e){++counters;if(!guard(e))return false;if(counter_error)return fail(e,"original DailyCount CAS failed");const auto pose=native_rows[0].pose;native_rows[0]=target;native_rows[0].pose=pose;if(flush_counter)native_rows[0].pose.position[0]+=100;return true;}
struct Memory {bool copy(uintptr_t,void* out,size_t size,std::string& e){if(size!=12)return fail(e,"unexpected spawn size");std::array<uint32_t,3> spawn{1,2,3};if(transforms&&spawn_drift)spawn[0]=99;std::memcpy(out,spawn.data(),size);return true;}} memory;
template<class T>bool at(uintptr_t,size_t offset,T& out,std::string& e){
 uintptr_t value=0;
 if(offset==0x30)value=0x1742e58;else if(offset==0x38)value=0x3c2de0;else if(offset==0x178)value=transforms&&region_drift?0x300:0x200;else if(offset==0x490)value=transforms&&follows_drift?1:0;else return fail(e,"unexpected transform field read");
 out=static_cast<T>(value);return true;
}
bool compare_fixed_i32(uintptr_t,int32_t,int32_t,std::string& e){return fail(e,"unexpected level CAS");}
struct Fence {bool read_current_fence(const Pin&,std::string&){return true;}};
void npc_transform(uintptr_t,uintptr_t,const mt2mp::game::subscriber_motion::Pose& pose){++transforms;native_rows[0].pose=pose;}
struct NativePort {
 const Owners* scalar_before_=nullptr;Fence fence_;
 bool current(const Owners& before,uintptr_t,bool,std::string& e,std::vector<QuestOwner>* owners=nullptr){if(current_error)return fail(e,"original current owner failed");return !owners||npc_probe::action_runtime::state_owners(before.history.pin.context,before.history.pin.binding,*owners,e);}
 bool current_npc_owners(const Owners& before,std::vector<QuestOwner>& owners,std::string& e){return current(before,0,true,e,&owners);}
 bool store_npc_state(const Owners&,size_t,const quest_state::Npc&,const quest_state::Npc&,std::string&);
 bool store_npc_pose(const Owners&,size_t,const quest_state::Npc&,const quest_state::Npc&,std::string&);
};
#include "npc_state_guard.inc"
quest_state::Npc row(){quest_state::Npc n;n.key="action:2e770ada-0000-4000-8000-000000000000";n.level=11;n.pose.region=67;n.pose.orientation={0,0,0,1};n.pose.scale={1,1,1};return n;}
void reset(){native_rows={row()};native_owners={{row().key,1}};reads=counters=transforms=0;flush_counter=spawn_drift=region_drift=follows_drift=false;read_error=current_error=corrupt_post=empty_post=counter_error=false;}
}
int main(){try{
 NativePort port;Owners before;before.state.npcs={row()};const auto old=row();auto target=old;target.accepted.today=1;std::string e;
 reset();flush_counter=true;need(port.store_npc_state(before,0,old,target,e)&&counters==1&&!transforms&&native_rows[0].pose.position[0]==100,"counter SaveScope pose drain rejected: "+e);
 reset();native_rows[0].pose.position[0]=8;need(port.store_npc_state(before,0,old,target,e)&&native_rows[0].pose.position[0]==8&&!transforms,"prior flushed pose rejected: "+e);
 reset();target=old;target.pose.position[0]=8070.52880859375f;need(port.store_npc_pose(before,0,old,target,e)&&transforms==1&&!counters&&native_rows[0]==target,"final original transform did not confirm once: "+e);
 reset();native_rows[0].pose.position[0]=1;need(!port.store_npc_pose(before,0,old,target,e)&&!transforms&&e.find("pose.position")!=std::string::npos,"stale final fresh-pose CAS accepted");
 reset();target.accepted.today=1;need(!port.store_npc_pose(before,0,old,target,e)&&!transforms&&!counters&&e.find("settled exact non-pose")!=std::string::npos,"pose phase called counter mutation");target.accepted.today=0;
 reset();corrupt_post=true;need(!port.store_npc_pose(before,0,old,target,e)&&transforms==1&&e.find("completed.today")!=std::string::npos,"pose full readback ignored counter drift");
 reset();spawn_drift=true;need(!port.store_npc_pose(before,0,old,target,e)&&transforms==1&&e.find("spawn_bits")!=std::string::npos,"spawn guard weakened");
 reset();region_drift=true;need(!port.store_npc_pose(before,0,old,target,e)&&transforms==1&&e.find("Region owner changed")!=std::string::npos,"Region owner guard weakened");
 reset();follows_drift=true;need(!port.store_npc_pose(before,0,old,target,e)&&transforms==1&&e.find("follows_region")!=std::string::npos,"follows guard weakened");
 reset();current_error=true;need(!port.store_npc_pose(before,0,old,target,e)&&!transforms&&e=="original current owner failed","World fence cause lost");
 reset();native_rows[0].accepted.today=99;need(!port.store_npc_state(before,0,old,target,e)&&!counters&&e.find("accepted.today")!=std::string::npos,"counter prior CAS weakened");
 reset();native_rows[0].pose.region=68;need(!port.store_npc_state(before,0,old,target,e)&&!counters&&e.find("pose.region")!=std::string::npos,"counter Region identity weakened");
 std::cout<<"PASS "<<checks<<" actual native counter/pose bodies\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
