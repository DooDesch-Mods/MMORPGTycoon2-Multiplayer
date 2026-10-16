#include "../game/bug_authority/runtime_internal.h"
#include "mt2mp/core/dll_main.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/installed_profile.h"
#include "mt2mp/core/private_profile_paths.h"
#include "mt2mp/net/steam_module.h"
#include <map>
#include "mt2mp/core/logger.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/hooks/hook_renderer.h"
#include "mt2mp/hooks/hook_input.h"
#include "mt2mp/hooks/hook_window.h"
#include "mt2mp/overlay/gl_overlay.h"
#include "mt2mp/overlay/notification_overlay.h"
#include "mt2mp/overlay/lobby_overlay.h"
#include "mt2mp/overlay/chat_overlay.h"
#include "mt2mp/net/local_session.h"
#include "mt2mp/net/sidecar_client.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/chat_runtime.h"
#include "mt2mp/net/chat_protocol.h"
#include "../game/lobby_operations.h"
#include "../game/prepare_flow.h"
#include "../game/native_lobby.h"
#include "../game/loan_diagnostics/loan_gate.h"
#include "../game/initial_release/release_fixture.h"
#include "../game/developer_staff/probe.h"
#include "../game/developer_staff/bug_policy.h"
#include "../game/bug_authority/manual_action.h"
#include "../game/developer_staff/action_native.h"
#include "../game/developer_staff/retirement/retire.h"
#include "../game/subscriber_lifecycle/runtime_capture.h"
#include "../game/subscriber_lifecycle/runtime_append.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/net/snapshot_world_path.h"
#include "mt2mp/net/snapshot_buffer.h"
#include "mt2mp/net/software_sync.h"
#include "mt2mp/net/diagnostic_placement.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/game/native_network.h"
#include "../game/time_diagnostics.h"
#include "../game/time_integration.h"
#include "../game/time_input_diagnostics.h"
#include "../game/global_building_catalog.h"
#include "../game/building_capture_identity.h"
#include "../game/npc_quest_target_runtime.h"
#include "../game/npc_saved_identity_runtime.h"
#include "../game/root_building_load.h"
#include "../game/network_identity.h"
#include "../game/native_open_runtime.h"
#include "../game/tutorial_observer.h"
#include "../game/tutorial_development.h"
#include "../game/tutorial_replication/native.h"
#include "../game/tutorial_movement/movement_input.h"
#include "../game/tutorial_startingpoint_fixture/startingpoint_fixture.h"
#include "../game/welcome_close.h"
#include "../game/native_free_diagnostics.h"
#include "../game/remove_diagnostics.h"
#include "../game/typed_action_runtime.h"
#include "../game/scenery_kiss/native_catalog.h"
#include "../game/ui_remove_runtime.h"
#include "../game/ui_build_runtime.h"
#include "../game/building_history_slice/native_bridge.h"
#include "../game/ui_move_runtime.h"
#include "../game/ui_region_runtime.h"
#include "../game/ui_price_runtime.h"
#include "../game/npc_probe.h"
#include "../game/ui_terrain_runtime.h"
#include "../game/ui_path_runtime.h"
#include "../game/ui_network_runtime.h"
#include "../game/observer_mcp.h"
#include "../game/observer_hooks.h"
#include "../game/live_host_state/economic_fence.h"
#include "../game/subscriber_lifecycle/retirement_runtime.h"
#include "../game/render_pool_guard/runtime.h"
#include "../game/live_host_state/day_rollover/native_day.h"
#include "../game/move_ui_probe/contract.h"
#include "../game/native_tool_prepare/tool_cancel.h"
#include "../game/placement_ui_diagnostics/placement_ui_diagnostics.h"
#include "../game/hud_time_reader_process.h"
#include "../game/hud_time_activation.h"
#include "../game/time_mouse_diagnostics.h"
#include "../game/hud_time_runtime.h"
#include "../game/hud_calendar_runtime.h"
#include "mt2mp/game/normal_load_identity.h"
#include "mt2mp/core/hash.h"
#include "vendor/nlohmann/json.hpp"
#include <SDL3/SDL.h>
#include <GL/gl.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <atomic>
#include <exception>
#include <cmath>
#include <limits>
#include "../game/live_host_state/apply_timing.h"
#include <windows.h>
#include <psapi.h>
#include <intrin.h>

namespace mt2mp::installed_profile {const std::filesystem::path& diagnostic_root();}
namespace mt2mp {
using json=nlohmann::json;
static HMODULE g_module=nullptr;
static std::atomic<bool> initialized=false;
static mt2mp::subscriber_lifecycle::Lookup original_subscriber_context_lookup=nullptr;
static void* subscriber_context_lookup(void* context,int32_t id) {
    return game::subscriber_lifecycle_append::lookup(context,id,original_subscriber_context_lookup);
}
static game::subscriber_retirement::Remove original_subscriber_remove=nullptr;
static void subscriber_remove(void* manager,void* subscriber){game::subscriber_retirement::observe(manager,subscriber,original_subscriber_remove);}
static uintptr_t native_image_begin=0,native_image_end=0;
static std::string runtime_root,run_id,last_request,last_mailbox_request;
static std::string loaded_dll_sha256;
static json installed_proof_receipt;
static uint64_t completed_game_updates=0;
static bool tutorial_movement_ready=false;
static json tutorial_movement_receipt() {
    const auto& r=game::tutorial_movement::receipt();
    const char* phases[]={"idle","down_queued","up_queued","confirmed","failed"};
    auto snapshot=[](const game::tutorial_movement::Snapshot& s) {
        return json{{"native_origin_uuid",s.origin},{"world_epoch",s.epoch},{"world_generation",s.generation},
                    {"axis",s.axis},{"progress",s.progress},{"key_down",s.key_down}};
    };
    return {{"schema",1},{"phase",phases[static_cast<size_t>(r.phase)]},{"token",r.token},
            {"error",r.error},{"release_error",r.release_error},{"before",snapshot(r.before)},{"after",snapshot(r.after)},
            {"down_attempted",r.down_attempted},{"down_queued",r.down_queued},{"down_unknown",r.down_unknown},
            {"down_polled",r.down_polled},{"axis_down",r.axis_down},{"up_attempted",r.up_attempted},
            {"up_queued",r.up_queued},{"up_polled",r.up_polled},{"axis_released",r.axis_released},
            {"progress_changed",r.progress_changed},{"down_events",r.down_events},{"up_events",r.up_events},
            {"down_frame",r.down_frame},{"up_frame",r.up_frame}};
}
// Bound game-thread aggregates attribute CPU-side wall time without per-frame logging.
struct FrameTimingTotals { uint64_t count=0,total_ns=0,max_ns=0; };
enum class FrameTimingPart : size_t { Update,Original,Lobby,Typed,Transport,Time,Hud,Diagnostics,Prepare,Count };
static FrameTimingTotals frame_timing[size_t(FrameTimingPart::Count)];
static FrameTimingTotals frame_intervals;
static std::chrono::steady_clock::time_point previous_frame_start{};
static void record_frame_timing(FrameTimingTotals& totals,uint64_t ns) {
    ++totals.count;totals.total_ns+=ns;if(ns>totals.max_ns)totals.max_ns=ns;
}
struct FrameTimingScope {
    FrameTimingPart part;
    std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
    explicit FrameTimingScope(FrameTimingPart p):part(p) {}
    ~FrameTimingScope(){record_frame_timing(frame_timing[size_t(part)],uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count()));}
};
static json frame_timing_status() {
    static constexpr const char* names[]={"game_update","original_update","native_lobby","typed_actions","transport","time","hud","diagnostics","lobby_prepare"};
    json parts=json::object();
    for(size_t i=0;i<size_t(FrameTimingPart::Count);++i){const auto& t=frame_timing[i];parts[names[i]]={{"count",t.count},{"total_ns",t.total_ns},{"max_ns",t.max_ns}};}
    parts["frame_interval"]={{"count",frame_intervals.count},{"total_ns",frame_intervals.total_ns},{"max_ns",frame_intervals.max_ns}};
    return {{"schema",1},{"unit","nanoseconds"},{"completed_game_updates",completed_game_updates},{"parts",std::move(parts)},{"apply_parts",game::live_host_state::apply_timing::snapshot()}};
}
static DWORD render_thread=0;
static DWORD engine_thread=0;
static json pending_frame;
static json pending_world;
static json pending_overlay;
static json pending_crash_dismiss;
static std::chrono::steady_clock::time_point crash_dismiss_started;
static std::chrono::steady_clock::time_point overlay_started;
static std::chrono::steady_clock::time_point world_started;
static json deferred_response;
static std::string diagnostic_failure;
static size_t diagnostic_failure_count=0;
static json pending_snapshot;
static json pending_placement;
static json pending_load;
static bool owned_native_profile=false;
static bool native_lobby_attempted=false,native_lobby_initialized=false;
static std::string native_lobby_error;
static json pending_camera;
static std::chrono::steady_clock::time_point camera_started;
static bool diagnostic_executor_idle() {
    return deferred_response.is_null()&&pending_frame.is_null()&&pending_world.is_null()&&
        pending_overlay.is_null()&&pending_crash_dismiss.is_null()&&pending_snapshot.is_null()&&
        pending_placement.is_null()&&pending_load.is_null()&&pending_camera.is_null();
}
static net::SnapshotLoadWatch load_watch;
static std::filesystem::path snapshot_path;
static std::chrono::steady_clock::time_point snapshot_started;
using GameUpdate=void(*)(void*,float);
static GameUpdate original_game_update=nullptr;
static GameUpdate original_clock_update=nullptr;
using TimeActivate=void(*)(void*,void*);
using TimeUpdate=void(*)(void*,void*,float);
static TimeActivate original_time_activate=nullptr;
static TimeUpdate original_time_update=nullptr;
static bool time_initialization_attempted=false,time_initialized=false,time_hooks_ready=false;
static std::string time_initialization_error;
static game::hud_time::Reader hud_time_reader;
static game::hud_time::Activation hud_time_activation;
static bool hud_time_reader_ready=false;
static std::string hud_time_reader_error;
static std::string time_tick_error;
static uint64_t time_tick_confirmed=0,time_tick_failed=0,time_tick_waiting=0;
static uint64_t time_activate_attempted=0,time_activate_matched=0,time_activate_suppressed=0;
static uint64_t time_activate_original=0;
static uint64_t time_update_attempted=0,time_update_suppressed=0;
static uintptr_t time_last_caller_rva=0;
static game::shared_time::Intent time_last_intent=game::shared_time::Intent::Pause;
static bool time_last_request_queued=false;
static std::string time_mouse_error;
static uint64_t time_mouse_error_repeats=0;
static bool time_mouse_context(game::hud_time::ActivationContext& out,std::string& error) {
    out={};game::time_integration::Status status;
    if(!game::time_integration::instance().status(status,error))return false;
    const auto& cfg=Config::instance();const auto& runtime=status.runtime;
    const auto& state=cfg.is_host?runtime.controller.observed:runtime.controller.applied;
    if(cfg.peer_id<1||cfg.peer_id>4){error="hud_activation_local_peer_invalid";return false;}
    out.epoch=runtime.epoch;out.local_peer=static_cast<uint8_t>(cfg.peer_id);out.peer_mask=net::session_wire::peer_mask();
    out.host=cfg.is_host;out.hook_mask=time_hooks_ready?runtime.hook_mask:0;
    out.active=owned_native_profile&&runtime.phase==game::time_runtime::Phase::Active&&runtime.controller.started&&
        !runtime.controller.write_locked&&!runtime.presentation_uncertain&&state.present&&!state.uncertain&&
        runtime.epoch&&runtime.epoch==net::current_world_epoch()&&net::session_wire::connected()&&
        net::pending_snapshot_load_id().empty()&&pending_load.is_null()&&pending_world.is_null();
    error.clear();return true;
}
static bool time_input_context(void*,game::native_time_input::Context& out,std::string& error) {
    game::time_integration::Status status;
    if(!game::time_integration::instance().status(status,error))return false;
    auto& cfg=Config::instance();const auto& runtime=status.runtime;
    const auto& state=cfg.is_host?runtime.controller.observed:runtime.controller.applied;
    game::native::WorldStatus world;
    if(!game::native::read_world_status(world,error))return false;
    auto* window=hooks::get_game_window();
    out.epoch=runtime.epoch;out.revision=state.revision;out.peer=static_cast<uint32_t>(cfg.peer_id);
    out.owned_window=reinterpret_cast<uintptr_t>(window);out.window_id=window?SDL_GetWindowID(window):0;
    out.owned_test=owned_native_profile&&(!cfg.installed_profile_verified||installed_profile::get()!=nullptr)&&!runtime_root.empty()&&!run_id.empty()&&!loaded_dll_sha256.empty();
    out.world_owned=runtime.epoch!=0&&runtime.epoch==net::current_world_epoch()&&world.world_present&&
        world.active_mode_index==3&&world.in_game_state==4&&!world.quitting&&!world.title_transition_pending;
    out.time_active=runtime.phase==game::time_runtime::Phase::Active&&runtime.hook_mask==3&&
        runtime.controller.started&&!runtime.controller.write_locked&&state.present&&!state.uncertain&&!runtime.presentation_uncertain;
    out.overlay_hidden=!overlay::LobbyOverlay::instance().is_visible();
    out.no_pending_operation=pending_frame.is_null()&&pending_world.is_null()&&pending_overlay.is_null()&&
        pending_crash_dismiss.is_null()&&pending_snapshot.is_null()&&pending_placement.is_null()&&pending_load.is_null()&&
        pending_camera.is_null()&&net::pending_snapshot_load_id().empty()&&world.saves_in_progress<=0;
    return true;
}
static void time_activate(void* type,void* item) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto rva=caller>=native_image_begin&&caller<native_image_end?caller-native_image_begin:0;
    ++time_activate_attempted;
    time_last_caller_rva=rva;
    auto result=game::time_integration::instance().on_activate(type,item,rva);
    if(!result.matched&&owned_native_profile&&rva==game::hud_time::Activation::mouse_caller_rva) {
        result=hud_time_activation.on_activate(reinterpret_cast<uintptr_t>(type),reinterpret_cast<uintptr_t>(item),rva);
        if(!result.error.empty()) {
            if(time_mouse_error==result.error)++time_mouse_error_repeats;
            else {time_mouse_error=result.error;time_mouse_error_repeats=1;
                LOG_ERROR("[HudTime] Mouse time change rejected cause={} original_suppressed={}",result.error,result.decision==game::shared_time::Decision::Suppress);}
        } else if(result.request_queued&&!time_mouse_error.empty()) {
            LOG_INFO("[HudTime] Mouse time request accepted previous={} repeats={}",time_mouse_error,time_mouse_error_repeats);
            time_mouse_error.clear();time_mouse_error_repeats=0;
        }
    }
    game::time_input_diagnostics::leaf(rva,result,completed_game_updates+1);
    if(result.matched) {
        ++time_activate_matched;
        time_last_intent=result.intent; time_last_request_queued=result.request_queued;
    }
    if(result.decision==game::shared_time::Decision::Forward) {++time_activate_original;original_time_activate(type,item);}
    else ++time_activate_suppressed;
}
static void time_update(void* type,void* item,float dt) {
    ++time_update_attempted;
    const auto result=game::time_integration::instance().on_update(type,item);
    if(result.decision==game::shared_time::Decision::Forward) {
        const auto hud=game::hud_time_runtime::on_update(type,item);
        if(hud.forward_original) original_time_update(type,item,dt);
        else ++time_update_suppressed;
    } else ++time_update_suppressed;
}
static void initialize_time_hooks() {
    if(time_initialization_attempted) return;
    time_initialization_attempted=true;
    try {
        game::shared_time::Config config{native_image_begin,GetCurrentThreadId(),
            "bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"};
        std::string error;
        hud_time_reader_ready=hud_time_reader.initialize({native_image_begin,GetCurrentThreadId(),config.verified_executable_sha256},
            game::hud_time::process_read_services(),hud_time_reader_error);
        if(!hud_time_reader_ready) LOG_ERROR("[HudTime] Read-only initialization failed cause={}; no automatic retry",hud_time_reader_error);
        if(!game::hud_time_runtime::initialize({native_image_begin,GetCurrentThreadId(),config.verified_executable_sha256},error))
            throw std::runtime_error("HUD writer initialization failed: "+error);
        if(!game::hud_calendar_runtime::initialize({native_image_begin,GetCurrentThreadId(),config.verified_executable_sha256},error))
            throw std::runtime_error("HUD Calendar initialization failed: "+error);
        if(!game::time_integration::initialize(config,error)) throw std::runtime_error(error);
        time_initialized=true;
        game::hud_time::ActivationServices mouse;
        mouse.client=!Config::instance().is_host;
        mouse.thread_id=[](){return GetCurrentThreadId();};mouse.context=time_mouse_context;
        mouse.submit=[](game::time_session::Intent intent){return game::time_integration::instance().submit(intent);};
        if(!hud_time_reader_ready||!hud_time_activation.initialize(GetCurrentThreadId(),hud_time_reader,std::move(mouse),error))
            throw std::runtime_error("HUD mouse activation initialization failed: "+(hud_time_reader_ready?error:hud_time_reader_error));
        if(!game::time_input_diagnostics::initialize(native_image_begin,GetCurrentThreadId(),run_id,loaded_dll_sha256,time_input_context,error))
            throw std::runtime_error("native time input initialization failed: "+error);
        game::time_mouse_diagnostics::Services mouse_diagnostics;
        mouse_diagnostics.context=[](game::native_time_input::Context& out,std::string& e){return time_input_context(nullptr,out,e);};
        mouse_diagnostics.hook_counts=[](){return game::time_mouse_diagnostics::HookCounts{time_activate_attempted,time_activate_matched,
            time_activate_suppressed,time_activate_original,time_last_caller_rva};};
        if(!game::time_mouse_diagnostics::initialize(native_image_begin,GetCurrentThreadId(),hud_time_reader,hud_time_activation,
            std::move(mouse_diagnostics),error))throw std::runtime_error("native mouse diagnostics initialization failed: "+error);
        if(Config::instance().local_test) {
            tutorial_movement_ready=game::tutorial_movement::initialize(native_image_begin,GetCurrentThreadId(),run_id,loaded_dll_sha256,error);
            if(!tutorial_movement_ready)LOG_ERROR("[TutorialMovement] Initialization failed cause={}; no retry",error);
        }
        hooks::set_input_event_observer([](void* event){
            game::time_input_diagnostics::poll(event,completed_game_updates+1);
            if(tutorial_movement_ready)game::tutorial_movement::poll(event,completed_game_updates+1);
        });
        if(!hooks::create_hook(reinterpret_cast<void*>(native_image_begin+game::time_integration::activate_rva),
            reinterpret_cast<void*>(&time_activate),reinterpret_cast<void**>(&original_time_activate)))
            throw std::runtime_error("native time Activate hook creation failed");
        if(!hooks::create_hook(reinterpret_cast<void*>(native_image_begin+game::time_integration::update_rva),
            reinterpret_cast<void*>(&time_update),reinterpret_cast<void**>(&original_time_update)))
            throw std::runtime_error("native time Update hook creation failed");
        if(!hooks::enable_all()) throw std::runtime_error("native time hook activation failed");
        if(!game::time_integration::instance().set_hook_mask(3,error)) throw std::runtime_error(error);
        if(!game::hud_time_runtime::hooks_enabled(original_time_update!=nullptr,original_clock_update!=nullptr,error))
            throw std::runtime_error("HUD writer hook confirmation failed: "+error);
        if(!game::hud_calendar_runtime::hooks_enabled(error))throw std::runtime_error("HUD Calendar hook confirmation failed: "+error);
        time_hooks_ready=true;
        LOG_INFO("[TimeIntegration] Native hooks ready mask=3 channel=8 game_thread={}",GetCurrentThreadId());
    } catch(const std::exception& error) {
        time_initialization_error=error.what();
        LOG_ERROR("[TimeIntegration] Initialization failed cause={}; no automatic retry",time_initialization_error);
    } catch(...) {
        time_initialization_error="non-standard time integration initialization exception";
        LOG_ERROR("[TimeIntegration] Initialization failed cause={}; no automatic retry",time_initialization_error);
    }
}
static bool suspend_time(const std::string& reason,std::string& error) {
    if(tutorial_movement_ready)game::tutorial_movement::suspend(reason);
    game::time_input_diagnostics::suspend(reason);
    if(!game::node_observer::suspend_if_prepared(reason,error))return false;
    if(!time_initialization_attempted&&!time_initialized){error.clear();return true;}
    if(!time_initialized) { error="time_integration_not_initialized: "+time_initialization_error; return false; }
    if(!game::hud_time_runtime::suspend(reason,error))return false;
    if(!game::hud_calendar_runtime::suspend(reason,error))return false;
    return game::time_integration::instance().suspend(reason,error);
}
static std::string load_clock_key,load_clock_error;
static uint64_t load_clock_attempted=0,load_clock_confirmed=0,load_clock_changed=0,load_clock_failed=0;
static void clock_update(void* receiver,float dt) {
    if(owned_native_profile) {
        std::string key=pending_load.is_object()?pending_load.at("id").get<std::string>():net::pending_snapshot_load_id();
        if(!key.empty()&&key!=load_clock_key) {
            game::native::WorldStatus world; std::string error;
            if(game::native::in_game()&&!game::native::read_world_status(world,error)) {
                load_clock_key=key; load_clock_error="pending load world guard failed: "+error;
                ++load_clock_attempted; ++load_clock_failed;
                LOG_ERROR("[LoadClock] World guard failed request={} cause={}; original ClockUpdate continues; no replay",key,error);
            } else if(game::native::in_game()&&world.in_game_state==4) {
                load_clock_key=key; load_clock_error.clear(); ++load_clock_attempted;
                bool changed=false; game::native::ClockState actual;
                if(game::native::pause_load_clock(receiver,changed,actual,error)) {
                    ++load_clock_confirmed; load_clock_changed+=changed?1:0;
                    LOG_INFO("[LoadClock] Pause confirmed request={} changed={} day={} seconds={}; original ClockUpdate continues",key,changed,actual.flat_day,actual.seconds);
                } else {
                    ++load_clock_failed; load_clock_error=error;
                    LOG_ERROR("[LoadClock] Pause failed request={} cause={}; original ClockUpdate continues; no replay",key,error);
                }
            }
        }
    }
    game::hud_time_runtime::clock_update(receiver,dt,original_clock_update);
}
using SoftwareTick=void(*)(void*);
static SoftwareTick original_software_tick=nullptr,original_software_ticker_tick=nullptr;
static SoftwareTick original_software_rating=nullptr,original_software_fun=nullptr;
using BusinessDay=void(*)(void*,int32_t);
static BusinessDay original_business_day=nullptr;
static SoftwareTick original_subscriber_day=nullptr;
using NeedWrite=void(*)(void*,int32_t,float);
using NeedAssign=void*(*)(void*,const void*);
static NeedWrite original_need_set=nullptr,original_need_increment=nullptr;
static NeedAssign original_need_add=nullptr,original_need_subtract=nullptr;
static GameUpdate original_brain_tick=nullptr;
static SoftwareTick original_update_needs=nullptr;
static void need_set(void* receiver,int32_t index,float value) {
    if(game::native::subscriber_needs_write_should_forward(receiver,game::native::NeedsWriteLeaf::Set,index,value)) {
        original_need_set(receiver,index,value);
        game::native::record_subscriber_needs_write_forwarded(game::native::NeedsWriteLeaf::Set);
    }
}
static void need_increment(void* receiver,int32_t index,float value) {
    if(game::native::subscriber_needs_write_should_forward(receiver,game::native::NeedsWriteLeaf::Increment,index,value)) {
        original_need_increment(receiver,index,value);
        game::native::record_subscriber_needs_write_forwarded(game::native::NeedsWriteLeaf::Increment);
    }
}
static void* need_add(void* receiver,const void* other) {
    if(!game::native::subscriber_needs_write_should_forward(receiver,game::native::NeedsWriteLeaf::AddAssign)) return receiver;
    void* result=original_need_add(receiver,other);
    game::native::record_subscriber_needs_write_forwarded(game::native::NeedsWriteLeaf::AddAssign);
    return result;
}
static void* need_subtract(void* receiver,const void* other) {
    if(!game::native::subscriber_needs_write_should_forward(receiver,game::native::NeedsWriteLeaf::SubtractAssign)) return receiver;
    void* result=original_need_subtract(receiver,other);
    game::native::record_subscriber_needs_write_forwarded(game::native::NeedsWriteLeaf::SubtractAssign);
    return result;
}
static void brain_tick(void* receiver,float dt) {
    bool selected=game::native::observe_subscriber_needs_tick(receiver,game::native::NeedsTickLeaf::BrainTick);
    original_brain_tick(receiver,dt);
    game::native::record_subscriber_needs_tick_forwarded(game::native::NeedsTickLeaf::BrainTick,selected);
}
static void update_needs(void* receiver) {
    bool selected=game::native::observe_subscriber_needs_tick(receiver,game::native::NeedsTickLeaf::UpdateNeeds);
    original_update_needs(receiver);
    game::native::record_subscriber_needs_tick_forwarded(game::native::NeedsTickLeaf::UpdateNeeds,selected);
}
static void business_day(void* receiver,int32_t day) {
    game::native::observe_day_tick(receiver,game::native::DayTickLeaf::BusinessFinances,day);
    if(!game::live_host_state::economic_fence().allow(game::live_host_state::EconomicLeaf::NextDay,receiver,day))return;
    original_business_day(receiver,day);
    game::native::record_day_tick_forwarded(game::native::DayTickLeaf::BusinessFinances);
}
static void subscriber_day(void* receiver) {
    game::native::observe_day_tick(receiver,game::native::DayTickLeaf::Subscriber);
    if(!game::live_host_state::economic_fence().allow(game::live_host_state::EconomicLeaf::SubscriberDay,receiver))return;
    original_subscriber_day(receiver);
    game::native::record_day_tick_forwarded(game::native::DayTickLeaf::Subscriber);
}
static game::npc_quest_target_runtime::PropertyCall original_save_zone_property=nullptr,original_load_zone_property=nullptr;
static bool save_zone_property(const void* property,void* manager,void* writer,void* context) {
    const auto ticket=game::npc_quest_target_runtime::save_before(property,manager,writer,context);
    bool result=false;
    try {result=original_save_zone_property(property,manager,writer,context);}
    catch(const std::exception& error){game::npc_quest_target_runtime::original_failed(ticket,error.what());throw;}
    catch(...){game::npc_quest_target_runtime::original_failed(ticket,"non-standard original SaveStream exception");throw;}
    game::npc_quest_target_runtime::save_after(ticket,property,manager,writer,context,result);return result;
}
static bool load_zone_property(const void* property,void* manager,void* reader,void* context) {
    const auto ticket=game::npc_quest_target_runtime::load_before(property,manager,reader,context);
    bool result=false;
    try {result=original_load_zone_property(property,manager,reader,context);}
    catch(const std::exception& error){game::npc_quest_target_runtime::original_failed(ticket,error.what());throw;}
    catch(...){game::npc_quest_target_runtime::original_failed(ticket,"non-standard original LoadStream exception");throw;}
    game::npc_quest_target_runtime::load_after(ticket,property,manager,reader,context,result);return result;
}
static game::npc_saved_identity_runtime::PropertyCall original_save_npc_property=nullptr,original_load_npc_property=nullptr;
static bool save_npc_property(const void* property,void* manager,void* writer,void* context) {
    const auto ticket=game::npc_saved_identity_runtime::save_before(property,manager,writer,context);
    bool result=false;
    try {result=original_save_npc_property(property,manager,writer,context);}
    catch(const std::exception& error){game::npc_saved_identity_runtime::original_failed(ticket,error.what());throw;}
    catch(...){game::npc_saved_identity_runtime::original_failed(ticket,"non-standard original SaveStream exception");throw;}
    game::npc_saved_identity_runtime::save_after(ticket,property,manager,writer,context,result);return result;
}
static bool load_npc_property(const void* property,void* manager,void* reader,void* context) {
    const auto ticket=game::npc_saved_identity_runtime::load_before(property,manager,reader,context);
    bool result=false;
    try {result=original_load_npc_property(property,manager,reader,context);}
    catch(const std::exception& error){game::npc_saved_identity_runtime::original_failed(ticket,error.what());throw;}
    catch(...){game::npc_saved_identity_runtime::original_failed(ticket,"non-standard original LoadStream exception");throw;}
    game::npc_saved_identity_runtime::load_after(ticket,property,manager,reader,context,result);return result;
}
static game::root_building_load::ArrayFn original_load_building_array=nullptr;
static game::root_building_load::ChildrenFn original_building_reader_children=nullptr;
static game::root_building_load::InlineFn original_inline_building=nullptr;
static void load_building_array(void* array,void* reader,void* context) {
    const auto ticket=game::root_building_load::before(array,reader,context);
    try { original_load_building_array(array,reader,context); }
    catch(const std::exception& error) {
        game::root_building_load::original_failed(ticket,error.what());
        LOG_ERROR("[RootBuildingLoad] Native original failed cause={}; exception continues",error.what()); throw;
    } catch(...) {
        game::root_building_load::original_failed(ticket,"non-standard native original exception");
        LOG_ERROR("[RootBuildingLoad] Native original failed with non-standard exception; exception continues"); throw;
    }
    game::root_building_load::after(ticket,array,reader,context);
}
static int32_t building_reader_children(void* reader) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto caller_rva=caller>=native_image_begin&&caller<native_image_end?caller-native_image_begin:0;
    game::npc_quest_target_runtime::children_before(caller_rva,reader);
    game::npc_saved_identity_runtime::children_before(caller_rva,reader);
    const auto result=original_building_reader_children(reader);
    game::npc_quest_target_runtime::children_after(caller_rva,reader,result);
    game::npc_saved_identity_runtime::children_after(caller_rva,reader,result);
    game::bug_authority::original_children(caller_rva,reader,result);
    game::root_building_load::children(caller>=native_image_begin&&caller<native_image_end?caller-native_image_begin:0,reader,result);
    return result;
}
static void inline_building(void* context,void* object) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    original_inline_building(context,object);
    game::npc_quest_target_runtime::registered(caller>=native_image_begin&&caller<native_image_end?caller-native_image_begin:0,context,0,object);
    game::npc_saved_identity_runtime::registered(caller>=native_image_begin&&caller<native_image_end?caller-native_image_begin:0,context,0,object);
    game::bug_authority::original_registered(caller>=native_image_begin&&caller<native_image_end?caller-native_image_begin:0,context,0,object);
    game::root_building_load::registered(caller>=native_image_begin&&caller<native_image_end?caller-native_image_begin:0,context,0,object);
}
static game::normal_load_identity::AddObjectFn original_add_object=nullptr;
static game::network_identity::CaptureSaveFn original_save_network_nodes=nullptr;
static bool save_network_nodes(const void* property,const void* manager,void* writer,void* context) {
    auto ticket=game::network_identity::capture_before(property,manager,writer,context);
    bool result=false;
    try { result=original_save_network_nodes(property,manager,writer,context); }
    catch(const std::exception& error) {
        game::network_identity::original_failed(ticket,error.what());
        LOG_ERROR("[NetworkIdentity] Native save original failed cause={}; exception continues",error.what());
        throw;
    }
    catch(...) {
        game::network_identity::original_failed(ticket,"non-standard native original exception");
        LOG_ERROR("[NetworkIdentity] Native save original failed with non-standard exception; exception continues");
        throw;
    }
    game::network_identity::capture_after(ticket,property,manager,writer,context,result);
    return result;
}
static game::building_capture_identity::SaveArrayFn original_save_building_array=nullptr;
static std::atomic<uint64_t> building_original_attempted{0},building_original_returned{0},building_original_failed{0};
static std::atomic<uint32_t> building_original_thread{0};
static uint64_t building_capture_original_start=0,building_capture_return_start=0,building_capture_failure_start=0;
static void save_building_array(const void* array,void* writer,void* context) {
    auto ticket=game::building_capture_identity::capture_before(array,writer,context);
    ++building_original_attempted; building_original_thread=GetCurrentThreadId();
    try { original_save_building_array(array,writer,context); ++building_original_returned; }
    catch(const std::exception& error) {
        ++building_original_failed;
        game::building_capture_identity::original_failed(ticket,error.what());
        LOG_ERROR("[BuildingCapture] Native save original failed cause={}; exception continues",error.what());
        throw;
    }
    catch(...) {
        ++building_original_failed;
        game::building_capture_identity::original_failed(ticket,"non-standard native original exception");
        LOG_ERROR("[BuildingCapture] Native save original failed with non-standard exception; exception continues");
        throw;
    }
    game::building_capture_identity::capture_after(ticket,array,writer,context);
}
static void add_object(void* context,int32_t link_id,void* object) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const uint64_t caller_rva=caller>=native_image_begin && caller<native_image_end
        ? static_cast<uint64_t>(caller-native_image_begin) : 0;
    original_add_object(context,link_id,object);
    game::network_identity::observe_client_registered(context,link_id,object,caller_rva);
    game::normal_load_identity::observe_registered(context,link_id,object);
    game::root_building_load::registered(caller_rva,context,link_id,object);
    game::npc_quest_target_runtime::registered(caller_rva,context,link_id,object);
    game::npc_saved_identity_runtime::registered(caller_rva,context,link_id,object);
    game::bug_authority::original_registered(caller_rva,context,link_id,object);
}
static void software_rating(void* receiver) {
    if(game::native::software_calculation_should_forward(receiver,game::native::SoftwareCalcLeaf::Rating)) {
        original_software_rating(receiver);
        game::native::record_software_calculation_forwarded(game::native::SoftwareCalcLeaf::Rating);
    }
}
static void software_fun(void* receiver) {
    if(game::native::software_calculation_should_forward(receiver,game::native::SoftwareCalcLeaf::Fun)) {
        original_software_fun(receiver);
        game::native::record_software_calculation_forwarded(game::native::SoftwareCalcLeaf::Fun);
    }
}
static void software_tick(void* receiver) {
    game::native::observe_software_tick(receiver,false);
    original_software_tick(receiver);
    game::native::record_software_tick_forwarded(false);
}
static void software_ticker_tick(void* receiver) {
    game::native::observe_software_tick(receiver,true);
    original_software_ticker_tick(receiver);
    game::native::record_software_tick_forwarded(true);
}
static void diagnostics_tick();
static void write_json(const std::filesystem::path& path,const json& data);
static void respond(const std::filesystem::path& dir,const json& response);
static json status();
static uint64_t positive_integer(const json& value,const char* name) {
    if(!value.is_number_integer()||(value.is_number_unsigned()?value.get<uint64_t>()==0:value.get<int64_t>()<=0))
        throw std::runtime_error(std::string(name)+" requires a positive uint64 integer");
    return value.get<uint64_t>();
}
static game::native::SubscriberNeeds subscriber_values(const json& value,const char* name) {
    if(!value.is_array()||value.size()!=7) throw std::runtime_error(std::string(name)+" requires seven numbers");
    game::native::SubscriberNeeds result{};
    for(size_t i=0;i<result.size();++i) {
        if(!value[i].is_number()) throw std::runtime_error(std::string(name)+" requires finite numbers");
        double number=value[i].get<double>();
        if(!std::isfinite(number)||number < -100||number > 100) throw std::runtime_error(std::string(name)+" requires values in [-100,100]");
        result[i]=static_cast<float>(number);
    }
    return result;
}
static void capture_frame() {
    if(pending_frame.is_null()) return;
    json response={{"schema_version",1},{"id",pending_frame.at("id")},{"run_id",run_id}};
    try {
        int width=0,height=0;
        if(!SDL_GetWindowSizeInPixels(hooks::get_game_window(),&width,&height)) throw std::runtime_error(SDL_GetError());
        if(width<1||height<1||width>8192||height>8192) throw std::runtime_error("invalid framebuffer dimensions");
        for(int count=0;count<16;++count) {
            GLenum upstream=glGetError(); if(upstream==GL_NO_ERROR) break;
            LOG_WARN("[MCP] Existing OpenGL error before frame capture: {}",upstream);
        }
        using BindFramebuffer=void(APIENTRY*)(GLenum,GLuint);
        using BindBuffer=void(APIENTRY*)(GLenum,GLuint);
        auto bind_framebuffer=reinterpret_cast<BindFramebuffer>(SDL_GL_GetProcAddress("glBindFramebuffer"));
        auto bind_buffer=reinterpret_cast<BindBuffer>(SDL_GL_GetProcAddress("glBindBuffer"));
        if(!bind_framebuffer||!bind_buffer) throw std::runtime_error("frame capture OpenGL entrypoints unavailable");
        GLint old_framebuffer,old_buffer,old_pack,old_pbo,old_row,old_rows,old_pixels;
        glGetIntegerv(0x8CAA,&old_framebuffer); glGetIntegerv(0x88ED,&old_pbo);
        bind_framebuffer(0x8CA8,0); bind_buffer(0x88EB,0);
        glGetIntegerv(GL_READ_BUFFER,&old_buffer); glGetIntegerv(GL_PACK_ALIGNMENT,&old_pack);
        glGetIntegerv(GL_PACK_ROW_LENGTH,&old_row); glGetIntegerv(GL_PACK_SKIP_ROWS,&old_rows); glGetIntegerv(GL_PACK_SKIP_PIXELS,&old_pixels);
        std::vector<unsigned char> pixels(size_t(width)*height*4);
        glReadBuffer(GL_BACK); glPixelStorei(GL_PACK_ALIGNMENT,1); glPixelStorei(GL_PACK_ROW_LENGTH,0); glPixelStorei(GL_PACK_SKIP_ROWS,0); glPixelStorei(GL_PACK_SKIP_PIXELS,0);
        glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        GLenum cause=glGetError(); glReadBuffer(old_buffer); glPixelStorei(GL_PACK_ALIGNMENT,old_pack);
        glPixelStorei(GL_PACK_ROW_LENGTH,old_row); glPixelStorei(GL_PACK_SKIP_ROWS,old_rows); glPixelStorei(GL_PACK_SKIP_PIXELS,old_pixels);
        bind_buffer(0x88EB,old_pbo); bind_framebuffer(0x8CA8,old_framebuffer);
        if(cause!=GL_NO_ERROR) throw std::runtime_error("glReadPixels failed: "+std::to_string(cause));
        for(size_t i=0;i<pixels.size();i+=4) std::swap(pixels[i],pixels[i+2]);
        BITMAPFILEHEADER file{}; file.bfType=0x4d42; file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER); file.bfSize=DWORD(file.bfOffBits+pixels.size());
        BITMAPINFOHEADER info{}; info.biSize=sizeof(info); info.biWidth=width; info.biHeight=height; info.biPlanes=1; info.biBitCount=32; info.biCompression=BI_RGB;
        auto path=std::filesystem::path(runtime_root)/"diagnostics"/("frame-"+pending_frame.at("id").get<std::string>()+".bmp");
        std::ofstream stream(path,std::ios::binary); stream.write(reinterpret_cast<const char*>(&file),sizeof(file)); stream.write(reinterpret_cast<const char*>(&info),sizeof(info)); stream.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()); stream.close();
        if(!stream) throw std::runtime_error("frame write failed: "+path.string());
        response["ok"]=true; response["result"]={{"path",path.string()},{"width",width},{"height",height},{"bytes",file.bfSize},{"pixel_sha256",sha256(pixels)}};
        LOG_INFO("[MCP] FRAME_CAPTURED path={} dimensions={}x{}",path.string(),width,height);
    } catch(const std::exception& error) { response["ok"]=false; response["error"]={{"code","FRAME_FAILED"},{"message",error.what()}}; LOG_ERROR("[MCP] Frame capture failed: {}",error.what()); }
    pending_frame=nullptr;
    try { respond(std::filesystem::path(runtime_root)/"diagnostics",response); }
    catch(const std::exception& error) { LOG_ERROR("[MCP] Frame response write failed: {}",error.what()); }
}
static void report_live_scope_entry(bool entered,const std::string& error) {
    static std::string last_error;
    static uint64_t failed_frames=0;
    if(entered) {
        if(failed_frames)LOG_INFO("[LiveState] GameUpdate entry recovered after {} failed frames; previous={}",failed_frames,last_error);
        failed_frames=0;last_error.clear();return;
    }
    if(!failed_frames||last_error!=error) {
        LOG_ERROR("[LiveState] GameUpdate entry failed: {}; previous repeated frames={}",error,failed_frames);
        failed_frames=0;last_error=error;
    }
    if(failed_frames!=UINT64_MAX)++failed_frames;
}

static void game_update(void* game,float dt) {
    FrameTimingScope measured_update(FrameTimingPart::Update);
    if(previous_frame_start!=std::chrono::steady_clock::time_point{})record_frame_timing(frame_intervals,uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(measured_update.started-previous_frame_start).count()));
    previous_frame_start=measured_update.started;
    game::initial_release::FixtureUpdateScope release_fixture_scope(game);
    game::developer_staff::FixtureUpdateScope developer_fixture_scope(game);
    game::tutorial_startingpoint_fixture::UpdateScope startingpoint_fixture_scope(game);
    bool live_scope=false;
    if(game::typed_action_runtime::live_state_active()) {std::string error;live_scope=game::live_host_state::economic_fence().enter_game_update(error);report_live_scope_entry(live_scope,error);}
    struct LiveScope {bool active;void close(){if(active){game::live_host_state::economic_fence().leave_game_update();active=false;}}~LiveScope(){close();}} scope{live_scope};
    {FrameTimingScope measured(FrameTimingPart::Original);original_game_update(game,dt);}
    game::time_input_diagnostics::frame(completed_game_updates+1);
    if(tutorial_movement_ready)game::tutorial_movement::frame(completed_game_updates+1);
    ++completed_game_updates;
    game::placement_ui_diagnostics::after_game_update(completed_game_updates,dt);
    if(!game::native::bind_game_thread()) return;
    engine_thread=GetCurrentThreadId();
    if(!native_lobby_attempted) {
        native_lobby_attempted=true;
        native_lobby_initialized=game::native_lobby::initialize(std::filesystem::path(Config::instance().game_dir)/"mt2mp_ui",reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),native_lobby_error);
        if(!native_lobby_initialized)LOG_ERROR("[NativeLobby] Initialization failed: {}",native_lobby_error);
    }
    if(native_lobby_initialized) {
        FrameTimingScope measured(FrameTimingPart::Lobby);
        std::string observed;
        if(!game::native_lobby::tick(observed)&&native_lobby_error!=observed){native_lobby_error=observed;LOG_ERROR("[NativeLobby] Native menu tick failed: {}",observed);}
    }
    if(owned_native_profile) {
        if(Config::instance().local_test||net::session_wire::connected())initialize_time_hooks();
        {FrameTimingScope measured(FrameTimingPart::Typed);game::typed_action_runtime::tick();}
        {FrameTimingScope measured(FrameTimingPart::Transport);
        if(Config::instance().local_test)net::tick_local_session();
        else {std::string wire_error;if(!net::session_wire::tick(wire_error)&&!wire_error.empty()&&!wire_error.starts_with("info:"))native_lobby_error=wire_error;}
        net::chat::Runtime::instance().tick();
        net::tick_snapshot_sync(); net::tick_diagnostic_placement(); net::tick_software_sync();}
        if(time_initialized) {
            FrameTimingScope measured(FrameTimingPart::Time);
            if(game::time_integration::instance().tick(time_tick_error)) { ++time_tick_confirmed; time_tick_error.clear(); }
            else if(time_tick_error=="snapshot_not_active") ++time_tick_waiting;
            else ++time_tick_failed;
        }
        {FrameTimingScope measured(FrameTimingPart::Hud);game::time_input_diagnostics::tick();
        game::hud_time_runtime::tick();
        game::hud_calendar_runtime::tick();
        game::native_free_diagnostics::drain_logs();}
        {FrameTimingScope measured(FrameTimingPart::Diagnostics);diagnostics_tick();}
        scope.close();
        std::string lobby_error;
        {FrameTimingScope measured(FrameTimingPart::Prepare);if(!game::lobby_prepare::tick(lobby_error))native_lobby_error=lobby_error;}
    }
}
static std::string env(const char* name) { char value[32768]; DWORD n=GetEnvironmentVariableA(name,value,sizeof(value)); return n&&n<sizeof(value)?std::string(value,n):""; }
static void init_paths() {
    auto& cfg=Config::instance();
    if(const auto* installed=installed_profile::get()){
        cfg.game_dir=installed->game_dir.string();cfg.user_data_dir=installed->user_data_dir.string();
        runtime_root=installed->runtime_root.string();run_id=installed->run_id;
        cfg.installed_profile_verified=true;cfg.diagnostic_mailbox_enabled=!installed_profile::diagnostic_root().empty();cfg.local_test=false;
        cfg.original_steam_dll=(installed->game_dir/L"steam_api64.dll").string();
        cfg.log_file=(installed->runtime_root/L"mt2mp.log").string();owned_native_profile=true;
    }else{
        char path[32768];const auto length=GetModuleFileNameA(nullptr,path,sizeof(path));
        if(!length||length>=sizeof(path))throw std::runtime_error("GetModuleFileName failed or truncated");
        cfg.game_dir=std::filesystem::path(path).parent_path().string();
        cfg.log_file=cfg.game_dir+"/mt2mp.log";cfg.original_steam_dll=cfg.game_dir+"/steam_api64_original.dll";
        cfg.user_data_dir=env("MT2MP_TEST_ROOT");cfg.local_test=!cfg.user_data_dir.empty();
        if(cfg.local_test){
            runtime_root=env("MT2MP_RUNTIME_ROOT");run_id=env("MT2MP_RUN_ID");
            cfg.peer_id=std::stoull(env("MT2MP_PEER_ID"));cfg.session_id=std::stoull(env("MT2MP_SESSION_ID"));
            cfg.session_token=env("MT2MP_SESSION_TOKEN");cfg.is_host=cfg.peer_id==1;
            auto control=std::stoul(env("MT2MP_CONTROL_PORT"));
            if(cfg.peer_id<1||cfg.peer_id>4||control<1||control>65535||runtime_root.empty()||run_id.empty())throw std::runtime_error("invalid local runtime configuration");
            cfg.sidecar_control_port=uint16_t(control);
        }
        owned_native_profile=cfg.local_test;cfg.diagnostic_mailbox_enabled=owned_native_profile;
        if(owned_native_profile&&env("MT2MP_TRANSPORT")=="steam")cfg.local_test=false;
    }
    cfg.runtime_root=runtime_root;cfg.run_id=run_id;
    game::lobby_operations::configure(run_id,diagnostic_executor_idle);
}
static void write_json(const std::filesystem::path& path,const json& data) {
    auto temporary=path; temporary+=".tmp";
    { std::ofstream stream(temporary,std::ios::binary|std::ios::trunc); if(!stream) throw std::runtime_error("open failed: "+temporary.string()); stream<<data.dump(2); stream.flush(); if(!stream) throw std::runtime_error("write failed: "+temporary.string()); }
    if(std::filesystem::exists(path)) {
        if(!ReplaceFileW(path.c_str(),temporary.c_str(),nullptr,0,nullptr,nullptr)) throw std::runtime_error("ReplaceFile failed target="+path.string()+" Win32="+std::to_string(GetLastError()));
    } else if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("MoveFile first publication failed target="+path.string()+" Win32="+std::to_string(GetLastError()));
}
static void respond(const std::filesystem::path& dir,const json& response) {
    if(game::lobby_operations::receive(response)) {last_request=response.at("id").get<std::string>();return;}
    deferred_response=response;
    last_request=response.at("id").get<std::string>();
    write_json(dir/"status.json",status());
    write_json(dir/"response.json",response);
    deferred_response=nullptr;
}
static json read_request(const std::filesystem::path& path) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) throw std::runtime_error("open request failed Win32="+std::to_string(GetLastError()));
    LARGE_INTEGER length{}; DWORD cause=0,read=0;
    if(!GetFileSizeEx(file,&length)) cause=GetLastError();
    if(cause||length.QuadPart<1||length.QuadPart>65536) { CloseHandle(file); throw std::runtime_error("request size invalid bytes="+std::to_string(length.QuadPart)+" Win32="+std::to_string(cause)); }
    std::string bytes(size_t(length.QuadPart),'\0');
    if(!ReadFile(file,bytes.data(),DWORD(bytes.size()),&read,nullptr)) cause=GetLastError();
    if(!CloseHandle(file)&&!cause) cause=GetLastError();
    if(cause||read!=bytes.size()) throw std::runtime_error("request read incomplete bytes="+std::to_string(read)+" required="+std::to_string(bytes.size())+" Win32="+std::to_string(cause));
    return json::parse(bytes);
}
static json status() {
    auto& cfg=Config::instance();
    using GetWriteDir=const char*(*)();
    auto physfs=GetModuleHandleA("libphysfs.dll");
    auto getter=physfs?reinterpret_cast<GetWriteDir>(GetProcAddress(physfs,"PHYSFS_getWriteDir")):nullptr;
    std::string write_dir=getter&&getter()?getter():"";
    game::native::WorldStatus world; std::string world_error;
    bool world_status_valid=game::native::read_world_status(world,world_error);
    auto capabilities=game::native::capabilities();
    const auto presents=hooks::render_present_snapshot();
    json result={{"schema_version",1},{"run_id",run_id},{"pid",GetCurrentProcessId()},{"frame_timing",frame_timing_status()},
        {"render_presents",{{"schema",1},{"unit","nanoseconds"},{"completed",presents.completed},
            {"sampled_monotonic_ns",presents.sampled_monotonic_ns},
            {"last_completed_monotonic_ns",presents.last_completed_monotonic_ns},
            {"window_identity",presents.window_identity}}},
        {"game_thread_ready",engine_thread!=0},{"game_thread_id",engine_thread},{"render_thread_id",render_thread},{"peer_id",cfg.peer_id},
        {"session_connected",net::session_wire::connected()},{"peer_count",net::session_wire::peers()},
        {"session_status",net::local_session_status()},{"write_dir",write_dir},
        {"snapshot_status",net::snapshot_sync_status()},
        {"snapshot_progress",net::snapshot_progress()},
        {"software_sync",net::software_sync_status()},
        {"placement_status",net::diagnostic_placement_status()},
        {"world_ready",false},{"blocker","Engine-Replikation noch nicht bewiesen"},
        {"overlay_visible",overlay::LobbyOverlay::instance().is_visible()},
        {"capabilities",{{"status",true},{"inspect_engine",true},{"capture_snapshot",capabilities.snapshot_request},{"set_time_scale",capabilities.clock},{"load_snapshot",capabilities.load_request},{"execute_action",capabilities.action_replay},{"probe_building_placement",capabilities.clock},{"place_test_building",capabilities.clock&&net::session_wire::connected()&&net::current_world_epoch()!=0},{"capture_frame",render_thread!=0},{"create_test_world",world_status_valid&&world.active_mode_index==2&&!world.title_transition_pending},{"dismiss_crash_report",world_status_valid&&world.crash_report_active&&!world.crash_report_dismiss_pending},{"toggle_overlay",render_thread!=0}}}};
    if(!installed_proof_receipt.is_null())result["installed_proof"]=installed_proof_receipt;
    result["capabilities"]["read_chat"]=true;
    result["capabilities"]["send_chat"]=net::chat::Runtime::instance().snapshot().connected;
    result["capabilities"]["read_chat_ui"]=render_thread!=0;
    result["capabilities"]["native_chat_ui_key"]=render_thread!=0;
    result["capabilities"]["native_chat_ui_text"]=render_thread!=0;
    result["in_game"]=game::native::in_game();
    result["native_free"]=game::native_free_diagnostics::status();
    result["native_remove"]=game::remove_diagnostics::status();
    result["typed_actions"]=game::typed_action_runtime::status();
    result["ui_remove"]=game::ui_remove_runtime::status();
    result["ui_build"]=game::ui_build_runtime::status();
    result["ui_move"]=game::ui_move_runtime::status();
    result["ui_region"]=game::ui_region_runtime::status();
    result["ui_path"]=game::ui_path_runtime::status();
    result["ui_network"]=game::ui_network_runtime::status();
    result["capabilities"]["enable_node_observer"]=game::ui_region_runtime::enabled();
    result["capabilities"]["read_node_observer"]=game::native::ready();
    result["capabilities"]["enable_ui_network"]=game::ui_region_runtime::enabled();
    result["capabilities"]["read_network_placement"]=game::ui_region_runtime::enabled();
    result["capabilities"]["native_network_node"]=game::ui_network_runtime::enabled();
    result["capabilities"]["native_network_pipe"]=game::ui_network_runtime::enabled();
    result["capabilities"]["enable_ui_path"]=game::ui_region_runtime::enabled();
    result["capabilities"]["read_path"]=game::ui_path_runtime::status().value("enabled",false);
    result["capabilities"]["native_path_delete"]=game::ui_path_runtime::status().value("enabled",false);
    result["capabilities"]["native_path_segment"]=game::ui_path_runtime::status().value("enabled",false);
    result["ui_terrain"]=game::ui_terrain_runtime::status();
    result["move_ui_probe_unresolved"]=game::move_ui_probe::unresolved();
    result["native_tool_prepare_unresolved"]=game::native_tool_prepare::unresolved();
                result["build_ui_probe_unresolved"]=game::placement_ui_diagnostics::unresolved();
    result["capabilities"]["read_native_free"]=engine_thread!=0;
    result["capabilities"]["probe_native_holder"]=world_status_valid&&world.active_mode_index==3&&world.world_present&&!world.quitting;
    result["capabilities"]["read_remove_bindings"]=world_status_valid&&world.active_mode_index==3&&world.world_present&&!world.quitting;
    result["capabilities"]["probe_remove_snapshot_building"]=result["capabilities"]["read_remove_bindings"];
    result["capabilities"]["retire_snapshot_remove"]=engine_thread!=0;
    result["capabilities"]["start_typed_actions"]=result["capabilities"]["read_remove_bindings"].get<bool>()&&net::session_wire::connected();
    result["capabilities"]["request_snapshot_remove"]=result["capabilities"]["start_typed_actions"];
    result["capabilities"]["enable_ui_remove"]=result["capabilities"]["start_typed_actions"];
    result["capabilities"]["enable_ui_build"]=result["capabilities"]["start_typed_actions"];
    result["capabilities"]["request_building_place"]=game::ui_build_runtime::enabled();
    result["capabilities"]["enable_ui_move"]=result["capabilities"]["start_typed_actions"];
    result["capabilities"]["enable_ui_terrain"]=game::ui_region_runtime::enabled();
    result["capabilities"]["read_terrain"]=game::ui_region_runtime::enabled();
    result["capabilities"]["native_terrain_height"]=game::ui_terrain_runtime::status().value("enabled",false);
    result["capabilities"]["native_terrain_stroke"]=game::ui_terrain_runtime::status().value("enabled",false);
    result["capabilities"]["enable_ui_region"]=result["capabilities"]["start_typed_actions"];
    result["capabilities"]["read_region_preset"]=game::ui_region_runtime::enabled();
    result["capabilities"]["read_npc_region"]=cfg.local_test&&game::native::in_game();
    result["capabilities"]["native_npc_tool_ui"]=cfg.local_test&&game::native::in_game();
    result["capabilities"]["native_monster_zone_ui"]=cfg.local_test&&game::native::in_game();
    result["capabilities"]["native_monster_zone_probe"]=cfg.local_test&&cfg.is_host&&game::native::in_game();
    result["capabilities"]["native_npc_place_probe"]=cfg.local_test&&game::native::in_game();
    result["capabilities"]["native_npc_place_ui"]=cfg.local_test&&game::native::in_game()&&
        game::npc_probe::action_runtime::status().value("enabled",false);
    result["capabilities"]["native_npc_remove_ui"]=
        result["capabilities"]["native_npc_place_ui"].get<bool>()&&game::ui_remove_runtime::enabled();
    result["capabilities"]["native_npc_move_ui"]=
        result["capabilities"]["native_npc_place_ui"].get<bool>()&&game::ui_move_runtime::enabled();
    result["capabilities"]["read_prices"]=game::typed_action_runtime::status().value("started",false);
      result["capabilities"]["native_price_ui"]=game::ui_price_runtime::status().value("enabled",false);
    result["capabilities"]["native_region_styles_ui"]=game::ui_region_runtime::enabled();
    result["capabilities"]["native_region_preset_ui"]=game::ui_region_runtime::enabled();
    result["capabilities"]["native_region_property_ui"]=game::ui_region_runtime::enabled();
    std::string live_admission;
    result["capabilities"]["enable_live_host_state"]=game::typed_action_runtime::live_state_available(live_admission);
    result["capabilities"]["read_live_host_state"]=game::typed_action_runtime::status().value("initialized",false);
    result["live_state_admission"]=live_admission;
    result["capabilities"]["request_building_move"]=game::ui_move_runtime::enabled();
    result["capabilities"]["native_move_ui"]=game::ui_move_runtime::enabled();
    result["capabilities"]["read_move_ui"]=true;
    result["capabilities"]["native_cancel_tool"]=game::typed_action_runtime::status().value("started",false);
    result["capabilities"]["read_cancel_tool"]=true;
    result["capabilities"]["native_buildings_tab"]=game::typed_action_runtime::status().value("started",false);
    result["capabilities"]["read_buildings_tab"]=true;
                result["capabilities"]["native_build_button"]=game::ui_build_runtime::enabled();
                result["capabilities"]["read_build_ui"]=true;
                result["capabilities"]["native_building_history_ui"]=game::ui_build_runtime::enabled();
                result["capabilities"]["read_building_history_ui"]=true;
    result["capabilities"]["probe_remove_ui"]=game::ui_remove_runtime::enabled();
    result["shared_time"]={{"initialization_attempted",time_initialization_attempted},{"initialized",time_initialized},
        {"hooks_ready",time_hooks_ready},{"initialization_error",time_initialization_error},
        {"tick_confirmed",time_tick_confirmed},{"tick_failed",time_tick_failed},{"tick_waiting",time_tick_waiting},{"tick_error",time_tick_error},
        {"activate_attempted",time_activate_attempted},{"activate_matched",time_activate_matched},
        {"activate_suppressed",time_activate_suppressed},{"activate_original",time_activate_original},{"update_attempted",time_update_attempted},
        {"update_suppressed",time_update_suppressed},{"last_caller_rva",time_last_caller_rva},
        {"last_intent",static_cast<int>(time_last_intent)},{"last_request_queued",time_last_request_queued}};
    result["hud_time_writer"]=game::hud_time_runtime::status_json();
    const auto& mouse=hud_time_activation.status();
    result["hud_time_activation"]={{"initialized",mouse.initialized},{"attempted",mouse.attempted},{"matched",mouse.matched},
        {"submitted",mouse.submitted},{"rejected",mouse.rejected},{"last_sequence",mouse.last_sequence},
        {"last_epoch",mouse.last_epoch},{"error",mouse.error},{"error_repeats",time_mouse_error_repeats}};
    result["hud_calendar"]=game::hud_calendar_runtime::status_json();
    if(time_initialized) {
        game::time_integration::Status time; std::string error;
        const bool okay=game::time_integration::instance().status(time,error);
        const auto& runtime=time.runtime; const auto& controller=runtime.controller;
        result["shared_time"]["status_read_confirmed"]=okay;
        result["shared_time"]["status_error"]=error;
        result["shared_time"]["runtime"]={{"phase",static_cast<int>(runtime.phase)},{"epoch",runtime.epoch},
            {"hook_mask",runtime.hook_mask},{"pending_outbound",runtime.pending_outbound},
            {"presentation_uncertain",runtime.presentation_uncertain},{"sends_accepted",runtime.sends_accepted},
            {"sends_failed",runtime.sends_failed},{"native_set_attempted",runtime.native_set_attempted},
            {"native_set_confirmed",runtime.native_set_confirmed},{"native_set_failed",runtime.native_set_failed},
            {"item_stores_attempted",runtime.item_stores_attempted},{"item_stores_completed",runtime.item_stores_completed},
            {"cancelled_intents",runtime.cancelled_intents},{"last_operation",runtime.last_operation},{"last_error",runtime.last_error},
            {"controller",{{"started",controller.started},{"write_locked",controller.write_locked},
                {"attempted",controller.attempted},{"confirmed",controller.confirmed},{"failed",controller.failed},
                {"duplicates",controller.duplicates},{"processed_sequences",controller.processed_sequences},
                {"presentation_acks",controller.presentation_acks},{"last_code",static_cast<int>(controller.last_code)},
                {"last_cause",controller.last_cause},{"observed",{{"present",controller.observed.present},
                    {"uncertain",controller.observed.uncertain},{"revision",controller.observed.revision},
                    {"selected",controller.observed.clock.selected},{"pause",controller.observed.clock.pause}}},
                {"applied",{{"present",controller.applied.present},{"uncertain",controller.applied.uncertain},
                    {"revision",controller.applied.revision},{"selected",controller.applied.clock.selected},{"pause",controller.applied.clock.pause}}}}}};
    }
    result["load_clock_pause"]={{"attempted",load_clock_attempted},{"confirmed",load_clock_confirmed},{"changed",load_clock_changed},
        {"failed",load_clock_failed},{"request_id",load_clock_key},{"error",load_clock_error},{"original_policy","always_forward"}};
    result["capabilities"]["read_native_lobby"]=owned_native_profile;
    result["capabilities"]["read_native_loans"]=owned_native_profile;
    result["capabilities"]["read_native_release"]=owned_native_profile;
    result["capabilities"]["read_native_bug_fix_ui"]=owned_native_profile && Config::instance().local_test;
    result["capabilities"]["native_bug_fix_ui"]=owned_native_profile && Config::instance().local_test;
    result["capabilities"]["read_bug_c_policy"]=owned_native_profile && Config::instance().local_test && Config::instance().is_host;
    result["capabilities"]["native_bug_c_policy"]=owned_native_profile && Config::instance().local_test && Config::instance().is_host;
    result["capabilities"]["read_native_developers"]=owned_native_profile;
    result["capabilities"]["native_developer_ui"]=owned_native_profile && Config::instance().local_test;
    result["capabilities"]["read_native_developer_retirement"]=owned_native_profile && Config::instance().local_test && Config::instance().is_host;
    result["capabilities"]["native_developer_retirement_fixture"]=owned_native_profile && Config::instance().local_test && Config::instance().is_host;
    result["capabilities"]["native_developer_fixture"]=owned_native_profile && Config::instance().local_test && Config::instance().is_host;
    result["capabilities"]["native_initial_release_fixture"]=owned_native_profile;
    result["capabilities"]["native_take_loan_fixture"]=owned_native_profile;
    result["capabilities"]["native_loan_payment"]=owned_native_profile;
    result["capabilities"]["native_lobby_button"]=native_lobby_initialized;
    result["native_lobby"]={{"initialized",native_lobby_initialized},{"error",native_lobby_error},{"prepare",game::lobby_prepare::status()}};
    result["local_snapshot_load"]={{"pending",!pending_load.is_null()},{"completed",load_watch.completed},
        {"timeout_seconds",net::snapshot_native_load_timeout_seconds},{"elapsed_ms",load_watch.elapsed_ms()},
        {"progress_idle_ms",load_watch.progress_idle_ms()},{"progress_events",load_watch.progress_events},
        {"active_mode",load_watch.active_mode},{"requested_mode",load_watch.requested_mode},{"registrations",load_watch.registrations}};
    result["local_snapshot_capture"]={{"pending",!pending_snapshot.is_null()},
        {"timeout_seconds",net::snapshot_native_capture_timeout_seconds},
        {"elapsed_ms",pending_snapshot.is_null()?int64_t(0):std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-snapshot_started).count()},
        {"file_hash_cached",pending_snapshot.is_object()&&pending_snapshot.contains("sha256")}};
    game::native::DayTickState day_ticks;
    game::native::read_day_tick_state(day_ticks);
    result["day_ticks"]={{"resolver_ready",day_ticks.resolver_ready},{"thread_bound",day_ticks.thread_bound},
        {"invalid_observe_calls",day_ticks.invalid_observe_calls},{"invalid_forward_calls",day_ticks.invalid_forward_calls},
        {"policy","observe_and_forward"},{"ticks",json::array()}};
    for(size_t i=0;i<day_ticks.ticks.size();++i) {
        const auto& tick=day_ticks.ticks[i];
        json guards=json::object();
        for(size_t j=0;j<tick.guards.size();++j)
            guards[game::native::day_tick_guard_name(static_cast<game::native::DayTickGuard>(j))]=tick.guards[j];
        result["day_ticks"]["ticks"].push_back({{"leaf",i==0?"business_finances":"subscriber_new_day"},
            {"entered",tick.entered},{"forwarded",tick.forwarded},{"accepted",tick.accepted},
            {"receiver_guard_rejected",tick.receiver_guard_rejected},{"wrong_thread",tick.wrong_thread},
            {"deferred_logs",tick.deferred_logs},{"first_rejection",game::native::day_tick_guard_name(tick.first_rejection)},
            {"last_guard",game::native::day_tick_guard_name(tick.last_guard)},
            {"last_valid_day",tick.last_valid_day},{"last_valid_subscriber_id",tick.last_valid_subscriber_id},{"guards",guards}});
    }
    auto identity=game::normal_load_identity::status();
    result["load_identity"]={{"state",identity.state},{"snapshot_sha256",identity.snapshot_sha256},{"snapshot_id",identity.snapshot_uuid},{"error",identity.error},
        {"registrations",identity.registrations},{"references",identity.reference_count},{"contexts",identity.context_count},{"rejected_buildings",identity.rejected_buildings},
        {"registration_checks",identity.registration_checks},{"building_registrations",identity.building_registrations},{"nonbuilding_registrations",identity.nonbuilding_registrations},
        {"building_replacements",identity.building_replacements},{"building_drops",identity.building_drops},
        {"peak_references",identity.peak_reference_count},{"peak_contexts",identity.peak_context_count},
        {"reference_limit",identity.reference_limit},{"context_limit",identity.context_limit},{"buildings_only",identity.buildings_only},{"all_references_retained",identity.all_references_retained},
        {"observer_sample_count",identity.observer_sample_count},{"observer_sample_total_ns",identity.observer_sample_total_ns},
        {"observer_sample_max_ns",identity.observer_sample_max_ns},{"observer_sample_stride",identity.observer_sample_stride},
        {"negative_cache_hits",identity.negative_cache_hits},{"negative_cache_misses",identity.negative_cache_misses},
        {"classification_policy",identity.classification_policy},{"negative_cache_enabled",identity.negative_cache_enabled},
        {"rtti_fallbacks",identity.rtti_fallbacks},{"rtti_fallback_reason",identity.rtti_fallback_reason},
        {"negative_cache_invalidations",identity.negative_cache_invalidations},{"negative_cache_entries",identity.negative_cache_entries},
        {"negative_cache_peak_entries",identity.negative_cache_peak_entries},{"negative_cache_limit",identity.negative_cache_limit},
        {"negative_cache_bytes",identity.negative_cache_bytes},{"negative_cache_byte_limit",identity.negative_cache_byte_limit},
        {"failed_rtti_memo_hits",identity.failed_rtti_memo_hits},{"failed_rtti_memo_overflows",identity.failed_rtti_memo_overflows},
        {"failed_rtti_memo_entries",identity.failed_rtti_memo_entries},{"failed_rtti_memo_peak_entries",identity.failed_rtti_memo_peak_entries},
        {"failed_rtti_memo_limit",identity.failed_rtti_memo_limit},{"rtti_graph_attempts",identity.rtti_graph_attempts},
        {"scenery_supported",identity.scenery_supported},{"characters_supported",identity.characters_supported},{"building_count",identity.buildings.size()},{"buildings",json::array()}};
    for(size_t i=0;i<identity.buildings.size()&&i<32;++i) {
        const auto& binding=identity.buildings[i];
        result["load_identity"]["buildings"].push_back({{"link_id",binding.link_id},{"local_uid",binding.local_uid},{"type",binding.building.type},{"variant",binding.building.variant},
            {"x",binding.building.prop.x},{"y",binding.building.prop.y},{"z",binding.building.prop.z},
            {"dungeon_membership",binding.building.dungeon_membership},{"dungeon_instance_membership",binding.building.dungeon_instance_membership},{"exterior_uid",binding.building.exterior_uid}});
    }
    size_t dungeon_buildings=0;
    for(const auto& binding:identity.buildings) if(binding.building.dungeon_membership) ++dungeon_buildings;
    result["load_identity"]["dungeon_building_count"]=dungeon_buildings;
    size_t instance_buildings=0;
    for(const auto& binding:identity.buildings) if(binding.building.dungeon_instance_membership) ++instance_buildings;
    result["load_identity"]["dungeon_instance_building_count"]=instance_buildings;
    auto capture=game::building_capture_identity::status();
    result["network_identity"]=game::network_identity::status_json(16);
    result["building_capture"]={{"state",capture.state},{"scope",capture.scope},{"uuid",capture.capture_uuid},
        {"sha256",capture.snapshot_sha256},{"session_epoch",capture.session_epoch},{"bytes",capture.snapshot_bytes},
        {"game_thread_id",capture.game_thread_id},{"observed_thread_ids",capture.observed_thread_ids},{"readback_calls",capture.readback_calls},
        {"world_witness_bound",capture.world_witness_bound},{"world_witness_current",capture.world_witness_current},{"world_witness_checks",capture.world_witness_checks},
        {"original_attempted",building_original_attempted.load()-building_capture_original_start},
        {"original_returned",building_original_returned.load()-building_capture_return_start},
        {"original_failed",building_original_failed.load()-building_capture_failure_start},{"original_last_thread",building_original_thread.load()},
        {"file_confirmed",capture.file_confirmed},{"bindings_confirmed",capture.bindings_confirmed},{"error",capture.error},
        {"wrapper_calls",capture.wrapper_calls},{"wrapper_returns",capture.wrapper_returns},{"inflight",capture.inflight_wrappers},
        {"arrays_verified",capture.arrays_verified},{"unique_main_arrays",capture.unique_main_arrays},{"contexts",capture.contexts_seen},
        {"native_array_buildings",capture.native_array_buildings},{"staged_buildings",capture.staged_buildings},
        {"readbacks_confirmed",capture.readbacks_confirmed},{"final_readbacks_checked",capture.final_readbacks_checked},
        {"foreign_writers_skipped",capture.foreign_writers_skipped},{"root_buildings",capture.root_buildings},
        {"dungeon_buildings",capture.dungeon_buildings},{"dungeon_instance_buildings",capture.dungeon_instance_buildings},
        {"full_coop_proven",false},{"samples",json::array()}};
    for(size_t i=0;i<capture.buildings.size()&&i<16;++i) {
        const auto& binding=capture.buildings[i];
        result["building_capture"]["samples"].push_back({{"link_id",binding.link_id},{"local_uid",binding.local_uid},{"key",binding.record_key},
            {"type",binding.building.type},{"variant",binding.building.variant}});
    }
    result["capabilities"]["purchase_test_region"]=cfg.is_host&&capabilities.clock;
    result["capabilities"]["focus_test_camera"]=capabilities.clock;
    result["capabilities"]["read_subscriber_needs"]=game::native::in_game();
    result["capabilities"]["read_subscriber_weapon"]=game::native::in_game();
    result["capabilities"]["capture_subscriber_record"]=Config::instance().local_test && Config::instance().is_host && game::native::in_game();
    result["capabilities"]["append_subscriber_record_probe"]=Config::instance().local_test && !Config::instance().is_host && game::native::in_game();
    result["capabilities"]["read_subscriber_candidates"]=game::native::in_game();
    result["capabilities"]["read_network_state"]=game::native::in_game();
    result["capabilities"]["read_tutorial"]=owned_native_profile&&game::native::in_game();
    result["capabilities"]["read_tutorial_development"]=owned_native_profile&&game::native::in_game();
    result["capabilities"]["tutorial_startingpoint"]=owned_native_profile&&cfg.local_test&&game::native::in_game();
    result["capabilities"]["read_tutorial_progress"]=owned_native_profile&&game::native::in_game();
    result["capabilities"]["tutorial_movement"]=tutorial_movement_ready&&cfg.is_host&&game::native::in_game();
    result["capabilities"]["read_tutorial_movement"]=tutorial_movement_ready;
    result["capabilities"]["tutorial_movement_focus"]=tutorial_movement_ready&&cfg.is_host&&game::native::in_game();
    result["capabilities"]["read_tutorial_movement_focus"]=tutorial_movement_ready&&cfg.is_host&&game::native::in_game();
    result["capabilities"]["tutorial_welcome_ok"]=owned_native_profile&&cfg.is_host&&game::native::in_game();
    result["capabilities"]["tutorial_leveltwo_message_ok"]=owned_native_profile&&cfg.is_host&&game::native::in_game();
    result["capabilities"]["tutorial_finish_ok"]=owned_native_profile&&cfg.is_host&&game::native::in_game();
    result["capabilities"]["tutorial_network_tab"]=owned_native_profile&&cfg.is_host&&game::native::in_game();
    result["capabilities"]["read_time_ui"]=owned_native_profile&&time_hooks_ready&&game::native::in_game();
    const bool native_time_input_ready=owned_native_profile&&time_hooks_ready&&game::time_input_diagnostics::ready()&&game::native::in_game();
    result["capabilities"]["read_time_acceptance"]=native_time_input_ready;
    result["capabilities"]["native_time_key"]=native_time_input_ready;
    result["capabilities"]["read_time_input"]=native_time_input_ready;
    result["capabilities"]["native_time_button"]=native_time_input_ready;
    result["capabilities"]["read_time_mouse"]=native_time_input_ready;
    result["capabilities"]["read_building_catalog"]=cfg.local_test&&game::native::in_game();
    bool needs_diagnostic=cfg.local_test&&!cfg.is_host&&game::native::in_game()&&net::session_wire::connected();
    result["capabilities"]["arm_subscriber_needs"]=needs_diagnostic;
    result["capabilities"]["apply_subscriber_needs"]=needs_diagnostic;
    result["capabilities"]["disarm_subscriber_needs"]=cfg.local_test&&!cfg.is_host;
    game::native::SubscriberNeedsReplicaState needs_replica; std::string needs_replica_error;
    if(game::native::read_subscriber_needs_replica_state(needs_replica,needs_replica_error)) {
        result["subscriber_needs_replica"]={{"policy","selected_subscriber_only"},{"armed",needs_replica.armed},{"bound",needs_replica.bound},
            {"logged_in",needs_replica.logged_in},{"epoch",needs_replica.epoch},{"revision",needs_replica.applied_revision},{"serialized_id",needs_replica.serialized_id},
            {"installed_hook_mask",needs_replica.installed_hook_mask},{"initial",needs_replica.initial},{"applied",needs_replica.applied},{"error",needs_replica.error},
            {"writes",json::array()},{"ticks",json::array()}};
        const char* write_names[]={"set","increment","add_assign","subtract_assign"};
        for(size_t i=0;i<needs_replica.writes.size();++i) {
            const auto& c=needs_replica.writes[i];
            result["subscriber_needs_replica"]["writes"].push_back({{"leaf",write_names[i]},{"entered",c.entered},{"forwarded",c.forwarded},
                {"selected",c.selected},{"suppressed",c.suppressed},{"permitted",c.permitted},{"guard_rejected",c.guard_rejected},{"wrong_thread",c.wrong_thread}});
        }
        for(size_t i=0;i<needs_replica.ticks.size();++i) {
            const auto& c=needs_replica.ticks[i];
            result["subscriber_needs_replica"]["ticks"].push_back({{"leaf",i==0?"brain_tick":"update_needs"},{"entered",c.entered},{"forwarded",c.forwarded},{"guard_rejected",c.guard_rejected}});
        }
    } else result["subscriber_needs_replica_error"]=needs_replica_error;
    result["capabilities"]["exit_game"]=world_status_valid&&world.active_mode_index==2&&!world.title_transition_pending&&world.saves_in_progress<=0;
    result["capabilities"]["capture_local_snapshot"]=capabilities.snapshot_request;
    auto window=hooks::get_game_window();
    if(window) {
        int width=0,height=0;
        if(SDL_GetWindowSizeInPixels(window,&width,&height)) result["window"]={{"title",SDL_GetWindowTitle(window)},{"width",width},{"height",height},{"fullscreen",bool(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)},{"relative_mouse",SDL_GetWindowRelativeMouseMode(window)}};
        else result["window_error"]=SDL_GetError();
    }
    if(world_status_valid) result["native_world"]={{"active_mode",world.active_mode_index},{"requested_mode",world.requested_mode_index},{"mode_state",world.in_game_state},{"world_present",world.world_present},{"quitting",world.quitting},{"title_pending",world.title_transition_pending},{"crash_report_active",world.crash_report_active},{"crash_report_dismiss_pending",world.crash_report_dismiss_pending},{"map_task_pending",world.map_task_pending},{"map_dirty",world.map_dirty},{"map_regions",world.map_regions},{"edits",world.edit_operations},{"saves",world.saves_in_progress}};
    else result["world_error"]=world_error;
    if(result["in_game"].get<bool>()) {
        std::vector<game::native::WorldPropState> props; std::string inventory_error;
        if(game::native::list_world_props(32,props,inventory_error)) {
            result["props"]=json::array(); for(const auto& object:props) {
                const auto& prop=object.prop;
                result["props"].push_back({{"uid",prop.uid},{"x",prop.x},{"y",prop.y},{"z",prop.z},{"region_owned",object.region_active},{"building",object.building},{"type",object.type},{"variant",object.variant}});
            }
        } else result["inventory_error"]=inventory_error;
        game::native::ClockState clock; std::string error;
        if(game::native::read_clock(clock,error)) result["clock"]={{"scale",clock.time_scale},{"pause_scale",clock.pause_scale},{"day",clock.flat_day},{"seconds",clock.seconds},{"paused",clock.paused}};
        else result["clock_error"]=error;
        game::native::SimulationState simulation;
        if(game::native::read_simulation_state(simulation,error)) result["simulation"]={{"cash",simulation.cash},{"record_present",simulation.record_present},{"record_day",simulation.record_day},{"record_cash",simulation.record_cash},{"record_income",simulation.record_income},{"record_expense",simulation.record_expense},{"logged_in_subscribers",simulation.logged_in_subscribers},{"line_counts",simulation.line_counts},{"line_income",simulation.line_income}};
        else result["simulation_error"]=error;
        game::native::SoftwareState software;
        if(game::native::read_software_state(software,error)) {
            result["software"]={{"rating",software.rating},{"fun",software.fun},{"policy",software.replica_armed?"replicate_rating_fun_only":"observe_and_forward"},{"ticks",json::array()},
                {"replica_armed",software.replica_armed},{"replica_bound",software.replica_bound},{"bound_epoch",software.bound_epoch},{"applied_revision",software.applied_revision},
                {"initial_rating",software.initial_rating},{"initial_fun",software.initial_fun},{"replica_error",software.replica_error},{"calculations",json::array()}};
            for(const auto& leaf:software.calculations) result["software"]["calculations"].push_back({{"entered",leaf.entered},{"forwarded",leaf.forwarded},{"suppressed",leaf.suppressed},{"guard_rejected",leaf.guard_rejected}});
            for(const auto& tick:software.ticks) {
                json guards=json::object();
                for(size_t i=0;i<tick.guards.size();++i) guards[game::native::software_tick_guard_name(static_cast<game::native::SoftwareTickGuard>(i))]=tick.guards[i];
                result["software"]["ticks"].push_back({{"entered",tick.entered},{"forwarded",tick.forwarded},{"guards",guards}});
            }
        } else result["software_error"]=error;
        game::native::CameraState camera;
        if(game::native::read_camera_state(camera,error)) result["camera"]={{"x",camera.x},{"y",camera.y},{"z",camera.z},{"focus_x",camera.focus_x},{"focus_y",camera.focus_y},{"focus_z",camera.focus_z},{"distance",camera.distance},{"minimum_distance",camera.minimum_distance},{"maximum_distance",camera.maximum_distance},{"edit_weight",camera.edit_weight},{"focus_pending",camera.focus_pending},{"target_x",camera.target_x},{"target_y",camera.target_y},{"target_z",camera.target_z},{"target_distance",camera.target_distance},{"zoom_index",camera.zoom_index},{"target_zoom_index",camera.target_zoom_index},{"desired_distance",camera.desired_distance},{"zoom_table_value",camera.zoom_table_value},{"zoom_lower_neighbor",camera.zoom_lower_neighbor},{"representable_target",camera.representable_target}};
        else result["camera_error"]=error;
    }
    if(cfg.local_test) {
        auto opened=game::native_open_runtime::status_json();
        opened["fresh_current_world_attempted"]=false;
        opened["fresh_current_world_readback"]=false;
        opened["fresh_current_world_error"]="info: current loaded world unavailable";
        if(world_status_valid&&world.in_game_state==4&&!world.quitting&&
           opened.value("current_origin_available",false)&&opened.value("game_thread",uint32_t(0))==GetCurrentThreadId()) {
            game::building_mixed_identity::opened_file::CurrentWorld current;
            std::string error;
            opened["fresh_current_world_attempted"]=true;
            const bool confirmed=game::native_open_runtime::read_current_world(current,error);
            opened["fresh_current_world_readback"]=confirmed;
            opened["fresh_current_world_error"]=error;
            if(!confirmed)opened["current_origin_available"]=false;
        }
        result["native_open_origin"]=std::move(opened);
    }
    return result;
}
static json chat_status() {
    const auto snapshot=net::chat::Runtime::instance().snapshot();
    json entries=json::array();
    for(const auto& entry:snapshot.entries) {
        const char* delivery="queued";
        if(entry.delivery==net::chat::Delivery::Confirmed)delivery="confirmed";
        else if(entry.delivery==net::chat::Delivery::Failed)delivery="failed";
        else if(entry.delivery==net::chat::Delivery::Received)delivery="received";
        entries.push_back({{"origin",entry.origin},{"sequence",entry.sequence},{"text",entry.text},
            {"delivery",delivery},{"expected_peer_mask",entry.expected_mask},{"ack_peer_mask",entry.ack_mask}});
    }
    return {{"connected",snapshot.connected},{"session",snapshot.session},{"local_peer",snapshot.local_peer},{"entries",entries}};
}
static json chat_ui_status() {
    const auto ui=overlay::ChatOverlay::instance().ui_snapshot();
    const auto input=hooks::read_chat_input_state();
    SDL_Window* window=hooks::get_game_window();
    return {{"visible",ui.visible},{"typing",ui.typing},{"rendered_visible",ui.rendered_visible},
        {"input_active",ui.input_active},{"rendered_frames",ui.rendered_frames},{"draft",ui.draft},
        {"sdl_text_input_active",window&&SDL_TextInputActive(window)},
        {"relative_mouse_mode",window&&SDL_GetWindowRelativeMouseMode(window)},
        {"hook_overlay_active",input.overlay_active},{"hook_text_input_requested",input.text_input_requested},
        {"f9_consumed",input.f9_consumed},{"escape_consumed",input.escape_consumed},
        {"enter_captured",input.enter_captured},{"text_captured",input.text_captured},
        {"key_forwarded",input.key_forwarded},{"probe_forwarded",input.probe_forwarded},
        {"text_pending",input.text_pending}};
}
static void diagnostics_tick() {
    if(runtime_root.empty()) return;
    static auto last=std::chrono::steady_clock::time_point{};
    auto now=std::chrono::steady_clock::now();
    if(now-last<std::chrono::milliseconds(100)) return; last=now;
    std::string request_in_progress;
    bool snapshot_observer_begun=false;
    bool snapshot_network_observer_begun=false;
    try {
        struct Recovery {
            int exceptions=std::uncaught_exceptions();
            ~Recovery() {
                if(std::uncaught_exceptions()==exceptions&&!diagnostic_failure.empty()) {
                    LOG_INFO("[MCP] Diagnostics recovered after {} failures: {}",diagnostic_failure_count,diagnostic_failure);
                    diagnostic_failure.clear(); diagnostic_failure_count=0;
                }
            }
        } recovery;
        auto dir=std::filesystem::path(runtime_root)/"diagnostics";
        static auto last_status=std::chrono::steady_clock::time_point{};
        if(Config::instance().diagnostic_mailbox_enabled&&now-last_status>=std::chrono::seconds(1)) { write_json(dir/"status.json",status()); last_status=now; }
        if(!deferred_response.is_null()) { write_json(dir/"response.json",deferred_response); deferred_response=nullptr; return; }
        if(!pending_frame.is_null()) return;
        if(!pending_camera.is_null()) {
            game::native::CameraState camera; std::string error;
            bool valid=game::native::read_camera_state(camera,error);
            const float tolerance=std::max(0.001f,std::abs(camera.desired_distance)*4*std::numeric_limits<float>::epsilon());
            const float requested=pending_camera.at("distance").get<float>();
            const float lower=std::min(requested,camera.representable_target),upper=std::max(requested,camera.representable_target);
            bool settled=valid&&camera.focus_target_available&&!camera.focus_pending&&camera.focus_end_blend==1&&camera.focus_blend==camera.focus_end_blend&&camera.target_distance==requested&&camera.zoom_index==camera.target_zoom_index&&camera.desired_distance==camera.representable_target&&std::abs(camera.focus_x-pending_camera.at("x").get<float>())<0.1f&&std::abs(camera.focus_y-pending_camera.at("y").get<float>())<0.1f&&std::abs(camera.focus_z-pending_camera.at("z").get<float>())<0.1f&&camera.distance>=lower-tolerance&&camera.distance<=upper+tolerance;
            bool timeout=now-camera_started>std::chrono::seconds(15);
            if(!valid||settled||timeout) {
                json response={{"schema_version",1},{"id",pending_camera.at("id")},{"run_id",run_id},{"ok",settled}};
                if(settled) response["result"]={{"state","camera_focus_applied"},{"engine",status()}};
                else response["error"]={{"code",timeout?"CAMERA_FOCUS_TIMEOUT":"CAMERA_REJECTED"},{"message",timeout?"Kamerafokus nach 15 Sekunden nicht bestätigt: actual="+std::to_string(camera.distance)+" required_interval=["+std::to_string(lower)+","+std::to_string(upper)+"] blend="+std::to_string(camera.focus_blend)+" required_blend="+std::to_string(camera.focus_end_blend)+" pending="+std::to_string(camera.focus_pending)+" tolerance="+std::to_string(tolerance):error}};
                pending_camera=nullptr; respond(dir,response);
            }
            return;
        }
        if(!pending_load.is_null()) {
            bool active=game::native::in_game();
            game::native::WorldStatus world; std::string world_error;
            if(!game::native::read_world_status(world,world_error)) throw std::runtime_error("local snapshot load observation failed: "+world_error);
            auto observation=game::normal_load_identity::status();
            load_watch.observe(world.active_mode_index,world.requested_mode_index,observation.registrations,now);
            bool timeout=load_watch.expired(now);
            if(timeout) active=false;
            if(active||timeout) {
                const std::string timeout_cause=timeout?load_watch.timeout_cause(now):std::string{};
                json response={{"schema_version",1},{"id",pending_load.at("id")},{"run_id",run_id},{"ok",active}};
                if(active) {
                    bool origin_pending=false; std::string origin_error;
                    if(!game::native_open_runtime::finish(pending_load.at("origin_generation").get<uint64_t>(),origin_pending,origin_error)) {
                        response["ok"]=false;
                        response["error"]={{"code","SNAPSHOT_ORIGIN_FAILED"},{"message",origin_error}};
                        game::normal_load_identity::cancel(origin_error);
                        pending_load=nullptr; respond(dir,response); return;
                    }
                    if(origin_pending) return;
                    if(load_clock_key==pending_load.at("id").get<std::string>()&&!load_clock_error.empty()) {
                        response["ok"]=false;
                        response["error"]={{"code","SNAPSHOT_LOAD_PAUSE_FAILED"},{"message",load_clock_error}};
                        game::normal_load_identity::cancel(load_clock_error);
                        pending_load=nullptr; respond(dir,response); return;
                    }
                    game::normal_load_identity::Status identity; std::string identity_error;
                    if(!game::normal_load_identity::finalize(identity,identity_error)) LOG_WARN("[MCP] Loaded world identity observation incomplete snapshot={}: {}",pending_load.at("snapshot_id").get<std::string>(),identity_error);
                    response["result"]={{"state","snapshot_world_active"},{"snapshot_id",pending_load.at("snapshot_id")},{"engine",status()}};
                }
                else response["error"]={{"code","SNAPSHOT_LOAD_TIMEOUT"},{"message",timeout_cause}};
                load_watch.finish(now);
                if(!active) game::normal_load_identity::cancel(timeout_cause);
                pending_load=nullptr; respond(dir,response);
            }
            return;
        }
        if(!pending_placement.is_null()) {
            bool complete=false; json result; std::string error;
            bool success=net::poll_diagnostic_placement(pending_placement.at("id"),complete,result,error);
            if(!success||complete) {
                json response={{"schema_version",1},{"id",pending_placement.at("id")},{"run_id",run_id},{"ok",success}};
                if(success) response["result"]={{"state",Config::instance().is_host?"host_executed":"host_result_applied"},{"placement",result},{"engine",status()}};
                else response["error"]={{"code","PLACEMENT_FAILED"},{"message",error}};
                pending_placement=nullptr; respond(dir,response);
            }
            return;
        }
        if(!pending_crash_dismiss.is_null()) {
            game::native::WorldStatus world; std::string error;
            bool valid=game::native::read_world_status(world,error);
            bool title=valid&&world.active_mode_index==2&&!world.title_transition_pending;
            bool timeout=now-crash_dismiss_started>std::chrono::seconds(15);
            if(!valid||title||timeout) {
                json response={{"schema_version",1},{"id",pending_crash_dismiss.at("id")},{"run_id",run_id},{"ok",title}};
                if(title) response["result"]={{"state","title_active"},{"report_uploaded",false},{"engine",status()}};
                else response["error"]={{"code",timeout?"CRASH_DISMISS_TIMEOUT":"ENGINE_REJECTED"},{"message",timeout?"Titelmenü nach Crash-Dialog nicht innerhalb von 15 Sekunden aktiv":error}};
                pending_crash_dismiss=nullptr; respond(dir,response);
            }
            return;
        }
        if(!pending_overlay.is_null()) {
            bool visible=overlay::LobbyOverlay::instance().is_visible();
            bool changed=visible!=pending_overlay.at("previous").get<bool>();
            bool timeout=now-overlay_started>std::chrono::seconds(5);
            if(changed||timeout) {
                json response={{"schema_version",1},{"id",pending_overlay.at("id")},{"run_id",run_id},{"ok",changed}};
                if(changed) response["result"]={{"state","SDL_F10_processed"},{"visible",visible},{"engine",status()}};
                else response["error"]={{"code","OVERLAY_EVENT_TIMEOUT"},{"message","SDL-F10-Ereignis hat die Overlay-Sichtbarkeit nicht geändert"}};
                respond(dir,response); pending_overlay=nullptr;
            }
            return;
        }
        if(!pending_world.is_null()) {
            std::string error; bool complete=false; bool success=true;
            if(pending_world.at("state")=="preparing") {
                success=game::native::test_world_ready(complete,error);
                if(success&&complete) {
                    success=game::native::create_test_world(pending_world.at("name").get<std::string>(),error);
                    if(success) pending_world["state"]="loading";
                }
            } else complete=game::native::in_game();
            bool active=game::native::in_game();
            bool timeout=now-world_started>std::chrono::seconds(60);
            if(!success||timeout||active) {
                json response={{"schema_version",1},{"id",pending_world.at("id")},{"run_id",run_id},{"ok",success&&!timeout&&active}};
                if(response["ok"].get<bool>()) response["result"]={{"state","world_active"},{"name",pending_world.at("name")},{"engine",status()}};
                else response["error"]={{"code",timeout?"WORLD_CREATE_TIMEOUT":"ENGINE_REJECTED"},{"message",timeout?"Testwelt nicht innerhalb von 60 Sekunden aktiv; state="+pending_world.at("state").get<std::string>():error}};
                respond(dir,response);
                LOG_INFO("[MCP] Test world completed: active={} success={} state={}",active,success,pending_world.at("state").get<std::string>());
                pending_world=nullptr;
            }
            return;
        }
        if(!pending_snapshot.is_null()) {
            int32_t count=-1; std::string error;
            if(!game::native::read_saves_in_progress(count,error)) throw std::runtime_error(error);
            if(now-snapshot_started>=std::chrono::seconds(net::snapshot_native_capture_timeout_seconds)) {
                auto cause="native snapshot capture/readback timeout saves_in_progress="+std::to_string(count)+" native_task_cancelled=false required idle save, complete file and identity decision within "+std::to_string(net::snapshot_native_capture_timeout_seconds)+"s";
                game::building_capture_identity::cancel(cause);
                if(pending_snapshot.value("distribute",Config::instance().is_host))game::network_identity::cancel_pending(cause);
                respond(dir,{{"schema_version",1},{"id",pending_snapshot.at("id")},{"run_id",run_id},{"ok",false},{"error",{{"code","SNAPSHOT_TIMEOUT"},{"message",cause}}}});
                LOG_ERROR("[MCP] {}",cause); pending_snapshot=nullptr; return;
            }
            if(count==0&&std::filesystem::exists(snapshot_path)&&std::filesystem::file_size(snapshot_path)>0) {
                if(!pending_snapshot.contains("sha256")) {
                    uint64_t length=0; std::string digest;
                    if(!net::hash_snapshot_file(snapshot_path,digest,length,error)) throw std::runtime_error("snapshot capture hash failed: "+error);
                    pending_snapshot["sha256"]=digest; pending_snapshot["bytes"]=length;
                    pending_snapshot["write_time"]=std::filesystem::last_write_time(snapshot_path).time_since_epoch().count();
                }
                auto digest=pending_snapshot.at("sha256").get<std::string>();
                auto length=pending_snapshot.at("bytes").get<uint64_t>();
                if(std::filesystem::file_size(snapshot_path)!=length||std::filesystem::last_write_time(snapshot_path).time_since_epoch().count()!=pending_snapshot.at("write_time").get<int64_t>())
                    throw std::runtime_error("owned snapshot file changed during identity readback; no replay");
                auto id=pending_snapshot.at("snapshot_id").get<std::string>();
                game::building_capture_identity::Status identity;
                auto sealed=game::building_capture_identity::seal(id,digest,length,Config::instance().session_id,identity,error);
                if(sealed==game::building_capture_identity::SealResult::Pending) return;
                uint64_t final_length=0; std::string final_digest;
                if(!net::hash_snapshot_file(snapshot_path,final_digest,final_length,error))
                    throw std::runtime_error("snapshot file final hash read failed: "+error);
                if(final_digest!=digest||final_length!=length)
                    throw std::runtime_error("snapshot file final receipt differs bytes="+std::to_string(final_length)+" required="+std::to_string(length)+" sha256="+final_digest+" required="+digest);
                const auto completed_at=std::chrono::steady_clock::now();
                if(completed_at-snapshot_started>=std::chrono::seconds(net::snapshot_native_capture_timeout_seconds))
                    throw std::runtime_error("snapshot capture completion deadline exceeded elapsed_ms="+std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(completed_at-snapshot_started).count())+" required<"+std::to_string(net::snapshot_native_capture_timeout_seconds*1000)+" native_task_cancelled=false");
                const bool distribute=pending_snapshot.value("distribute",Config::instance().is_host);
                if(distribute) {
                    std::string network_error;
                    if(!game::network_identity::seal_host_capture(id,digest,network_error))
                        LOG_ERROR("[MCP] Snapshot file complete id={} but network identity failed cause={}",id,network_error);
                }
                if(sealed==game::building_capture_identity::SealResult::Failed)
                    LOG_ERROR("[MCP] Snapshot file complete id={} but building identity failed cause={}",id,identity.error);
                const auto world_name=pending_snapshot.at("world_name").get<std::string>();
                json record={{"snapshot_id",id},{"world_name",world_name},{"path",net::snapshot_world_path::utf8(snapshot_path)},{"bytes",length},{"sha256",digest},{"run_id",run_id},
                    {"state","snapshot_file_complete"},{"distribution_requested",distribute},{"building_bindings_confirmed",identity.bindings_confirmed},{"building_identity_error",identity.error}};
                write_json(dir/("snapshot-"+id+".json"),record);
                json response={{"schema_version",1},{"id",pending_snapshot.at("id")},{"run_id",run_id},{"ok",true},{"result",record}};
                respond(dir,response);
                LOG_INFO("[MCP] SNAPSHOT_CAPTURED bytes={} sha256={} path={}",length,digest,snapshot_path.string());
                pending_snapshot=nullptr;
                if(distribute&&!net::distribute_snapshot(id,digest,snapshot_path,error,world_name)) LOG_ERROR("[SNAPSHOT] Captured file distribution failed: {}",error);
                if(!distribute)LOG_INFO("[MCP] Local snapshot retained id={} bytes={}; distribution deliberately skipped",id,length);
            }
            return;
        }
        auto request_path=dir/"request.json";
        json request;
        const bool internal_request=game::lobby_operations::take_request(request);
        if(!internal_request) {
            if(!Config::instance().diagnostic_mailbox_enabled||game::lobby_operations::pending()||!std::filesystem::exists(request_path))return;
            request=read_request(request_path);
        }
        auto id=request.at("id").get<std::string>(); if(!internal_request&&id==last_mailbox_request) return;
        if(request.at("run_id")!=run_id) { if(!internal_request)last_mailbox_request=id; last_request=id; LOG_INFO("[MCP] Previous-run request skipped: id={}",id); return; }
        if(id.size()!=36||request.at("schema_version")!=1) throw std::runtime_error("request identity mismatch");
        request_in_progress=id;
        // Internal operations must not make an unchanged external mailbox request new again.
        if(!internal_request)last_mailbox_request=id;
        auto operation=request.at("op").get<std::string>();
        json response={{"schema_version",1},{"id",id},{"run_id",run_id}};
        if(game::tutorial_movement::receipt().down_attempted&&!game::tutorial_movement::receipt().axis_released&&
           (game::tutorial_movement::receipt().down_queued||game::tutorial_movement::receipt().down_unknown)&&
           operation!="read_tutorial_movement"&&operation!="read_tutorial_progress"&&operation!="status"&&operation!="capture_frame")
            throw std::runtime_error("tutorial_movement_release_unresolved; competing operation rejected");
        if(game::ui_network_runtime::unresolved()&&operation!="read_node_observer"&&operation!="read_network_placement"&&operation!="read_network_state"&&operation!="read_tutorial"&&operation!="read_tutorial_development"&&operation!="read_tutorial_progress"&&operation!="status"&&operation!="inspect_engine"&&operation!="capture_frame"&&operation!="read_remove_bindings"&&operation!="dismiss_crash_report"&&operation!="exit_game")
            throw std::runtime_error("network_ui_previous_effect_unresolved; competing native operation rejected");
        if(game::ui_path_runtime::unresolved()&&operation!="read_path"&&operation!="status"&&operation!="inspect_engine"&&operation!="capture_frame"&&operation!="read_remove_bindings"&&operation!="dismiss_crash_report"&&operation!="exit_game")
            throw std::runtime_error("path_ui_previous_effect_unresolved; competing native operation rejected");
        if(game::ui_terrain_runtime::unresolved()&&operation!="read_terrain"&&operation!="status"&&operation!="inspect_engine"&&operation!="capture_frame"&&operation!="read_remove_bindings"&&operation!="dismiss_crash_report"&&operation!="exit_game")
            throw std::runtime_error("terrain_ui_previous_effect_unresolved; competing native operation rejected");
        if(game::native_tool_prepare::unresolved() && operation!="read_cancel_tool" && operation!="read_buildings_tab" && operation!="status" && operation!="inspect_engine" && operation!="capture_frame" && operation!="read_remove_bindings" && operation!="dismiss_crash_report" && operation!="exit_game")
            throw std::runtime_error("native_tool_prepare_previous_effect_unresolved; competing native operation rejected");
        if(game::move_ui_probe::unresolved() && operation!="read_move_ui" && operation!="status" && operation!="inspect_engine" && operation!="capture_frame" && operation!="read_remove_bindings" && operation!="dismiss_crash_report" && operation!="exit_game")
            throw std::runtime_error("move_ui_previous_effect_unresolved; competing native operation rejected");
        if(game::placement_ui_diagnostics::unresolved() && operation!="read_build_ui" && operation!="status" && operation!="inspect_engine" && operation!="capture_frame" && operation!="read_remove_bindings" && operation!="dismiss_crash_report" && operation!="exit_game")
            throw std::runtime_error("build_ui_previous_effect_unresolved; competing native operation rejected");
        if((game::time_input_diagnostics::unresolved()||game::time_mouse_diagnostics::unresolved())&&
            operation!="read_time_mouse"&&operation!="read_time_input"&&operation!="read_time_acceptance"&&
            operation!="status"&&operation!="inspect_engine"&&operation!="read_time_ui")
            throw std::runtime_error("time_input_previous_effect_unresolved; competing native operation rejected");
        if(operation=="read_native_developer_retirement"||operation=="native_developer_retirement_fixture") {
            json observed;std::string error;
            if(operation=="read_native_developer_retirement") {
                if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("Developer retirement read requires empty arguments");
                response["ok"]=game::developer_staff::retirement::observe_fixture(observed,error);
            }else response["ok"]=game::developer_staff::retirement::run_fixture(request.at("args"),observed,error);
            response["result"]=observed;
            if(!response["ok"].get<bool>())response["error"]={{"code","DEVELOPER_RETIREMENT_FAILED"},{"message",error}};
        }
        else if(operation=="read_native_bug_fix_ui"||operation=="native_bug_fix_ui") {
            json observed;std::string error;
            response["ok"]=operation=="read_native_bug_fix_ui"?
                game::bug_authority::manual_action::read_ui_fixture(request.at("args"),observed,error):
                game::bug_authority::manual_action::run_ui_fixture(request.at("args"),observed,error);
            response["result"]=observed;
            if(!response["ok"].get<bool>())response["error"]={{"code","BUG_FIX_UI_REJECTED"},{"message",error}};
        }
        else if(operation=="read_bug_c_policy"||operation=="native_bug_c_policy") {
            json observed;std::string error;
            if(operation=="read_bug_c_policy") {
                if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("Bug-C policy read requires empty arguments");
                response["ok"]=game::developer_staff::bug_policy::observe(observed,error);
            }else response["ok"]=game::developer_staff::bug_policy::run(request.at("args"),observed,error);
            response["result"]=observed;
            if(!response["ok"].get<bool>())response["error"]={{"code","BUG_C_POLICY_UI_FAILED"},{"message",error}};
        }
        else if(operation=="native_developer_ui") {
            json observed;std::string error;
            response["ok"]=game::developer_staff::action_runtime::ui_fixture(request.at("args"),observed,error);
            response["result"]=observed;
            if(!response["ok"].get<bool>())response["error"]={{"code","DEVELOPER_UI_ADMISSION_FAILED"},{"message",error}};
        }
        else if(operation=="read_native_developers"||operation=="native_developer_fixture") {
            json observed;std::string error;
            if(operation=="read_native_developers") {
                if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("Developer read requires empty arguments");
                response["ok"]=game::developer_staff::observe_fixture(observed,error);
            }else response["ok"]=game::developer_staff::run_fixture(request.at("args"),observed,error);
            response["result"]=observed;
            if(!response["ok"].get<bool>())response["error"]={{"code","DEVELOPER_FIXTURE_FAILED"},{"message",error}};
        }
        else if(operation=="read_native_release"||operation=="native_initial_release_fixture") {
            json observed;std::string error;
            if(operation=="read_native_release") {
                if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("InitialRelease read requires empty arguments");
                response["ok"]=game::initial_release::observe_fixture(observed,error);
            }else response["ok"]=game::initial_release::run_fixture(request.at("args"),observed,error);
            response["result"]=observed;
            if(!response["ok"].get<bool>())response["error"]={{"code","INITIAL_RELEASE_FIXTURE_FAILED"},{"message",error}};
        }
        else if(operation=="read_native_loans"||operation=="native_take_loan_fixture"||operation=="native_loan_payment") {
            json observed;std::string error;
            if(operation=="read_native_loans") {
                if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("native Loan read requires empty arguments");
                response["ok"]=game::loan_diagnostics::observe(observed,error);
            }else response["ok"]=game::loan_diagnostics::mutate(operation=="native_take_loan_fixture"?game::loan_diagnostics::Operation::TakeFixture:game::loan_diagnostics::Operation::Payment,request.at("args"),observed,error);
            response["result"]=observed;
            if(!response["ok"].get<bool>())response["error"]={{"code","NATIVE_LOAN_FAILED"},{"message",error}};
        }
        else if(operation=="read_native_lobby"||operation=="native_lobby_button") {
            json observed;std::string error;
            if(operation=="read_native_lobby"&&!request.at("args").empty())throw std::runtime_error("native lobby read requires empty arguments");
            response["ok"]=operation=="read_native_lobby"?game::native_lobby::observe(observed,error):game::native_lobby::activate(request.at("args"),observed,error);
            response["result"]=observed;
            if(!response["ok"].get<bool>())response["error"]={{"code","NATIVE_LOBBY_FAILED"},{"message",error.empty()?native_lobby_error:error}};
        }
        else if(operation=="status"||operation=="inspect_engine") { response["ok"]=true; response["result"]=status(); }
        else if(operation=="read_chat_ui"||operation=="native_chat_ui_key"||operation=="native_chat_ui_text") {
            const auto& args=request.at("args");
            SDL_Window* window=hooks::get_game_window();
            if(!window)throw std::runtime_error("chat UI game window unavailable");
            if(operation=="read_chat_ui") {
                if(!args.is_object()||!args.empty())throw std::runtime_error("read_chat_ui requires empty object arguments");
                response["ok"]=true;response["result"]=chat_ui_status();
            } else if(operation=="native_chat_ui_key") {
                if(!args.is_object()||args.size()!=1||!args.contains("key")||!args.at("key").is_string())throw std::runtime_error("native_chat_ui_key requires exactly one key string");
                const std::string key=args.at("key").get<std::string>();
                const auto before=chat_ui_status();
                SDL_Scancode scancode=SDL_SCANCODE_UNKNOWN;
                SDL_Keycode keycode=SDLK_UNKNOWN;
                if(key=="F9") { scancode=SDL_SCANCODE_F9;keycode=SDLK_F9; }
                else if(key=="ENTER") { scancode=SDL_SCANCODE_RETURN;keycode=SDLK_RETURN; }
                else if(key=="ESCAPE") { scancode=SDL_SCANCODE_ESCAPE;keycode=SDLK_ESCAPE; }
                else if(key!="PROBE")throw std::runtime_error("native_chat_ui_key accepts F9, ENTER, ESCAPE or PROBE");
                if((key=="ENTER"||key=="ESCAPE")&&(!before.at("visible").get<bool>()||!before.at("input_active").get<bool>()))
                    throw std::runtime_error("chat input field is not active");
                if(key=="PROBE"&&(before.at("visible").get<bool>()||overlay::LobbyOverlay::instance().is_visible()))
                    throw std::runtime_error("close chat and lobby before forwarding probe");
                SDL_Event event{};
                event.type=key=="PROBE"?SDL_EVENT_KEY_UP:SDL_EVENT_KEY_DOWN;
                event.key.windowID=SDL_GetWindowID(window);
                event.key.scancode=scancode;event.key.key=keycode;event.key.down=key!="PROBE";
                if(!SDL_PushEvent(&event))throw std::runtime_error("SDL chat key queue failed: "+std::string(SDL_GetError()));
                if(key!="PROBE") {
                    event.type=SDL_EVENT_KEY_UP;event.key.down=false;
                    if(!SDL_PushEvent(&event))throw std::runtime_error("SDL chat key up queue failed after down queued: "+std::string(SDL_GetError()));
                }
                response["ok"]=true;response["result"]={{"state","queued"},{"key",key},{"before",before}};
            } else {
                if(!args.is_object()||args.size()!=1||!args.contains("text")||!args.at("text").is_string())throw std::runtime_error("native_chat_ui_text requires exactly one text string");
                const std::string value=args.at("text").get<std::string>();
                if(!net::chat::valid_text(value))throw std::runtime_error("chat UI text must be 1 to 240 valid UTF-8 bytes without controls");
                const auto before=chat_ui_status();
                if(!before.at("visible").get<bool>()||!before.at("input_active").get<bool>()||!SDL_TextInputActive(window))
                    throw std::runtime_error("chat input field and SDL text input must be active");
                std::string error;
                if(!hooks::queue_chat_ui_text(value,error))throw std::runtime_error(error);
                response["ok"]=true;response["result"]={{"state","queued"},{"utf8_bytes",value.size()},{"before",before}};
            }
        }
        else if(operation=="read_chat"||operation=="send_chat") {
            const auto& args=request.at("args");
            if(operation=="read_chat") {
                if(!args.is_object()||!args.empty())throw std::runtime_error("read_chat requires empty object arguments");
                response["ok"]=true;response["result"]=chat_status();
            } else {
                if(!args.is_object()||args.size()!=1||!args.contains("text")||!args.at("text").is_string())throw std::runtime_error("send_chat requires exactly one text string");
                std::string error;uint64_t sequence=0;
                response["ok"]=net::chat::Runtime::instance().submit(args.at("text").get<std::string>(),error,&sequence);
                if(response["ok"].get<bool>())response["result"]={{"state","queued"},{"sequence",sequence},{"chat",chat_status()}};
                else response["error"]={{"code","CHAT_REJECTED"},{"message",error}};
            }
        }
        else if(operation=="read_time_acceptance") {
            if(!request.at("args").empty())throw std::runtime_error("read_time_acceptance requires empty arguments");
            response["result"]=game::time_input_diagnostics::read(completed_game_updates,time_update_attempted);
            response["ok"]=!response["result"].contains("read_error");
            if(!response["ok"].get<bool>())response["error"]={{"code","TIME_INPUT_READ_REJECTED"},{"message",response["result"]["read_error"]}};
        }
        else if(operation=="native_time_key"||operation=="read_time_input") {
            std::string error;json result;
            const bool okay=operation=="native_time_key"?
                game::time_input_diagnostics::queue(request.at("args"),completed_game_updates,result,error):
                game::time_input_diagnostics::receipt(request.at("args"),result,error);
            response["ok"]=okay;response["result"]=result;
            if(!okay)response["error"]={{"code","TIME_INPUT_REJECTED"},{"message",error}};
        }
        else if(operation=="native_time_button"||operation=="read_time_mouse") {
            std::string error;json result=json::object();
            const bool okay=operation=="native_time_button"?
                game::time_mouse_diagnostics::probe(request.at("args"),result,error):
                game::time_mouse_diagnostics::receipt(request.at("args"),result,error);
            response["ok"]=okay;response["result"]=std::move(result);
            if(!okay)response["error"]={{"code","TIME_MOUSE_REJECTED"},{"message",error}};
        }
        else if(operation=="exit_game") {
            if(!request.at("args").empty()) throw std::runtime_error("exit_game requires empty arguments");
            std::string error;
            response["ok"]=suspend_time("native MCP exit",error)&&game::native::request_exit_game(error);
            if(response["ok"].get<bool>()) response["result"]={{"state","native_exit_flag_confirmed"}};
            else response["error"]={{"code","EXIT_REJECTED"},{"message",error}};
        }
        else if(operation=="dismiss_crash_report") {
            std::string error;
            response["ok"]=game::native::request_dismiss_crash_report(error);
            if(response["ok"].get<bool>()) { pending_crash_dismiss={{"id",id}}; crash_dismiss_started=now; last_request=id; return; }
            response["error"]={{"code","ENGINE_REJECTED"},{"message",error}};
        }
        else if(operation=="probe_building_placement") {
            const auto& args=request.at("args"); game::native::PropState grounded; std::string error;
            response["ok"]=game::native::probe_building_placement(args.at("type"),args.at("variant"),args.at("x"),args.at("z"),grounded,error);
            if(response["ok"].get<bool>()) response["result"]={{"state","placement_validated_without_mutation"},{"type",args.at("type")},{"variant",args.at("variant")},{"x",grounded.x},{"y",grounded.y},{"z",grounded.z}};
            else response["error"]={{"code","PLACEMENT_PROBE_REJECTED"},{"message",error}};
        }
        else if(operation=="read_subscriber_candidates") {
            if(!request.at("args").empty()) throw std::runtime_error("read_subscriber_candidates requires empty arguments");
            game::native::SubscriberCandidates candidates; std::string error;
            response["ok"]=game::native::read_subscriber_candidates(candidates,error);
            if(response["ok"].get<bool>()) response["result"]={{"existing_count",candidates.existing_count},{"scanned_count",candidates.scanned_count},
                {"eligible_count",candidates.eligible_count},{"scan_complete",candidates.scan_complete},{"serialized_ids",candidates.serialized_ids}};
            else response["error"]={{"code","SUBSCRIBER_CANDIDATES_REJECTED"},{"message",error}};
        }
        else if(operation=="read_building_catalog") {
            if(!request.at("args").empty()) throw std::runtime_error("building catalog read requires empty arguments");
            game::global_catalog::State state; std::string error;
            response["ok"]=game::global_catalog::read_native_catalog(state,error);
            auto counts=[](const game::global_catalog::Counts& count) {
                return json{{"observed",count.observed},{"scan_complete",count.scan_complete},{"regions",count.regions},
                    {"managers",count.managers},{"native_main_slots",count.native_main_slots},{"non_null",count.non_null},
                    {"typed_valid",count.typed_valid},{"null_slots",count.null_slots},{"rejected",count.rejected},{"duplicates",count.duplicates}};
            };
            json samples=json::array(), reasons=json::array();
            for(const auto& sample:state.samples) samples.push_back({{"uid",sample.uid},{"type",sample.type},{"variant",sample.variant},
                {"root_region_index",sample.root_region_index},{"region_id_available",sample.region_id_available},{"scope",sample.scope}});
            for(const auto& reason:state.reasons) reasons.push_back({{"reason",reason.reason},{"count",reason.count}});
            response["result"]={{"shared",state.shared},{"scope","root_main_building_arrays"},{"scan_complete",state.scan_complete},
                {"root",counts(state.root)},{"dungeon_template",counts(state.dungeon_template)},{"dungeon_instance",counts(state.dungeon_instance)},
                {"unobserved",state.unobserved},{"first_error",state.first_error},{"reasons",reasons},{"samples",samples}};
            json scenery;std::string scene_error;
            if(scenery_kiss::read_native_scenery_samples(scenery,scene_error))response["result"]["scenery"]=std::move(scenery);
            else{response["result"]["scenery"]={{"observed",false},{"error",scene_error}};LOG_ERROR("[Catalog] Native Scene sample read failed cause={}",scene_error);}
            if(!response["ok"].get<bool>()) response["error"]={{"code","BUILDING_CATALOG_REJECTED"},{"message",error}};
        }
        else if(operation=="read_time_ui") {
            if(!request.at("args").empty()) throw std::runtime_error("time UI read requires empty arguments");
            game::shared_time::Presentation state; std::string error;
            response["ok"]=game::time_diagnostics::read(state,error);
            if(response["ok"].get<bool>()) {
                json items=json::array();
                for(const auto& item:state.items) items.push_back({{"scale",item.scale},{"native_state",item.native_state},{"active",item.active}});
                response["result"]={{"policy","observe_native_time_items"},{"epoch",state.epoch},{"capture_enabled",state.capture_enabled},
                    {"host_state_available",state.host_state_available},{"revision",state.revision},{"local_host",state.local_host},
                    {"selected_scale",state.selected_scale},{"pause_scale",state.pause_scale},{"effective_scale",state.effective_scale},{"items",items}};
                game::hud_time::State hud;
                std::string hud_error=hud_time_reader_error;
                const bool hud_complete=hud_time_reader_ready&&hud_time_reader.read(hud,hud_error);
                json hud_items=json::array();
                for(const auto& item:hud.items) hud_items.push_back({{"scale",item.scale},{"native_state",item.native_state},
                    {"active",item.active},{"button_cached_active",item.button_cached_active},{"button_enabled",item.button_enabled},
                    {"actor_owner_present",item.actor_owner_present}});
                response["result"]["native_hud"]={{"complete",hud_complete},{"error",hud_error},{"items",hud_items},
                    {"clock_panel_visible",hud.clock_panel_visible},{"pause_window_present",hud.pause_window_present},
                    {"pause_window_visible",hud.pause_window_visible},{"native_pause_indicator",hud.native_pause_indicator},
                    {"pause_option_enabled",hud.pause_option_enabled},{"native_selected_scale",hud.native_selected_scale},
                    {"native_pause_scale",hud.native_pause_scale},{"flat_day",hud.flat_day},{"seconds",hud.seconds},
                    {"panel_children",hud.panel_children},{"time_items",hud.time_items}};
                response["result"]["hud_writer"]=game::hud_time_runtime::status_json();
                response["result"]["calendar"]=game::hud_calendar_runtime::status_json();
            } else response["error"]={{"code","TIME_UI_READ_REJECTED"},{"message",error}};
        }
        else if(operation=="read_remove_bindings") {
            if(!request.at("args").empty())throw std::runtime_error("remove bindings read requires empty arguments");
            std::string error;json result;
            response["ok"]=game::remove_diagnostics::bindings(result,error);response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","REMOVE_BINDINGS_REJECTED"},{"message",error}};
        }
        else if(operation=="enable_live_host_state"||operation=="read_live_host_state") {
            if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("live state requires exact empty arguments");
            std::string error;response["ok"]=operation=="read_live_host_state"||game::typed_action_runtime::enable_live_state(error);
            response["result"]=game::typed_action_runtime::status();
            if(!response["ok"].get<bool>())response["error"]={{"code","LIVE_HOST_STATE_REJECTED"},{"message",error}};
        }
        else if(operation=="start_typed_actions") {
            if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("typed action start requires empty arguments");
            std::string error;response["ok"]=game::typed_action_runtime::start(error);
            response["result"]=game::typed_action_runtime::status();
            if(!response["ok"].get<bool>())response["error"]={{"code","TYPED_ACTION_START_REJECTED"},{"message",error}};
        }
        else if(operation=="native_build_button"||operation=="read_build_ui") {
            std::string error;json result;
            response["ok"]=operation=="native_build_button"?game::placement_ui_diagnostics::probe(request.at("args"),result,error):game::placement_ui_diagnostics::receipt(request.at("args"),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","BUILD_UI_PROBE_FAILED"},{"message",error}};
        }
        else if(operation=="native_building_history_ui"||operation=="read_building_history_ui") {
            std::string error;json result=json::object();
            if(operation=="read_building_history_ui"&&(!request.at("args").is_object()||!request.at("args").empty()))
                throw std::runtime_error("read_building_history_ui requires empty object arguments");
            const bool okay=operation=="native_building_history_ui"?
                game::building_history_slice::invoke_ui(request.at("args"),result,error):
                game::building_history_slice::read_ui(result,error);
            response["ok"]=okay;response["result"]=std::move(result);
            if(!okay)response["error"]={{"code","BUILDING_HISTORY_UI_REJECTED"},{"message",error}};
        }
        else if(operation=="native_cancel_tool"||operation=="read_cancel_tool"||operation=="native_buildings_tab"||operation=="read_buildings_tab") {
            std::string error;json result;
            response["ok"]=operation=="native_buildings_tab"?game::native_tool_prepare::prepare_buildings_tab(request.at("args"),result,error):
                operation=="read_buildings_tab"?game::native_tool_prepare::read_buildings_tab(request.at("args"),result,error):
                operation=="native_cancel_tool"?game::native_tool_prepare::invoke(request.at("args"),result,error):game::native_tool_prepare::read(request.at("args"),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","NATIVE_TOOL_PREPARE_FAILED"},{"message",error}};
        }
        else if(operation=="native_move_ui"||operation=="read_move_ui") {
            std::string error;json result;
            response["ok"]=operation=="native_move_ui"?game::move_ui_probe::probe(request.at("args"),result,error):game::move_ui_probe::read(request.at("args"),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","MOVE_UI_PROBE_FAILED"},{"message",error}};
        }
        else if(operation=="enable_node_observer"||operation=="read_node_observer") {
            std::string error;json result;
            response["ok"]=game::node_observer::dispatch(operation,request.at("args"),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","NODE_OBSERVER_FAILED"},{"message",error}};
        }
        else if(operation=="enable_ui_network") {
            if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("enable_ui_network requires empty args");
            std::string error;response["ok"]=game::ui_network_runtime::enable(error);response["result"]=game::ui_network_runtime::status();
            if(!response["ok"].get<bool>())response["error"]={{"code","UI_NETWORK_REJECTED"},{"message",error}};
        }
        else if(operation=="read_network_placement"||operation=="native_network_node"||operation=="native_network_pipe") {
            std::string error;json result;const auto& a=request.at("args");
            response["ok"]=operation=="native_network_pipe"?game::ui_network_runtime::native_pipe(a,result,error):operation=="native_network_node"?game::ui_network_runtime::native_place(a,result,error):game::ui_network_runtime::query(a,result,error);
            response["result"]=std::move(result);if(!response["ok"].get<bool>())response["error"]={{"code","UI_NETWORK_REJECTED"},{"message",error}};
        }
        else if(operation=="enable_ui_path") {
            if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("enable_ui_path requires empty args");
            std::string error;response["ok"]=game::ui_path_runtime::enable(error);response["result"]=game::ui_path_runtime::status();
            if(!response["ok"].get<bool>())response["error"]={{"code","UI_PATH_REJECTED"},{"message",error}};
        }
        else if(operation=="read_path"||operation=="native_path_segment"||operation=="native_path_delete") {
            const auto& a=request.at("args");std::string error;json result;
            if(operation=="native_path_delete")response["ok"]=game::ui_path_runtime::native_delete(a,result,error);
            else if(operation=="native_path_segment")response["ok"]=game::ui_path_runtime::native_segment(a,result,error);
            else{
                if(!a.is_object()||a.size()!=1||!a.contains("id")||!a.at("id").is_number_integer())throw std::runtime_error("read_path requires exact id");
                const auto path_id=a.at("id").get<int64_t>();if(path_id<0||path_id>INT32_MAX)throw std::runtime_error("path id requires nonnegative int32");
                response["ok"]=game::ui_path_runtime::query(int32_t(path_id),result,error);result["ui_path"]=game::ui_path_runtime::status();
            }
            response["result"]=std::move(result);if(!response["ok"].get<bool>())response["error"]={{"code","UI_PATH_REJECTED"},{"message",error}};
        }
        else if(operation=="enable_ui_terrain") {
            if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("enable_ui_terrain requires empty args");
            std::string error;response["ok"]=game::ui_terrain_runtime::enable(error);response["result"]=game::ui_terrain_runtime::status();
            if(!response["ok"].get<bool>())response["error"]={{"code","UI_TERRAIN_REJECTED"},{"message",error}};
        }
        else if(operation=="read_terrain"||operation=="native_terrain_stroke"||operation=="native_terrain_height") {
            const auto& a=request.at("args");std::string error;json result;
            if(operation=="native_terrain_height")response["ok"]=game::ui_terrain_runtime::native_height(a,result,error);
            else if(operation=="native_terrain_stroke")response["ok"]=game::ui_terrain_runtime::native_stroke(a,result,error);
            else{
                if(!a.is_object()||a.size()!=1||!a.contains("id")||!a.at("id").is_number_integer())throw std::runtime_error("read_terrain requires exact id");
                const auto terrain_id=a.at("id").get<int64_t>();if(terrain_id<0||terrain_id>INT32_MAX)throw std::runtime_error("terrain id requires nonnegative int32");
                response["ok"]=game::ui_terrain_runtime::query(int32_t(terrain_id),result,error);result["ui_terrain"]=game::ui_terrain_runtime::status();
            }
            response["result"]=std::move(result);if(!response["ok"].get<bool>())response["error"]={{"code","UI_TERRAIN_REJECTED"},{"message",error}};
        }
        else if(operation=="read_prices") {
            if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("read_prices requires empty args");
            std::string error;json result;response["ok"]=game::ui_price_runtime::query(result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","UI_PRICE_READ_REJECTED"},{"message",error}};
        }
        else if(operation=="native_price_ui") {
            const auto& a=request.at("args");
            if(!a.is_object()||a.size()!=3||!a.contains("diagnostic_uuid")||!a.at("diagnostic_uuid").is_string()||
               !a.contains("purchase_fee")||!a.at("purchase_fee").is_number_integer()||
               !a.contains("subscription_fee")||!a.at("subscription_fee").is_number_integer())
                throw std::runtime_error("native_price_ui requires exact diagnostic_uuid, purchase_fee and subscription_fee");
            const auto purchase=a.at("purchase_fee").get<int64_t>(),subscription=a.at("subscription_fee").get<int64_t>();
            if(purchase<0||purchase>INT32_MAX||subscription<0||subscription>INT32_MAX)
                throw std::runtime_error("native_price_ui prices require nonnegative int32 values");
            std::string error;json result;response["ok"]=game::ui_price_runtime::native_ui(
                a.at("diagnostic_uuid").get<std::string>(),int32_t(purchase),int32_t(subscription),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","UI_PRICE_REJECTED"},{"message",error}};
        }
        else if(operation=="enable_ui_region") {
            if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("enable_ui_region requires empty args");
            std::string error;response["ok"]=game::ui_region_runtime::enable(error);response["result"]=game::ui_region_runtime::status();
            if(!response["ok"].get<bool>())response["error"]={{"code","UI_REGION_REJECTED"},{"message",error}};
        }
        else if(operation=="native_region_styles_ui") {
            const auto& a=request.at("args");
            if(!a.is_object()||a.size()!=3||!a.contains("region_id")||!a.at("region_id").is_number_integer()||
               !a.contains("diagnostic_uuid")||!a.at("diagnostic_uuid").is_string()||!a.contains("styles")||!a.at("styles").is_array()||a.at("styles").size()!=4)
                throw std::runtime_error("region styles UI requires exact region_id, diagnostic_uuid and four strings");
            const auto region_id=a.at("region_id").get<int64_t>();if(region_id<0||region_id>INT32_MAX)throw std::runtime_error("region_id requires nonnegative int32");
            std::array<std::string,4> styles;for(size_t i=0;i<4;++i)styles[i]=a.at("styles")[i].get<std::string>();
            std::string error;json result;response["ok"]=game::ui_region_runtime::styles_ui(a.at("diagnostic_uuid").get<std::string>(),int32_t(region_id),styles,result,error);
            response["result"]=std::move(result);if(!response["ok"].get<bool>())response["error"]={{"code","UI_REGION_REJECTED"},{"message",error}};
        }
        else if(operation=="read_npc_region") {
            const auto& a=request.at("args");
            if(!a.is_object()||a.size()!=1||!a.contains("region_id")||!a.at("region_id").is_number_integer())
                throw std::runtime_error("read_npc_region requires exact region_id");
            const auto region_id=a.at("region_id").get<int64_t>();
            if(region_id<0||region_id>INT32_MAX)throw std::runtime_error("read_npc_region requires nonnegative int32 region_id");
            std::string error;json result;
            response["ok"]=game::npc_probe::query(int32_t(region_id),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","NPC_REGION_READ_REJECTED"},{"message",error}};
        }
        else if(operation=="native_npc_tool_ui") {
            const auto& a=request.at("args");
            if(!a.is_object()||a.size()!=6||!a.contains("token")||!a.at("token").is_string()||
               !a.contains("region_id")||!a.at("region_id").is_number_integer()||
               !a.contains("tool")||!a.at("tool").is_string()||
               !a.contains("phase")||!a.at("phase").is_string()||
               !a.contains("character_slot")||!a.at("character_slot").is_number_integer()||
               !a.contains("character_type")||!a.at("character_type").is_number_integer())
                throw std::runtime_error("native_npc_tool_ui requires exact token, region_id, tool, phase and CharacterTypeID pair");
            const auto npc_region=a.at("region_id").get<int64_t>();
            const auto slot=a.at("character_slot").get<int64_t>();
            const auto type=a.at("character_type").get<int64_t>();
            if(npc_region<0||npc_region>INT32_MAX||slot<INT32_MIN||slot>INT32_MAX||type<INT32_MIN||type>INT32_MAX)
                throw std::runtime_error("native_npc_tool_ui integer outside int32");
            std::string error;json result;
            response["ok"]=game::npc_probe::tool_ui(a.at("token").get<std::string>(),int32_t(npc_region),
                a.at("tool").get<std::string>(),a.at("phase").get<std::string>(),int32_t(slot),int32_t(type),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","NPC_TOOL_UI_REJECTED"},{"message",error}};
        }
        else if(operation=="native_monster_zone_ui") {
            json result;std::string error;
            response["ok"]=game::npc_probe::monster_zone_ui(request.at("args"),result,error);
            response["result"]=std::move(result);if(!response["ok"].get<bool>())response["error"]={{"code","MONSTER_ZONE_UI_REJECTED"},{"message",error}};
        }
        else if(operation=="native_monster_zone_probe") {
            std::string error;json result;
            response["ok"]=game::npc_probe::monster_zone_probe(request.at("args"),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","MONSTER_ZONE_PROBE_REJECTED"},{"message",error}};
        }
        else if(operation=="native_npc_place_probe") {
            std::string error;json result;
            response["ok"]=game::npc_probe::place_probe(request.at("args"),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","NPC_PLACE_PROBE_REJECTED"},{"message",error}};
        }
        else if(operation=="native_npc_place_ui") {
            std::string error;json result;
            response["ok"]=game::npc_probe::action_runtime::native_ui(request.at("args"),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","NPC_PLACE_UI_REJECTED"},{"message",error}};
        }
        else if(operation=="native_npc_remove_ui") {
            std::string error;json result;
            response["ok"]=game::npc_probe::action_runtime::native_remove_ui(request.at("args"),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","NPC_REMOVE_UI_REJECTED"},{"message",error}};
        }
        else if(operation=="native_npc_move_ui") {
            std::string error;json result;
            response["ok"]=game::npc_probe::action_runtime::native_move_ui(request.at("args"),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","NPC_MOVE_UI_REJECTED"},{"message",error}};
        }
        else if(operation=="read_region_preset"||operation=="native_region_preset_ui") {
            const auto& a=request.at("args");const bool command=operation=="native_region_preset_ui";
            if(!a.is_object()||a.size()!=(command?3u:1u)||!a.contains("region_id")||!a.at("region_id").is_number_integer())throw std::runtime_error("region operation requires exact region_id and UI command fields");
            const auto region_id=a.at("region_id").get<int64_t>();if(region_id<0||region_id>INT32_MAX)throw std::runtime_error("region_id requires nonnegative int32");
            if(command&&(!a.contains("diagnostic_uuid")||!a.at("diagnostic_uuid").is_string()||!a.contains("preset")||!a.at("preset").is_string()))throw std::runtime_error("region UI requires diagnostic_uuid and preset strings");
            std::string error;json result;
            response["ok"]=command?game::ui_region_runtime::preset_ui(a.at("diagnostic_uuid").get<std::string>(),int32_t(region_id),a.at("preset").get<std::string>(),result,error):game::ui_region_runtime::query(int32_t(region_id),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","UI_REGION_REJECTED"},{"message",error}};
        }
        else if(operation=="native_region_property_ui") {
            const auto& a=request.at("args");
            if(!a.is_object()||a.size()!=(a.contains("levels")?7:6)||!a.contains("diagnostic_uuid")||!a.at("diagnostic_uuid").is_string()||
               !a.contains("region_id")||!a.at("region_id").is_number_integer()||
               !a.contains("kind")||!a.at("kind").is_string()||!a.contains("name")||!a.at("name").is_string()||
               !a.contains("type")||!a.at("type").is_number_integer()||!a.contains("hit")||
               !a.at("hit").is_array()||a.at("hit").size()!=3)
                throw std::runtime_error("region property UI requires exact diagnostic_uuid, region_id, kind, name, type and hit");
            const auto region_id=a.at("region_id").get<int64_t>(),type=a.at("type").get<int64_t>();
            if(region_id<0||region_id>INT32_MAX||type<-1||type>1)throw std::runtime_error("region property id/type outside domain");
            action_session::RegionPropertyIntent intent;intent.id=int32_t(region_id);intent.type=int32_t(type);
            const auto kind=a.at("kind").get<std::string>();intent.name=a.at("name").get<std::string>();
            if(kind=="name")intent.kind=action_session::RegionPropertyKind::Name;
            else if(kind=="type")intent.kind=action_session::RegionPropertyKind::Type;
            else if(kind=="purchase")intent.kind=action_session::RegionPropertyKind::Purchase;
            else if(kind=="levels"){
                intent.kind=action_session::RegionPropertyKind::LevelRange;
                const auto& levels=a.at("levels");if(!levels.is_array()||levels.size()!=2)throw std::runtime_error("region levels requires two integers");
                for(size_t i=0;i<2;++i){if(!levels[i].is_number_integer())throw std::runtime_error("region level requires integer");const auto value=levels[i].get<int64_t>();
                    if(value<1||value>INT32_MAX/10-10)throw std::runtime_error("region level outside safe native integer domain");intent.levels[i]=int32_t(value);}
            }else throw std::runtime_error("region property kind requires name, type, purchase or levels");
            if((kind=="levels")!=a.contains("levels"))throw std::runtime_error("region levels field differs from kind");
            for(size_t i=0;i<3;++i){if(!a.at("hit")[i].is_number())throw std::runtime_error("region property hit requires finite numbers");
                intent.hit[i]=a.at("hit")[i].get<float>();}
            if((kind=="name"&&(intent.name.empty()||intent.type!=-1||intent.hit!=std::array<float,3>{}))||
               (kind=="type"&&(!intent.name.empty()||intent.type<0))||
               (kind=="purchase"&&(!intent.name.empty()||intent.type!=-1))||
               (kind=="levels"&&(!intent.name.empty()||intent.type!=-1||intent.levels[0]>intent.levels[1])))
                throw std::runtime_error("region property arguments differ from kind");
            std::string error;json result;response["ok"]=game::ui_region_runtime::property_ui(
                a.at("diagnostic_uuid").get<std::string>(),intent,result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","UI_REGION_PROPERTY_REJECTED"},{"message",error}};
        }
        else if(operation=="enable_ui_move") {
            if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("enable_ui_move requires empty args");
            std::string error;response["ok"]=game::ui_move_runtime::enable(error);response["result"]=game::ui_move_runtime::status();
            if(!response["ok"].get<bool>())response["error"]={{"code","UI_MOVE_FAILED"},{"message",error}};
        }
        else if(operation=="request_building_move") {
            std::string error;json result;response["ok"]=game::typed_action_runtime::request_move(request.at("args"),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","MOVE_REQUEST_FAILED"},{"message",error}};
        }
        else if(operation=="enable_ui_build") {
            if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("enable_ui_build requires empty args");
            std::string error;response["ok"]=game::ui_build_runtime::enable(error);response["result"]=game::ui_build_runtime::status();
            if(!response["ok"].get<bool>())response["error"]={{"code","UI_BUILD_FAILED"},{"message",error}};
        }
        else if(operation=="request_building_place") {
            std::string error;json result;response["ok"]=game::typed_action_runtime::request_place(request.at("args"),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","BUILD_REQUEST_FAILED"},{"message",error}};
        }
        else if(operation=="enable_ui_remove") {
            if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("UI Remove enable requires empty arguments");
            std::string error;response["ok"]=game::ui_remove_runtime::enable(error);
            response["result"]=game::ui_remove_runtime::status();
            if(!response["ok"].get<bool>())response["error"]={{"code","UI_REMOVE_ENABLE_REJECTED"},{"message",error}};
        }
        else if(operation=="probe_remove_ui") {
            std::string error;json result;
            response["ok"]=game::ui_remove_runtime::probe(request.at("args"),result,error);response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","UI_REMOVE_PROBE_REJECTED"},{"message",error}};
        }
        else if(operation=="request_snapshot_remove") {
            std::string error;json result;
            response["ok"]=game::typed_action_runtime::request_remove(request.at("args"),result,error);response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","TYPED_ACTION_REMOVE_REJECTED"},{"message",error}};
        }
        else if(operation=="probe_remove_snapshot_building") {
            std::string error;json result;
            response["ok"]=game::remove_diagnostics::remove(request.at("args"),result,error);response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","NATIVE_REMOVE_REJECTED"},{"message",error}};
        }
        else if(operation=="retire_snapshot_remove") {
            std::string error;json result;
            response["ok"]=game::remove_diagnostics::retire(request.at("args"),result,error);response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","NATIVE_REMOVE_RETIREMENT_REJECTED"},{"message",error}};
        }
        else if(operation=="read_native_free") {
            if(!request.at("args").empty())throw std::runtime_error("native Free read requires empty arguments");
            response["ok"]=true;response["result"]=game::native_free_diagnostics::status();
        }
        else if(operation=="probe_native_holder") {
            std::string error;json result;
            response["ok"]=game::native_free_diagnostics::probe(request.at("args"),result,error);
            response["result"]=std::move(result);
            if(!response["ok"].get<bool>())response["error"]={{"code","NATIVE_HOLDER_REJECTED"},{"message",error}};
        }
        else if(operation=="tutorial_startingpoint") {
            if(!owned_native_profile||!Config::instance().local_test)
                throw std::runtime_error("startingpoint fixture requires owned local-test runtime");
            const auto& args=request.at("args");
            if(!args.is_object()||args.size()!=6||!args.at("token").is_string()||!args.at("native_origin_uuid").is_string()||
               !args.at("operation").is_string()||!args.at("world_epoch").is_number_unsigned()||
               !args.at("world_generation").is_number_unsigned()||!args.at("expected_development").is_object())
                throw std::runtime_error("startingpoint fixture requires exact token/operation/origin/development fields");
            game::tutorial_startingpoint_fixture::Request effect{args.at("token").get<std::string>(),args.at("native_origin_uuid").get<std::string>(),
                args.at("operation").get<std::string>(),args.at("world_epoch").get<uint64_t>(),args.at("world_generation").get<uint64_t>(),args.at("expected_development")};
            std::string error;json receipt;
            response["ok"]=game::tutorial_startingpoint_fixture::run(effect,receipt,error);
            response["result"]=std::move(receipt);
            if(!response["ok"].get<bool>())response["error"]={{"code","TUTORIAL_STARTINGPOINT_REJECTED"},{"message",error}};
        }
        else if(operation=="tutorial_movement_focus"||operation=="read_tutorial_movement_focus") {
            if(!tutorial_movement_ready||!owned_native_profile||!Config::instance().local_test||!Config::instance().is_host)
                throw std::runtime_error("tutorial movement focus requires initialized owned local Host");
            const auto& args=request.at("args");std::string error;bool ok=true;
            if(operation=="tutorial_movement_focus") {
                if(!args.is_object()||args.size()!=1||!args.at("native_origin_uuid").is_string())
                    throw std::runtime_error("tutorial movement focus requires exact native origin");
                ok=game::tutorial_movement::request_focus(args.at("native_origin_uuid").get<std::string>(),error);
            } else if(!args.is_object()||!args.empty())throw std::runtime_error("tutorial movement focus read requires empty arguments");
            game::tutorial_movement::FocusReceipt focus;
            std::string read_error;const bool read=game::tutorial_movement::read_focus(focus,read_error);
            if(!read){if(error.empty())error=read_error;else error+="; readback: "+read_error;}
            response["ok"]=ok&&read;
            response["result"]={{"requested",focus.requested},{"attempted",focus.attempted},{"returned",focus.returned},
                {"native_origin_uuid",focus.origin},{"keyboard_focus",focus.keyboard_focus},{"input_enabled",focus.input_enabled},
                {"ready",focus.ready},{"error",focus.error}};
            if(!ok||!read)response["error"]={{"code","TUTORIAL_MOVEMENT_FOCUS_REJECTED"},{"message",error}};
        }
        else if(operation=="tutorial_movement"||operation=="read_tutorial_movement") {
            if(!tutorial_movement_ready||!owned_native_profile||!Config::instance().local_test)
                throw std::runtime_error("tutorial movement requires initialized owned local test");
            const auto& args=request.at("args");
            if(operation=="read_tutorial_movement") {
                if(!args.is_object()||!args.empty())throw std::runtime_error("tutorial movement read requires empty arguments");
                response["ok"]=true;
            } else {
                if(!Config::instance().is_host||!args.is_object()||args.size()!=2||!args.at("acceptance_token").is_string()||!args.at("native_origin_uuid").is_string())
                    throw std::runtime_error("tutorial movement requires Host and exact token/origin fields");
                std::string error;
                response["ok"]=game::tutorial_movement::start(args.at("acceptance_token").get<std::string>(),args.at("native_origin_uuid").get<std::string>(),completed_game_updates,error);
                if(!response["ok"].get<bool>())response["error"]={{"code","TUTORIAL_MOVEMENT_REJECTED"},{"message",error}};
            }
            response["result"]=tutorial_movement_receipt();
        }
        else if(operation=="tutorial_welcome_ok"||operation=="tutorial_finish_ok"||operation=="tutorial_network_tab"||operation=="tutorial_leveltwo_message_ok") {
            if(!owned_native_profile||!Config::instance().is_host)throw std::runtime_error("tutorial welcome diagnostic requires owned Host profile");
            const auto& args=request.at("args");
            const bool leveltwo=operation=="tutorial_leveltwo_message_ok";
            const int32_t expected_index=operation=="tutorial_welcome_ok"?4:operation=="tutorial_finish_ok"?103:56;
            if(!args.is_object()||args.size()!=5||!args.at("token").is_string()||!args.at("snapshot_uuid").is_string()||
               !args.at("world_epoch").is_number_unsigned()||!args.at("world_generation").is_number_unsigned()||!args.at("current_step_index").is_number_unsigned()||(leveltwo?(args.at("current_step_index").get<uint64_t>()>14||!game::tutorial_welcome::leveltwo_message(args.at("current_step_index").get<int32_t>())):args.at("current_step_index")!=expected_index))
                throw std::runtime_error("tutorial welcome requires exact token/origin/index fields");
            game::tutorial_welcome::Request close{args.at("token").get<std::string>(),args.at("snapshot_uuid").get<std::string>(),
                args.at("world_epoch").get<uint64_t>(),args.at("world_generation").get<uint64_t>(),args.at("current_step_index").get<int32_t>(),leveltwo};
            std::string error;json receipt;
            response["ok"]=operation=="tutorial_network_tab"?game::tutorial_welcome::network_tab(close,receipt,error):game::tutorial_welcome::close(close,receipt,error);response["result"]=std::move(receipt);
            if(!response["ok"].get<bool>())response["error"]={{"code",operation=="tutorial_leveltwo_message_ok"?"TUTORIAL_LEVELTWO_MESSAGE_REJECTED":operation=="tutorial_welcome_ok"?"TUTORIAL_WELCOME_REJECTED":operation=="tutorial_finish_ok"?"TUTORIAL_FINISH_REJECTED":"TUTORIAL_NETWORK_TAB_REJECTED"},{"message",error}};
        }
        else if(operation=="read_tutorial") {
            if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("tutorial read requires empty object arguments");
            std::string error;json observed;
            response["ok"]=game::tutorial_observer::read(observed,error);
            if(response["ok"].get<bool>())response["result"]=std::move(observed);
            else response["error"]={{"code","TUTORIAL_READ_REJECTED"},{"message",error}};
        }
        else if(operation=="read_tutorial_progress") {
            if(!owned_native_profile)throw std::runtime_error("tutorial progress read requires owned native profile");
            if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("tutorial progress read requires empty object arguments");
            std::string error;json observed;
            response["ok"]=game::tutorial_replication::read_progress(observed,error);
            if(response["ok"].get<bool>())response["result"]=std::move(observed);
            else response["error"]={{"code","TUTORIAL_PROGRESS_READ_REJECTED"},{"message",error}};
        }
        else if(operation=="read_tutorial_development") {
            if(!owned_native_profile)throw std::runtime_error("development read requires owned native profile");
            if(!request.at("args").is_object()||!request.at("args").empty())throw std::runtime_error("development read requires empty object arguments");
            std::string error;json observed;
            response["ok"]=game::tutorial_development::read(observed,error);
            if(response["ok"].get<bool>())response["result"]=std::move(observed);
            else response["error"]={{"code","TUTORIAL_DEVELOPMENT_READ_REJECTED"},{"message",error}};
        }
        else if(operation=="read_network_state") {
            if(!request.at("args").empty()) throw std::runtime_error("network read requires empty arguments");
            game::native_network::State network; std::string error;
            response["ok"]=game::native_network::read_state(network,error);
            if(response["ok"].get<bool>()) response["result"]=game::native_network::state_json(network);
            else response["error"]={{"code","NETWORK_READ_REJECTED"},{"message",error}};
        }
        else if(operation=="append_subscriber_record_probe") {
            const auto& args=request.at("args");
            if(!args.is_object()||args.size()!=3||!args.contains("artifact_basename")||!args.contains("artifact_sha256")||!args.contains("native_origin_uuid")||
               !args.at("artifact_basename").is_string()||!args.at("artifact_sha256").is_string()||!args.at("native_origin_uuid").is_string())
                throw std::runtime_error("append_subscriber_record_probe requires artifact_basename, artifact_sha256, native_origin_uuid");
            std::string error;json receipt;
            response["ok"]=game::subscriber_lifecycle_append::probe(runtime_root,args.at("artifact_basename").get<std::string>(),
                args.at("artifact_sha256").get<std::string>(),args.at("native_origin_uuid").get<std::string>(),receipt,error);
            response["result"]=std::move(receipt);
            if(!response["ok"].get<bool>())response["error"]={{"code","SUBSCRIBER_APPEND_REJECTED"},{"message",error}};
        }
        else if(operation=="capture_subscriber_record") {
            const auto& args=request.at("args");
            if(!args.is_object()||args.size()!=2||!args.contains("serialized_id")||!args.contains("native_origin_uuid"))
                throw std::runtime_error("capture_subscriber_record requires serialized_id and native_origin_uuid");
            const auto& subscriber_id=args.at("serialized_id");
            if(!subscriber_id.is_number_integer()||subscriber_id.is_boolean()||
               (subscriber_id.is_number_unsigned()&&subscriber_id.get<uint64_t>()>INT32_MAX)||
               (!subscriber_id.is_number_unsigned()&&(subscriber_id.get<int64_t>()<0||subscriber_id.get<int64_t>()>INT32_MAX)))
                throw std::runtime_error("serialized_id requires int32 range 0..2147483647");
            if(!args.at("native_origin_uuid").is_string())
                throw std::runtime_error("native_origin_uuid requires canonical UUID text");
            std::string error; nlohmann::json receipt;
            response["ok"]=game::subscriber_lifecycle_capture::capture(
                runtime_root,subscriber_id.get<int32_t>(),args.at("native_origin_uuid").get<std::string>(),receipt,error);
            response["result"]=std::move(receipt);
            if(!response["ok"].get<bool>())
                response["error"]={{"code","SUBSCRIBER_CAPTURE_REJECTED"},{"message",error}};
        }
        else if(operation=="read_subscriber_weapon") {
            const auto& args=request.at("args");
            if(!args.is_object()||args.size()!=1||!args.contains("serialized_id"))
                throw std::runtime_error("read_subscriber_weapon requires exactly serialized_id");
            const auto& number=args.at("serialized_id");
            if(!number.is_number_integer()||number.is_boolean()||
               (number.is_number_unsigned()&&number.get<uint64_t>()>INT32_MAX)||
               (!number.is_number_unsigned()&&(number.get<int64_t>()<0||number.get<int64_t>()>INT32_MAX)))
                throw std::runtime_error("subscriber ID requires int32 range 0..2147483647");
            std::string error;json observed;
            response["ok"]=game::typed_action_runtime::read_subscriber_weapon(number.get<int32_t>(),observed,error);
            response["result"]=std::move(observed);
            if(!response["ok"].get<bool>())response["error"]={{"code","SUBSCRIBER_WEAPON_READ_REJECTED"},{"message",error}};
        }
        else if(operation=="read_subscriber_needs") {
            const auto& number=request.at("args").at("serialized_id");
            if(!number.is_number_integer()||number.get<int64_t>()<0||number.get<int64_t>()>INT32_MAX)
                throw std::runtime_error("subscriber ID requires int32 range 0..2147483647");
            game::native::SubscriberNeedsState subscriber; std::string error;
            response["ok"]=game::native::read_subscriber_needs(number.get<int32_t>(),subscriber,error);
            if(response["ok"].get<bool>()) response["result"]={{"serialized_id",subscriber.serialized_id},{"subscriber_count",subscriber.subscriber_count},
                {"satisfaction",subscriber.satisfaction},{"addiction",subscriber.addiction},{"logged_in",subscriber.logged_in},{"logged_in_time",subscriber.logged_in_seconds},{"needs",subscriber.needs}};
            else response["error"]={{"code","SUBSCRIBER_READ_REJECTED"},{"message",error}};
        }
        else if(operation=="arm_subscriber_needs"||operation=="apply_subscriber_needs"||operation=="disarm_subscriber_needs") {
            auto& cfg=Config::instance();
            if(!cfg.local_test||cfg.is_host) throw std::runtime_error("subscriber needs diagnostic requires an owned local client");
            const auto& args=request.at("args");
            const uint64_t epoch=positive_integer(args.at("epoch"),"epoch");
            std::string error;
            if(operation=="disarm_subscriber_needs") {
                if(args.size()!=1) throw std::runtime_error("disarm requires only epoch");
                response["ok"]=game::native::disarm_subscriber_needs_replica(epoch,error);
                if(response["ok"].get<bool>()) response["result"]={{"state","selected_subscriber_disarmed"},{"epoch",epoch}};
            } else {
                const auto& number=args.at("serialized_id");
                if(!number.is_number_integer()||number.get<int64_t>()<0||number.get<int64_t>()>INT32_MAX) throw std::runtime_error("subscriber ID requires int32 range");
                const auto subscriber_id=number.get<int32_t>();
                if(operation=="arm_subscriber_needs") {
                    if(args.size()!=3) throw std::runtime_error("arm requires epoch, serialized_id and expected_initial");
                    response["ok"]=game::native::arm_subscriber_needs_replica(epoch,subscriber_id,subscriber_values(args.at("expected_initial"),"expected_initial"),error);
                    if(response["ok"].get<bool>()) response["result"]={{"state","selected_subscriber_armed"},{"epoch",epoch},{"serialized_id",subscriber_id}};
                } else {
                    if(args.size()!=5) throw std::runtime_error("apply requires epoch, serialized_id, revision, expected_before and host_needs");
                    game::native::SubscriberNeedsReceipt receipt;
                    const uint64_t revision=positive_integer(args.at("revision"),"revision");
                    response["ok"]=game::native::apply_subscriber_needs(epoch,subscriber_id,revision,subscriber_values(args.at("expected_before"),"expected_before"),
                        subscriber_values(args.at("host_needs"),"host_needs"),receipt,error);
                    response["result"]={{"epoch",receipt.epoch},{"revision",receipt.revision},{"serialized_id",receipt.serialized_id},
                        {"written",receipt.written},{"already_applied",receipt.already_applied},{"before_readable",receipt.before_readable},{"after_readable",receipt.after_readable},
                        {"attempted_setters",receipt.attempted_setters},{"completed_setters",receipt.completed_setters},{"native_exception",receipt.native_exception},
                        {"before",receipt.before},{"after",receipt.after}};
                }
            }
            if(!response["ok"].get<bool>()) response["error"]={{"code","SUBSCRIBER_REPLICA_REJECTED"},{"message",error}};
        }
        else if(operation=="purchase_test_region") {
            const auto& args=request.at("args"); game::native::RegionState region; std::string error;
            response["ok"]=Config::instance().is_host&&game::native::diagnostic_purchase_region(args.at("x"),args.at("z"),region,error);
            if(response["ok"].get<bool>()) response["result"]={{"state","test_region_owned"},{"type",region.type},{"owned",region.owned},{"purchase_price",region.purchase_price},{"cash",region.cash},{"building_manager_present",region.building_manager_present},{"path_manager_present",region.path_manager_present},{"engine",status()}};
            else response["error"]={{"code","REGION_PURCHASE_REJECTED"},{"message",Config::instance().is_host?error:"Testregion wird ausschließlich beim Host gekauft"}};
        }
        else if(operation=="focus_test_camera") {
            const auto& args=request.at("args"); game::native::CameraState camera; std::string error;
            response["ok"]=game::native::diagnostic_focus_camera(args.at("x"),args.at("y"),args.at("z"),args.at("distance"),camera,error);
            if(response["ok"].get<bool>()) { pending_camera={{"id",id},{"x",args.at("x")},{"y",args.at("y")},{"z",args.at("z")},{"distance",args.at("distance")}}; camera_started=now; last_request=id; return; }
            response["error"]={{"code","CAMERA_REJECTED"},{"message",error}};
        }
        else if(operation=="place_test_building") {
            std::string error;
            response["ok"]=net::begin_diagnostic_placement(id,request.at("args"),error);
            if(response["ok"].get<bool>()) { pending_placement={{"id",id}}; last_request=id; return; }
            response["error"]={{"code","PLACEMENT_REJECTED"},{"message",error}};
        }
        else if(operation=="set_time_scale") {
            float scale=request.at("args").at("scale").get<float>(); std::string error;
            response["ok"]=game::native::set_clock(scale==0,scale,error);
            if(response["ok"].get<bool>()) response["result"]=status();
            else response["error"]={{"code","ENGINE_REJECTED"},{"message",error}};
        }
        else if(operation=="toggle_overlay") {
            bool previous=overlay::LobbyOverlay::instance().is_visible();
            SDL_Event event{}; event.type=SDL_EVENT_KEY_DOWN; event.key.windowID=SDL_GetWindowID(hooks::get_game_window()); event.key.scancode=SDL_SCANCODE_F10; event.key.key=SDLK_F10; event.key.down=true;
            if(!SDL_PushEvent(&event)) throw std::runtime_error("SDL F10 down queue failed: "+std::string(SDL_GetError()));
            event.type=SDL_EVENT_KEY_UP; event.key.down=false;
            if(!SDL_PushEvent(&event)) throw std::runtime_error("SDL F10 up queue failed: "+std::string(SDL_GetError()));
            pending_overlay={{"id",id},{"previous",previous}}; overlay_started=now; last_request=id; return;
        }
        else if(operation=="create_test_world") {
            // Keep diagnostic world folders short enough for native metadata paths.
            std::string name="MPTest"+id.substr(0,16); std::string error;
            response["ok"]=game::native::prepare_test_world(name,error);
            if(response["ok"].get<bool>()) { pending_world={{"id",id},{"name",name},{"state","preparing"}}; world_started=now; last_request=id; LOG_INFO("[MCP] Test world preparation requested name={}",name); return; }
            response["error"]={{"code","ENGINE_REJECTED"},{"message",error}};
        }
        else if(operation=="capture_frame") {
            pending_frame={{"id",id}}; last_request=id; return;
        }
        else if(operation=="capture_snapshot"||operation=="capture_local_snapshot") {
            if(!request.at("args").empty())throw std::runtime_error("snapshot capture requires empty arguments");
            const bool distribute=operation=="capture_snapshot"&&Config::instance().is_host;
            snapshot_network_observer_begun=distribute;
            auto current=status(); std::string error;
            auto actual_root=current.at("write_dir").get<std::string>();
            if(actual_root.empty()) throw std::runtime_error("PHYSFS write directory unavailable");
            std::string world_name;
            if(!game::native::read_active_world_folder(world_name,error)) throw std::runtime_error(error);
            snapshot_path=net::snapshot_world_path::physical(std::filesystem::path(actual_root),world_name,id);
            std::filesystem::create_directories(snapshot_path.parent_path());
            const bool bind_snapshot_identities=distribute||net::current_snapshot_id().empty()||!net::current_world_epoch();
            if(!game::building_capture_identity::begin(id,Config::instance().session_id,error,world_name,bind_snapshot_identities)) throw std::runtime_error("building capture begin failed before native save: "+error);
            snapshot_observer_begun=true;
            building_capture_original_start=building_original_attempted.load();
            building_capture_return_start=building_original_returned.load();
            building_capture_failure_start=building_original_failed.load();
            if(distribute) {
                game::native_network::State nodes;
                if(game::native_network::read_state(nodes,error)) {
                    if(nodes.node_slots<nodes.null_node_slots) throw std::runtime_error("network node census invalid slots="+std::to_string(nodes.node_slots)+" null="+std::to_string(nodes.null_node_slots));
                    if(!game::network_identity::begin_host_capture(id,nodes.node_slots-nodes.null_node_slots,error,world_name))
                        LOG_ERROR("[MCP] Network capture begin rejected id={} cause={}; native file capture continues",id,error);
                } else {
                    game::network_identity::cancel_pending("network census failed before native capture: "+error);
                    LOG_ERROR("[MCP] Network census failed id={} cause={}; native file capture continues",id,error);
                }
            }
            response["ok"]=game::native::capture_snapshot(net::snapshot_world_path::capture(world_name,id),error);
            if(response["ok"].get<bool>()) {
                pending_snapshot={{"id",id},{"snapshot_id",id},{"world_name",world_name},{"distribute",distribute}}; snapshot_started=now; last_request=id;
                LOG_INFO("[MCP] Snapshot pending: request={} target={}",id,snapshot_path.string()); return;
            }
            game::building_capture_identity::cancel(error);
            if(distribute)game::network_identity::cancel_pending(error);
            response["error"]={{"code","ENGINE_REJECTED"},{"message",error}};
        }
        else if(operation=="load_snapshot") {
            auto snapshot_id=request.at("args").at("snapshot_id").get<std::string>();
            if(snapshot_id.size()!=36||snapshot_id.find_first_not_of("0123456789abcdef-")!=std::string::npos) throw std::runtime_error("invalid snapshot id");
            auto manifest=dir/("snapshot-"+snapshot_id+".json");
            std::ifstream stream(manifest); json record; stream>>record;
            auto actual_root=status().at("write_dir").get<std::string>();
            const auto world_name=net::snapshot_world_path::from_manifest(record);
            auto path=net::snapshot_world_path::physical(std::filesystem::path(actual_root),world_name,snapshot_id);
            if(record.at("snapshot_id").get<std::string>()!=snapshot_id) throw std::runtime_error("local snapshot manifest UUID differs");
            if(record.contains("world_name")&&record.contains("path")&&std::filesystem::weakly_canonical(net::snapshot_world_path::utf8_path(record.at("path").get<std::string>()))!=std::filesystem::weakly_canonical(path)) throw std::runtime_error("local snapshot manifest path differs from owned world folder");
            const auto expected=record.at("bytes").get<uint64_t>();
            if(expected==0||expected>net::snapshot_max_bytes||std::filesystem::file_size(path)!=expected) throw std::runtime_error("local snapshot size differs from manifest");
            std::string error;
            std::string digest; uint64_t length=0;
            if(!net::hash_snapshot_file(path,digest,length,error)) throw std::runtime_error("local snapshot hash failed: "+error);
            if(length!=expected||digest!=record.at("sha256").get<std::string>()) throw std::runtime_error("local snapshot incomplete or SHA256 differs");
            uint64_t origin_generation=0;
            if(!game::native_open_runtime::prepare(runtime_root,snapshot_id,digest,expected,origin_generation,error,world_name))
                throw std::runtime_error("native snapshot origin preparation failed: "+error);
            try {
            if(!suspend_time("MCP verified local snapshot replaces world",error)) throw std::runtime_error("time authority retire before load failed: "+error);
            game::network_identity::invalidate_world("MCP verified local snapshot replaces world");
            net::invalidate_snapshot_world("MCP verified local snapshot replaces world");
            if(!game::normal_load_identity::begin(record.at("sha256").get<std::string>(),snapshot_id,error)) throw std::runtime_error(error);
            } catch(...) {
                std::string cleanup_error;
                if(!game::native_open_runtime::retire_unstarted(origin_generation,cleanup_error))
                    LOG_ERROR("[NativeOpen] Unstarted local load retirement failed generation={} cause={}",origin_generation,cleanup_error);
                throw;
            }
            if(!game::native_open_runtime::before_request(origin_generation,error)) throw std::runtime_error("native origin request binding failed: "+error);
            response["ok"]=game::native::request_load(net::snapshot_world_path::load(world_name,snapshot_id),error);
            if(response["ok"].get<bool>()) { pending_load={{"id",id},{"snapshot_id",snapshot_id},{"origin_generation",origin_generation}}; load_watch.begin(now); last_request=id; return; }
            else { game::normal_load_identity::cancel(error); response["error"]={{"code","ENGINE_REJECTED"},{"message",error}}; }
        }
        else { response["ok"]=false; response["error"]={{"code","CAPABILITY_UNAVAILABLE"},{"message","Engine-Eingriff noch nicht bewiesen: "+operation}}; }
        respond(dir,response);
        LOG_INFO("[MCP] Request completed: id={} op={} ok={}",id,operation,response["ok"].get<bool>());
    } catch(const std::exception& error) {
        if(diagnostic_failure!=error.what()) {
            diagnostic_failure=error.what(); diagnostic_failure_count=0;
            LOG_ERROR("[MCP] Diagnostics failed run={}: {}",run_id,diagnostic_failure);
        }
        ++diagnostic_failure_count;
        if(!deferred_response.is_null()) return;
        json* pending=!pending_snapshot.is_null()?&pending_snapshot:!pending_world.is_null()?&pending_world:!pending_overlay.is_null()?&pending_overlay:!pending_crash_dismiss.is_null()?&pending_crash_dismiss:!pending_placement.is_null()?&pending_placement:!pending_load.is_null()?&pending_load:!pending_camera.is_null()?&pending_camera:nullptr;
        if(pending||!request_in_progress.empty()) {
            json failure={{"schema_version",1},{"id",pending?pending->at("id"):json(request_in_progress)},{"run_id",run_id},{"ok",false},{"error",{{"code","DIAGNOSTIC_FAILED"},{"message",diagnostic_failure}}}};
            if(pending==&pending_load) {
                load_watch.finish();
                game::normal_load_identity::cancel(diagnostic_failure);
            }
            if(pending==&pending_snapshot||snapshot_observer_begun) {
                game::building_capture_identity::cancel(diagnostic_failure);
                if((pending==&pending_snapshot&&pending_snapshot.value("distribute",Config::instance().is_host))||snapshot_network_observer_begun)
                    game::network_identity::cancel_pending(diagnostic_failure);
            }
            if(pending) *pending=nullptr;
            try { respond(std::filesystem::path(runtime_root)/"diagnostics",failure); }
            catch(const std::exception& publication_error) { LOG_ERROR("[MCP] Failure response publication failed run={}: {}",run_id,publication_error.what()); }
        }
    }
}
static void publish_installed_proof(){
    const auto* profile=installed_profile::get();
    const auto& proof=installed_profile::diagnostic_root();
    if(!profile||proof.empty())return;
    FILETIME creation{},exit{},kernel{},user{};
    if(!GetProcessTimes(GetCurrentProcess(),&creation,&exit,&kernel,&user))throw std::runtime_error("drop-in proof process start query failed Win32="+std::to_string(GetLastError()));
    const auto start=std::to_string((uint64_t(creation.dwHighDateTime)<<32)|creation.dwLowDateTime);
    const auto exe=private_profile::module_file(nullptr);
    if(!private_profile::equal(exe,proof/L"game/MT2.exe"))throw std::runtime_error("drop-in proof executable identity differs");
    json modules=json::object();
    const std::pair<const char*,HMODULE> actual[]={{"executable",GetModuleHandleW(nullptr)},
        {"loader",GetModuleHandleW((profile->game_dir/L"WSOCK32.dll").c_str())},{"mod",g_module},{"steam",net::steam_module()}};
    const std::map<std::string,std::filesystem::path> expected={{"executable",exe},{"loader",profile->game_dir/L"WSOCK32.dll"},
        {"mod",profile->game_dir/L"MT2_Multiplayer.dll"},{"steam",profile->game_dir/L"steam_api64.dll"}};
    for(const auto& [name,module]:actual){
        if(!module)throw std::runtime_error("drop-in proof loaded module missing: "+std::string(name));
        const auto path=private_profile::module_file(module);
        if(!private_profile::equal(path,expected.at(name)))throw std::runtime_error("drop-in proof actual module path differs: "+std::string(name));
        std::string hash,error;uint64_t bytes=0;
        if(!net::hash_snapshot_file(path,hash,bytes,error)||!bytes)throw std::runtime_error("drop-in proof module hash failed: "+std::string(name)+" cause="+error);
        modules[name]={{"path",path.string()},{"sha256",hash},{"bytes",bytes}};
    }
    installed_proof_receipt={{"acceptance_root",proof.string()},{"game_dir",profile->game_dir.string()},
        {"runtime_root",profile->runtime_root.string()},{"user_data_dir",profile->user_data_dir.string()},
        {"diagnostic_mailbox_enabled",Config::instance().diagnostic_mailbox_enabled},{"local_test",Config::instance().local_test},
        {"isolation_flags",3},{"process_start_time",start},{"modules",modules}};
    if(!Config::instance().diagnostic_mailbox_enabled||Config::instance().local_test)throw std::runtime_error("drop-in proof diagnostic policy differs");
    json manifest={{"schema_version",1},{"profile","installed_dropin_proof"},{"run_id",run_id},{"pid",GetCurrentProcessId()},
        {"process_start_time",start},{"runtime_root",profile->runtime_root.string()},{"executable_path",exe.string()},
        {"owner",{{"kind","MPTest"},{"run_id",run_id}}},{"installed_proof",installed_proof_receipt}};
    write_json(profile->runtime_root/L"instance.json",manifest);
    LOG_INFO("[DropinProof] Owned installed manifest published root={} local_test=false",profile->runtime_root.string());
}
bool initialize(HMODULE module) {
    try {
        g_module=module; init_paths(); Logger::instance().init(Config::instance().installed_profile_verified?runtime_root:Config::instance().game_dir);
        if(owned_native_profile) {
            wchar_t dll_path[32768];
            const auto length=GetModuleFileNameW(module,dll_path,32768);
            if(!length||length>=32768)throw std::runtime_error("loaded DLL path query failed Win32="+std::to_string(GetLastError()));
            uint64_t bytes=0;std::string hash_error;
            if(!net::hash_snapshot_file(std::filesystem::path(dll_path),loaded_dll_sha256,bytes,hash_error))
                throw std::runtime_error("loaded DLL hash failed: "+hash_error);
            if(!bytes)throw std::runtime_error("loaded DLL file is empty");
        }
        LOG_INFO("MT2_Multiplayer SDL3 runtime initialization");
        if(!hooks::init()) throw std::runtime_error("MinHook initialization failed");
        std::string native_error;
        if(!game::native::initialize(native_error)) throw std::runtime_error(native_error);
        if(!hooks::create_hook(game::native::entrypoint(game::native::EntryPoint::GameUpdate),reinterpret_cast<void*>(&game_update),reinterpret_cast<void**>(&original_game_update))) throw std::runtime_error("native game Update hook failed");
        if(owned_native_profile) {
            MODULEINFO image_info{};
            const auto native_module=GetModuleHandleW(nullptr);
            if(!native_module || !K32GetModuleInformation(GetCurrentProcess(),native_module,&image_info,sizeof(image_info)))
                throw std::runtime_error("native caller image bounds query failed error="+std::to_string(GetLastError()));
            native_image_begin=reinterpret_cast<uintptr_t>(image_info.lpBaseOfDll);
            if(!native_image_begin || !image_info.SizeOfImage || image_info.SizeOfImage>std::numeric_limits<uintptr_t>::max()-native_image_begin)
                throw std::runtime_error("native caller image bounds invalid");
            native_image_end=native_image_begin+image_info.SizeOfImage;
            if(!game::render_pool_guard::initialize(native_image_begin,image_info.SizeOfImage,native_error))
                throw std::runtime_error("native render pool guard initialization failed: "+native_error);
            if(!game::tutorial_replication::initialize(native_image_begin,native_error))throw std::runtime_error("tutorial initialization failed: "+native_error);
            if(!game::native_open_runtime::initialize(runtime_root,native_image_begin,native_error))
                throw std::runtime_error("native snapshot origin initialization failed: "+native_error);
            if(!hooks::create_hook(game::native::entrypoint(game::native::EntryPoint::ClockUpdate),reinterpret_cast<void*>(&clock_update),reinterpret_cast<void**>(&original_clock_update))) throw std::runtime_error("native pending-load clock pause hook failed");
            if(!game::normal_load_identity::initialize(native_error)) throw std::runtime_error(native_error);
            if(!game::building_capture_identity::initialize(native_error)) throw std::runtime_error(native_error);
            if(!game::root_building_load::initialize(native_error)) throw std::runtime_error(native_error);
            if(!hooks::create_hook(game::root_building_load::entrypoint(0),reinterpret_cast<void*>(&load_building_array),reinterpret_cast<void**>(&original_load_building_array))||
               !hooks::create_hook(game::root_building_load::entrypoint(1),reinterpret_cast<void*>(&building_reader_children),reinterpret_cast<void**>(&original_building_reader_children))||
               !hooks::create_hook(game::root_building_load::entrypoint(2),reinterpret_cast<void*>(&inline_building),reinterpret_cast<void**>(&original_inline_building)))
                throw std::runtime_error("native Building array/reader/inline observation hooks failed");
            if(!game::root_building_load::confirm_hooks(7,native_error)) throw std::runtime_error(native_error);
            if(!game::npc_quest_target_runtime::initialize(native_error))throw std::runtime_error(native_error);
            if(!hooks::create_hook(game::npc_quest_target_runtime::entrypoint(true),reinterpret_cast<void*>(&save_zone_property),reinterpret_cast<void**>(&original_save_zone_property))||
               !hooks::create_hook(game::npc_quest_target_runtime::entrypoint(false),reinterpret_cast<void*>(&load_zone_property),reinterpret_cast<void**>(&original_load_zone_property)))
                throw std::runtime_error("native Zone property observation hooks failed");
            if(!game::npc_quest_target_runtime::confirm_hooks(native_error))throw std::runtime_error(native_error);
            if(!game::npc_saved_identity_runtime::initialize(native_error))throw std::runtime_error(native_error);
            if(!hooks::create_hook(game::npc_saved_identity_runtime::entrypoint(true),reinterpret_cast<void*>(&save_npc_property),reinterpret_cast<void**>(&original_save_npc_property))||
               !hooks::create_hook(game::npc_saved_identity_runtime::entrypoint(false),reinterpret_cast<void*>(&load_npc_property),reinterpret_cast<void**>(&original_load_npc_property)))
                throw std::runtime_error("native NPC property observation hooks failed");
            if(!game::npc_saved_identity_runtime::confirm_hooks(native_error))throw std::runtime_error(native_error);
            if(!game::bug_authority::initialize(native_error))throw std::runtime_error(native_error);
            if(!hooks::create_hook(game::building_capture_identity::capture_entrypoint(),reinterpret_cast<void*>(&save_building_array),reinterpret_cast<void**>(&original_save_building_array))) throw std::runtime_error("native building save array observation hook failed");
            if(!game::network_identity::initialize(native_error)) throw std::runtime_error(native_error);
            if(!game::subscriber_lifecycle_append::initialize(native_image_begin,native_error)) throw std::runtime_error(native_error);
            if(!game::subscriber_retirement::initialize(native_image_begin,native_error))throw std::runtime_error(native_error);
            if(!hooks::create_hook(game::subscriber_retirement::entrypoint(),reinterpret_cast<void*>(&subscriber_remove),reinterpret_cast<void**>(&original_subscriber_remove)))throw std::runtime_error("native Subscriber Manager retirement observation hook failed");
            if(!hooks::create_hook(game::subscriber_lifecycle_append::lookup_entrypoint(),reinterpret_cast<void*>(&subscriber_context_lookup),reinterpret_cast<void**>(&original_subscriber_context_lookup)))
                throw std::runtime_error("native Subscriber private-context lookup hook failed");
            if(!hooks::create_hook(game::network_identity::capture_entrypoint(),reinterpret_cast<void*>(&save_network_nodes),reinterpret_cast<void**>(&original_save_network_nodes))) throw std::runtime_error("native network save property observation hook failed");
            if(!hooks::create_hook(game::normal_load_identity::entrypoint(),reinterpret_cast<void*>(&add_object),reinterpret_cast<void**>(&original_add_object))) throw std::runtime_error("native normal-load reference observation hook failed");
            if(!hooks::create_hook(game::native::entrypoint(game::native::EntryPoint::SoftwareTick),reinterpret_cast<void*>(&software_tick),reinterpret_cast<void**>(&original_software_tick))) throw std::runtime_error("native software Tick observation hook failed");
            if(!hooks::create_hook(game::native::entrypoint(game::native::EntryPoint::SoftwareTickerTick),reinterpret_cast<void*>(&software_ticker_tick),reinterpret_cast<void**>(&original_software_ticker_tick))) throw std::runtime_error("native software ticker observation hook failed");
            if(!hooks::create_hook(game::native::entrypoint(game::native::EntryPoint::SoftwareCalculateRating),reinterpret_cast<void*>(&software_rating),reinterpret_cast<void**>(&original_software_rating))) throw std::runtime_error("native rating calculation hook failed");
            if(!hooks::create_hook(game::native::entrypoint(game::native::EntryPoint::SoftwareCalculateFun),reinterpret_cast<void*>(&software_fun),reinterpret_cast<void**>(&original_software_fun))) throw std::runtime_error("native fun calculation hook failed");
            if(!game::live_host_state::day::prepare_original_guards(native_error))throw std::runtime_error("native Day preparation: "+native_error);
            if(!game::live_host_state::economic_fence().prepare_before_hooks(native_error))throw std::runtime_error("native economic preparation: "+native_error);
            if(!game::live_host_state::economic_fence().bind_existing_day_routes(reinterpret_cast<uintptr_t>(&business_day),reinterpret_cast<uintptr_t>(&subscriber_day),native_error))throw std::runtime_error("native economic existing routes: "+native_error);
            if(!hooks::create_hook(game::native::entrypoint(game::native::EntryPoint::BusinessNextDayFinances),reinterpret_cast<void*>(&business_day),reinterpret_cast<void**>(&original_business_day))) throw std::runtime_error("native business day observation hook failed");
            if(!hooks::create_hook(game::native::entrypoint(game::native::EntryPoint::SubscriberNewDay),reinterpret_cast<void*>(&subscriber_day),reinterpret_cast<void**>(&original_subscriber_day))) throw std::runtime_error("native subscriber day observation hook failed");
            if(!hooks::create_hook(game::native::entrypoint(game::native::EntryPoint::NeedSetSet),reinterpret_cast<void*>(&need_set),reinterpret_cast<void**>(&original_need_set))) throw std::runtime_error("native NeedSet Set hook failed");
            if(!hooks::create_hook(game::native::entrypoint(game::native::EntryPoint::NeedSetIncrement),reinterpret_cast<void*>(&need_increment),reinterpret_cast<void**>(&original_need_increment))) throw std::runtime_error("native NeedSet Increment hook failed");
            if(!hooks::create_hook(game::native::entrypoint(game::native::EntryPoint::NeedSetAddAssign),reinterpret_cast<void*>(&need_add),reinterpret_cast<void**>(&original_need_add))) throw std::runtime_error("native NeedSet += hook failed");
            if(!hooks::create_hook(game::native::entrypoint(game::native::EntryPoint::NeedSetSubtractAssign),reinterpret_cast<void*>(&need_subtract),reinterpret_cast<void**>(&original_need_subtract))) throw std::runtime_error("native NeedSet -= hook failed");
            if(!hooks::create_hook(game::native::entrypoint(game::native::EntryPoint::SubscriberBrainTick),reinterpret_cast<void*>(&brain_tick),reinterpret_cast<void**>(&original_brain_tick))) throw std::runtime_error("native subscriber Brain Tick observation hook failed");
            if(!hooks::create_hook(game::native::entrypoint(game::native::EntryPoint::SubscriberUpdateNeeds),reinterpret_cast<void*>(&update_needs),reinterpret_cast<void**>(&original_update_needs))) throw std::runtime_error("native subscriber UpdateNeeds observation hook failed");
        }
        if(!hooks::install_renderer_hooks()) throw std::runtime_error("SDL_GL_SwapWindow hook failed");
        if(!hooks::install_input_hooks()) throw std::runtime_error("SDL3 input hooks failed");
        if(!hooks::install_window_hooks()) throw std::runtime_error("SDL3 window hooks failed");
        hooks::set_render_callback([]() {
            DWORD thread=GetCurrentThreadId();
            if(!render_thread) { render_thread=thread; LOG_INFO("[RUNTIME] Game render thread={}",thread); }
            overlay::NotificationOverlay::instance().update(1.0f/60.0f);
            overlay::GLOverlay::instance().render();
            capture_frame();
        });
        if(owned_native_profile) {
            net::initialize_snapshot_sync();
            net::initialize_software_sync();
            { std::string chat_error; if (!net::chat::Runtime::instance().initialize(chat_error)) throw std::runtime_error("Chat-Kanal: " + chat_error); }
            net::initialize_diagnostic_placement();
            if(Config::instance().local_test&&!net::start_local_session()) throw std::runtime_error("local transport initialization failed");
        }
        initialized=true;
        if(!hooks::enable_all()) { initialized=false; throw std::runtime_error("hook activation failed"); }
        if(owned_native_profile&&!game::native_open_runtime::hooks_enabled(native_error)) {
            initialized=false; throw std::runtime_error("native snapshot origin hook confirmation failed: "+native_error);
        }
        if(owned_native_profile&&!game::tutorial_replication::hooks_enabled(native_error)) {
            initialized=false; throw std::runtime_error("tutorial hook confirmation failed: "+native_error);
        }
        if(owned_native_profile) game::native::set_subscriber_needs_hook_mask(0x3f);
        if(owned_native_profile) game::subscriber_lifecycle_append::set_lookup_hook_ready(true);
        publish_installed_proof();
        LOG_INFO("[RUNTIME] REQUIRED_HOOKS_READY SDL3 input renderer window"); return true;
    } catch(const std::exception& error) { LOG_ERROR("[RUNTIME] Initialization failed: {}",error.what()); return false; }
    catch(...) { LOG_ERROR("[RUNTIME] Initialization failed: unknown exception"); return false; }
}
void shutdown() {
    if(!initialized.exchange(false)) return;
    if(time_initialized) {
        if(GetCurrentThreadId()==engine_thread) {
            std::string error;
            if(!suspend_time("runtime shutdown",error)) LOG_ERROR("[TimeIntegration] Shutdown retire failed cause={}",error);
            if(!game::time_integration::instance().set_hook_mask(0,error)) LOG_ERROR("[TimeIntegration] Shutdown hook mask failed cause={}",error);
        } else LOG_WARN("[TimeIntegration] Shutdown authority callbacks skipped: thread={} required_game_thread={}; process hooks are removed",GetCurrentThreadId(),engine_thread);
    }
    LOG_INFO("[RUNTIME] Shutdown"); net::SidecarClient::instance().shutdown();
    game::subscriber_lifecycle_append::set_lookup_hook_ready(false);
    game::native::set_subscriber_needs_hook_mask(0);
    hooks::disable_all();
    game::native_open_runtime::shutdown("runtime shutdown");
    game::building_capture_identity::cancel("runtime shutdown");
    game::network_identity::invalidate_world("runtime shutdown");
    game::normal_load_identity::shutdown(); hooks::cleanup(); Logger::instance().shutdown();
}
HMODULE get_module_handle() { return g_module; }
}
extern "C" __declspec(dllexport) bool MT2MP_InitInstalled(HMODULE module,const mt2mp::installed_profile::Receipt* receipt) {
    std::string error;
    if(!receipt||!mt2mp::installed_profile::accept(*receipt,error))return false;
    return mt2mp::initialize(module);
}
extern "C" __declspec(dllexport) bool MT2MP_Init(HMODULE module) { return mt2mp::initialize(module); }
extern "C" __declspec(dllexport) void MT2MP_Shutdown() { mt2mp::shutdown(); }
extern "C" __declspec(dllexport) void MT2MP_Tick() {}
