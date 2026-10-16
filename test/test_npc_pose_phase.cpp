#include "live_host_state/day_rollover/economy/state_difference.h"
#include "live_host_state/day_rollover/economy/capture_pause_restore.h"
#include "live_host_state/day_rollover/economy/codec.h"
#include <iostream>
#include <limits>
#include <stdexcept>
namespace live=mt2mp::game::live_host_state;
namespace economy=live::day::economy;
namespace quest=mt2mp::subscriber_quest_state;
using namespace economy;
namespace {
unsigned checks=0;
void need(bool value,const std::string& e){if(!value)throw std::runtime_error(e);++checks;}
State baseline(){
 State s;auto& f=s.day.frame;f.session=41;f.snapshot="11111111-1111-4111-8111-111111111111";f.epoch=7;f.source_generation=9;f.revision=3;f.selected=1;f.pause=1;
 f.values.calendar={1,1,2,1,30,7};f.values.record={1,93640,0,0,0};f.values.cash=93640;s.day.history={{1,live::day::record(f.values)}};
 quest::Npc n;n.key="action:00000000-0000-4000-8000-000000000001";n.level=11;n.pose.region=67;n.pose.orientation={0,0,0,1};n.pose.position={8067.98046875f,68.59115600585938f,8575.021484375f};n.pose.scale={1,1,1};s.npcs.push_back(n);n.key="action:00000000-0000-4000-8000-000000000002";s.npcs.push_back(n);
 Subscriber a;a.serialized_id=56;a.motion=n.pose;a.quest_state=quest::Toon{};s.subscribers.push_back(a);return s;
}
struct Mock final:Port {
 Owners value;std::vector<std::string> calls;unsigned poses=0,counters=0,reads=0;bool corrupt_counter=false,corrupt_region=false,fail_pose=false,after_drift=false,after_owner=false,foreign_subscriber=false,lose_npc=false;
 void flush(){for(auto& n:value.state.npcs)n.pose.position[0]+=100;}
 bool read(Owners& out,std::string&)override{++reads;out=value;if(poses&&after_drift)out.state.npcs[0].completed.today=99;if(poses&&after_owner)out.history.pin.world+=1;return true;}
 bool preflight_motion(const Owners&,const State&,std::string&)override{return true;}
 bool store_subscriber(const Owners&,size_t,size_t,int32_t,int32_t,std::string& e)override{e="unexpected scalar";return false;}
 bool store_loan(const Owners&,size_t,size_t,int32_t,int32_t,std::string& e)override{e="unexpected loan";return false;}
 bool store_quest_state(const Owners&,size_t i,const std::optional<quest::Toon>&,const std::optional<quest::Toon>& target,std::string&)override{calls.push_back("quest-scope");value.state.subscribers[i].quest_state=target;flush();return true;}
 bool store_npc_state(const Owners&,size_t i,const quest::Npc& old,const quest::Npc& target,std::string& e)override{
  if(!same_npc_non_pose(value.state.npcs[i],old)){e="original counter prior differs";return false;}
  calls.push_back("npc-scope");++counters;auto pose=value.state.npcs[i].pose;value.state.npcs[i]=target;value.state.npcs[i].pose=pose;flush();return true;
 }
 bool store_motion(const Owners&,size_t i,const mt2mp::game::subscriber_motion::Pose& old,const mt2mp::game::subscriber_motion::Pose& target,std::string& e)override{
  if(value.state.subscribers[i].motion!=old){e="motion CAS differs";return false;}
  calls.push_back("subscriber-pose");value.state.subscribers[i].motion=target;flush();if(corrupt_counter)value.state.npcs[0].accepted.today=99;if(corrupt_region)value.state.npcs[0].pose.region=68;if(foreign_subscriber)value.state.subscribers[0].pay[0]=99;if(lose_npc)value.state.npcs.clear();return true;
 }
 bool store_npc_pose(const Owners&,size_t i,const quest::Npc& old,const quest::Npc& target,std::string& e)override{
  if(value.state.npcs[i]!=old||!same_npc_non_pose(old,target)){e="final fresh row differs";return false;}
  calls.push_back("npc-pose");++poses;if(fail_pose){e="original transform exception";return false;}value.state.npcs[i]=target;return true;
 }
};
Mock fixture(const State& base){Mock p;p.value.state=base;p.value.history.pin.context.role=mt2mp::action_session::Role::Client;p.value.subscribers={1};p.value.subscriber_slots={0};p.value.needs_owners.resize(1);return p;}
State desired(const State& base){auto t=base;t.subscribers[0].quest_state->gold=1;t.subscribers[0].motion->position[0]+=1;for(auto& n:t.npcs){n.accepted.today=1;n.pose.position={8070.52880859375f,68.8605728149414f,8571.46484375f};}return t;}
void capture_motion(){
 auto before=fixture(baseline()).value;before.history.pin.pause=1;
 before.state.npcs[0].pose.position={8135.8984375f,85.70109558105469f,8538.6884765625f};
 auto after=before;after.state.npcs[0].pose.position={8139.6318359375f,85.70109558105469f,8539.0f};
 after.state.npcs[0].pose.orientation={0,1,0,0};
 const auto observed=after;std::string error;
 need(!capture_readback(before,after,error),"strict capture admitted changed NPC movement");
 const bool refreshed=capture_readback(before,after,error,true);
 need(refreshed,"Host SaveScope movement blocked capture refresh: "+error);
 need(after.state==observed.state,"capture refresh replaced final NPC state with prior pose");
 auto reject=[&](auto mutate,const std::string& field){auto current=after;mutate(current);
  need(!capture_readback(before,current,error,true)&&error.find(field)!=std::string::npos,"capture refresh lost guard "+field+": "+error);};
 reject([](auto& v){v.state.npcs[0].pose.region=68;},"pose.region");
 reject([](auto& v){v.state.npcs[0].pose.scale[0]=2;},"pose.scale");
 reject([](auto& v){v.state.npcs[0].level+=1;},".level");
 reject([](auto& v){v.state.npcs[0].key="action:00000000-0000-4000-8000-000000000003";},".key");
 reject([](auto& v){v.state.npcs.pop_back();},"npc.count");
 reject([](auto& v){v.state.subscribers[0].pay[0]+=1;},"subscriber[56].pay");
 reject([](auto& v){v.history.pin.world+=1;},"native_owners_changed");
 auto invalid=after;invalid.state.npcs[0].pose.position[0]=std::numeric_limits<float>::infinity();
 need(!capture_readback(before,invalid,error,true)&&error.find("invalid")!=std::string::npos,"capture refresh accepted nonfinite NPC pose");
 after=observed;after.history.pin.pause=0;unsigned restored=0,reads=0;
 const bool paused=restore_capture_pause(before,after,[&](const live::Pin& pin,std::string&){++restored;after.history.pin.pause=pin.pause;return true;},
  [&](Owners& value,std::string&){++reads;value=after;return true;},error);
 need(paused&&restored==1&&reads==1&&after.history.pin.pause==1&&after.state==observed.state,
      "Host SaveScope movement blocked original pause restoration/final state: "+error);
 after=observed;after.history.pin.pause=0;restored=reads=0;
 const bool failed=restore_capture_pause(before,after,[&](const live::Pin&,std::string& e){++restored;e="original pause setter exception";return false;},
  [&](Owners& value,std::string&){++reads;value=after;return true;},error);
 need(!failed&&restored==1&&reads==0&&error.find("original pause setter exception")!=std::string::npos,"capture pause failure lost original cause or replayed");
}
}
#include "subscriber_save_transition_checks.inc"
int main(){try{
 subscriber_save_transition::run();
 capture_motion();
 const auto base=baseline();auto target=desired(base);auto p=fixture(base);const auto before=p.value;Apply a;auto result=a.run(p,before,target);
 need(result.confirmed&&!result.unknown&&p.value.state==target&&p.counters==2&&p.poses==2,"two NPC/Subscriber scopes did not settle: "+result.cause);
 need(p.calls==std::vector<std::string>{"quest-scope","npc-scope","npc-scope","subscriber-pose","npc-pose","npc-pose"},"poses were interleaved with scopes");
 p=fixture(base);auto t=base;t.subscribers[0].quest_state->gold=1;Apply b;const auto second_before=p.value;result=b.run(p,second_before,t);need(result.confirmed&&p.counters==0&&p.poses==2&&p.value.state==t,"unchanged NPC poses were not restored after Quest scope: "+result.cause);
 p=fixture(base);t=base;t.npcs[0].pose.position[0]+=1;Apply c;auto pin=p.value;result=c.run(p,pin,t);need(result.confirmed&&p.counters==0&&p.poses==1,"pose-only frame opened counter scope");
 p=fixture(base);Apply d;pin=p.value;result=d.run(p,pin,base);need(result.confirmed&&!result.attempted&&p.calls.empty(),"duplicate state performed native effects");
 auto reject=[&](auto configure,const std::string& cause,bool pose_attempt){auto port=fixture(base);configure(port);const auto owner=port.value;Apply apply;const auto failed=apply.run(port,owner,target);need(failed.unknown&&!failed.confirmed&&failed.cause.find(cause)!=std::string::npos&&(pose_attempt?port.poses==1:port.poses==0),"guard lost: "+failed.cause);const auto stores=port.calls;const auto read_count=port.reads;apply.run(port,owner,target);need(port.calls==stores&&port.reads==read_count,"Unknown replayed effects/read");};
 reject([](auto& q){q.corrupt_counter=true;},"accepted.today actual=99 required=1",false);
 reject([](auto& q){q.corrupt_region=true;},"pose.region actual=68 required=67",false);
 reject([](auto& q){q.foreign_subscriber=true;},"subscriber[56].pay",false);
 reject([](auto& q){q.lose_npc=true;},"npc.count",false);
 reject([](auto& q){q.fail_pose=true;},"original transform exception",true);
 p=fixture(base);p.after_drift=true;Apply e;pin=p.value;result=e.run(p,pin,target);need(result.unknown&&!result.confirmed&&p.poses==2&&result.cause.find("completed.today")!=std::string::npos,"final strict values weakened");
 p=fixture(base);p.after_owner=true;Apply f;pin=p.value;result=f.run(p,pin,target);need(result.unknown&&!result.confirmed&&p.poses==2&&result.cause.find("owner.")!=std::string::npos,"final owner check weakened");
 std::cout<<"PASS "<<checks<<" actual phase/orchestration cases\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
