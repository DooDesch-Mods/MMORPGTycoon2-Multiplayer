#include "live_host_state/day_rollover/economy/codec.h"
#include "live_host_state/day_rollover/economy/state_difference.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
namespace live=mt2mp::game::live_host_state;
namespace econ=live::day::economy;
namespace qs=mt2mp::subscriber_quest_state;
namespace {
unsigned checks=0;void need(bool b,const std::string& e){++checks;if(!b)throw std::runtime_error(e);}
econ::State fixture(){econ::State s;auto& f=s.day.frame;f.session=41;f.snapshot="11111111-1111-4111-8111-111111111111";f.epoch=7;f.source_generation=9;f.revision=3;f.selected=1;f.pause=1;
 f.values.calendar={1,1,2,1,30,7};f.values.record={1,93640,0,0,0};f.values.cash=93640;s.day.history={{1,live::day::record(f.values)}};
 qs::Npc n;n.key="action:22222222-2222-4222-8222-222222222222";n.level=1;n.pose.region=67;n.pose.orientation={0,0,0,1};n.pose.scale={1,1,1};n.quests.push_back({0,{},{},{}});s.npcs.push_back(n);return s;}
bool fail(std::string& e,const std::string& why){e=why;return false;}
struct ApplyPort:econ::Port{
 econ::Owners current;unsigned reads=0,stores=0;bool partial=false;
 bool read(econ::Owners& out,std::string&)override{++reads;out=current;return true;}
 bool store_subscriber(const econ::Owners&,size_t,size_t,int32_t,int32_t,std::string& e)override{return fail(e,"unexpected Subscriber store");}
 bool store_loan(const econ::Owners&,size_t,size_t,int32_t,int32_t,std::string& e)override{return fail(e,"unexpected Loan store");}
 bool store_npc_state(const econ::Owners&,size_t index,const qs::Npc& old,const qs::Npc& target,std::string& e)override{
  need(current.state.npcs[index]==old,"actual Economy prior row changed");++stores;
  if(partial){current.state.npcs[index].quests[0].runtime.rating_bits[0]=target.quests[0].runtime.rating_bits[0];return fail(e,"original rating scalar partial failure");}
  current.state.npcs[index]=target;return true;
 }
};
}
namespace mt2mp::game::live_host_state {
#include "quest_rating_tutorial_compare.inc"
struct Gates{
 bool okay=true,waiting=false;bool timely(std::chrono::steady_clock::time_point,std::string& e){return okay||::fail(e,"exact full ACK timeout");}
 bool ready(std::chrono::steady_clock::time_point,bool& out,std::string&){out=waiting;return true;}
};
struct FakePort{bool publication_ready(uint64_t,bool& waiting,std::string&){waiting=false;return true;}};
struct FakeDay{uint64_t sequence(){return 1;}};
struct Receipt{};
class Bridge{
public:
 Context context_;uint64_t common_epoch_=7,source_generation_=9,host_revision_=0,last_day_sequence_=1;
 bool suspended_=false,locked_=false;std::string publication_wait_,publication_last_wait_,publication_first_wait_;uint64_t publication_wait_count_=0,publication_recovery_count_=0;bool publication_was_waiting_=false;
 Gates fragments_,apply_acks_;FakePort port_;FakeDay day_value_;FakeDay* day_=&day_value_;bool economy_value_=true;bool* economy_=&economy_value_;
 std::function<bool(uint64_t&,std::string&)> time_=[](uint64_t& out,std::string&){out=1;return true;};
 std::optional<Frame> last_,queued_;std::optional<econ::State> captured_economy_,last_economy_,queued_economy_;std::optional<int> queued_day_;
 std::optional<std::chrono::steady_clock::time_point> last_capture_;econ::State source;unsigned sends=0,takes=0,captures=0;
 bool capture(uint64_t action,Frame& frame,std::string&){++captures;frame=source.day.frame;frame.base_action_revision=action;frame.time_revision=1;captured_economy_=source;return true;}
 bool send(const Frame& frame,std::string&){++sends;last_=frame;last_economy_=captured_economy_;return true;}
 bool take(const Frame&,std::string&){++takes;return true;}
 bool tick(uint64_t,bool,std::string&,std::chrono::steady_clock::time_point);
};
#include "quest_rating_bridge_tick.inc"
}
int main(){try{auto s=fixture();auto& v=s.npcs[0].quests[0].runtime;v.rating_bits={std::bit_cast<uint32_t>(-0.25f),std::bit_cast<uint32_t>(1.5f),0};v.rating_set=true;v.xp_reward=-1;v.gold_reward=-7;
 std::string e;std::vector<uint8_t> wire;need(econ::encode(s,wire,e),e);auto json=nlohmann::json::parse(wire);need(json["schema"]==13,"new exact schema missing");econ::State decoded;need(econ::decode(wire.data(),wire.size(),decoded,e)&&decoded==s,e);
 for(unsigned mode=0;mode<9;++mode){auto bad=json;auto& row=bad["npcs"][0]["quests"][0]["runtime"];
  if(mode==0)bad["schema"]=12;if(mode==1)row.erase("rating_bits");if(mode==2)row["extra"]=1;if(mode==3)row["rating_set"]=1;
  if(mode==4)row["rating_bits"][0]=0x7fc00000u;if(mode==5)row["rating_bits"][1]=0x7f800000u;if(mode==6)row["rating_bits"].push_back(0);
  if(mode==7)bad["npcs"][0]["quests"].push_back(bad["npcs"][0]["quests"][0]);if(mode==8)row["xp_reward"]=2147483648ull;
  const auto text=bad.dump();need(!econ::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,e)&&!e.empty(),"invalid exact runtime codec admitted");}
 auto zero=s;zero.npcs[0].quests[0].runtime.rating_bits={};need(econ::encode(zero,wire,e)&&econ::decode(wire.data(),wire.size(),decoded,e)&&decoded==zero,"legitimate zero/set rating rejected");
 for(unsigned field=0;field<4;++field){auto changed=s;auto& r=changed.npcs[0].quests[0].runtime;if(field==0)++r.rating_bits[0];if(field==1)r.rating_set=false;if(field==2)++r.xp_reward;if(field==3)++r.gold_reward;const auto why=econ::state_difference(s,changed);need(!why.empty()&&why.find("quest[0]")!=std::string::npos,"runtime exact field cause missing");need(!econ::same_npc_non_pose(s.npcs[0],changed.npcs[0]),"NPC native store phase omitted runtime-only change");}
 for(bool partial:{false,true}){ApplyPort port;port.current.state=s;port.current.history.pin.context.role=mt2mp::action_session::Role::Client;port.partial=partial;
  auto target=s;target.npcs[0].quests[0].runtime.rating_bits[0]++;target.npcs[0].quests[0].runtime.gold_reward++;
  auto before=port.current;econ::Apply apply;const auto receipt=apply.run(port,before,target);
  need(receipt.confirmed==!partial&&receipt.unknown==partial&&receipt.attempted&&port.stores==1,"actual Economy runtime store/readback did not gate confirmation: "+receipt.cause);
  if(partial){need(receipt.cause.find("original rating scalar partial failure")!=std::string::npos,"actual Economy lost partial scalar cause");const auto reads=port.reads;auto again=apply.run(port,before,target);need(again.unknown&&port.reads==reads&&port.stores==1,"actual Economy replayed Unknown mutation");}}
 live::Bridge bridge;bridge.context_.role=mt2mp::action_session::Role::Host;bridge.source=s;const auto now=std::chrono::steady_clock::now();need(bridge.tick(5,false,e,now)&&bridge.sends==1,e);
 need(bridge.tick(5,false,e,now+std::chrono::milliseconds(260))&&bridge.sends==1,"duplicate unchanged rating frame sent");
 ++bridge.source.npcs[0].quests[0].runtime.rating_bits[0];need(bridge.tick(5,false,e,now+std::chrono::milliseconds(520))&&bridge.sends==2,"actual Bridge omitted rating-only frame");
 ++bridge.source.npcs[0].quests[0].runtime.gold_reward;bridge.apply_acks_.waiting=true;need(bridge.tick(5,false,e,now+std::chrono::milliseconds(780))&&bridge.sends==2,"full Apply ACK wait bypassed");
 bridge.apply_acks_.waiting=false;need(bridge.tick(5,true,e,now+std::chrono::milliseconds(1040))&&bridge.sends==2,"pending action ordinary frame sent");
 need(bridge.tick(5,false,e,now+std::chrono::milliseconds(1300))&&bridge.sends==3,"changed reward state failed after pending/ACK recovery");
 bridge.apply_acks_.okay=false;need(!bridge.tick(5,false,e,now+std::chrono::milliseconds(1560))&&bridge.locked_&&e=="exact full ACK timeout","lost full ACK cause");
 std::cout<<checks<<" actual Quest codec/NPC phase/Bridge cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
