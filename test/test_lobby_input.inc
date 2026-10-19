#include <deque>
#include <map>

namespace input_fixture {
using json=nlohmann::json;
using fixture::need;
struct FrameworkWidget {std::string type="button",text,group="session";bool visible=true,disabled=false;};
struct FrameworkAction {
    std::string widget,command,lobby_id,chat,report_comment;
    void* mode=nullptr;
    uint64_t admission=0,discovery=0,sequence=0;
    size_t page=0;
    bool choosing=false;
};
std::map<std::string,FrameworkWidget> framework_widgets;
const std::map<std::string,std::string> framework_commands={{"pick","Pick"},{"list0","Select"},{"leave","Leave"}};
std::deque<FrameworkAction> framework_actions;
json framework_response,framework_last_action;
bool framework_ready=false,framework_observed_open=false,framework_open=false,framework_visibility_known=false;
bool framework_visible=false,framework_suspended=false,framework_report=false,framework_browser=false,choosing=false;
bool pending=false,list_pending=false,framework_new_world_requested=false,member=true;
uint64_t framework_queued=0,framework_processed=0,framework_rejected=0,generation=1,list_generation=1;
size_t list_page=0;
int owner=0,other_owner=0,command_failures=0,dispatches=0;
void* ui_owner=&owner;
std::string framework_report_comment,chat_error,cause;
bool throw_dispatch=false,fail_dispatch=false;
namespace net::chat {constexpr size_t max_text_bytes=1000;}
namespace reporting {
void open_report(){}
void send_report(const std::string&){}
json status(){return {{"automatic",false}};}
void automatic_reports(bool){}
}
bool admitted(){return member;}
bool fail(const std::string& detail){++command_failures;cause=detail;return false;}
FrameworkWidget* framework_widget(const char* id){auto it=framework_widgets.find(id);return it==framework_widgets.end()?nullptr:&it->second;}
void dispatch_action(void*,const std::string&){++dispatches;if(throw_dispatch)throw std::runtime_error("original dispatch exception");if(fail_dispatch)fail("original dispatch failure");}
#include "lobby_input_bodies.inc"
json response(){return {{"ready",true},{"open",true},{"visibility_known",true},{"visible",true},{"suspended",false},{"values",json::object()},{"clicked",json::array()}};}
void reset(){
    framework_widgets.clear();
    for(const char* id:{"pick","list0","leave","lobby_id","chat_input"})framework_widgets[id]={};
    framework_widgets.at("list0").group="picker";
    framework_widgets.at("lobby_id").type="input";framework_widgets.at("chat_input").type="input";
    framework_actions.clear();framework_response=nullptr;framework_last_action=nullptr;
    framework_ready=framework_observed_open=framework_open=framework_visibility_known=framework_visible=false;
    framework_suspended=framework_report=framework_browser=choosing=pending=list_pending=framework_new_world_requested=false;
    member=true;framework_queued=framework_processed=framework_rejected=0;generation=list_generation=1;list_page=0;
    ui_owner=&owner;command_failures=dispatches=0;throw_dispatch=fail_dispatch=false;
    cause.clear();chat_error.clear();framework_report_comment.clear();
}
void cases(){
    for(int guard=0;guard<9;++guard){
        reset();auto reply=response();reply["clicked"]={"pick"};
        if(guard==0){framework_browser=true;choosing=true;}
        if(guard==1)framework_widgets.at("pick").disabled=true;
        if(guard==2)reply["open"]=false;
        if(guard==3)reply["visibility_known"]=false;
        if(guard==4)reply["visible"]=false;
        if(guard==5)reply["suspended"]=true;
        if(guard==6)ui_owner=nullptr;
        if(guard==7)framework_widgets.at("pick").visible=false;
        if(guard==8)framework_actions.resize(16);
        const auto old_size=framework_actions.size();std::string error;
        need(framework_apply_response(reply,error)&&error.empty(),"pre-effect click rejection keeps render service healthy");
        need(framework_rejected==1&&command_failures==1&&!cause.empty()&&framework_queued==0&&framework_processed==0&&
             framework_last_action.at("attempted")==false&&framework_last_action.at("handler_failed")==true&&
             framework_last_action.at("sequence")==0&&framework_last_action.at("widget")=="pick"&&
             framework_last_action.at("error")==cause,"unqueued rejected click has a visible failed receipt without an admitted sequence");
        need(framework_actions.size()==old_size&&dispatches==0,"rejected click cannot queue or dispatch effects");
        need(framework_apply_response(response(),error)&&framework_rejected==1&&command_failures==1&&dispatches==0,
             "next render stays healthy without replaying rejected click");
    }
    for(int stale=0;stale<8;++stale){
        reset();auto reply=response();reply["clicked"]={"pick"};std::string error;
        need(framework_apply_response(reply,error)&&framework_queued==1,"valid click queues once before context changes");
        if(stale==0)ui_owner=&other_owner;
        if(stale==1)++generation;
        if(stale==2)++list_generation;
        if(stale==3)++list_page;
        if(stale==4)choosing=true;
        if(stale==5)framework_widgets.at("pick").disabled=true;
        if(stale==6)framework_widgets.at("pick").visible=false;
        if(stale==7)framework_browser=true;
        need(framework_process_action(error)&&error.empty(),"stale unattempted action does not invalidate Tick");
        need(framework_actions.empty()&&framework_processed==1&&framework_rejected==1&&command_failures==1&&dispatches==0&&
             framework_last_action.at("sequence")==1&&framework_last_action.at("attempted")==false&&
             framework_last_action.at("handler_failed")==true&&framework_last_action.at("error")==cause,
             "stale queued action is consumed exactly once with its original failed identity");
        need(framework_process_action(error)&&framework_processed==1&&dispatches==0,"next Tick never retries stale action");
    }
    for(int malformed=0;malformed<3;++malformed){
        reset();auto reply=response();std::string error;
        if(malformed==0)reply["clicked"]={7};
        if(malformed==1)reply["values"]={{"unknown","text"}};
        if(malformed==2)reply["values"]={{"lobby_id",std::string(21,'x')}};
        need(!framework_apply_response(reply,error)&&!error.empty()&&command_failures==1&&framework_actions.empty(),
             "protocol/input validation failures remain fatal and preserve cause");
    }
    for(int outcome=0;outcome<3;++outcome){
        reset();auto reply=response();reply["clicked"]={"pick"};std::string error;
        need(framework_apply_response(reply,error),"current valid click remains admissible");
        throw_dispatch=outcome==1;fail_dispatch=outcome==2;
        const bool healthy=framework_process_action(error);
        need(healthy==(outcome==0)&&dispatches==1&&framework_last_action.at("attempted")==true&&
             framework_last_action.at("handler_failed")==bool(outcome),"attempted native failures remain fatal; success dispatches once");
        need(outcome==0||(!error.empty()&&!cause.empty()),"attempted failure preserves original cause");
        need(framework_process_action(error)&&dispatches==1,"attempted effect is never replayed after success or failure");
    }
}
}
