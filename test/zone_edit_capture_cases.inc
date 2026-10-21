namespace zone_edit_finish_fixture {
namespace a=mt2mp::action_session;
using json=nlohmann::json;
struct Memory {};
struct Sample {int value=0;};
struct Capture {
    uintptr_t brush=100,graph=200;a::Binding binding;std::string snapshot,id,cause,ui_token="probe";
    a::ZoneEditIntent intent;
    bool cleanup_entered=false,cleanup_destroyed=false,cleanup_returned=false;
};
struct Pending {a::Binding binding;std::string snapshot,id,ui_token;a::ZoneEditIntent intent;};
std::optional<Capture> captured;
std::optional<Pending> queued;
bool capture_fault=false,replaying=false,connected=true;
unsigned current_thread=7,original_calls=0,capture_count=0;
int world_value=20;
std::string fault,cause;
namespace net::session_wire {inline bool connected(){return zone_edit_finish_fixture::connected;}}
namespace za {
struct Runtime {uintptr_t image=0;unsigned thread=7;void(*finished)(void*,const void*)=nullptr;std::map<std::string,json> ui_jobs;} state;
inline bool native_ok(uint32_t result,const char*,std::string& error){
    if(result){error="original cleanup exception="+std::to_string(result);return false;}return true;
}
}
inline unsigned GetCurrentThreadId(){return current_thread;}
inline void reject(const std::string& error){cause=error;}
inline bool initial_graph(Memory&,const Capture&,std::string& error){
    if(fault=="graph"){error="unowned initial graph";return false;}return true;
}
inline bool sample(Memory&,const a::Binding&,const std::string&,int32_t,const std::string&,Sample& out,std::string& error){
    if(fault=="sample"){error="fresh state unavailable";return false;}out.value=world_value;return true;
}
inline bool same(const Sample& before,const Sample& after){return before.value==after.value;}
namespace zone_edit_calls {
inline uint32_t discard_capture(uintptr_t,uintptr_t,uintptr_t,void(*)(void*,const void*),const void*,bool& destroyed,bool& returned){
    ++original_calls;if(fault=="exception")return 5;destroyed=true;returned=true;
    if(fault=="effect")++world_value;return 0;
}
}
#include "zone_edit_finish_body.inc"
inline void reset(){
    captured=Capture{};queued.reset();capture_fault=replaying=false;connected=true;current_thread=7;
    original_calls=capture_count=0;world_value=20;fault.clear();cause.clear();za::state={};
    auto& intent=captured->intent;intent.region=67;intent.zone_type=1;intent.character_type=0;intent.level=1;
    intent.stamps={{{1,0,2},{{{0,0},{1,0},{1,1}}}}};
    za::state.ui_jobs["probe"]={{"captured",false}};
}
inline unsigned run(){
    unsigned checks=0;const auto check=[&](bool okay,const char* reason){++checks;if(!okay)throw std::runtime_error(reason);};
    bool handled=false;reset();
    check(capture_finished(reinterpret_cast<void*>(100),nullptr,handled)&&handled,"Zone58 Finish did not admit actual capture");
    check(original_calls==1&&!captured&&queued&&capture_count==1&&za::state.ui_jobs["probe"]["captured"]==true,"Zone58 cleanup/queue receipt differs");
    check(!capture_finished(reinterpret_cast<void*>(100),nullptr,handled)&&original_calls==1,"Zone58 repeated Finish replayed cleanup");
    for(const auto* failure:{"graph","sample","thread","prior_fault"}){
        reset();fault=failure;if(fault=="thread")current_thread=8;if(fault=="prior_fault")capture_fault=true;
        check(!capture_finished(reinterpret_cast<void*>(100),nullptr,handled),"Zone58 invalid fresh capture admitted");
        check(!original_calls&&captured&&!queued&&!cause.empty(),"Zone58 invalid capture reached cleanup or lost cause");
    }
    for(const auto* failure:{"exception","effect"}){
        reset();fault=failure;
        check(!capture_finished(reinterpret_cast<void*>(100),nullptr,handled),"Zone58 unknown cleanup reported success");
        check(original_calls==1&&capture_fault&&captured&&!queued&&!cause.empty(),"Zone58 partial original lifetime not retained");
        check(!capture_finished(reinterpret_cast<void*>(100),nullptr,handled)&&original_calls==1,"Zone58 unknown cleanup replayed");
    }
    reset();captured->cause="invalid captured geometry";
    check(!capture_finished(reinterpret_cast<void*>(100),nullptr,handled)&&original_calls==1&&!captured&&!queued&&cause=="invalid captured geometry",
        "Zone58 rejected stroke skipped cleanup or reached Controller");
    reset();world_value=42;
    check(capture_finished(reinterpret_cast<void*>(100),nullptr,handled)&&world_value==42&&queued,"Zone58 rejected legitimate between-callback simulation");
    reset();connected=false;
    check(capture_finished(reinterpret_cast<void*>(100),nullptr,handled)&&!handled&&!original_calls,"Zone58 captured unbound vanilla Finish");
    reset();replaying=true;
    check(capture_finished(reinterpret_cast<void*>(100),nullptr,handled)&&!handled&&!original_calls,"Zone58 intercepted its private original Finish");
    return checks;
}
}
