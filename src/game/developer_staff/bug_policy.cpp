#include "bug_policy.h"
#include <regex>

namespace mt2mp::game::developer_staff::bug_policy {
namespace {
bool fail(std::string& e,const std::string& why){e=why;return false;}
bool uuid(const std::string& s){static const std::regex pattern("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}");return std::regex_match(s,pattern);}
bool positive(const nlohmann::json& j,uint64_t& out){
    if(!j.is_number_integer()||(!j.is_number_unsigned()&&j.get<int64_t>()<=0))return false;
    out=j.get<uint64_t>();return out!=0;
}
}
size_t index(Scope scope){return scope==Scope::Online?2:5;}
const char* property(Scope scope){return scope==Scope::Online?"fixCBugsOnline":"fixCBugsOffline";}
bool ready_controller(const nlohmann::json& status,bool live_active,std::string& e){
    if(!status.is_object())return fail(e,"Bug-C policy actual Controller status is not an object");
    const auto original=status.value("error",status.value("cause",std::string{}));
    if(!status.value("initialized",false)||!status.value("started",false)||status.value("locked",true)||
       status.value("pending",true)||status.value("lobby_retiring",true)||!original.empty())
        return fail(e,"Bug-C policy UI requires idle unlocked initialized Controller; original="+original);
    if(!live_active)return fail(e,"Bug-C policy UI requires active live authority");
    e.clear();return true;
}
bool parse(const nlohmann::json& args,Request& r,std::string& e){
    r={};e.clear();
    if(!args.is_object()||args.size()!=7)return fail(e,"Bug-C policy requires exact seven fields");
    for(const auto* name:{"command","scope","token","native_origin_uuid","epoch","generation","expected"})
        if(!args.contains(name))return fail(e,"Bug-C policy argument field missing");
    if(!args.at("command").is_string()||!args.at("scope").is_string()||!args.at("token").is_string()||!args.at("native_origin_uuid").is_string()||
       !args.at("expected").is_boolean())return fail(e,"Bug-C policy exact scalar types required");
    const auto command=args.at("command").get<std::string>(),scope=args.at("scope").get<std::string>();
    if(command!="Show"&&command!="Toggle")return fail(e,"Bug-C policy command must be Show or Toggle");
    if(scope!="online"&&scope!="offline")return fail(e,"Bug-C policy scope must be online or offline");
    r.command=command=="Show"?Command::Show:Command::Toggle;r.scope=scope=="online"?Scope::Online:Scope::Offline;
    r.token=args.at("token").get<std::string>();r.origin_uuid=args.at("native_origin_uuid").get<std::string>();r.expected=args.at("expected").get<bool>();
    if(!uuid(r.token)||!uuid(r.origin_uuid)||!positive(args.at("epoch"),r.epoch)||!positive(args.at("generation"),r.generation))return fail(e,"Bug-C policy canonical token/Origin/positive epoch/generation required");
    return true;
}
bool same_owner(const Snapshot& a,const Snapshot& b){
    return same_origin(a.capture.pin.origin,b.capture.pin.origin)&&a.capture.image==b.capture.image&&a.capture.thread==b.capture.thread&&
        a.capture.local==b.capture.local&&a.capture.pin.ledger==b.capture.pin.ledger&&
        a.capture.pin.time_scale==b.capture.pin.time_scale&&a.capture.pin.pause_scale==b.capture.pin.pause_scale&&
        a.server_current==b.server_current&&a.server_target==b.server_target;
}
bool Fixture::run(const Request& request,Port& port,Receipt& out,std::string& e){
    out={};out.request=request;e.clear();
    if(!uuid(request.token)||!uuid(request.origin_uuid)||!request.epoch||!request.generation||
       (request.scope!=Scope::Online&&request.scope!=Scope::Offline)||(request.command!=Command::Show&&request.command!=Command::Toggle))
        return fail(e,"Bug-C policy fixed request invalid");
    if(const auto found=receipts_.find(request.token);found!=receipts_.end()){out=found->second;out.replay_rejected=true;return fail(e,"Bug-C policy token consumed; inspect retained receipt, no replay");}
    if(locked_||busy_)return fail(e,locked_?cause_:"Bug-C policy UI operation already executing");
    if(receipts_.size()>=64)return fail(e,"Bug-C policy retained receipt limit reached64");
    Snapshot first,again;Control control,stable;
    if(!port.read(first,e))return false;
    const auto& origin=first.capture.pin.origin;
    if(origin.uuid!=request.origin_uuid||origin.world_epoch!=request.epoch||origin.generation!=request.generation||first.policies[index(request.scope)]!=uint8_t(request.expected))
        return fail(e,"Bug-C policy actual Origin/expected value differs");
    const bool show=request.command==Command::Show;
    if(!(show?port.window(first,control,e):port.control(first,request.scope,control,false,e)))return false;
    if(!port.read(again,e)||!same_owner(first,again)||first.capture.state!=again.capture.state||first.policies!=again.policies){
        if(e.empty())e="Bug-C policy actual owner/pool/policy changed before UI";return false;}
    if(!(show?port.window(again,stable,e):port.control(again,request.scope,stable,false,e)))return false;
    if(control!=stable)return fail(e,"Bug-C policy native UI binding changed before input");
    out.before=first;out.unknown=true;out.cause="Bug-C policy original UI attempt outstanding; no replay";
    receipts_.emplace(request.token,out);busy_=true;locked_=true;cause_=out.cause;
    struct Busy{bool& value;~Busy(){value=false;}} busy{busy_};
    const auto stop=[&](){if(e.empty())e="Bug-C policy UI failed without original cause";out.cause=e;cause_=e;receipts_.at(request.token)=out;return false;};
    try{
        if(show){
            out.attempted_mask|=1;if(!port.show(control,e))return stop();out.returned_mask|=1;
            Snapshot shown;Control bound;
            if(!port.read(shown,e)||!same_owner(first,shown)||shown.policies!=first.policies){if(e.empty())e="Bug-C policy Show changed native world/policy";return stop();}
            if(!port.window(shown,bound,e,true)||bound.window!=control.window||bound.tabs!=control.tabs||bound.view!=control.view){if(e.empty())e="Bug-C policy Show changed page owner";return stop();}
            out.attempted_mask|=2;if(!port.page(bound,e))return stop();out.returned_mask|=2;
        }else{
            out.attempted_mask|=1;if(!port.input(control,Input::Hover,e))return stop();out.returned_mask|=1;
            out.attempted_mask|=2;if(!port.input(control,Input::Press,e))return stop();out.returned_mask|=2;
            Snapshot pressed;Control bound;
            if(!port.read(pressed,e)||!same_owner(first,pressed)||pressed.policies!=first.policies){if(e.empty())e="Bug-C policy Press changed world/policy before Release";return stop();}
            if(!port.control(pressed,request.scope,bound,true,e)||bound!=control){if(e.empty())e="Bug-C policy pressed binding changed before Release";return stop();}
            out.attempted_mask|=4;if(!port.input(control,Input::Release,e))return stop();out.returned_mask|=4;
        }
        if(!port.read(out.after,e)||!same_owner(first,out.after)){if(e.empty())e="Bug-C policy full native Origin/owners/ledger/Clock changed after UI";return stop();}
        auto expected=first.policies;if(!show)expected[index(request.scope)]=uint8_t(!request.expected);
        if(out.after.policies!=expected){e="Bug-C policy original UI exact policy readback differs";return stop();}
        Control after;
        if(show){if(!port.window(out.after,after,e,true)||after.window!=control.window||after.tabs!=control.tabs||after.view!=control.view){if(e.empty())e="Bug-C policy requested page owner changed";return stop();}
            if(!port.requested(after,e))return stop();}
        else if(!port.control(out.after,request.scope,after,false,e)||after!=control){if(e.empty())e="Bug-C policy final checkbox binding/readback differs";return stop();}
        out.confirmed=true;out.unknown=false;out.cause.clear();locked_=false;cause_.clear();receipts_.at(request.token)=out;e.clear();return true;
    }catch(const std::exception& ex){e=std::string("Bug-C policy UI exception original=")+ex.what();return stop();}
    catch(...){e="Bug-C policy UI nonstandard exception; no replay";return stop();}
}
nlohmann::json encode(const Snapshot& s){return {{"native",developer_staff::encode(s.capture)},
    {"policies",{{"fixABugsOnline",bool(s.policies[0])},{"fixBBugsOnline",bool(s.policies[1])},{"fixCBugsOnline",bool(s.policies[2])},
                 {"fixABugsOffline",bool(s.policies[3])},{"fixBBugsOffline",bool(s.policies[4])},{"fixCBugsOffline",bool(s.policies[5])}}},
    {"server_current",s.server_current},{"server_target",s.server_target}};}
nlohmann::json encode(const Receipt& r){return {{"token",r.request.token},{"command",r.request.command==Command::Show?"Show":"Toggle"},
    {"scope",r.request.scope==Scope::Online?"online":"offline"},{"property",property(r.request.scope)},{"expected",r.request.expected},
    {"attempted_mask",r.attempted_mask},{"returned_mask",r.returned_mask},{"confirmed",r.confirmed},{"unknown",r.unknown},
    {"replay_rejected",r.replay_rejected},{"cause",r.cause},{"before",encode(r.before)},{"after",encode(r.after)},{"peer_ack",false}};}
}
