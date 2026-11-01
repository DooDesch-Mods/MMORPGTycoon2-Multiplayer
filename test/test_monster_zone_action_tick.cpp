#include "typed_action/action_session.h"
#include <iostream>
#include <optional>
#include <stdexcept>
namespace tick_fixture {
namespace a=mt2mp::action_session;using json=nlohmann::json;
struct Memory{};struct Pin{int owner=1;};
struct Capture{std::string ui_token="token",snapshot="snapshot";a::Binding binding{3,4};Pin pin;a::ZoneIntent intent;};
struct Runtime{bool ready=true;unsigned thread=7,submits=0;std::optional<Capture> pending;std::map<std::string,json> ui_jobs;std::string last_action,cause;}state;
std::string fault;unsigned actual_thread=7,submit_calls=0;bool inside_go=false;json controller_value;
unsigned GetCurrentThreadId(){return actual_thread;}
void reject(const std::string& e){state.cause=e;}
namespace typed_action_runtime{
json controller_status(){return controller_value;}
bool original_go_admission(bool& out,std::string& e){out=inside_go;if(fault=="go"){e="Go owner changed";return false;}return true;}
bool network_binding(const std::string&,a::Binding& b,std::string& e){b={3,4};if(fault=="binding"){e="current owner unavailable";return false;}return true;}
bool submit_zone(const std::string&,const a::ZoneIntent&,std::string& e){++submit_calls;if(fault=="submit"){e="Controller rejected actual intent";return false;}return true;}
}
bool pin(Memory&,const a::Binding&,const std::string&,int32_t,int32_t,Pin&,std::string& e){if(fault=="pin"){e="full Origin changed";return false;}return true;}
bool same_origin(const Pin&,const Pin&){return fault!="origin";}
namespace action_runtime{std::string uuid(std::string& e){if(fault=="uuid"){e="UUID generation failed";return {};}return "action";}}
bool ordered_mode(){return controller_value.value("ordered",false);}
bool current_capture(const Capture&,std::string& e){if(fault=="binding"){e="current owner unavailable";return false;}return true;}
#include "zone_tick_body.inc"
void need(bool v,const std::string& e){if(!v)throw std::runtime_error(e);}
void reset(){state={};fault.clear();actual_thread=7;submit_calls=0;inside_go=false;controller_value={{"started",true},{"locked",false},{"pending",false},{"ordered",false}};state.pending=Capture{};
 state.ui_jobs["token"]={{"captured",true},{"submitted",false},{"terminal",false},{"confirmed",false},{"action_uuid",""}};}
}
int main(){using namespace tick_fixture;try{unsigned count=0;reset();tick();need(submit_calls==1&&state.submits==1&&!state.pending&&state.ui_jobs["token"]["submitted"]==true,"submission missing");++count;
 tick();need(submit_calls==1,"submission replayed");++count;
 controller_value={{"action_uuid","action"},{"pending",false},{"outcome",int(a::Outcome::Confirmed)},{"cause",""}};
 tick();need(state.ui_jobs["token"]["terminal"]==true&&state.ui_jobs["token"]["confirmed"]==true,"ACK not observed");++count;
 for(const auto* cause:{"binding","pin","origin","uuid","submit","thread","ready"}){reset();fault=cause;if(fault=="thread")actual_thread=8;if(fault=="ready")state.ready=false;
  tick();const auto job=state.ui_jobs.at("token");need(job["terminal"]==true&&job["submitted"]==false&&job["cause"]==state.cause&&!state.cause.empty()&&!state.pending,"failed intent disappeared");
  const auto calls=submit_calls;tick();need(submit_calls==calls,"rejected intent replayed");++count;}
 reset();tick();controller_value={{"action_uuid","other"},{"pending",false},{"outcome",1}};tick();need(state.ui_jobs["token"]["terminal"]==false,"foreign ACK accepted");++count;
 controller_value={{"action_uuid","action"},{"pending",false},{"outcome",int(a::Outcome::Unknown)},{"cause","original partial failure"}};tick();
 need(state.ui_jobs["token"]["confirmed"]==false&&state.ui_jobs["token"]["cause"]=="original partial failure","Unknown hidden");++count;
 reset();controller_value["ordered"]=true;controller_value["pending"]=true;tick();need(state.pending&&submit_calls==0,"pending capture discarded or submitted");++count;
 controller_value["pending"]=false;inside_go=true;tick();need(state.pending&&submit_calls==0,"capture submitted inside OriginalGo");++count;
 inside_go=false;tick();need(!state.pending&&submit_calls==1,"held flush did not submit once");tick();need(submit_calls==1,"held flush replayed");++count;
 reset();controller_value["ordered"]=true;fault="go";tick();need(!state.pending&&submit_calls==0&&state.cause=="Go owner changed","unknown Go owner submitted capture");++count;
 reset();controller_value["pending"]=true;fault="binding";tick();need(!state.pending&&submit_calls==0&&!state.cause.empty(),"retired pending owner retained");++count;
 std::cout<<count<<" actual pending UI tick cases PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
