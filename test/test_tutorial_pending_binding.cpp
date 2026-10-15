#include "typed_action/action_session.h"
#include <atomic>
#include <iostream>
#include <stdexcept>
namespace action=mt2mp::action_session;
namespace action_session=action;
namespace {
using json=nlohmann::json;
uint32_t GetCurrentThreadId(){return 7;}
struct Controller{action::Status s;action::Context c;action::Binding b;
    action::Status status(){return s;}action::Context bound_context(){return c;}action::Binding bound_binding(){return b;}};
struct Session:action::Session{action::Context c;bool current(action::Context& out,std::string&)override{out=c;return true;}};
struct Engine{int reads=0;bool available=true;bool current(const action::Binding&,const std::string&,std::string& e){++reads;if(!available){e="original NativeEngine current owner mismatch";return false;}return true;}};
struct Receipt{bool unknown=false;std::string cause;};
struct Live{bool blocked=false;Receipt value;bool locked(){return blocked;}Receipt receipt(){return value;}};
struct Fence{bool blocked=false;std::string reason;bool locked()const{return blocked;}std::string cause()const{return reason;}}fixture_fence;
namespace live_host_state{const Fence& economic_fence(){return fixture_fence;}}
struct Runtime{uint32_t thread=7;Controller controller;Session session;Engine engine;Live* live_state=nullptr;bool lobby_retiring=false;std::string live_error;}fixture_runtime;
std::atomic<Runtime*> published_runtime;
namespace typed_action_runtime {
#include "binding_providers.inc"
}
struct World{uint64_t world_epoch=8,generation=9;};struct Pin{World world;};
bool fail(std::string& e,const std::string& why){e=why;return false;}
}
namespace mt2mp::action_session {Session& selected_session(){return ::fixture_runtime.session;}}
namespace {
// This is the exact shared fixture pin branch, including its idle provider call.
bool fixture_binding(const json& controller,Pin& p,bool client,const std::string& pending_id,
    const action::Context* submitted_context,const action::Binding* submitted_binding,std::string& e){
#include "fixture_binding.inc"
    return true;
}
Live live;
const std::string id="1ff2cd3c-05a4-4ae4-94a9-c7e97d82cef9";
void reset(){fixture_runtime={};live={};fixture_fence={};fixture_runtime.live_state=&live;fixture_runtime.controller.c={4,2,1,3,action::Role::Client,"common"};
    fixture_runtime.session.c=fixture_runtime.controller.c;fixture_runtime.controller.b={8,9};fixture_runtime.controller.s.started=true;published_runtime=&fixture_runtime;}
void need(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
void run(){std::string e;action::Binding binding;Pin p;const json controller={{"initialized",true}};
    reset();const auto context=fixture_runtime.controller.c;const auto original=fixture_runtime.controller.b;
    need(typed_action_runtime::network_binding("common",binding,e)&&binding==original,"idle original binding rejected");
    fixture_runtime.controller.s.pending=true;fixture_runtime.controller.s.action_uuid=id;
    need(!typed_action_runtime::network_binding("common",binding,e)&&e=="network admission requires idle unlocked Controller","real pending failure not reproduced");
    need(typed_action_runtime::observe_startingpoint_submission(context,original,id,e)&&fixture_runtime.engine.reads==2,"pending observation did not validate native owner");
    const auto reads=fixture_runtime.engine.reads;
    need(fixture_binding(controller,p,true,id,&context,&original,e)&&fixture_runtime.engine.reads==reads+1,"actual fixture pin still calls idle admission");
    need(!fixture_binding(controller,p,true,id,nullptr,&original,e),"post-pin without prebound Context accepted");
    need(!typed_action_runtime::observe_startingpoint_submission(context,original,"foreign",e),"foreign UUID accepted");
    fixture_runtime.controller.s.outcome=action::Outcome::Unknown;need(!typed_action_runtime::observe_startingpoint_submission(context,original,id,e),"Unknown accepted");fixture_runtime.controller.s.outcome=action::Outcome::Pending;
    fixture_runtime.controller.s.locked=true;need(!typed_action_runtime::observe_startingpoint_submission(context,original,id,e),"locked pending accepted");fixture_runtime.controller.s.locked=false;
    fixture_runtime.controller.b.native_generation=10;need(!typed_action_runtime::observe_startingpoint_submission(context,original,id,e),"changed Controller binding accepted");fixture_runtime.controller.b=original;
    fixture_runtime.controller.c.session=99;need(!typed_action_runtime::observe_startingpoint_submission(context,original,id,e),"changed Controller Context accepted");fixture_runtime.controller.c=context;
    fixture_runtime.session.c.snapshot_uuid="other";need(!typed_action_runtime::observe_startingpoint_submission(context,original,id,e),"changed actual Session accepted");fixture_runtime.session.c=context;
    fixture_runtime.engine.available=false;need(!typed_action_runtime::observe_startingpoint_submission(context,original,id,e)&&e=="original NativeEngine current owner mismatch","native owner cause lost");fixture_runtime.engine.available=true;
    live.value={true,"original partial ledger"};need(!typed_action_runtime::observe_startingpoint_submission(context,original,id,e)&&e.find("original partial ledger")!=std::string::npos,"live Unknown cause lost");live.value={};
    fixture_runtime.lobby_retiring=true;need(!typed_action_runtime::observe_startingpoint_submission(context,original,id,e),"retiring accepted");fixture_runtime.lobby_retiring=false;
    fixture_runtime.controller.s.pending=false;fixture_runtime.controller.s.outcome=action::Outcome::Confirmed;need(!typed_action_runtime::observe_startingpoint_submission(context,original,id,e),"completed action treated as immediate Pending");
    need(typed_action_runtime::network_binding("common",binding,e),"original idle admission changed");
}
}
int main(){try{run();std::cout<<"PASS real idle provider rejection, exact Pending observer and actual fixture pin branch\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
