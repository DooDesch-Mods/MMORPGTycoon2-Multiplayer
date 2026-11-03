#include "npc_quest_authoring_fixture.h"
#include "npc_quest_authoring_read.h"
#include "ui_remove/ui_capture.h"
#include "typed_action/tick_commands.h"
#include <atomic>
#include <cstring>
#include <iostream>
#include <optional>
namespace a=mt2mp::action_session;
namespace qr=mt2mp::game::monster_quest_retirement;
namespace npc_quest_authoring_read=mt2mp::game::npc_quest_authoring_read;
namespace q=mt2mp::npc_quest_records;
using json=nlohmann::json;
namespace mt2mp::action_session::ui_diagnostic{struct Window{uintptr_t window=10000;};}
namespace fixture {
bool fail(std::string& e,const std::string& why){e=why;return false;}
bool fixture_ordered=false,consumed=false,scope_valid=true,inside_go=false,change_refs=false,change_targets=false,change_release=false,change_state=false;
a::Context ordered_context;
a::Request ordered_request;
struct Config{bool is_host=true;static Config& instance(){static Config value;return value;}};
struct OrderedSession{bool current(a::Context& value,std::string&){value=ordered_context;return true;}};
}
namespace mt2mp::action_session {
fixture::OrderedSession& selected_session(){static fixture::OrderedSession value;return value;}
namespace tick_commands::authority {
bool active(){return fixture::consumed;}
bool matches(const Context& context,const Binding& binding,const Request& request,std::string& e){
    return (fixture::consumed&&fixture::scope_valid&&context==fixture::ordered_context&&context.snapshot_uuid==request.snapshot_uuid&&binding==Binding{3,4}&&request==fixture::ordered_request)||fixture::fail(e,"Consumed destination scope differs");}
}
}
namespace fixture {
unsigned thread=1;unsigned GetCurrentThreadId(){return thread;}
struct Memory{};
std::map<uintptr_t,uint64_t> fields;
template<class T>bool field(Memory&,uintptr_t object,size_t offset,T& out,std::string& e){
 auto found=fields.find(object+offset);if(found==fields.end())return fail(e,"unreadable exact field");out=T(found->second);return true;}
namespace action_runtime{
namespace ui=a::ui_capture;
bool local_key(uintptr_t npc,uint64_t uid,std::string& key,std::string& e){if(npc!=5000||uid!=0x100001388ull)return fail(e,"NPC UID mismatch");key=owner_key;return true;}
bool live_npc_window(Memory&,uintptr_t,const ui::LocalBinding&,a::ui_diagnostic::Window&,json&,std::string&){return true;}
uint32_t seed(uintptr_t,const std::array<uint64_t,4>&);
}
namespace typed_action_runtime{
namespace action=a;
struct Status{bool started=true,locked=false,pending=false;std::string action_uuid=action_id;};
struct Controller{bool ordered_mode()const{return false;}Status value;Status status()const{return value;}a::Context bound_context()const{a::Context c;c.snapshot_uuid=snapshot_id;return c;}a::Binding bound_binding()const{return {3,4};}};
struct Session{bool failed=false;unsigned reads=0;std::string snapshot=snapshot_id;
 bool current(a::Context& out,std::string& e){++reads;if(failed)return fail(e,"retained Session current failure");out.snapshot_uuid=snapshot;return true;}};
struct Engine{bool failed=false;unsigned reads=0;
 bool current(const a::Binding& binding,const std::string& snapshot,std::string& e){++reads;
  if(failed)return fail(e,"retained native Engine current failure");
  return (binding==a::Binding{3,4}&&snapshot==snapshot_id)||fail(e,"native Engine exact binding differs");}};
struct Runtime{bool live_state=false,live_port=false;unsigned thread=1;bool lobby_retiring=false;Controller controller;Session session;Engine engine;}fixture;
std::atomic<Runtime*> published_runtime{&fixture};
bool network_binding_with_mode(const std::string&,a::Binding&,std::string&,bool*);
bool held_tutorial_session_read(Session&,a::Context&,std::string& e){e="unexpected held tutorial read";return false;}
#include "quest_network_binding_body.inc"
json controller_status(){auto v=fixture.controller.value;return{{"started",v.started},{"locked",v.locked},{"pending",v.pending},{"ordered",fixture_ordered},{"action_uuid",v.action_uuid}};}
bool original_go_admission(bool& value,std::string&){value=inside_go;return true;}
}
namespace net{std::string current_snapshot_id(){return snapshot_id;}namespace session_wire{bool online=true;bool connected(){return online;}}}
struct World{uintptr_t root=10;};struct Pin{World world;};
namespace d{struct Sample{struct State{std::array<uint64_t,4> region_random{5,6,7,8},global_random{1,2,3,4};int protected_value=0;bool operator==(const State&)const=default;}state;
 struct Undo{uintptr_t active_graph=0;std::vector<uintptr_t> undo;}undo;a::Ledger money;unsigned release_edits=0;};}
struct State{uintptr_t image=0x10000000;unsigned thread=1;}state;
struct Pending{a::Binding binding;std::string snapshot,owner,token;a::NpcQuestIntent intent;std::optional<a::Context> context;};
struct Operation{a::Binding binding;a::Request request;Pin pin;qr::Witness before,after;d::Sample sample;
 uintptr_t owner=0,quest=0,destination=0,graph=0;uint64_t destination_uid=0;a::Evidence evidence;
 bool attempted=false,returned=false,seed_attempted=false,confirmed=false,retired=false;std::string cause;
 json refs_before;std::string targets_before;bool ordered=false;};
bool ready=true,fault=false;std::optional<Pending> pending;std::map<std::string,Operation> operations;
std::map<std::string,json> jobs;std::string ui_token,cause;uint64_t captures=0,rejected=0;unsigned forwards=0,originals=0,seeds=0,mode=0;bool mutated=false;
void forward(void*,void*){++forwards;}void(*original)(void*,void*)=&forward;
bool reject(const std::string& e){++rejected;cause=e;return false;}
bool codes(Memory&,bool,std::string& e){return mode!=1||fail(e,"exact body changed");}
bool same_world(const World& x,const World& y){return x.root==y.root;}
a::Ledger ledger(const a::Ledger& value){return value;}
bool owner(Memory&,const a::Binding& binding,const std::string& snapshot,const std::string& key,int32_t region,action_runtime::ui::LocalBinding& out,Pin& pin,std::string& e){
 if(binding!=a::Binding{3,4}||snapshot!=snapshot_id||key!=owner_key||region!=67)return fail(e,"bound owner differs");
 out={40,5000,0x100001388ull,3,4,snapshot_id,owner_key,false,false,true,false};if(mode==7&&mutated)pin.world.root=11;return true;}
qr::Witness world(){qr::Witness w;w.manager=100;w.completed=1;
 for(size_t i=0;i<3;++i){const uintptr_t quest=1000+i*100,npc=i<2?5000:6000,destination=7000+i*100;
  auto record=intent().before;record.sequence_id=i<2?int32_t(i):0;if(i)record.name="Survivor"+std::to_string(i);
  const auto key=i<2?owner_key:std::string("action:66666666-6666-4666-8666-666666666666");
  w.quests.push_back(quest);w.rows.push_back({key,record});w.links.push_back({destination,9000+i*10,destination+0x20,npc,npc+50,npc+0x20});
  auto& own=w.owners[npc];own.pointer=npc;own.uid=npc+0x100000000ull;own.key=key;
  own.state.records.push_back(record);own.state.local.push_back({quest,npc,destination,true,true,true,true,q::TaskStatus::Absent});own.destination_uids.push_back(destination+0x100000000ull);
 }return w;}
qr::Witness changed(qr::Witness w){auto value=after(before()).record;
 if(fixture_ordered){const auto& intent=ordered_request.npc_quest;value=intent.before;value.destination_key=intent.destination_key;value.action=intent.action;value.monsters_to_kill=intent.kills;value.monster_type=intent.monster_type;value.name="Visit original Building";}
 w.rows[0].record=value;w.links[0][0]=8000;w.links[0][1]=8888;w.links[0][2]=8032;
 auto& own=w.owners.at(5000);own.state.records[0]=value;own.state.local[0].destination=8000;own.state.local[0].task=q::TaskStatus::Pending;own.destination_uids[0]=0x100001f40ull;
 if(mode==8)w.rows[1].record.name="Changed survivor";return w;}
bool state_read(Memory&,const a::Binding&,const std::string&,const Pin&,uintptr_t,int32_t,qr::Witness& witness,d::Sample& sample,a::NpcQuestState& value,uintptr_t& quest,std::string&){
 witness=mutated?changed(world()):world();sample={};value=mutated?after(before()):before();if(fixture_ordered)value.record=witness.rows[0].record;quest=1000;
 if(mutated){sample.undo.undo.push_back(50);if(mode==9)++sample.money.cash;if(mode==10)++sample.state.region_random[0];if(mode==11)++value.global_random[0];}
 else if(mode==2)++value.record.xp_reward;
 sample.state.global_random=value.global_random;if(mutated&&change_release)++sample.release_edits;if(mutated&&change_state)++sample.state.protected_value;return true;}
bool destination(Memory&,const a::Binding&,const std::string&,int32_t,a::QuestDestinationFamily,const std::string&,uintptr_t& ptr,uint64_t& uid,std::string& e){ptr=8000;uid=0x100001f40ull;return mode!=3||fail(e,"destination full UID mismatch");}
bool target_intent(Memory&,const a::Binding&,const std::string&,uintptr_t ptr,a::NpcQuestIntent& out,std::string& e){if(ptr!=8000)return fail(e,"destination pointer differs");out=fixture_ordered?ordered_request.npc_quest:intent();return true;}
bool inverse(Memory&,const Operation&,const d::Sample&,std::string& e){return mode!=12||fail(e,"DoSetNPCQuests inverse differs");}
uint32_t set_destination(uintptr_t quest,uintptr_t ptr,int32_t action,int32_t kills){need(quest==1000&&ptr==8000&&action==(fixture_ordered?2:6)&&kills==(fixture_ordered?0:1),"original ABI differs");++originals;mutated=true;return mode==4?1:0;}
namespace action_runtime{uint32_t seed(uintptr_t,const std::array<uint64_t,4>&){++seeds;return mode==5?1:0;}}
namespace quest_list {
bool authoring_effect(const a::Binding&,const std::string&,int32_t,const qr::Witness&,json& refs,std::string& targets,std::string& digest,std::string&){
    refs=json::array({{{"subscriber_id",7}}});if(mutated&&change_refs)refs[0]["gold"]=1;
    targets=mutated&&change_targets?"changed-pool":"pool";digest=std::string(64,mutated?'b':'a');return true;}
}
#include "npc_quest_ordered_destination.inc"
#include "quest_ui_binding_body.inc"
#include "quest_capture_body.inc"
#include "quest_perform_body.inc"
namespace building_history_slice {
bool failed=false;unsigned calls=0;
bool remember_quest(const std::string& id,uintptr_t graph,std::string& e){
 ++calls;if(failed)return fail(e,"retained native history association failure");
 return (id==action_id&&graph==50)||fail(e,"native history association action/graph differs");
}
}
namespace quest_history {
struct Source{a::Binding binding;std::string snapshot,key;int32_t region=-1;
 uintptr_t owner=0,manager=0;qr::Owner before,after;};
std::map<std::string,Source> sources;bool graph_failed=false;unsigned graph_reads=0;
bool graph(Memory&,uintptr_t pointer,const qr::Owner& owner,std::string& e){
 ++graph_reads;if(graph_failed)return fail(e,"retained original graph validation failure");
 return (pointer==50&&owner.pointer==5000&&owner.key==owner_key)||fail(e,"original graph exact owner differs");
}
#include "quest_remember_body.inc"
}
#include "quest_observe_body.inc"
void reset(unsigned selected=0){mode=selected;thread=1;ready=true;fault=false;pending.reset();operations.clear();jobs.clear();ui_token.clear();cause.clear();captures=rejected=0;forwards=originals=seeds=0;mutated=false;net::session_wire::online=true;typed_action_runtime::fixture={};
 fixture_ordered=consumed=inside_go=change_refs=change_targets=change_release=change_state=false;scope_valid=true;Config::instance().is_host=true;
 ordered_context={5,1,1,3,a::Role::Host,snapshot_id};ordered_request={};
 quest_history::sources.clear();quest_history::graph_failed=false;quest_history::graph_reads=0;
 building_history_slice::failed=false;building_history_slice::calls=0;
 const auto image=state.image;fields={{image+0x13df400,20000},{20000,image+0x1701f80},{20000+0x1b0,1000},{20000+0x1b8,30000},{30000,1018},
 {10000+0x530,11000},{11000,image+0x16e02b0},{11000+0x360,5000},{11000+0x368,31000},{31000,5032},{11000+0x350,12000},
 {12000,image+0x17088a0},{12000+0x320,11000},{12000+0x3e0,1000},{12000+0x3e8,30000},{12000+0x3f8,1},{12000+0x3fa,1},
 {11000+0x340,40000},{11000+0x348,1},{11000+0x34c,2},{40000,12000},{40008,12000},
 {image+0x13df9d0,14000},{14000,image+0x1786dd0},{14000+0x190,15000},{15000,image+0x17418b0},{15000+0x148,12000+0x318},
 {1000+0xb8,5000},{1000+0x84,0},{5000+0x18,0x100001388ull},{5000+0x178,16000},{16000+0x834,67}};
 fields[30000]=1000+0x18;
}
}
int main(){using namespace fixture;try{unsigned n=0;std::string e;
 reset();capture(state.image+0x5ce471,reinterpret_cast<void*>(20000),reinterpret_cast<void*>(8000));need(captures==1&&pending&&pending->intent==intent()&&!originals&&!forwards,"player Quest did not capture without local effect: "+cause+"");++n;
 for(unsigned which=0;which<2;++which){reset();if(which)net::session_wire::online=false;capture(state.image+0x5ce471+(which?0:1),reinterpret_cast<void*>(20000),reinterpret_cast<void*>(8000));need(forwards==1&&!captures&&!pending,"foreign/offline Quest did not forward once");++n;}
 for(unsigned guard=0;guard<8;++guard){reset();if(guard==0)ready=false;if(guard==1)fault=true;if(guard==2)pending=Pending{};if(guard==3)thread=2;if(guard==4)mode=1;if(guard==5)typed_action_runtime::fixture.controller.value.pending=true;if(guard==6)typed_action_runtime::fixture.controller.value.locked=true;if(guard==7)typed_action_runtime::fixture.lobby_retiring=true;
  capture(state.image+0x5ce471,reinterpret_cast<void*>(20000),reinterpret_cast<void*>(8000));need(rejected==1&&!captures&&!forwards&&!originals,"player admission guard forwarded or mutated");if(guard==5)need(cause=="network admission requires idle unlocked Controller","real pending guard not exercised");++n;}
 for(uintptr_t address:{uintptr_t(30000),uintptr_t(31000),uintptr_t(12000+0x320),uintptr_t(12000+0x3fa),uintptr_t(15000+0x148),uintptr_t(14000+0x190),uintptr_t(11000+0x348)}){reset();++fields[address];capture(state.image+0x5ce471,reinterpret_cast<void*>(20000),reinterpret_cast<void*>(8000));need(rejected==1&&!captures&&!forwards,"wrong Quest UI weak/selector/cursor/membership accepted");++n;}
 for(unsigned which=0;which<=12;++which){reset(which);typed_action_runtime::fixture.controller.value.pending=true;auto req=request().request;const auto result=perform({3,4},req,nullptr,e);
  if(which==0||which==5||which==6||which==11)need(result==a::Outcome::Confirmed&&originals==1,"Host original Quest failed: "+e);
  else if(which<=3)need(result==a::Outcome::RejectedNoEffect&&!originals&&operations.empty(),"preflight Quest failure mutated");
  else need(result==a::Outcome::Unknown&&originals==1,"changed original Quest escaped Unknown");
  if(operations.contains(action_id)){const auto count=originals;need(perform({3,4},req,nullptr,e)==a::Outcome::Unknown&&originals==count,"Quest replayed native original");}++n;}
 for(unsigned which:{0u,5u,11u}){reset(which);typed_action_runtime::fixture.controller.value.pending=true;auto host=result().evidence;auto outcome=perform({3,4},request().request,&host,e);need(which==0?outcome==a::Outcome::Confirmed:outcome==a::Outcome::Unknown,"Client Host seed/after mismatch escaped");need(seeds==1,"Client seed count differs");++n;}
 reset();typed_action_runtime::fixture.controller.value.pending=true;auto host=result().evidence;++host.quest_before.record.xp_reward;need(perform({3,4},request().request,&host,e)==a::Outcome::RejectedNoEffect&&!seeds&&!originals,"Client before mismatch seeded or mutated");++n;
 reset();need(perform({3,4},request().request,nullptr,e)==a::Outcome::RejectedNoEffect&&!originals,"missing pending operation mutated");++n;
 for(unsigned which=0;which<4;++which){reset();auto& controller=typed_action_runtime::fixture.controller.value;controller.pending=true;
  if(which==0)controller.action_uuid=snapshot_id;if(which==1)controller.locked=true;if(which==2)controller.started=false;
  need(perform(which==3?a::Binding{3,5}:a::Binding{3,4},request().request,nullptr,e)==a::Outcome::RejectedNoEffect&&!originals&&!seeds,"wrong pending UUID/role binding mutated");++n;}
 reset();typed_action_runtime::fixture.controller.value.pending=true;auto req=request().request;need(perform({3,4},req,nullptr,e)==a::Outcome::Confirmed,e);a::Evidence observed;need(observe({3,4},req,observed,e)&&retire({3,4},req,e)&&observe({3,4},req,observed,e)&&observed.retirement_confirmed,e);need(!retire({3,4},req,e),"Quest receipt retired twice");++n;
 const auto& retained=quest_history::sources.at(action_id);
 need(retained.binding==a::Binding{3,4}&&retained.snapshot==snapshot_id&&retained.key==owner_key&&retained.region==67&&
      retained.owner==5000&&retained.manager==100&&retained.before.state.records==world().owners.at(5000).state.records&&
      retained.after.state.records==changed(world()).owners.at(5000).state.records&&
      quest_history::graph_reads==1&&building_history_slice::calls==1,"actual retirement did not retain exact before/after history once");++n;
 for(unsigned which=0;which<3;++which){reset();a::Binding observed_binding{8,9};
  if(which==0)typed_action_runtime::fixture.session.failed=true;
  if(which==1)typed_action_runtime::fixture.session.snapshot=action_id;
  if(which==2)typed_action_runtime::fixture.engine.failed=true;
  need(!typed_action_runtime::network_binding(snapshot_id,observed_binding,e)&&observed_binding==a::Binding{},"failed actual binding admission published a binding");
  need(typed_action_runtime::fixture.session.reads==1&&typed_action_runtime::fixture.engine.reads==unsigned(which==2),"actual binding skipped Session/native-owner gates");
  need(e==(which==0?"retained Session current failure":which==1?"binding read actual Session/snapshot differs from Controller":"retained native Engine current failure"),"actual binding replaced original cause");++n;
 }
 for(unsigned which=0;which<2;++which){reset();typed_action_runtime::fixture.controller.value.pending=true;
  const auto source=request().request;need(perform({3,4},source,nullptr,e)==a::Outcome::Confirmed,e);
  if(which==0)quest_history::graph_failed=true;else building_history_slice::failed=true;
  need(!retire({3,4},source,e)&&!operations.at(action_id).retired&&quest_history::sources.empty()&&originals==1,
       "partial history retirement became confirmed or replayed original");
  need(e==(which==0?"retained original graph validation failure":"retained native history association failure"),"history retirement lost original cause");
  need(quest_history::graph_reads==1&&building_history_slice::calls==unsigned(which!=0),"history failure crossed the wrong original boundary");
  need(perform({3,4},source,nullptr,e)==a::Outcome::Unknown&&originals==1,"history failure replayed original world mutation");++n;
 }
 reset();auto source=request().request;auto original_world=world(),after_world=changed(original_world);
 ++after_world.owners.at(5000).uid;
 need(!quest_history::remember({3,4},source,5000,50,original_world,after_world,e)&&quest_history::sources.empty()&&
      !quest_history::graph_reads&&!building_history_slice::calls,"foreign history owner reached native retention");++n;
 after_world=changed(original_world);
 need(quest_history::remember({3,4},source,5000,50,original_world,after_world,e),e);
 need(!quest_history::remember({3,4},source,5000,50,original_world,after_world,e)&&quest_history::sources.size()==1&&
      quest_history::graph_reads==1&&building_history_slice::calls==1,"duplicate history source repeated original retention");++n;
 for(bool origin_host:{true,false}){
  reset();fixture_ordered=consumed=true;Config::instance().is_host=origin_host;ordered_context.role=origin_host?a::Role::Host:a::Role::Client;ordered_context.local_peer=origin_host?1:2;
  ordered_request=request().request;auto& intent=ordered_request.npc_quest;intent.family=a::QuestDestinationFamily::Building;intent.action=2;intent.kills=0;
  inside_go=true;
  capture(state.image+0x5ce471,reinterpret_cast<void*>(20000),reinterpret_cast<void*>(8000));
  need(captures==1&&pending&&pending->context==ordered_context&&!originals&&!forwards,"Ordered original-Go destination capture mutated or lost Context: "+cause);++n;
  auto captured=*pending;pending.reset();inside_go=false;need(current_capture(captured,e),e);++ordered_context.session;need(!current_capture(captured,e),"Changed destination capture Session accepted");--ordered_context.session;
  typed_action_runtime::fixture.controller.value.pending=true;
  need(perform({3,4},ordered_request,nullptr,e)==a::Outcome::Confirmed&&originals==1&&!seeds,e);
  a::Evidence evidence;need(observe({3,4},ordered_request,evidence,e)&&evidence.undo_confirmed,e);
  scope_valid=false;need(!observe({3,4},ordered_request,evidence,e),"Destination observation escaped consumed scope");
  need(perform({3,4},ordered_request,nullptr,e)==a::Outcome::Unknown&&originals==1,"Ordered destination replayed");++n;
 }
 for(int failure=0;failure<9;++failure){
  reset();fixture_ordered=consumed=true;ordered_request=request().request;auto& intent=ordered_request.npc_quest;intent.family=a::QuestDestinationFamily::Building;intent.action=2;intent.kills=0;
  typed_action_runtime::fixture.controller.value.pending=true;
  if(failure==0)consumed=false;if(failure==1)scope_valid=false;if(failure==2)Config::instance().is_host=false;
  if(failure==3)intent.family=a::QuestDestinationFamily::Zone;if(failure==4)intent.destination_region=68;
  if(failure==5)intent.family=a::QuestDestinationFamily::NPC;if(failure==6)intent.family=a::QuestDestinationFamily::DirectMonster;
  if(failure==7)ordered_context.snapshot_uuid="different";
  a::Evidence authority;const auto* supplied=failure==8?&authority:nullptr;
  need(perform({3,4},ordered_request,supplied,e)==a::Outcome::RejectedNoEffect&&!originals&&!seeds,"Unsupported destination authority caused native effect");++n;
 }
 for(int effect=0;effect<7;++effect){
  reset();fixture_ordered=consumed=true;ordered_request=request().request;auto& intent=ordered_request.npc_quest;intent.family=a::QuestDestinationFamily::Building;intent.action=2;intent.kills=0;
  typed_action_runtime::fixture.controller.value.pending=true;change_refs=effect==0;change_targets=effect==1;change_release=effect==2;change_state=effect==3;
  if(effect==4)mode=9;if(effect==5)mode=10;if(effect==6)mode=8;
  need(perform({3,4},ordered_request,nullptr,e)==a::Outcome::Unknown&&originals==1&&!seeds,"Unexpected ordered destination effect confirmed");
  need(perform({3,4},ordered_request,nullptr,e)==a::Outcome::Unknown&&originals==1,"Unknown destination effect replayed");++n;
 }
 std::cout<<n<<" actual Quest capture/UI binding/pending perform/observe cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
