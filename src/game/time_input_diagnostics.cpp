#include "time_input_diagnostics.h"
#include "time_integration.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/net/local_session.h"
#include <SDL3/SDL.h>
#include <Windows.h>
#include <map>
#include <stdexcept>

namespace mt2mp::game::time_input_diagnostics {
namespace {
namespace input=native_time_input;
namespace wire=time_session;
constexpr const char* exe_sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
input::Adapter adapter;
bool initialized=false;
uint32_t bound_thread=0;
bool(*read_context)(void*,input::Context&,std::string&)=nullptr;
std::string runtime_id,dll_sha,active_token,last_error;
uint64_t repeated_errors=0;
struct Observation {input::Request request;bool host_available=false;wire::Message host{};};
std::map<std::string,Observation> observations;
bool terminal(input::Phase p) {
    return p==input::Phase::Confirmed||p==input::Phase::Rejected||p==input::Phase::Partial||p==input::Phase::Unknown;
}
void report(const char* operation,const std::string& cause) {
    if(cause==last_error) {++repeated_errors;return;}
    last_error=cause;repeated_errors=1;
    LOG_ERROR("[TimeInput] operation={} token={} cause={}; no replay",operation,active_token,cause);
}
input::Key key(wire::Intent intent) {
    switch(intent) {
        case wire::Intent::Pause:return input::Key::F1;
        case wire::Intent::Normal:return input::Key::F2;
        case wire::Intent::Fast:return input::Key::F3;
        case wire::Intent::Fastest:return input::Key::F4;
        case wire::Intent::TogglePauseNormal:return input::Key::Space;
    }
    throw std::runtime_error("time_input_host_intent_invalid");
}
const char* intent(input::Key k) {
    switch(k) {
        case input::Key::Space:return "TogglePauseNormal";
        case input::Key::F1:return "Pause";
        case input::Key::F2:return "Normal";
        case input::Key::F3:return "Fast";
        case input::Key::F4:return "Fastest";
    }
    throw std::runtime_error("time_input_key_invalid");
}
const char* phase(input::Phase p) {
    switch(p) {
        case input::Phase::Queued:case input::Phase::Observed:return "pending";
        case input::Phase::Confirmed:return "confirmed";
        case input::Phase::Rejected:return "rejected";
        case input::Phase::Partial:return "partial";
        case input::Phase::Unknown:return "unknown";
    }
    throw std::runtime_error("time_input_phase_invalid");
}
bool token_valid(const std::string& token) {
    if(token.size()!=36)return false;
    for(size_t i=0;i<token.size();++i) {
        if(i==8||i==13||i==18||i==23) {if(token[i]!='-')return false;}
        else if(!((token[i]>='0'&&token[i]<='9')||(token[i]>='a'&&token[i]<='f')))return false;
    }
    return true;
}
std::string token(const json& args) {
    const auto value=args.at("acceptance_token").get<std::string>();
    if(!token_valid(value))throw std::runtime_error("acceptance_token requires a lowercase UUID");
    return value;
}
uint64_t positive(const json& value,const char* name) {
    if(!value.is_number_integer()||(value.is_number_unsigned()?value.get<uint64_t>()==0:value.get<int64_t>()<=0))
        throw std::runtime_error(std::string(name)+" requires a positive uint64 integer");
    return value.get<uint64_t>();
}
json format(const input::Receipt& r) {
    json out={{"acceptance_token",r.request.token},{"state",phase(r.phase)},
        {"down",{{"queued",r.down.queued},{"consumed",r.native_down_consumed}}},
        {"up",{{"queued",r.up.queued},{"consumed",r.native_up_consumed}}},
        {"held_after",!r.native_up_consumed},
        {"frames",{{"down_queued",r.down.queued_frame},{"down_consumed",r.down.poll_frame},
            {"up_queued",r.up.queued_frame},{"up_consumed",r.up.poll_frame}}},
        {"hook",{{"caller_rva",r.caller_rva},{"origin","Keyboard"},{"intent",intent(r.request.key)},
            {"matched",r.native_route_observed},{"captured_count",r.captured_count}}},
        {"request_sequence",r.sequence},{"error",r.error},
        {"observer_error",last_error},{"observer_error_repeats",repeated_errors}};
    const auto found=observations.find(r.request.token);
    if(found!=observations.end()&&found->second.host_available) {
        const auto& h=found->second.host;
        const char* result=h.result==wire::Result::Confirmed?"Confirmed":h.result==wire::Result::NoChange?"NoChange":"Rejected";
        out["host_receipt"]={{"epoch",h.epoch},{"peer",h.peer},{"sequence",h.sequence},
            {"observed_revision",h.observed_revision},{"revision",h.state.revision},{"result",result},
            {"code",h.code==wire::Code::None?json("None"):json(static_cast<int>(h.code))},
            {"state_present",h.state.present},{"uncertain",h.state.uncertain},
            {"selected_scale",h.state.clock.selected},{"pause_scale",h.state.clock.pause}};
    }
    return out;
}
}
bool initialize(uintptr_t image,uint32_t thread,const std::string& run,const std::string& sha,
                bool(*context)(void*,input::Context&,std::string&),std::string& error) {
    if(run.empty()||sha.size()!=64) {error="time_input_runtime_identity_missing";return false;}
    runtime_id=run;dll_sha=sha;bound_thread=thread;read_context=context;
    initialized=adapter.initialize({image,thread,exe_sha},input::process_services(nullptr,context),error);
    return initialized;
}
bool ready(){return initialized;}
bool unresolved(){return !active_token.empty();}
void poll(void* raw,uint64_t next_frame) {
    if(!initialized||active_token.empty())return;
    const auto* event=static_cast<SDL_Event*>(raw);
    if(!event||(event->type!=SDL_EVENT_KEY_DOWN&&event->type!=SDL_EVENT_KEY_UP))return;
    const input::Event edge{event->key.windowID,uint32_t(event->key.scancode),event->key.key,
        event->type==SDL_EVENT_KEY_DOWN,uintptr_t(SDL_GetWindowFromID(event->key.windowID)),event->key.timestamp};
    std::string error;if(!adapter.observe_poll(edge,next_frame,error))report("observe_poll",error);
}
void leaf(uintptr_t caller,const shared_time::HookResult& result,uint64_t next_frame) {
    if(!initialized||active_token.empty())return;
    std::string error;
    if(!adapter.observe_leaf(caller,key(static_cast<wire::Intent>(result.intent)),result.matched,result.request_queued,next_frame,error))
        report("observe_leaf",error);
}
void frame(uint64_t completed_frame) {
    if(!initialized||active_token.empty())return;
    std::string error;if(!adapter.observe_frame(completed_frame,error))report("observe_frame",error);
}
void tick() {
    if(!initialized||active_token.empty())return;
    input::Receipt current;std::string error;
    if(!adapter.receipt(active_token,current,error)){report("receipt",error);return;}
    if(terminal(current.phase))return;
    auto& observation=observations.at(active_token);wire::Message host;bool available=false;
    if(!time_integration::instance().observe_receipt(Config::instance().peer_id,current.request.expected_sequence,host,available,error)) {
        report("observe_host_receipt",error);return;
    }
    if(!available)return;
    observation.host=host;observation.host_available=true;
    input::HostReceipt receipt{host.epoch,host.sequence,host.observed_revision,host.state.revision,host.peer,
        host.result==wire::Result::Confirmed,host.result==wire::Result::NoChange,host.state.present,host.state.uncertain,
        key(host.intent),host.state.clock.selected,host.state.clock.pause,
        host.code==wire::Code::None?std::string{}:"time_host_receipt_code="+std::to_string(static_cast<int>(host.code))};
    input::Receipt output;
    if(!adapter.confirm(active_token,receipt,output,error)) {
        if(error!="time_input_waiting_for_native_observation")report("confirm",error);
        return;
    }
    LOG_INFO("[TimeInput] Confirmed token={} peer={} sequence={} revision={} caller_rva={}",active_token,
        host.peer,host.sequence,host.state.revision,output.caller_rva);
    active_token.clear();
    if(!last_error.empty()) {LOG_INFO("[TimeInput] Recovered cause={} repeats={}",last_error,repeated_errors);last_error.clear();repeated_errors=0;}
}
void suspend(const std::string& reason) {
    if(!initialized)return;
    std::string error;if(!adapter.suspend(reason,error))report("suspend",error);
}
json read(uint64_t completed_frames,uint64_t ui_updates) {
    input::Readback bindings;std::string error;
    const bool scanned=adapter.read(bindings,error);
    const auto scan_error=error;
    const char* physical_keys[]={"SPACE","F1","F2","F3","F4"};
    json readiness=json::object(),collisions=json::array();bool all_ready=scanned,all_released=scanned;
    for(size_t i=0;i<5;++i) {
        const auto& binding=bindings.bindings[i];
        readiness[physical_keys[i]]={{"ready",scanned&&binding.ready},{"blocker",scanned?binding.blocker:scan_error},
            {"axis",binding.axis},{"matches",binding.matches},{"collisions",binding.collisions},
            {"customized",binding.customized},{"axis_value",binding.axis_value}};
        all_ready=all_ready&&binding.ready;all_released=all_released&&binding.axis_value==0;
    }
    for(size_t i=0;i<bindings.collision_sample_count;++i) {
        const auto& collision=bindings.collision_samples[i];
        collisions.push_back({{"key",physical_keys[static_cast<size_t>(collision.key)]},{"expected_axis",collision.expected_axis},
            {"axis",collision.axis},{"control_index",collision.control_index},{"control_type",collision.control_type},
            {"control_code",collision.control_code},{"ctrl",collision.ctrl},{"customized",collision.customized},{"axis_value",collision.axis_value}});
    }
    json diagnostic={{"scan_complete",bindings.scan_complete},{"axis_count",bindings.axis_count},{"controls_scanned",bindings.controls_scanned},
        {"panel_visible",bindings.panel_visible},{"input_clear",bindings.input_clear},{"collision_count",bindings.collision_count},
        {"collision_samples_truncated",bindings.collision_samples_truncated},{"collision_samples",collisions}};
    if(!scanned)return {{"read_error",scan_error},{"key_readiness",readiness},{"binding_diagnostics",diagnostic}};
    input::Context context;
    if(!read_context||!read_context(nullptr,context,error))throw std::runtime_error(error.empty()?"time_input_context_missing":error);
    time_integration::Status status;
    if(!time_integration::instance().status(status,error))throw std::runtime_error(error);
    shared_time::Presentation presentation;
    if(!time_integration::instance().read_presentation(presentation,error))throw std::runtime_error(error);
    native::ClockState clock;if(!native::read_clock(clock,error))throw std::runtime_error(error);
    native::DayTickState days;native::read_day_tick_state(days);
    const auto& cfg=Config::instance();const auto& runtime=status.runtime;const auto& controller=runtime.controller;
    const auto& state=cfg.is_host?controller.observed:controller.applied;
    json items=json::array(),acks=json::object(),sequences=json::object();
    for(const auto& item:presentation.items)items.push_back({{"scale",item.scale},{"native_state",item.native_state},{"active",item.active}});
    for(size_t i=0;i<4;++i) {acks[std::to_string(i+1)]=controller.presentation_acks[i];sequences[std::to_string(i+1)]=controller.processed_sequences[i];}
    return {{"schema_version",1},{"identity",{{"run_id",runtime_id},{"peer_id",cfg.peer_id},{"dll_sha256",dll_sha},{"exe_sha256",exe_sha}}},
        {"peer_mask",net::local_session_peer_mask()},
        {"key_readiness",readiness},{"binding_diagnostics",diagnostic},
        {"guards",{{"owned_world",context.owned_test&&context.world_owned&&bindings.epoch==runtime.epoch},{"main_thread",GetCurrentThreadId()==bound_thread},
            {"world_active",context.time_active&&runtime.phase==time_runtime::Phase::Active},{"session_connected",net::local_session_connected()},
            {"native_keybindings",all_ready},{"panel_input_clear",bindings.panel_visible&&bindings.input_clear},
            {"overlay_hidden",context.overlay_hidden},{"no_pending_operation",context.no_pending_operation},{"keys_released",all_released},
            {"hook_origins_guarded",runtime.hook_mask==3}}},
        {"keybindings",{{"SPACE",bindings.bindings[0].axis},{"F1",bindings.bindings[1].axis},{"F2",bindings.bindings[2].axis},
            {"F3",bindings.bindings[3].axis},{"F4",bindings.bindings[4].axis}}},
        {"time",{{"epoch",runtime.epoch},{"revision",state.revision},{"phase",runtime.phase==time_runtime::Phase::Active?"Active":"Inactive"},
            {"hook_mask",runtime.hook_mask},{"write_locked",controller.write_locked},{"selected_scale",state.clock.selected},
            {"pause_scale",state.clock.pause},{"native_clock_scale",clock.time_scale}}},
        {"presentation",{{"epoch",presentation.epoch},{"revision",presentation.revision},{"selected_scale",presentation.selected_scale},
            {"pause_scale",presentation.pause_scale},{"items",items}}},
        {"counters",{{"game_updates",completed_frames},{"ui_updates",ui_updates},
            {"host_setter_confirmed",cfg.is_host?runtime.native_set_confirmed:0},
            {"client_clock_writes",cfg.is_host?0:runtime.native_set_confirmed},
            {"business_day_forwarded",days.ticks[0].forwarded},{"subscriber_day_forwarded",days.ticks[1].forwarded}}},
        {"presentation_acks",acks},{"processed_sequences",sequences}};
}
bool queue(const json& args,uint64_t completed_frames,json& result,std::string& error) {
    if(args.size()!=5)throw std::runtime_error("native_time_key requires exactly five fields");
    input::Request request;request.token=token(args);
    const auto physical=args.at("key").get<std::string>();
    if(physical=="SPACE")request.key=input::Key::Space;
    else if(physical=="F1")request.key=input::Key::F1;
    else if(physical=="F2")request.key=input::Key::F2;
    else if(physical=="F3")request.key=input::Key::F3;
    else if(physical=="F4")request.key=input::Key::F4;
    else throw std::runtime_error("native_time_key requires SPACE or F1 through F4");
    request.epoch=positive(args.at("expected_epoch"),"expected_epoch");
    request.revision=positive(args.at("expected_revision"),"expected_revision");
    request.expected_sequence=positive(args.at("expected_request_sequence"),"expected_request_sequence");
    if(observations.contains(request.token)) {error="time_input_token_already_attempted_no_replay";return false;}
    if(unresolved()) {error="time_input_previous_effect_unresolved";return false;}
    wire::Message previous;bool available=false;
    if(!time_integration::instance().observe_receipt(Config::instance().peer_id,request.expected_sequence,previous,available,error))return false;
    if(available){error="time_input_expected_sequence_already_has_host_receipt";return false;}
    if(observations.size()==64){error="time_input_diagnostic_memo_full";return false;}
    observations.emplace(request.token,Observation{request,false,{}});
    active_token=request.token;
    input::Receipt receipt;
    const bool queued=adapter.queue(request,completed_frames,receipt,error);
    if(receipt.request.token.empty()) {active_token.clear();result={{"acceptance_token",request.token},{"state","rejected"},{"error",error}};return false;}
    result=format(receipt);
    if(queued)result["state"]="queued";
    else if(receipt.phase==input::Phase::Rejected)active_token.clear();
    return queued;
}
bool receipt(const json& args,json& result,std::string& error) {
    if(args.size()!=1)throw std::runtime_error("read_time_input requires only acceptance_token");
    input::Receipt receipt;
    if(!adapter.receipt(token(args),receipt,error))return false;
    result=format(receipt);
    if(receipt.phase==input::Phase::Rejected||receipt.phase==input::Phase::Partial||receipt.phase==input::Phase::Unknown) {
        error=receipt.error;return false;
    }
    return true;
}
}
