#include "monster_remove_fixture.h"
#include "monster_zone_read.h"
#include "npc_census.h"
#include "direct_monster_identity.h"
#include "ui_remove/ui_capture.h"
#include <iostream>
#include <map>
#include <atomic>
#include <memory>
#include <cstring>
using json=nlohmann::json;
namespace a=mt2mp::action_session;
namespace identity=mt2mp::game::direct_monster_identity;
namespace ur=a::ui_capture;
namespace mz=mt2mp::game::monster_zone_read;
namespace monster_zone_read=mt2mp::game::monster_zone_read;
namespace npc_census=mt2mp::npc_census;
unsigned GetCurrentThreadId(){return 1;}
bool fail(std::string& e,const std::string& why){e=why;return false;}
struct Memory{bool copy(uintptr_t object,void* out,size_t size,std::string& e){
 if(object!=100||size!=sizeof(uintptr_t))return fail(e,"service memory span differs");
 const uintptr_t ap=1+0x16c0770;std::memcpy(out,&ap,size);return true;}};
namespace mt2mp::npc_census{bool read_name(::Memory&,uintptr_t,std::string& out,std::string&){out="Deathblood";return true;}}
struct World{uintptr_t root=10,selection=40;bool saves=false,edits=false,active_graph=false;};
struct Pin{World world;struct Region{uintptr_t region=20;}region;};
namespace d {struct Sample{mz::State state;a::Ledger money;struct Undo{uintptr_t active_graph=0;std::vector<uintptr_t> undo;}undo;};}
namespace qr {struct Witness{std::vector<uintptr_t> quests;int completed=0;};bool survivors(const Witness&,const Witness&,std::string&){return true;}bool unchanged(const Witness&,const Witness&,std::string&){return true;}}
namespace rr {
template<class Name>bool read_names(const mz::State&,Name&&,std::map<uint64_t,std::string>& out,std::string&){out[0x100000001ull]="Deathblood";return true;}}
unsigned mode=0;bool in_callback=false;
namespace typed_action_runtime{
namespace action=a;
struct Status{bool started=true,locked=false,pending=true;std::string action_uuid=action_id;};
struct Controller{Status value;Status status()const{return value;}
 a::Context bound_context()const{a::Context context;context.snapshot_uuid=snapshot_id;return context;}
 a::Binding bound_binding()const{return {3,4};}};
struct Session{bool current(a::Context& context,std::string&){context.snapshot_uuid=snapshot_id;return true;}};
struct Engine{bool current(const a::Binding& binding,const std::string& snapshot,std::string&){return binding==a::Binding{3,4}&&snapshot==snapshot_id;}};
struct Runtime{unsigned thread=1;bool lobby_retiring=false;Controller controller;Session session;Engine engine;}fixture_runtime;
std::atomic<Runtime*> published_runtime{&fixture_runtime};
#include "remove_network_binding_body.inc"
json controller_status(){const auto value=fixture_runtime.controller.status();return {{"started",value.started},{"locked",value.locked},{"pending",value.pending&&mode!=13},{"action_uuid",value.action_uuid}};}
}
struct Runtime{bool ready=true;unsigned thread=1;uintptr_t image=1;}state;
struct RemoveOperation {
 a::Binding binding;a::Request request;Pin pin;a::Evidence evidence;identity::Entry birth;mz::Monster row;d::Sample before;
 qr::Witness quests;uintptr_t graph=0;uint64_t character_id=0;
 std::map<uint64_t,std::string> names_before;
 bool attempted=false,returned=false,confirmed=false,retired=false,seed_attempted=false;uint64_t original_calls=0;std::string cause;
};
std::map<std::string,RemoveOperation> removals;const ur::Replay* remove_replay=nullptr;
unsigned original_count=0,seeds=0;bool mutated=false;MonsterState before_value;
identity::Scope current_scope{snapshot_id,3,4,10,20,30};
identity::Scope scope(const Pin&,const d::Sample&){return current_scope;}
bool same_world(const World&,const World&){return mode!=9;}
a::Ledger ledger(const a::Ledger& v){return v;}
bool remove_code(Memory&,std::string& e){return mode!=1||fail(e,"body guard");}
bool pin(Memory&,const a::Binding& binding,const std::string& snapshot,int32_t,Pin&,std::string& e){
 if(binding!=a::Binding{3,4}||snapshot!=snapshot_id)return fail(e,"current binding differs");
 return mode!=4||removals.empty()||fail(e,"current local binding changed");}
namespace d {bool sample(Memory&,const Pin::Region&,Sample& out,std::string&){out.state.monster_manager=30;return true;}}
bool move_target(Memory&,const a::Binding&,const std::string&,int32_t,const std::string&,Pin&,d::Sample& s,mz::Monster& row,std::string& e){
 if(mode==2)return fail(e,"full UID/current manager mismatch");row.pointer=100;row.uid=0x100000001ull;row.rank=1;s.money=money();return true;}
bool effect(Memory&,const Pin&,d::Sample& s,MonsterState& v,std::string&,uint64_t=0){
 v=mutated?removed(before_value):before_value;s.money=money();s.state.monster_manager=30;s.undo.undo=mutated?std::vector<uintptr_t>{50}:std::vector<uintptr_t>{};
 if(mutated&&mode==10)++s.money.line_income[19];if(mutated&&mode==11)++v.roster.created_monsters;
 if(mutated&&mode==12)++v.roster.global_random[0];return true;}
template<class T>bool field(Memory&,uintptr_t object,size_t offset,T& v,std::string&){
 uint64_t value=1;if(object==100){if(offset==0)value=state.image+0x16c0770+(in_callback&&mode==15?8:0);
 else if(offset==0x18)value=0x100000001ull;else if(offset==0x178)value=20;}
 if(object==20&&offset==0x834)value=63;v=T(value);return true;}
bool read_remove_quests(Memory&,const a::Binding&,const std::string&,uintptr_t,const std::string&,qr::Witness&,std::string& e,const qr::Witness* =nullptr){return mode!=3||fail(e,"stale Quest weak owner");}
bool quest_before(const qr::Witness&,a::MonsterQuestRetirement& out,std::string&){out=remove_result().evidence.monster_quests;return true;}
bool quest_before_matches(const a::MonsterQuestRetirement& a,const a::MonsterQuestRetirement& b){return a==b;}
bool quest_receipt_fits(const a::MonsterQuestRetirement&){return true;}
bool quest_digest(const qr::Witness&,std::string& out,std::string&){out=std::string(64,'d');return true;}
bool remove_absent(Memory&,const Pin&,const d::Sample&,const RemoveOperation&,std::string& e){return mode!=7||fail(e,"PendingDelete target UID still registered");}
bool remove_inverse(Memory&,const Pin&,const RemoveOperation&,const d::Sample&,std::string& e){return mode!=8||fail(e,"DoMakeMonster prior pose differs");}
uint32_t remove_original(uintptr_t,uintptr_t);
namespace action_runtime{uint32_t seed(uintptr_t,const std::array<uint64_t,4>&){++seeds;return mode==6?1:0;}}
struct Config{bool is_host=true;static Config& instance(){static Config config;return config;}};
namespace net {std::string current_snapshot_id(){return snapshot_id;}}
bool authorize_remove_replay(const ur::Replay&,std::string&);
bool remove_local_binding(uintptr_t,uintptr_t,ur::LocalBinding&,std::string&);
namespace npc_probe {
namespace action_runtime {bool remove_local_binding(uintptr_t,uintptr_t,ur::LocalBinding&,std::string& e){return fail(e,"unexpected NPC service branch");}}
namespace monster_action_runtime {using ::remove_local_binding;using ::authorize_remove_replay;}
}
namespace remove_diagnostics {
struct LocalTarget{bool normal_building=false,normal_scenery=false;std::string kind_cause;};
bool resolve_local_target(uintptr_t,uintptr_t,LocalTarget&,std::string& e){return fail(e,"unexpected Building service branch");}
}
ur::LocalBinding local_binding(const remove_diagnostics::LocalTarget&){throw std::runtime_error("unexpected Building conversion");}
namespace ui=ur;
struct ActualServices:ur::Services {

 Memory memory;uintptr_t image=1;
 bool on_game_thread()noexcept override{return true;}
 ur::Policy policy(std::string&)override{return ur::Policy::AuthorityRequired;}
 bool array(ur::NativePropArray* a,ur::ArrayObservation& out,std::string&)override{out={a->addresspoint,uintptr_t(a->data),a->count?a->data[0]:0,a->count,a->capacity};return true;}
 #include "remove_service_resolve_body.inc"
 bool fresh_action_uuid(std::string& out,std::string&)override{out=action_id;return true;}
 bool submit(const ur::Intent&,std::string& e)override{return fail(e,"unexpected public submit");}
 bool authorize_replay(const ur::Replay& replay,std::string& e)override{return ::authorize_remove_replay(replay,e);}
 void original(ur::DestroyProps function,void* manager,ur::NativePropArray* array)override{function(manager,array);}
 void log(const char*,const char*,const std::string&)noexcept override{}
 void show_rejection(const std::string&)noexcept override{}
} services;
std::unique_ptr<ur::Capture> capture;
void actual_original(void*,ur::NativePropArray*){++original_count;mutated=true;if(mode==5)throw std::runtime_error("fixture original Destroy failure");}
uint32_t remove_original(uintptr_t manager,uintptr_t object){
 in_callback=true;struct Reset{~Reset(){in_callback=false;}}reset;
 if(mode==16)Config::instance().is_host=!Config::instance().is_host;
 uintptr_t target=object;ur::NativePropArray array{1,&target,1,1};
 try{capture->destroy_props(reinterpret_cast<void*>(manager),&array,ur::Origin::Foreign);return 0;}
 catch(const std::exception& problem){capture->record_entry_failure(std::string("UI Remove Framework authority: ")+problem.what());return 0;}
}
namespace ui_remove_runtime{
 bool enabled(){return mode!=14;}
 json status(){return {{"replay_originals",capture?capture->status().replay_originals:0}};}
 bool with_replay(const ur::Replay& replay,const std::function<bool(std::string&)>& callback,std::string& e){return capture->with_replay(replay,callback,e);}
}
#include "remove_binding_body.inc"
#include "remove_replay_binding_body.inc"
#include "remove_perform_body.inc"
void reset(unsigned selected){
 mode=selected;in_callback=false;capture=std::make_unique<ur::Capture>(services);std::string setup;need(capture->publish_disabled(&actual_original,setup)&&capture->set_enabled(true,setup),setup);Config::instance().is_host=true;typed_action_runtime::fixture_runtime.controller.value={};remove_replay=nullptr;original_count=seeds=0;mutated=false;removals.clear();identity::registry=identity::Registry{};
 before_value=born(empty());std::string e;need(identity::registry.stage(birth_id,current_scope,100,0x100000001ull,e),e);
 need(identity::registry.confirm(birth_id,e),e);
}
int main(){try{unsigned n=0;std::string e;
 for(unsigned which=0;which<=16;++which){reset(which);auto request=remove_request().request;
  const auto result=remove_perform({3,4},request,nullptr,e);
  if(which==0){need(result==a::Outcome::Confirmed&&original_count==1&&removals.at(action_id).confirmed,"original Remove did not confirm");identity::Entry entry;need(!identity::registry.lookup(birth_id,current_scope,entry,e),"confirmed absence did not tombstone birth");}
  else if(which==1||which==2||which==3||which==13||which==14){need(result==a::Outcome::RejectedNoEffect&&!original_count&&removals.empty(),"preflight rejection executed original");}
  else if(which==6||which==12){need(result==a::Outcome::Confirmed,"Host-only flow rejected an inactive Client-only failure port");}
  else{need(result==a::Outcome::Unknown,"partial or changed native effect escaped Unknown");identity::Entry entry;need(identity::registry.lookup(birth_id,current_scope,entry,e),"failed native proof retired birth");}
  if(which==15)need(!original_count&&e.find("own replay current target binding differs: Monster Remove capture class/full UID/Region differs")!=std::string::npos&&
      e.find("did not return exactly one")==std::string::npos,"first nested resolver cause was overwritten");
  if(which==16)need(!original_count&&e.find("private replay binding, Controller or role differs")!=std::string::npos,"callback role change escaped reauthorization");
  if(removals.contains(action_id)){const auto previous=original_count;need(remove_perform({3,4},request,nullptr,e)==a::Outcome::Unknown&&original_count==previous,"repeated mutation replayed native Destroy");}++n;
 }
 for(unsigned which:{0u,6u,12u}){reset(which);Config::instance().is_host=false;auto request=remove_request().request;auto host=remove_result().evidence;
  const auto result=remove_perform({3,4},request,&host,e);
  need(which==0?result==a::Outcome::Confirmed:result==a::Outcome::Unknown,"Client RNG adoption or authoritative after-state check missed");
  need(seeds==1,"Client did not seed exactly once");++n;
 }
 reset(0);auto host=remove_result().evidence;++host.monster_before.actor.level;
 need(remove_perform({3,4},remove_request().request,&host,e)==a::Outcome::RejectedNoEffect&&!original_count&&!seeds,"Host-before mismatch mutated Client");++n;
 reset(0);host=remove_result().evidence;host.monster_quests.before_sha256=std::string(64,'f');
 need(remove_perform({3,4},remove_request().request,&host,e)==a::Outcome::RejectedNoEffect&&!original_count&&!seeds&&
      e.find("Quest Client before")!=std::string::npos,"Host Quest mismatch seeded RNG or executed Destroy");++n;
 reset(0);ur::LocalBinding local;a::Binding admitted{9,9};
 need(!typed_action_runtime::network_binding(snapshot_id,admitted,e)&&admitted==a::Binding{}&&
      e=="network admission requires idle unlocked Controller","real pending admission guard bypassed");++n;
 need(!remove_local_binding(40,100,local,e)&&local==ur::LocalBinding{}&&
      e=="network admission requires idle unlocked Controller","public Remove capture accepted pending Controller");++n;
 typed_action_runtime::fixture_runtime.controller.value.pending=false;
 need(remove_local_binding(40,100,local,e)&&local.uid==0x100000001ull,"idle public Remove capture rejected");++n;
 for(unsigned guard=0;guard<3;++guard){reset(0);auto& value=typed_action_runtime::fixture_runtime.controller.value;
  if(guard==0)value.started=false;else if(guard==1)value.locked=true;else value.action_uuid=birth_id;
  need(remove_perform({3,4},remove_request().request,nullptr,e)==a::Outcome::RejectedNoEffect&&!original_count&&removals.empty(),"wrong pending Controller executed original");++n;}
 reset(0);Config::instance().is_host=false;
 need(remove_perform({3,4},remove_request().request,nullptr,e)==a::Outcome::Unknown&&!original_count&&
      e=="Monster Remove private replay binding, Controller or role differs","wrong replay role executed original");++n;
 reset(0);need(remove_bound_local({3,4},snapshot_id,40,100,local,e),e);
 ur::Replay replay{ur::ReplayPurpose::HostExecution,{action_id,snapshot_id,std::string("action:")+birth_id},local};
 need(!authorize_remove_replay(replay,e)&&!original_count,"unbound private replay accepted");++n;
 reset(0);need(remove_bound_local({3,4},snapshot_id,40,100,local,e),e);
 replay={ur::ReplayPurpose::HostExecution,{action_id,snapshot_id,std::string("action:")+birth_id},local};
 RemoveOperation reserved;reserved.binding={3,4};reserved.request=remove_request().request;reserved.birth={};
 reserved.birth.pointer=100;reserved.birth.uid=0x100000001ull;removals.emplace(action_id,reserved);remove_replay=&replay;
 need(remove_local_binding(40,100,local,e)&&local==replay.binding,"exact private pending resolver did not return full re-read binding");++n;
 need(!remove_local_binding(41,100,local,e)&&local==ur::LocalBinding{}&&e=="Monster Remove private resolver target differs","private manager mismatch admitted");++n;
 need(!remove_local_binding(40,101,local,e)&&local==ur::LocalBinding{}&&e=="Monster Remove private resolver target differs","private object mismatch admitted");++n;
 remove_replay=nullptr;need(!remove_local_binding(40,100,local,e)&&e=="network admission requires idle unlocked Controller","private scope leaked into ordinary pending resolver");++n;
 reset(0);need(remove_bound_local({3,4},snapshot_id,40,100,local,e),e);
 replay={ur::ReplayPurpose::HostExecution,{action_id,snapshot_id,std::string("action:")+birth_id},local};
 reserved.binding={3,4};reserved.request=remove_request().request;reserved.birth.pointer=100;reserved.birth.uid=0x100000001ull;
 removals.emplace(action_id,reserved);remove_replay=&replay;
 need(!capture->with_replay(replay,[&](std::string& nested){capture->record_entry_failure("exact nested caller failure");nested="later generic callback failure";return false;},e)&&
      e=="exact nested caller failure"&&capture->status().cause==e&&!original_count,"nested cause did not survive callback failure");++n;
 remove_replay=nullptr;
 std::cout<<n<<" actual Remove perform/shared Capture/Services pending and no-replay cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
