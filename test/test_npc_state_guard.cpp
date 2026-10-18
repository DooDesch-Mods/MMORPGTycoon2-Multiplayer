#include "live_host_state/day_rollover/economy/state_difference.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
namespace economy=mt2mp::game::live_host_state::day::economy;
namespace quest_state=mt2mp::subscriber_quest_state;
using namespace economy;
using Pin=mt2mp::game::live_host_state::Pin;

#include "npc_daily_guard_cases.inc"
namespace {
struct QuestOwner {std::string key;uintptr_t npc=1;};
std::vector<QuestOwner> native_owners;
std::vector<quest_state::Npc> native_rows;
unsigned checks=0,reads=0,counters=0,collections=0,passes=0,fail_pass=0,fail_collection=0;
unsigned motion_passes=0,fail_motion_pass=0;
std::function<void(MotionRead&,unsigned)> mutate_motion;
uintptr_t used_owner=0,preflight_owner=0;bool fence_error=false,change_second=false,drift_second=false,change_after_current=false;
Owners snapshot;
std::vector<std::string> events;
namespace action_session=mt2mp::action_session;
namespace apply_timing {enum class Part {EconomyRead};struct Scope{explicit Scope(Part){}};struct PageReadScope{~PageReadScope(){}};}
bool read_error=false,current_error=false,corrupt_post=false,empty_post=false,counter_error=false;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
void need(bool value,const std::string& e){if(!value)throw std::runtime_error(e);++checks;}
namespace npc_probe::action_runtime {bool state_owners(const auto&,const auto&,std::vector<QuestOwner>& out,std::string& e){++collections;events.push_back("census");if(read_error||collections==fail_collection)return fail(e,"original owner read failed");out=native_owners;return true;}}
bool quest_npcs(const Pin&,const std::vector<QuestOwner>& owners,std::vector<quest_state::Npc>& out,std::string&){++reads;events.push_back("values");if(reads==1){preflight_owner=owners.empty()?0:owners[0].npc;if(change_after_current)native_rows[0].accepted.today=99;}out=native_rows;if(reads==2){if(corrupt_post)out[0].completed.today+=1;if(empty_post)out.clear();}return true;}
bool quest_npc(const Pin& pin,const QuestOwner& owner,quest_state::Npc& out,std::string& e){
 std::vector<quest_state::Npc> rows;if(!quest_npcs(pin,native_owners,rows,e))return false;
 const auto found=std::find_if(native_owners.begin(),native_owners.end(),[&](const auto& item){return item.key==owner.key;});
 const auto index=size_t(found-native_owners.begin());
 if(found==native_owners.end()||index>=rows.size())return fail(e,"selected NPC value read failed actual.count="+std::to_string(rows.size()));
 out=rows[index];return true;
}
bool store_npc_counters(const Pin&,const QuestOwner& owner,const quest_state::Npc&,const quest_state::Npc& target,const std::function<bool(std::string&)>& guard,std::string& e){++counters;used_owner=owner.npc;events.push_back("counter");if(!guard(e))return false;if(counter_error)return fail(e,"original DailyCount CAS failed");const auto pose=native_rows[0].pose;native_rows[0]=target;native_rows[0].pose=pose;return true;}
struct Memory {bool copy(uintptr_t,void*,size_t,std::string& e){return fail(e,"unexpected transform memory read");}} memory;
template<class T>bool at(uintptr_t,size_t,T&,std::string& e){return fail(e,"unexpected transform field read");}
bool compare_fixed_i32(uintptr_t,int32_t,int32_t,std::string& e){return fail(e,"unexpected level CAS");}
struct Fence {bool read_current_fence(const Pin&,std::string& e){events.push_back("fence");return !fence_error||fail(e,"original current fence failed");}};
void npc_transform(uintptr_t,uintptr_t,const mt2mp::game::subscriber_motion::Pose&){throw std::runtime_error("unexpected original transform");}
struct NativePort {
 const Owners* scalar_before_=nullptr;Fence fence_;
 bool once(Owners& out,std::string& e,std::vector<QuestOwner>* owners=nullptr){
  ++passes;events.push_back("pass"+std::to_string(passes));
  if(current_error)return fail(e,"original current owner failed");
  if(passes==fail_pass)return fail(e,"original complete read failed");
  out=snapshot;if(change_second&&passes==2)native_owners[0].npc=2;
  std::vector<QuestOwner> actual;
  if(!npc_probe::action_runtime::state_owners(out.history.pin.context,out.history.pin.binding,actual,e))return false;
  if(drift_second&&passes==2)out.next_id+=1;
  if(owners)*owners=std::move(actual);return true;
 }
 bool read(Owners&,std::string&);
 bool motion_once(MotionRead& out,std::string& e){
  ++motion_passes;events.push_back("motion"+std::to_string(motion_passes));
  if(current_error)return fail(e,"original current owner failed");
  if(motion_passes==fail_motion_pass)return fail(e,"original motion owner read failed");
  out={};out.history=snapshot.history;out.subscriber_manager=snapshot.subscriber_manager;
  out.subscriber_data=snapshot.subscriber_data;out.loan_data=snapshot.loan_data;out.offer_data=snapshot.offer_data;
  out.subscribers=snapshot.subscribers;out.subscriber_slots=snapshot.subscriber_slots;out.needs_owners=snapshot.needs_owners;
  out.loans=snapshot.loans;out.offers=snapshot.offers;out.next_id=snapshot.next_id;
  for(const auto& row:snapshot.state.subscribers)out.rows.push_back({row.serialized_id,row.motion});
  if(mutate_motion)mutate_motion(out,motion_passes);
  return true;
 }
 bool read_motion(MotionRead&,std::string&);
 bool current_npc_owners(const Owners&,std::vector<QuestOwner>&,std::string&);
 bool read_with_npc_owners(Owners&,std::string&,std::vector<QuestOwner>*);
 bool current(const Owners&,uintptr_t,bool,std::string&,std::vector<QuestOwner>* owners=nullptr);
 bool store_npc_state(const Owners&,size_t,const quest_state::Npc&,const quest_state::Npc&,std::string&);
 bool store_npc_pose(const Owners&,size_t,const quest_state::Npc&,const quest_state::Npc&,std::string&);
};
#include "npc_current_receipt.inc"
#include "npc_state_guard.inc"
quest_state::Npc row(){quest_state::Npc n;n.key="action:2e770ada-0000-4000-8000-000000000000";n.level=11;n.pose.region=67;n.pose.orientation={0,0,0,1};n.pose.scale={1,1,1};return n;}
void reset(){collections=passes=fail_pass=fail_collection=0;motion_passes=fail_motion_pass=0;mutate_motion={};used_owner=preflight_owner=0;fence_error=change_second=drift_second=change_after_current=false;events.clear();native_rows={row()};native_owners={{row().key,1}};reads=counters=0;read_error=current_error=corrupt_post=empty_post=counter_error=false;}
}
int main(){try{
 NativePort port;Owners before;before.state.npcs={row()};before.history.pin.context.role=action_session::Role::Client;snapshot=before;const auto old=row();auto target=old;target.accepted.today=1;std::string e;
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
 reset();change_second=true;e.clear();std::vector<QuestOwner> legacy;
 need(port.current(before,0,true,e,&legacy)&&legacy[0].npc==2&&passes==2&&collections==2,"unchanged full current lost second-pass identity output: "+e);
 reset();e.clear();need(port.store_npc_state(before,0,old,target,e),"fresh NPC ownership admission failed: "+e);
 need(passes==0&&motion_passes==2&&collections==2&&reads==2&&used_owner==1&&preflight_owner==1,"NPC store resampled unrelated full economy or lost fresh identities/values");
 need(events==std::vector<std::string>({"census","motion1","motion2","fence","values","counter","fence","census","values"}),"census callback must precede fresh double ownership read and original counter/post boundaries");
 reset();change_after_current=true;e.clear();need(!port.store_npc_state(before,0,old,target,e)&&!counters&&e.find("accepted.today actual=99")!=std::string::npos,"preflight reused cached NPC values");
 reset();fail_collection=2;e.clear();need(!port.store_npc_state(before,0,old,target,e)&&counters==1&&e=="original owner read failed","post-effect census was reused or original failure lost");
 for(unsigned fault=1;fault<=2;++fault){reset();fail_pass=fault;std::vector<QuestOwner> receipt{{"sentinel",77}};e.clear();
  need(!port.current(before,0,true,e,&receipt)&&e=="original complete read failed"&&receipt[0].npc==77&&passes==fault,"failed complete pass published identities or lost cause");}
 reset();drift_second=true;std::vector<QuestOwner> receipt{{"sentinel",77}};e.clear();
 need(!port.current(before,0,true,e,&receipt)&&e=="economic actual owned arrays/values changed during double read"&&receipt[0].npc==77,"double-read equality guard weakened or published failed identities");
 reset();fence_error=true;e.clear();need(!port.current(before,0,true,e,&receipt)&&e=="original current fence failed"&&receipt[0].npc==77,"current fence failure published identities");
 reset();auto changed=before;changed.subscriber_manager=123;e.clear();need(!port.current(changed,0,true,e,&receipt)&&e=="economic current actual Client owner/fence differs"&&receipt[0].npc==77,"current owner failure published identities");
 reset();e.clear();need(!port.current(before,123,true,e,&receipt)&&e=="economic scalar owner absent from actual manager"&&receipt[0].npc==77,"selected scalar membership failure published identities");
 reset();Owners actual;e.clear();need(port.read(actual,e)&&passes==2&&collections==2,"standalone read lost complete double pass");
 reset();e.clear();need(port.current(before,0,true,e)&&passes==2&&collections==2,"standalone current lost complete double pass");
 // The actual new helper and actual read_motion double-read body are extracted.
 // Only the existing complete motion_once leaf is replaced by owned snapshots.
 const auto owner_failure=[&](const std::function<void(MotionRead&)>& change,const char* label){
  reset();mutate_motion=[&](MotionRead& value,unsigned){change(value);};e.clear();
  std::vector<QuestOwner> output{{"sentinel",77}};
  need(!port.current_npc_owners(before,output,e)&&e=="economic current actual Client owner/fence differs"&&output[0].npc==77&&passes==0&&motion_passes==2,label);
 };
 owner_failure([](auto& v){v.subscriber_manager=123;},"changed Subscriber manager accepted");
 owner_failure([](auto& v){v.subscriber_data=123;},"changed Subscriber data accepted");
 owner_failure([](auto& v){v.subscribers.push_back(123);},"changed Subscriber member accepted");
 owner_failure([](auto& v){v.subscriber_slots.push_back(1);},"changed canonical native slot accepted");
 owner_failure([](auto& v){v.needs_owners.emplace_back();},"changed Brain/Needs ownership accepted");
 owner_failure([](auto& v){v.loans.push_back(123);},"changed Loan owner accepted");
 owner_failure([](auto& v){v.offers.push_back(123);},"changed Offer owner accepted");
 owner_failure([](auto& v){v.loan_data=123;},"changed Loan array accepted");
 owner_failure([](auto& v){v.offer_data=123;},"changed Offer array accepted");
 owner_failure([](auto& v){v.history.owners.push_back(123);},"changed History owner accepted");
 owner_failure([](auto& v){v.history.pin.world=123;},"changed World accepted");
 owner_failure([](auto& v){v.history.pin.clock=123;},"changed Clock accepted");
 owner_failure([](auto& v){v.history.pin.common_epoch+=1;},"changed epoch accepted");
 owner_failure([](auto& v){v.history.pin.selected=1;},"nonzero Client Clock accepted");
 owner_failure([](auto& v){v.history.pin.context.role=action_session::Role::Host;},"Host accepted by Client NPC store");
 reset();mutate_motion=[](auto& v,unsigned pass){if(pass==2)v.loan_data=123;};e.clear();
 need(!port.store_npc_state(before,0,old,target,e)&&!counters&&e=="economic motion actual owned arrays/poses changed during double read","between-pass ownership drift accepted");
 reset();mutate_motion=[](auto& v,unsigned pass){if(pass==2)v.rows.push_back({});};e.clear();
 need(!port.store_npc_state(before,0,old,target,e)&&!counters&&e=="economic motion actual owned arrays/poses changed during double read","between-pass pose drift accepted");
 for(unsigned fault=1;fault<=2;++fault){reset();fail_motion_pass=fault;e.clear();std::vector<QuestOwner> output{{"sentinel",77}};
  need(!port.current_npc_owners(before,output,e)&&e=="original motion owner read failed"&&output[0].npc==77&&motion_passes==fault,"failed fresh motion ownership pass published NPC receipt");}
 reset();fence_error=true;e.clear();need(!port.store_npc_state(before,0,old,target,e)&&!counters&&e=="original current fence failed","fresh NPC admission lost original fence failure");
 reset();e.clear();need(port.store_npc_state(before,0,old,target,e),"first row failed");
 native_rows={old};snapshot.loan_data=123;e.clear();
 need(!port.store_npc_state(before,0,old,target,e)&&counters==1&&motion_passes==4&&e=="economic current actual Client owner/fence differs","next NPC row reused ownership across previous counter/post callbacks");snapshot=before;
 std::cout<<"NPC admission complete economy passes: 2 -> 0; existing motion ownership passes: 0 -> 2; canonical census calls including post: 3 -> 2 (counter leaf mocked)\n";
 checks+=daily_guard_test::run();
 std::cout<<"PASS "<<checks<<" actual store_npc_state body cases\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
