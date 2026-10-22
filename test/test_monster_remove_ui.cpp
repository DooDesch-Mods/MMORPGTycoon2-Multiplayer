#include "monster_remove_fixture.h"
#include "monster_target_key.h"
#include "npc_target_key.h"
namespace monster_target_key=mt2mp::game::monster_target_key;
#include "ui_remove/ui_capture.h"
#include <iostream>
namespace a=mt2mp::action_session;
namespace ur=a::ui_capture;
using json=nlohmann::json;
bool fail(std::string& e,const std::string& why){e=why;return false;}
unsigned GetCurrentThreadId(){return 1;}
struct Memory{};struct Pin{};
namespace d{struct Sample{int content=1;struct Undo{uintptr_t active_graph=0;}undo;};}
struct State{bool ready=true,fault=false,pending=false;unsigned thread=1;uintptr_t image=1;std::map<std::string,json> ui_jobs;}state;
struct Config{bool is_host=false;static Config& instance(){static Config config;return config;}};
struct Op{a::Binding binding{3,4};a::Request request=remove_request().request;
 struct Birth{uintptr_t pointer=20;uint64_t uid=0x100000001ull;}birth;
 bool attempted=true,returned=true,confirmed=true;uint64_t original_calls=1;a::Evidence evidence;d::Sample before;std::string cause;};
std::map<std::string,Op> removals;
std::string remove_ui_token;const std::string exe_sha="fixture";
constexpr const char* ui_token="77777777-7777-4777-8777-777777777777";
unsigned mode=0,effects=0,observations=0,commands=0,captured=0;bool dead=false;
a::MonsterState actor;
bool token_valid(const std::string& s){return s.size()==36;}
namespace net::session_wire{bool connected(){return true;}}
namespace typed_action_runtime{json controller_status(){return {{"started",true},{"locked",mode==12&&commands>0},{"pending",false},{"action_uuid",commands? (mode==13?birth_id:action_id):""}};}}
namespace ui_remove_runtime{bool enabled(){return true;}json status(){return {{"captured",captured},{"last_origin",uint8_t(ur::Origin::MonsterWindow)}};}}
bool info_code(Memory&,std::string&){return true;}
bool info_target(const std::string&,a::Binding& binding,Pin&,d::Sample&,a::MonsterState& value,ur::LocalBinding& local,std::string&){
 binding={3,4};value=actor;local={10,20,0x100000001ull,3,4,snapshot_id,std::string("action:")+birth_id,false,false,false,true};return true;}
bool remove_local_binding(uintptr_t,uintptr_t,ur::LocalBinding& local,std::string&){
 local={10,20,0x100000001ull,3,4,snapshot_id,std::string("action:")+birth_id,false,false,false,true};return true;}
bool no_gameplay_change(const d::Sample& x,const d::Sample& y){return x.content==y.content;}
bool effect(Memory&,const Pin&,d::Sample& sample,a::MonsterState& value,std::string& e,uint64_t uid){
 ++effects;if(dead&&uid)return fail(e,"Monster created full UID is absent/direct Zone differs");value=actor;if(commands&&mode==16)sample.content=2;return true;}
bool info_default_tool(Memory&,json&,std::string&){return true;}
namespace mt2mp::action_session::ui_diagnostic{
enum class Family{Building};using UiCommand=void(*)();
struct Api{UiCommand command=nullptr;const char* destroy_label=nullptr;};
struct Context{uintptr_t image,manager,object;uint64_t uid,epoch,generation;std::string sha;Family family;};
struct Receipt{bool ui_command_returned=false,record_cleanup_confirmed=false;};struct Window{};
bool resolve_api(Memory&,uintptr_t,const std::string&,Api&,std::string&){return true;}
struct Calls{
 bool select(const Api&,const Context&,Receipt&,std::string&){return true;}
 bool command(const Api&,const Window&,Receipt& receipt,std::string&){
  ++commands;++captured;receipt.ui_command_returned=receipt.record_cleanup_confirmed=true;
  auto& job=state.ui_jobs.at(remove_ui_token);job["captured"]=job["submitted"]=true;job["action_uuid"]=action_id;
  if(Config::instance().is_host){dead=true;Op op;op.evidence.monster_before=actor;
   if(mode==1)return true;
   if(mode==2)++op.binding.native_generation;if(mode==3)op.request.target_action_uuid=action_id;
   if(mode==4)++op.birth.uid;if(mode==5)op.birth.pointer=21;if(mode==6)op.confirmed=false;
   if(mode==7)op.original_calls=0;if(mode==8)++op.evidence.monster_before.actor.level;
   if(mode==9)op.before.content=2;if(mode==10){op.confirmed=false;op.cause="exact original failure";}
   if(mode==17)op.request.snapshot_uuid=action_id;if(mode==18)op.request.canonical_record_key="action:other";
   removals.emplace(action_id,op);
  }else if(mode==15)dead=true;
  return true;
 }
};Calls& process_calls(){static Calls calls;return calls;}
}
bool info_window(Memory&,const ur::LocalBinding&,a::ui_diagnostic::Window&,json&,std::string&){return true;}
bool remove_observe(const a::Binding&,const a::Request&,a::Evidence&,std::string& e){++observations;return mode!=11||fail(e,"exact absence/Undo readback failure");}
#include "monster_remove_ui.inc"
void reset(unsigned selected,bool host){mode=selected;state={};Config::instance().is_host=host;removals.clear();remove_ui_token.clear();effects=observations=commands=captured=0;dead=false;actor=born(empty());}

namespace npc_ui_receipt_test {
namespace action=a;
namespace npc_target_key=mt2mp::game::npc_target_key;
namespace npc_census {struct State {};}
namespace undo {struct UndoWitness {bool operator==(const UndoWitness&)const=default;};}
struct Operation {
 a::Binding binding{3,4};a::Request request;uintptr_t local_pointer=20;uint64_t local_uid=0x100000001ull;
 unsigned attempted=3,returned=3,destroyprops_original_calls=1;bool confirmed=true;
 a::Evidence evidence;json quest_tasks_before=json::array();std::string cause;
};
struct State {std::map<std::string,Operation> operations;uintptr_t image=1;} state;
unsigned mode=0,reads=0,observations=0;bool dead=false;
a::NpcState actor;
bool read_effect(Memory&,uintptr_t,uintptr_t,uint64_t uid,a::NpcState& out,a::Ledger&,undo::UndoWitness&,npc_census::State&,std::string& error,bool,json* tasks){
 ++reads;if(dead&&uid)return fail(error,"NPC action created UID absent from actual manager");out=actor;
 if(mode==9)++out.count;*tasks=json::array();return true;
}
bool remove_observe(const a::Binding&,const a::Request&,a::Evidence&,std::string& error){
 ++observations;return mode!=7||fail(error,"original NPC absence/Undo observation failure");
}
bool receipt(json& result,std::string& error){
 const a::Binding binding{3,4};const ur::LocalBinding local{10,20,0x100000001ull,3,4,snapshot_id,std::string("action:")+birth_id,false,false,true};
 const std::string target=birth_id;const auto before=actor;const a::Ledger money{};const undo::UndoWitness undo_before{};
 const json before_tasks=json::array();Memory memory;const uintptr_t region=1;const bool quest_target=false;
 json job={{"action_uuid",action_id}};
#include "npc_remove_ui_receipt.inc"
 result=job;error.clear();return true;
}
unsigned run(){
 unsigned cases=0;
 for(unsigned value=0;value<=7;++value){
  mode=value;reads=observations=0;state={};dead=true;Config::instance().is_host=true;
  ::mode=0;::commands=1;Operation op;op.request=remove_request().request;op.request.action=a::Action::RemoveNPC;op.evidence.npc_before=actor;
  if(value==2)++op.local_uid;if(value==3)op.confirmed=false;if(value==4)++op.evidence.npc_before.count;
  if(value==5){op.confirmed=false;op.cause="original NPC removal failure";}if(value==6)::mode=12;
  if(value!=1)state.operations.emplace(action_id,op);
  json result;std::string error;const bool ok=receipt(result,error);
  need(value==0?ok:!ok,"NPC synchronous Host receipt outcome differs");need(!reads,"Host tried reading a removed NPC as live");
  if(value==0)need(result.value("execution_confirmed",false)&&observations==1,"Host skipped existing exact absence/Undo observer");
  else need(result.value("unknown",false),"failed Host receipt lost Unknown status");
  if(value==5)need(error=="original NPC removal failure","NPC operation failure cause lost");
  if(value==7)need(error=="original NPC absence/Undo observation failure","NPC observation cause lost");++cases;
 }
 for(unsigned value:{0u,8u,9u}){
  mode=value;reads=observations=0;dead=value==8;Config::instance().is_host=false;json result;std::string error;
  const bool ok=receipt(result,error);need(value==0?ok:!ok,"NPC Client unchanged-before-Host check differs");
  need(reads==1&&!observations,"NPC Client bypassed live unchanged readback");
  if(value==0)need(!result.value("execution_confirmed",true),"Client capture claimed native execution");++cases;
 }
 return cases;
}
}

int main(){try{unsigned n=npc_ui_receipt_test::run();json result;std::string e;
 const json args={{"family","monster"},{"token",ui_token},{"target_action_uuid",birth_id}};
 for(unsigned value=0;value<=18;++value){if(value==14||value==15||value==16)continue;reset(value,true);e.clear();
  const auto ok=native_remove_ui(args,result,e);
  if(value==0)need(ok&&result.value("execution_confirmed",false)&&!result.value("unknown",true)&&effects==1&&observations==1,"synchronous Host removal was treated as missing live actor");
  else need(!ok&&result.value("unknown",false)&&effects==1,"wrong Host operation bypassed exact receipt check");
  if(value==10)need(e=="exact original failure","original operation cause lost");
  if(value==11)need(e=="exact absence/Undo readback failure","fresh observer cause lost");
  need(commands==1&&remove_ui_token.empty(),"UI command count or scope differs");
  need(!native_remove_ui(args,result,e)&&commands==1,"used UI token replayed");++n;
 }
 for(unsigned value:{0u,15u,16u}){reset(value,false);e.clear();const auto ok=native_remove_ui(args,result,e);
  need(value==0?ok:!ok,"Client capture-only alive/no-effect guard differs");need(effects==2&&!observations&&commands==1,"Client bypassed original capture-only readback");
  if(value==0)need(!result.value("execution_confirmed",true)&&!result.value("unknown",true),"Client capture claimed execution");++n;
 }
 std::cout<<n<<" actual Monster Remove UI Host/Client receipt cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
