#include "typed_action/action_session.h"
#include "ui_remove/diagnostic/ui_diagnostic.h"
#include <iostream>
#include <map>
#include <regex>
namespace a=mt2mp::action_session;
using json=nlohmann::json;
void need(bool v,const std::string& e){if(!v)throw std::runtime_error(e);}
bool fail(std::string& e,const std::string& why){e=why;return false;}
constexpr const char* fixture_action_id="11111111-1111-4111-8111-111111111111",*birth_id="33333333-3333-4333-8333-333333333333",*exe_sha="exact fixture";
constexpr uintptr_t image=0x10000000;
unsigned mode=0,selection_calls=0,commanded=0,inputs=0;std::string move_ui_token;
struct Runtime{uintptr_t image=::image;unsigned thread=1;bool ready=true,fault=false,pending=false;std::map<std::string,json> ui_jobs;}state;
unsigned GetCurrentThreadId(){return 1;}
bool token_valid(const std::string& value){return std::regex_match(value,std::regex("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"));}
struct Memory:a::ui_capture::Memory{
 bool copy(uintptr_t p,void* output,size_t n,std::string& e)override{
  if(p!=2000+0x400||n!=2)return fail(e,"unexpected raw read");
  auto out=static_cast<unsigned char*>(output);out[0]=inputs<3?1:0;out[1]=inputs<3?0:1;
  if(mode==13)out[1]=0;return true;}
 bool region(uintptr_t,a::ui_capture::Region&,std::string&)override{return false;}
};
struct Pin{struct{uintptr_t root=10;}world;struct{uintptr_t region=20;}region;};
namespace d{struct Sample{struct{bool active_graph=false;}undo;};}
template<class T>bool field(Memory&,uintptr_t base,size_t off,T& out,std::string& e){
 const std::map<std::pair<uintptr_t,size_t>,uintptr_t> fields{
  {{image,0x13df9d0},1000},{{1000,0x190},2000},{{2000,0},image+0x172f1f0},
  {{2000,0x460},3000},{{2000,0x468},1},{{2000,0x46c},1},{{3000,8},4000},{{4000,0},image+0x1705230},{{3000,0x10},5000}};
 auto f=fields.find({base,off});if(f==fields.end())return fail(e,"unexpected field read");out=T(f->second);
 if(mode==11&&base==3000&&off==0x10)out=T(5001);return true;}
namespace typed_action_runtime{json controller_status(){return {{"started",true},{"locked",mode==1},{"pending",false}};}}
namespace ui_move_runtime{bool enabled(){return true;}}
namespace net::session_wire{bool connected(){return true;}}
bool info_code(Memory&,std::string& e){return mode!=2||fail(e,"native code changed");}
bool info_target(const std::string&,a::Binding& binding,Pin&,d::Sample& sample,a::MonsterState& value,a::ui_capture::LocalBinding& local,std::string& e){
 if(mode==3)return fail(e,"stale full UID birth_id");binding={3,4};sample.undo.active_graph=mode==4;value.actor.name="Deathblood";
 local={40,5000,7,3,4,"snapshot",std::string("action:")+birth_id};return true;}
uint32_t shard_region(uintptr_t,uintptr_t,const std::array<float,3>&,uintptr_t& destination){destination=mode==5?21:20;return 0;}
bool info_default_tool(Memory&,json& receipt,std::string& e){receipt={{"original",true}};return mode!=6||fail(e,"default original tool differs");}
bool info_window(Memory&,const a::ui_capture::LocalBinding&,a::ui_diagnostic::Window& out,json& receipt,std::string& e){
 out.window=6000;out.target=5000;receipt={{"window_matches",mode!=8}};return mode!=8||fail(e,"wrong live Monster window owner");}
bool effect(Memory&,const Pin&,d::Sample&,a::MonsterState& out,std::string&,uint64_t){out.actor.name="Deathblood";return true;}
bool no_gameplay_change(const d::Sample&,const d::Sample&){return mode!=9;}
namespace move_kiss{enum class Family{DirectMonster,Other};struct Intent{Family family=Family::DirectMonster;std::string object_key;struct{float x=0,y=0,z=0;std::array<float,4> rotation{};std::array<float,3> scale{};}after;};}
void move_ui_failed(const std::string&);
#include "monster_move_ui_callbacks_body.inc"
namespace action_runtime{uint32_t move_input(uintptr_t,uintptr_t,uintptr_t,const std::array<float,3>&,uintptr_t){++inputs;return mode==12?5:0;}}
namespace mt2mp::action_session::ui_diagnostic {
bool resolve_api(ui::Memory&,uintptr_t,std::string_view,Api&,std::string&,Family){return true;}
struct Port:Calls{
 bool select(const Api&,const Context& c,Receipt& r,std::string& e)override{++selection_calls;need(c.target==5000&&c.uid==7,"wrong native select target");r.selection_attempted=true;r.selection_returned=mode!=7;return mode!=7||fail(e,"original select failed");}
 bool command(const Api& api,const Window& w,Receipt& r,std::string& e)override{++commanded;
  need(uintptr_t(api.command)==image+0x7490c0&&std::string(api.destroy_label)=="Move"&&w.window==6000,"original MonsterInfoWindow Move route substituted");
  r.ui_command_attempted=true;r.ui_command_returned=mode!=10;r.record_cleanup_confirmed=mode!=10;return mode!=10||fail(e,"original command failed");}
};
Calls& process_calls(){static Port port;return port;}
}
#include "monster_move_ui_body.inc"
int main(){try{std::string e;json out;unsigned n=0;
 const json base={{"family","monster"},{"token",fixture_action_id},{"target_action_uuid",birth_id},{"position",{8021,70,8550}}};
 for(unsigned current=0;current<14;++current){mode=current;state=Runtime{};move_ui_token.clear();selection_calls=commanded=inputs=0;
  const auto result=native_move_ui(base,out,e);
  if(!current){need(result&&out["returned"]==true&&out["awaiting_original_update"]==true&&!out.value("submitted",true)&&inputs==3&&selection_calls==1&&commanded==1,"normal Move UI did not arm original Update/Purchase");
    move_kiss::Intent intent;intent.object_key=std::string("action:")+birth_id;
    need(move_ui_captured(intent,fixture_action_id,e)&&state.ui_jobs.at(fixture_action_id).value("captured",false)&&!state.ui_jobs.at(fixture_action_id).value("submitted",true)&&!state.ui_jobs.at(fixture_action_id).value("awaiting_original_update",true),"actual Purchase capture did not retain pending typed request");
    need(!move_ui_captured(intent,fixture_action_id,e),"diagnostic captured two native purchases");
    move_ui_submitted(fixture_action_id);need(state.ui_jobs.at(fixture_action_id).value("submitted",false)&&move_ui_token.empty(),"Controller submission did not settle exact UI token");
    const auto old=inputs;need(!native_move_ui(base,out,e)&&inputs==old,"used Move UI token replayed inputs");}
  else if(current<=5)need(!result&&!selection_calls&&!commanded&&!inputs&&state.ui_jobs.empty(),"failed preflight performed UI effects");
  else need(!result&&state.ui_jobs.at(fixture_action_id).value("unknown",false),"attempted UI failure lost Unknown receipt");
  if(current==8||current==9)need(!commanded&&!inputs,"wrong owner or changed before-state invoked Move");++n;
 }
 for(auto field:{"family","token","target_action_uuid","position"}){mode=0;state=Runtime{};move_ui_token.clear();auto bad=base;bad.erase(field);need(!native_move_ui(bad,out,e),"missing Move UI field accepted");++n;}
 std::cout<<n<<" actual original Monster Move UI invocation cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
