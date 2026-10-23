#include "prepare_flow.h"
#include "session_reload/production.h"
#include "mt2mp/net/chat_runtime.h"
#include "native_lobby.h"
#include "lobby_operations.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/net/software_sync.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "typed_action_runtime.h"
#include "ui_build_runtime.h"
#include "ui_remove_runtime.h"
#include "ui_move_runtime.h"
#include "ui_region_runtime.h"
#include "ui_terrain_runtime.h"
#include "ui_path_runtime.h"
#include "ui_network_runtime.h"
#include "typed_action/steam_action_adapter.h"
#include "session_provider.h"
#include "live_host_state/native_port.h"
#include <chrono>
#include "startup_calendar_native.inc"
#include "startup_timing.h"
namespace mt2mp::game::lobby_prepare {
namespace {
enum class Step {Idle,LoadSelected,CaptureLocal,ReturnTitle,LoadOrigin,CaptureShared,AwaitAck,Ready,Starting,Playing,Failed,Handshake,Leaving};
Step step=Step::Idle;std::string operation,snapshot,failure,selected_path;bool start_requested=false;
bool source_current_world=false,zero_day_reload_attempted=false;
bool leave_requested=false;
auto leave_began=std::chrono::steady_clock::time_point{};
auto began=std::chrono::steady_clock::time_point{};
uint64_t leave_confirmed=0;
const char* label(){switch(step){case Step::Idle:return "Choose a world";case Step::LoadSelected:case Step::LoadOrigin:return "Loading world";
case Step::CaptureLocal:case Step::CaptureShared:return "Capturing save";case Step::ReturnTitle:return "Preparing world";
case Step::AwaitAck:return "Transferring save";case Step::Ready:return "World ready";case Step::Starting:return "Starting session";
case Step::Playing:return "Connected";case Step::Leaving:return "Leaving session";case Step::Handshake:return "Confirming connection";case Step::Failed:return "Session unavailable";}return "Session unavailable";}
bool fail(const std::string& error){if(step!=Step::Failed)LOG_ERROR("[NativeLobby] Prepare failed state={} cause={}",label(),error);failure=error;step=Step::Failed;return false;}
void enter(Step next){step=next;began=std::chrono::steady_clock::now();LOG_INFO("[NativeLobby] Prepare state={}",label());}
bool submit(const char* op,const nlohmann::json& args,Step next,std::string& error){if(!lobby_operations::submit(op,args,operation,error))return fail(error);enter(next);return true;}
bool terrain_ready(){return ui_terrain_runtime::status().value("enabled",false);}
bool path_ready(){return ui_path_runtime::status().value("enabled",false);}
bool captures_ready(){return ui_build_runtime::enabled()&&ui_remove_runtime::enabled()&&ui_move_runtime::enabled()&&ui_region_runtime::enabled()&&terrain_ready()&&path_ready()&&ui_network_runtime::enabled();}
bool start_runtime(std::string& error){
    if(!startup_timing::run("typed_action_start",error,[&]{return typed_action_runtime::start(error);}))return fail(error);
    if((!ui_build_runtime::enabled()&&!startup_timing::run("capture_build",error,[&]{return ui_build_runtime::enable(error);}))||
       (!ui_remove_runtime::enabled()&&!startup_timing::run("capture_remove",error,[&]{return ui_remove_runtime::enable(error);}))||
       (!ui_move_runtime::enabled()&&!startup_timing::run("capture_move",error,[&]{return ui_move_runtime::enable(error);}))||
       (!ui_region_runtime::enabled()&&!startup_timing::run("capture_region",error,[&]{return ui_region_runtime::enable(error);}))||
       (!terrain_ready()&&!startup_timing::run("capture_terrain",error,[&]{return ui_terrain_runtime::enable(error);}))||
       (!path_ready()&&!startup_timing::run("capture_path",error,[&]{return ui_path_runtime::enable(error);}))||
       (!ui_network_runtime::enabled()&&!startup_timing::run("capture_network",error,[&]{return ui_network_runtime::enable(error); })))return fail("native action capture installation failed: "+error);
    if(!captures_ready())return fail("native action capture installation returned without actual enabled readback");
    // Claiming Channel9 and arming its live discriminator must occur before the next packet poll.
    if(!typed_action_runtime::enable_live_state(error))return fail(error);
    snapshot=net::current_snapshot_id();enter(Step::Starting);return true;
}
bool completed(nlohmann::json& response,std::string& error){if(!lobby_operations::take_response(operation,response))return false;
if(!response.value("ok",false)){error=response.value("error",nlohmann::json::object()).value("message",std::string("native operation failed"));fail(error);return false;}return true;}
bool all_ack(std::string& error){
    auto& session=action_session::selected_session();action_session::Context context;
    if(!session.current(context,error))return false;
    if(!net::session_wire::connected()){error="physical Steam handshake ACK incomplete";return false;}
    const auto software=net::software_sync_status();
    if(!software.value("error",std::string{}).empty()){error="software native ACK failed: "+software.at("error").get<std::string>();return false;}
    if(software.value("epoch",uint64_t{})!=net::current_world_epoch()){error="software ACK epoch differs from current world";return false;}
    if(Config::instance().is_host){
        const auto peers=software.value("peers",nlohmann::json::array());
        if(peers.size()!=net::session_wire::peers()){error="software initial ACK count incomplete";return false;}
        uint8_t received=0;
        for(const auto& peer:peers){const auto id=peer.value("peer",uint64_t{});if(id<2||id>4||(received&(1u<<(id-1)))||
          !(net::session_wire::peer_mask()&(1u<<(id-1)))||peer.value("acknowledged_revision",uint64_t{})!=software.value("revision",uint64_t{})||
          peer.value("pending_revision",uint64_t{})||peer.value("stalled",true)){error="software peer ACK is missing, stale or pending";return false;}received|=uint8_t(1u<<(id-1));}
        if(received!=uint8_t(net::session_wire::peer_mask()&~1u)){error="software ACK slots differ from confirmed membership";return false;}
    }
    uint64_t revision=0;
    if(!live_host_state::read_time_authority(context,revision,error)||!revision){if(error.empty())error="native Time authority receipt missing";return false;}
    error.clear();return true;
}
}
bool prepare(const std::string& path,std::string& error){
    if(step!=Step::Idle||leave_requested){error="native preparation has already been attempted or Leave is pending; no replay";return false;}
    if(!Config::instance().is_host){error="native preparation requires confirmed Host";return false;}
    if(!lobby_operations::available()){error="owned native snapshot executor is unavailable; world load not attempted";return false;}
    if(!path.starts_with("save/")||!path.ends_with(".vrz")||path.find("..")!=std::string::npos){error="native selected save path invalid";return false;}
    source_current_world=false;zero_day_reload_attempted=false;
    if(!net::session_wire::prepare(error)){
        if(!error.starts_with("info:"))return false;
        selected_path=path;enter(Step::Handshake);error.clear();return true;
    }
    if(!net::session_wire::connected()){selected_path=path;enter(Step::Handshake);error.clear();return true;}
    selected_path=path;return submit("load_snapshot",{{"selected_file",path}},Step::LoadOrigin,error);
}
bool prepare_current(std::string& error){
    if(step!=Step::Idle||leave_requested){error="native preparation has already been attempted or Leave is pending; no replay";return false;}
    if(!Config::instance().is_host){error="native preparation requires confirmed Host";return false;}
    if(!lobby_operations::available()){error="owned native snapshot executor is unavailable; current world capture not attempted";return false;}
    if(!native::in_game()){error="current Host world is no longer active";return false;}
    source_current_world=true;zero_day_reload_attempted=false;selected_path.clear();
    if(!net::session_wire::prepare(error)){
        if(!error.starts_with("info:")){source_current_world=false;return false;}
        enter(Step::Handshake);error.clear();return true;
    }
    if(!net::session_wire::connected()){enter(Step::Handshake);error.clear();return true;}
    return submit("capture_local_snapshot",nlohmann::json::object(),Step::CaptureLocal,error);
}
bool start(std::string& error){
    if(leave_requested||step==Step::Idle||step==Step::Failed||step==Step::Playing||step==Step::Leaving){error="native start requires pending or ready preparation";return false;}
    start_requested=true;error.clear();return true;
}
bool leave(std::string& error){
    if(ui_network_runtime::unresolved()){
        const auto network=ui_network_runtime::status();
        error="native Leave blocked by unresolved Network cause="+network.value("cause",std::string{});
        return false;
    }
    if(ui_path_runtime::unresolved()){
        const auto path=ui_path_runtime::status();
        error="native Leave blocked by unresolved Path cause="+path.value("cause",std::string{});
        return false;
    }
    if(ui_terrain_runtime::unresolved()){
        const auto terrain=ui_terrain_runtime::status();
        error="native Leave blocked by Terrain capture/pending/unresolved capturing="+std::to_string(terrain.value("capturing",false))+
            " pending="+std::to_string(terrain.value("pending",false))+" cause="+terrain.value("cause",std::string{});
        return false;
    }
    if(step==Step::Leaving){error="native Leave already in progress";return false;}
    bool operation_ready=false;
    if(!lobby_operations::release_for_leave(operation_ready,error))return false;
    if(!operation_ready){
        if(!leave_requested){
            leave_requested=true;leave_began=std::chrono::steady_clock::now();
            LOG_INFO("[NativeLobby] Leave requested; waiting for executing native operation without cancelling it");
        }
        error.clear();return true;
    }
    native::WorldStatus actual;if(!native::read_world_status(actual,error))return false;
    if(actual.title_transition_pending||actual.saves_in_progress>0||(actual.active_mode_index!=2&&!native::in_game())){error="native Leave requires actual idle Title or World";return false;}
    const bool reload=session_reload::busy();
    if(reload&&!session_reload::release_for_leave(error))return false;
    if((!reload||!typed_action_runtime::controller_status().value("lobby_retiring",false))&&
       !typed_action_runtime::begin_lobby_leave(error))return fail(error);
    if(reload)net::chat::Runtime::instance().set_lobby_admitted(false);
    leave_requested=false;enter(Step::Leaving);
    if(native::in_game()&&!native::request_return_to_title(error))return fail(error);
    error.clear();return true;
}
bool leaving(){return leave_requested||step==Step::Leaving;}
bool tick(std::string& error){
    if(leave_requested){
        if(std::chrono::steady_clock::now()-leave_began>=std::chrono::seconds(net::snapshot_native_load_timeout_seconds)){
            leave_requested=false;error="native Leave timed out awaiting executing snapshot operation required<900s; native task retained";return fail(error);
        }
        if(!leave(error)){leave_requested=false;return fail(error);}
        if(leave_requested)return true;
    }
    if(step==Step::Failed){error=failure;return false;}
    if(step==Step::Leaving){
        bool ready=false;if(!typed_action_runtime::finish_lobby_leave(ready,error))return fail(error);
        if(ready){operation.clear();snapshot.clear();selected_path.clear();failure.clear();start_requested=false;source_current_world=false;zero_day_reload_attempted=false;++leave_confirmed;enter(Step::Idle);}
        else if(std::chrono::steady_clock::now()-began>=std::chrono::seconds(net::snapshot_native_load_timeout_seconds)){error="native Leave timed out awaiting actual Title/Time retirement required<900s; owner retained";return fail(error);}
        error.clear();return true;
    }
    if(step==Step::Idle){
        error.clear();
        if(!Config::instance().is_host&&(!Config::instance().local_test||native_lobby::admitted())&&net::session_wire::connected()&&
           net::snapshot_progress().value("phase",std::string{})=="active"){
            std::string pending;
            if(all_ack(pending))return start_runtime(error);
        }
        return true;
    }
    if(step==Step::Playing){
        const auto actual=typed_action_runtime::controller_status();
        const auto live_error=actual.value("live_error",std::string{});
        if(!live_error.empty()){error=live_error;return fail(error);}
        if(!captures_ready()||!actual.value("started",false)||actual.value("locked",true)){error="native playing authority/capture readiness lost: "+actual.value("cause",std::string{});return fail(error);}
        error.clear();return true;
    }
    if(std::chrono::steady_clock::now()-began>=std::chrono::seconds(net::snapshot_native_load_timeout_seconds)){
        error="native preparation state timed out state="+std::string(label())+" required<900s; native task not cancelled";return fail(error);}
    if(!net::session_wire::connected()){
        if(step==Step::Handshake&&net::session_wire::failure_reason().empty()){
            if(!net::session_wire::prepare(error)&&!error.starts_with("info:"))return fail(error);
            error.clear();return true;
        }
        error="Steam transport lost during native preparation: "+net::session_wire::failure_reason();return fail(error);
    }
    nlohmann::json response;error.clear();
    if(step==Step::Handshake){
        if(source_current_world){
            if(!native::in_game()){error="current Host world left before capture";return fail(error);}
            return submit("capture_local_snapshot",nlohmann::json::object(),Step::CaptureLocal,error);
        }
        return submit("load_snapshot",{{"selected_file",selected_path}},Step::LoadOrigin,error);
    }
    else if(step==Step::CaptureLocal){if(completed(response,error)){snapshot=response.at("result").at("snapshot_id").get<std::string>();
        if(!response.at("result").value("building_bindings_confirmed",false)){error="Host snapshot building bindings are incomplete: "+response.at("result").value("building_identity_error",std::string("native capture cause unavailable"));return fail(error);}
        if(!native::request_return_to_title(error))return fail(error);enter(Step::ReturnTitle);}}
    else if(step==Step::ReturnTitle){native::WorldStatus world;if(!native::read_world_status(world,error))return fail(error);
        if(world.active_mode_index==2&&!world.title_transition_pending)return submit("load_snapshot",{{"snapshot_id",snapshot}},Step::LoadOrigin,error);}
    else if(step==Step::LoadOrigin){if(completed(response,error)){
        native::SimulationState actual;
        if(!native::read_simulation_state(actual,error))return fail("Host loaded Clock/CurrentRecord read failed: "+error);
        if(!actual.clock.paused||actual.clock.time_scale!=0||!actual.record_present||
           actual.record_day!=actual.clock.flat_day||actual.record_cash!=actual.cash||actual.clock.flat_day<0){
            error="Host loaded Clock/CurrentRecord baseline differs paused="+std::to_string(actual.clock.paused)+
                " scale="+std::to_string(actual.clock.time_scale)+" day="+std::to_string(actual.clock.flat_day)+
                " record_present="+std::to_string(actual.record_present)+" record_day="+std::to_string(actual.record_day)+
                " cash="+std::to_string(actual.cash)+" record_cash="+std::to_string(actual.record_cash);return fail(error);
        }
        if(actual.clock.flat_day==0){
            if(zero_day_reload_attempted){error="Host native zero-day normalization incomplete after one original reload day="+
                std::to_string(actual.clock.flat_day)+" record_day="+std::to_string(actual.record_day)+
                " cash="+std::to_string(actual.cash)+" record_cash="+std::to_string(actual.record_cash);return fail(error);}
            const auto normalized=startup_calendar::normalize_loaded(error);
            if(normalized==startup_calendar::Result::Failed)return fail("Host loaded calendar normalization failed: "+error);
            if(normalized==startup_calendar::Result::Normalized){
                LOG_INFO("[StartupCalendar] Original Resolve/PostResolve confirmed day=1 record_day=1; ledger/seconds/owners unchanged; extra reload skipped");
                return submit("capture_snapshot",nlohmann::json::object(),Step::CaptureShared,error);
            }
            LOG_INFO("[StartupCalendar] Original reload retained cause={}",error);error.clear();
            zero_day_reload_attempted=true;
            return submit("capture_local_snapshot",nlohmann::json::object(),Step::CaptureLocal,error);
        }
        return submit("capture_snapshot",nlohmann::json::object(),Step::CaptureShared,error);
    }}
    else if(step==Step::CaptureShared){if(completed(response,error)){snapshot=response.at("result").at("snapshot_id").get<std::string>();
        if(!response.at("result").value("building_bindings_confirmed",false)){error="shared snapshot building bindings are incomplete: "+response.at("result").value("building_identity_error",std::string("native capture cause unavailable"));return fail(error);}enter(Step::AwaitAck);}}
    else if(step==Step::AwaitAck||step==Step::Ready){const auto progress=net::snapshot_progress();if(progress.value("phase",std::string{})=="failed"){error="snapshot transfer/load failed: "+net::snapshot_sync_status();return fail(error);}
        std::string pending;if(all_ack(pending)){if(step!=Step::Ready)enter(Step::Ready);if(start_requested)return start_runtime(error);}
        else if(step==Step::Ready){enter(Step::AwaitAck);}}
    else if(step==Step::Starting){const auto actual=typed_action_runtime::status();if(actual.value("locked",false)){error="common Controller locked during start";return fail(error);}
        if(actual.value("started",false)){
            const auto live=actual.value("live_state",nlohmann::json::object());
            if(!live.value("requested",false)){if(!typed_action_runtime::enable_live_state(error))return fail(error);}
            else if(live.value("locked",false)||!live.value("cause",std::string{}).empty()){error="native live simulation start failed: "+live.value("cause",std::string("locked"));return fail(error);}
            else if(live.value("active",false)&&(Config::instance().is_host||live.value("confirmed",false)))enter(Step::Playing);
        }}
    if(step==Step::Failed){error=failure;return false;}return true;
}
nlohmann::json status(){auto value=nlohmann::json{{"leave_confirmed",leave_confirmed},{"leaving",step==Step::Leaving},{"state",int(step)},{"label",label()},{"source",source_current_world?"current_world":"selected_save"},{"snapshot_id",snapshot},{"operation_id",operation},{"start_requested",start_requested},{"prepared",step==Step::Ready},{"captures_ready",captures_ready()},{"ready",step==Step::Playing&&captures_ready()},{"playing",step==Step::Playing},{"error",failure},{"captures",{{"build",ui_build_runtime::status()},{"remove",ui_remove_runtime::status()},{"move",ui_move_runtime::status()},{"region",ui_region_runtime::status()},{"terrain",ui_terrain_runtime::status()},{"path",ui_path_runtime::status()}}}};
    const auto reload=session_reload::status();value["reload"]=reload;value["reloading"]=session_reload::busy();
    if(leave_requested){value["leaving"]=true;value["ready"]=false;value["prepared"]=false;value["state"]=int(Step::Leaving);value["label"]="Leaving session";}
    else if(session_reload::busy()){value["ready"]=false;value["prepared"]=false;value["state"]=int(Step::Handshake);value["label"]=reload.at("label");
        if(value.at("error").get<std::string>().empty())value["error"]=reload.at("error");}
    return value;
}
}

namespace mt2mp::game::lobby_prepare {
bool reset_after_world_reload(std::string& error){
    native::WorldStatus world;
    if(!native::read_world_status(world,error))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0||
       lobby_operations::pending()||typed_action_runtime::controller_status().value("initialized",false)){
        error="world reload reset requires actual idle Title and retired native Controller";return false;
    }
    operation.clear();snapshot.clear();selected_path.clear();failure.clear();start_requested=false;
    source_current_world=zero_day_reload_attempted=leave_requested=false;enter(Step::Idle);error.clear();return true;
}
bool prepare_snapshot(const std::string& id,std::string& error){
    if(step!=Step::Idle||leave_requested||!Config::instance().is_host||!net::session_wire::connected()||
       id.size()!=36||id.find_first_not_of("0123456789abcdef-")!=std::string::npos){
        error="shared reload requires idle Host, confirmed physical members and captured snapshot ID";return false;
    }
    source_current_world=zero_day_reload_attempted=false;selected_path.clear();start_requested=false;
    return submit("load_snapshot",{{"snapshot_id",id}},Step::LoadOrigin,error);
}
}
