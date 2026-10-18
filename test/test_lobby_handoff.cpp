#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fixture {
using json=nlohmann::json;
int checks=0;
void need(bool value,const char* detail){++checks;if(!value)throw std::runtime_error(detail);}
enum class Phase {Action,Render};
Phase phase=Phase::Action;
int owner_a=0,owner_b=0;
void* ui_owner=&owner_a;
int ui_mode=2;
uint64_t generation=7;
bool pending=false,load_attempted=false,framework_creating_world=false,framework_new_world_requested=false;
void* framework_new_world_owner=nullptr;
uint64_t framework_new_world_generation=0;
json framework_new_world_receipt,framework_new_world_observation,framework_last_action;
bool choosing=true,host=true,mp_visible=true,picker_visible=true;
std::string cause,ui_status,list_status;
int command_failures=0;
int sdk_calls=0,launch_calls=0,observation_calls=0,picker_calls=0,hide_failure_at=0;
bool sdk_throws=false,picker_ok=true;
std::vector<std::pair<bool,bool>> visibility_calls;
std::vector<json> sdk_inputs;
json launch_reply,observe_reply;
namespace native {
struct WorldStatus {int32_t active_mode_index=2,requested_mode_index=2;bool title_transition_pending=false;};
WorldStatus actual_world;
bool world_read_ok=true;
int world_reads=0;
bool read_world_status(WorldStatus& out,std::string& error){
    ++world_reads;
    if(!world_read_ok){error="original native WorldStatus read failure";return false;}
    out=actual_world;return true;
}
}
bool lobby_host(){return host;}
bool fail(const std::string& detail){++command_failures;cause=detail;ui_status="Sitzung nicht verfügbar";return false;}
int view_show(bool browser,bool shown){
    visibility_calls.emplace_back(browser,shown);
    if(hide_failure_at==int(visibility_calls.size()))return 9;
    (browser?picker_visible:mp_visible)=shown;return 0;
}
bool open_world_picker(){
    ++picker_calls;
    if(!picker_ok){cause="original picker failure";return false;}
    choosing=true;picker_visible=true;mp_visible=false;return true;
}
json framework_request(const std::string& command,const json& input){
    need(phase==Phase::Render,"SDK UI calls must run from render, never action/Tick");
    need(command=="ui.new_world","handoff uses the original native wizard API");
    ++sdk_calls;sdk_inputs.push_back(input);
    if(sdk_throws)throw std::runtime_error("original SDK request exception");
    if(native::actual_world.active_mode_index!=2||native::actual_world.requested_mode_index!=2||native::actual_world.title_transition_pending)
        return {{"status","rejected"},{"confirmed",false},{"attempted",false},
                {"cause","New Game requires idle Title with no pending mode transition"}};
    if(input.value("observe",false)){++observation_calls;return observe_reply;}
    ++launch_calls;
    need(mp_visible&&picker_visible,"Multiplayer visibility must be retained until the SDK acknowledgement");
    return launch_reply;
}
#include "lobby_handoff_bodies.inc"
bool render_edge(std::string& error){
#include "lobby_handoff_render_edge.inc"
    return true;
}
void reset(){
    phase=Phase::Action;ui_owner=&owner_a;ui_mode=2;generation=7;
    pending=false;load_attempted=false;framework_creating_world=false;framework_new_world_requested=false;
    framework_new_world_owner=nullptr;framework_new_world_generation=0;
    framework_new_world_receipt=nullptr;framework_new_world_observation=nullptr;
    framework_last_action={{"handler_failed",false},{"callback_pending",true}};
    command_failures=0;list_status.clear();
    choosing=true;host=true;mp_visible=true;picker_visible=true;cause.clear();ui_status.clear();
    sdk_calls=launch_calls=observation_calls=picker_calls=hide_failure_at=0;
    sdk_throws=false;picker_ok=true;visibility_calls.clear();sdk_inputs.clear();
    native::actual_world={};native::world_read_ok=true;native::world_reads=0;
    launch_reply={{"confirmed",true},{"status","opened"},{"mode",2},{"window","NewGame"},{"visible",true},{"attempted",true}};
    observe_reply={{"confirmed",true},{"status","observed"},{"attempted",false},{"mode",2},{"window","NewGame"},{"visible",true}};
}
void queue(){
    need(open_native_new_world(),"actual action queues a valid Host request");
    need(framework_new_world_requested&&framework_new_world_owner==ui_owner&&framework_new_world_generation==generation,
         "queued request retains the current owner and admission");
    need(sdk_calls==0&&visibility_calls.empty()&&mp_visible&&picker_visible,"action phase neither calls SDK nor hides Multiplayer");
    phase=Phase::Render;
}
void cases(){
    for(const char* status:{"opened","already_open"}){
        reset();launch_reply["status"]=status;queue();std::string error;
        need(render_native_new_world(error),"confirmed original wizard handoff succeeds");
        need(launch_calls==1&&!framework_new_world_requested&&framework_creating_world&&!choosing,
             "confirmed launch consumes the request once");
        need(!mp_visible&&!picker_visible&&visibility_calls==std::vector<std::pair<bool,bool>>{{false,false},{true,false}},
             "only acknowledged native wizard opening hides both Multiplayer panels");
        need(framework_new_world_receipt==launch_reply,"full SDK launch receipt retained");
        need(render_native_new_world(error)&&launch_calls==1,"later render cannot replay consumed launch");
    }
    for(int rejected=0;rejected<7;++rejected){
        reset();
        if(rejected==0)launch_reply["confirmed"]=false;
        if(rejected==1)launch_reply["status"]="rejected";
        if(rejected==2)launch_reply["status"]="unknown";
        if(rejected==3)launch_reply["mode"]=3;
        if(rejected==4)launch_reply["window"]="Other";
        if(rejected==5)launch_reply["visible"]=false;
        if(rejected==6)launch_reply=json::object();
        queue();std::string error;
        need(!render_edge(error)&&!error.empty(),"unconfirmed or malformed native acknowledgement fails visibly");
        need(!framework_new_world_requested&&!framework_creating_world&&mp_visible&&picker_visible&&visibility_calls.empty(),
             "rejected/Unknown handoff keeps Multiplayer visible");
        need(sdk_calls==1&&observation_calls==0&&render_edge(error)&&sdk_calls==1,"failed launch is never automatically retried or observed as opened");
    }
    {
        reset();launch_reply={{"status","rejected"},{"confirmed",false},{"attempted",false},{"cause","Original Title/window ownership differs"}};
        queue();std::string error="previous error";
        need(render_edge(error)&&error.empty(),"unattempted rejection records a failed action without failing the render callback");
        need(command_failures==1&&framework_new_world_receipt==launch_reply&&cause.find(launch_reply.at("cause").get<std::string>())!=std::string::npos,
             "original rejection receipt, cause and one command failure remain visible");
        need(framework_last_action.at("handler_failed")==true&&framework_last_action.at("callback_pending")==false&&framework_last_action.at("error")==cause,
             "failed asynchronous action is acknowledged with the complete cause");
        need(!framework_new_world_requested&&!framework_creating_world&&choosing&&mp_visible&&picker_visible&&visibility_calls.empty()&&
             list_status=="Neue Welt konnte nicht geöffnet werden.","unattempted rejection leaves the picker usable with a clear notice");
        need(render_edge(error)&&sdk_calls==1&&command_failures==1,"later render remains healthy without retrying the failed native launch");
    }
    for(int invalid=0;invalid<9;++invalid){
        reset();launch_reply={{"status","rejected"},{"confirmed",false},{"attempted",false},{"cause","Original native rejection"}};
        if(invalid==0)launch_reply["attempted"]=true;
        if(invalid==1)launch_reply["status"]="unknown";
        if(invalid==2)launch_reply.erase("confirmed");
        if(invalid==3)launch_reply["confirmed"]=0;
        if(invalid==4)launch_reply.erase("attempted");
        if(invalid==5)launch_reply["attempted"]=0;
        if(invalid==6)launch_reply.erase("cause");
        if(invalid==7)launch_reply["cause"]="";
        if(invalid==8)launch_reply["cause"]=7;
        queue();std::string error;bool handled=false;
        try{handled=render_edge(error);}catch(const json::exception& exception){error=exception.what();}
        need(!handled&&!error.empty()&&framework_new_world_receipt==launch_reply,
             "unknown, attempted or malformed results remain failures with the original receipt");
        need(framework_last_action.at("handler_failed")==false&&list_status.empty()&&!framework_new_world_requested&&launch_calls==1,
             "unsafe result never enters the handled-rejection branch or retries its native call");
    }
    for(int changed=0;changed<7;++changed){
        reset();queue();
        if(changed==0)ui_owner=&owner_b;
        if(changed==1)++generation;
        if(changed==2)ui_mode=3;
        if(changed==3)pending=true;
        if(changed==4)load_attempted=true;
        if(changed==5)framework_creating_world=true;
        if(changed==6)host=false;
        std::string error;
        need(!render_native_new_world(error)&&error.find("owner or admission changed")!=std::string::npos,
             "stale owner/generation or changed admission rejects queued launch");
        need(sdk_calls==0&&!framework_new_world_requested&&visibility_calls.empty(),"stale request is consumed without SDK effects");
        need(render_native_new_world(error)&&sdk_calls==0,"stale request does not retry next render");
    }
    for(int blocked=0;blocked<6;++blocked){
        reset();
        if(blocked==0)ui_mode=3;
        if(blocked==1)pending=true;
        if(blocked==2)load_attempted=true;
        if(blocked==3)framework_creating_world=true;
        if(blocked==4)framework_new_world_requested=true;
        if(blocked==5)host=false;
        need(!open_native_new_world()&&sdk_calls==0&&visibility_calls.empty(),"action admission rejects without any UI call");
    }
    {
        reset();queue();std::string error;
        need(render_edge(error)&&launch_calls==1&&observation_calls==1,"actual render edge performs then passively observes");
        need(sdk_inputs[0].empty()&&sdk_inputs[1]==json{{"observe",true}},"passive observation explicitly disables launch");
        observe_reply["visible"]=false;
        need(render_edge(error)&&launch_calls==1&&observation_calls==2&&picker_calls==1&&!framework_creating_world&&choosing&&picker_visible,
             "native Cancel returns to picker through observation without another launch");
        need(framework_new_world_observation==observe_reply,"complete cancellation acknowledgement retained");
        need(render_edge(error)&&sdk_calls==3&&picker_calls==1,"completed cancellation is not repeated");
    }
    for(int mode:{-1,3}){
        reset();phase=Phase::Render;framework_creating_world=true;ui_mode=mode;std::string error;
        need(render_edge(error)&&sdk_calls==0&&picker_calls==0,"transition/InGame observation never calls the wizard UI API");
        need(framework_creating_world==(mode==-1),"transition retains observation and InGame retires it");
        need(native::world_reads==0,"cached transition/InGame requires no Title-specific native state read");
    }
    for(const auto actual:{native::WorldStatus{2,3,true},native::WorldStatus{2,3,false},native::WorldStatus{2,2,true},
                           native::WorldStatus{3,3,false},native::WorldStatus{-1,-1,false}}){
        reset();phase=Phase::Render;framework_creating_world=true;ui_mode=2;choosing=false;
        mp_visible=false;picker_visible=false;native::actual_world=actual;
        framework_new_world_observation={{"retained","last idle Title observation"}};
        const auto previous_observation=framework_new_world_observation;
        observe_reply["visible"]=false;std::string error;
        const bool healthy=render_edge(error);
        need(healthy,("actual native transition with stale cached Title must remain healthy: "+error).c_str());
        need(native::world_reads==1&&framework_creating_world&&!framework_new_world_requested&&
             sdk_calls==0&&launch_calls==0&&picker_calls==0&&visibility_calls.empty()&&!mp_visible&&!picker_visible,
             "fresh native transition skips SDK observation, picker return and relaunch while retaining creation");
        need(error.empty()&&cause.empty()&&command_failures==0&&framework_new_world_observation==previous_observation,
             "normal native transition neither latches an MP failure nor fabricates a new observation");
        need(render_edge(error)&&native::world_reads==2&&sdk_calls==0&&picker_calls==0,
             "another stale-Title frame rereads actual state without replaying UI effects");
        ui_mode=3;
        need(render_edge(error)&&!framework_creating_world&&native::world_reads==2&&sdk_calls==0&&picker_calls==0,
             "eventual cached InGame retires creation without native wizard or picker calls");
    }
    {
        reset();phase=Phase::Render;framework_creating_world=true;native::world_read_ok=false;std::string error;
        need(!render_edge(error)&&error=="New World transition observation failed: original native WorldStatus read failure",
             "actual native state read failure preserves its original cause");
        need(cause==error&&command_failures==1&&framework_creating_world&&native::world_reads==1&&sdk_calls==0&&picker_calls==0,
             "failed native read remains a visible failure with no SDK effects or false creation completion");
    }
    for(int invalid=0;invalid<5;++invalid){
        reset();phase=Phase::Render;framework_creating_world=true;
        if(invalid==0)observe_reply["confirmed"]=false;
        if(invalid==1)observe_reply["status"]="unknown";
        if(invalid==2)observe_reply["attempted"]=true;
        if(invalid==3)observe_reply["mode"]=3;
        if(invalid==4)observe_reply["window"]="Other";
        std::string error;
        need(!observe_native_new_world(true,false,error)&&framework_creating_world&&picker_calls==0&&launch_calls==0,
             "bad passive observation cannot claim cancellation or relaunch");
    }
    {
        reset();queue();hide_failure_at=1;std::string error;
        need(!render_native_new_world(error)&&cause.find("could not be hidden")!=std::string::npos,
             "confirmed launch with failed panel hide retains original failure");
        need(framework_creating_world&&!framework_new_world_requested&&launch_calls==1&&render_native_new_world(error)&&launch_calls==1,
             "hide failure cannot replay the native launch");
    }
    {
        reset();queue();sdk_throws=true;std::string error;bool caught=false;
        try{render_native_new_world(error);}catch(const std::runtime_error& e){caught=std::string(e.what())=="original SDK request exception";}
        need(caught&&!framework_new_world_requested&&mp_visible&&picker_visible,"SDK exception consumes launch and preserves visible Multiplayer");
        need(render_native_new_world(error)&&sdk_calls==1,"SDK exception is never retried automatically");
    }
    {
        reset();phase=Phase::Render;framework_creating_world=true;observe_reply["visible"]=false;picker_ok=false;std::string error;
        need(!observe_native_new_world(true,false,error)&&error=="original picker failure"&&launch_calls==0,
             "cancellation preserves original picker failure without launch");
    }
}
}
int main(){try{fixture::cases();std::cout<<fixture::checks<<" actual lobby handoff checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<"Lobby handoff failure after "<<fixture::checks<<": "<<e.what()<<'\n';return 1;}}
