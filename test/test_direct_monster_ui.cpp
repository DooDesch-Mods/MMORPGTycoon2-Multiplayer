// Use the same bounded native port fixture for the actual diagnostic body.
#define main direct_perform_check_main
#include "test_direct_monster_native.cpp"
#undef main
namespace {
std::array<float,3> cursor_hit{};unsigned dispatch_calls=0;
bool reject(const std::string& e){state.cause=e;return false;}
bool token_valid(const std::string& v){return v.size()==36;}
namespace net{std::string current_snapshot_id(){return snapshot_id;}namespace session_wire{bool connected(){return model.fault!="disconnected";}}}
namespace typed_action_runtime{bool network_binding(const std::string&,a::Binding& out,std::string& e){out={3,4};return gate("binding",e);}}
bool tools(Memory&,uintptr_t,ToolPin& out,json&,std::string& e){out={};out.cursor=reinterpret_cast<uintptr_t>(cursor_hit.data())-0x1e4;return gate("tools",e);}
namespace d{json sample_json(const Sample& v){return {{"money",v.money.cash},{"undo",v.undo.undo},{"release",v.release_edits},{"count",v.state.monsters.size()}};}}
namespace action_runtime{
uint32_t warp_cursor(uintptr_t,uintptr_t,const std::array<float,3>& p,bool& warped){cursor_hit=p;warped=model.fault!="warp_false";return model.fault=="warp"?5:0;}
uint32_t calculate_cursor_hit(uintptr_t,uintptr_t,const float*& out){out=model.fault=="hitptr"?nullptr:cursor_hit.data();return model.fault=="hit"?5:0;}
uint32_t modal_window(uintptr_t,bool& modal){modal=model.fault=="modal";return model.fault=="modal_seh"?5:0;}
uint32_t generic_release(uintptr_t,uintptr_t,const std::array<float,2>&,const std::array<float,3>& p){++dispatch_calls;
 if(model.fault=="dispatcher")return 5;
 if(model.fault!="capture_missing"){++state.captures;auto i=intent();i.position=p;state.pending=Pending{i,{3,4},snapshot_id,state.ui_token};state.ui_jobs.at(state.ui_token)["captured"]=true;}
 if(model.fault=="dispatcher_gameplay")++model.sample.money.cash;return 0;
}}
uint32_t SDL_GetMouseState(float* x,float* y){*x=*y=0;return 0;}
#include "monster_ui_body.inc"
json arguments(){return {{"token",action_id},{"region_id",63},{"tool","item_elitemonster"},{"character_slot",1},{"character_type",0},{"level",1},{"position",intent().position}};}
void ui_reset(){reset();model.pending=false;dispatch_calls=0;}
}
int main(){try{unsigned count=0;std::string e;json out;auto args=arguments();
 ui_reset();need(native_ui(args,out,e),e);need(out["attempted"]==true&&out["returned"]==true&&out["captured"]==true&&out["submitted"]==false&&out["unknown"]==false&&out["actual_hit"]==json(intent().position),"normal dispatcher UI receipt differs");
 need(dispatch_calls==1&&model.release==0&&state.pending&&state.pending->token==action_id,"adapter bypassed dispatcher or lost capture token");++count;
 need(!native_ui(args,out,e)&&dispatch_calls==1,"UI token replayed dispatcher");++count;
 for(const auto* f:{"code","pin","point","binding","tools","disconnected"}){ui_reset();model.fault=f;need(!native_ui(args,out,e)&&!dispatch_calls&&!model.prepare,e);++count;}
 for(const auto* f:{"prepare","prepare_gameplay","warp","warp_false","hit","hitptr","hover","pose_before","press","pressed","modal","modal_seh","dispatcher","capture_missing","dispatcher_gameplay"}){
  ui_reset();model.fault=f;need(!native_ui(args,out,e),std::string("bad adapter succeeded: ")+f);need(state.fault&&!state.pending&&state.ui_token.empty()&&out["unknown"]==true&&out["terminal"]==true&&!out["cause"].get<std::string>().empty(),std::string("Unknown not retained: ")+f);
  const auto calls=dispatch_calls+model.prepare+model.hover+model.press;need(!native_ui(args,out,e)&&calls==dispatch_calls+model.prepare+model.hover+model.press,"Unknown UI token replayed");++count;}
 for(const auto* key:{"token","region_id","tool","character_slot","character_type","level","position"}){ui_reset();auto bad=args;bad.erase(key);need(!native_ui(bad,out,e)&&!model.prepare,"missing UI field accepted");++count;}
 for(const auto* key:{"region_id","character_slot","character_type","level"}){ui_reset();auto bad=args;bad[key]=true;need(!native_ui(bad,out,e)&&!model.prepare,"boolean UI integer accepted");++count;}
 ui_reset();auto bad=args;bad["level"]=0;need(!native_ui(bad,out,e)&&!model.prepare,"unselected level accepted");++count;
 ui_reset();bad=args;bad["character_slot"]=0;need(!native_ui(bad,out,e)&&!model.prepare,"non-Monster catalogue slot accepted");++count;
 std::cout<<count<<" actual native UI adapter body cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
