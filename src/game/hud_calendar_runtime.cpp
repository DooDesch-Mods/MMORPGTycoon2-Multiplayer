#include "hud_calendar_runtime.h"
#include "hud_calendar.h"
#include "hud_calendar_transport.h"
#include "hud_time_reader_process.h"
#include "time_integration.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/net/local_session.h"
#include "mt2mp/net/sidecar_client.h"
#include "mt2mp/net/snapshot_sync.h"
#include <array>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <windows.h>
#include <intrin.h>

namespace mt2mp::game::hud_calendar_runtime {
namespace {
hud_time::Calendar calendar;
hud_time::transport::Adapter transport;
std::atomic<bool> initialized=false;
uintptr_t image=0;
bool panel_hook_created=false,formatter_hook_created=false;
hud_time::OriginalPanelUpdate original_panel=nullptr;
hud_time::OriginalTimeString original_formatter=nullptr;
uint64_t bind_epoch_attempted=0;
struct Observation {std::string cause;uint64_t repeats=0;};
std::array<Observation,5> observations;
std::mutex observation_mutex;
void observe(size_t index,const char* operation,const std::string& error) noexcept {
    try {
        std::lock_guard lock(observation_mutex);auto& entry=observations.at(index);
        if(error.empty()) {
            if(!entry.cause.empty())LOG_INFO("[HudCalendar] operation={} recovered previous={} repeats={}",operation,entry.cause,entry.repeats);
            entry={};return;
        }
        if(entry.cause==error){++entry.repeats;return;}
        entry={error,1};
        if(error=="calendar_presentation_waiting")LOG_INFO("[HudCalendar] operation={} waiting cause={}",operation,error);
        else LOG_ERROR("[HudCalendar] operation={} failed cause={}",operation,error);
    }catch(const std::exception& cause){OutputDebugStringA("error: HUD Calendar observation failed; native OriginalOnce continues. cause=");OutputDebugStringA(cause.what());}
     catch(...){OutputDebugStringA("error: HUD Calendar observation failed; native OriginalOnce continues. cause=non-standard exception");}
}
bool context(void*,hud_time::CalendarContext& out,std::string& error) {
    out={};time_integration::Status current;
    if(!time_integration::instance().status(current,error))return false;
    const auto& state=current.runtime;
    const auto& config=Config::instance();
    const bool client=!config.is_host;
    const auto& snapshot=client?state.controller.applied:state.controller.observed;
    out.client=client;out.epoch=state.epoch;out.time_revision=snapshot.revision;
    auto& sidecar=net::SidecarClient::instance();
    out.active=config.local_test&&state.phase==time_runtime::Phase::Active&&state.hook_mask==3&&
        !state.presentation_uncertain&&!state.controller.write_locked&&snapshot.present&&!snapshot.uncertain&&
        out.epoch&&out.epoch==net::current_world_epoch()&&net::local_session_connected()&&
        (net::local_session_peer_mask()&1u)&&sidecar.connected()&&sidecar.ready()&&net::pending_snapshot_load_id().empty();
    error.clear();return true;
}
bool transport_context(hud_time::transport::Context& out,std::string& error) {
    out={};hud_time::CalendarContext current;
    if(!context(nullptr,current,error))return false;
    const auto& config=Config::instance();
    if(config.peer_id<1||config.peer_id>4){error="calendar local peer outside 1..4";return false;}
    const auto bound=calendar.status();
    out.connected=net::local_session_connected()&&net::SidecarClient::instance().connected()&&net::SidecarClient::instance().ready();
    out.active=current.active;out.client=current.client;out.local_peer=static_cast<uint8_t>(config.peer_id);
    out.peer_mask=net::local_session_peer_mask();out.epoch=current.epoch;out.time_revision=current.time_revision;
    out.calendar_bound=bound.bound&&!bound.locked;out.calendar_epoch=bound.epoch;return true;
}
void panel_update(void* receiver,float dt) {
    if(!initialized){original_panel(receiver,dt);return;}
    std::string error;
    if(!calendar.panel_update(receiver,dt,original_panel,error))observe(2,"PanelUpdate OriginalOnce",error);
    else {
        const auto state=calendar.status();const auto wire=transport.status();
        // Passive OriginalOnce does not establish that Host calendar presentation recovered.
        if(state.bound&&state.has_sample&&!state.locked&&!wire.locked&&state.error.empty())observe(2,"PanelUpdate OriginalOnce",{});
    }
}
void* format(void* result,void* clock) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto rva=caller>=image?caller-image:0;
    try{return calendar.format(result,clock,rva,original_formatter);}
    catch(const std::exception& cause){observe(3,"GetTimeString OriginalOnce",cause.what());throw;}
    catch(...){observe(3,"GetTimeString OriginalOnce","non-standard native exception");throw;}
}
nlohmann::json fields(const hud_time::CalendarFields& f) {
    return {{"year",f.year},{"month",f.month},{"day",f.day},{"flat_day",f.flat_day},{"seconds",f.seconds},
        {"software_calendar_enabled",f.software_calendar_enabled},{"calendar_format",f.calendar_format}};
}
}
bool initialize(const hud_time::Config& config,std::string& error) {
    if(initialized){error="hud_calendar_runtime_already_initialized";return false;}
    if(!calendar.initialize(config,hud_time::process_read_services(),{nullptr,context,hud_time::process_call_calendar_panel},error))return false;
    image=config.image;
    if(!hooks::create_hook(reinterpret_cast<void*>(image+0x62c400),reinterpret_cast<void*>(&panel_update),reinterpret_cast<void**>(&original_panel))){error="calendar PanelUpdate hook creation failed";return false;}
    panel_hook_created=true;
    if(!hooks::create_hook(reinterpret_cast<void*>(image+0x25a190),reinterpret_cast<void*>(&format),reinterpret_cast<void**>(&original_formatter))){error="calendar GetTimeString hook creation failed";return false;}
    formatter_hook_created=true;
    hud_time::transport::Services services;
    services.thread_id=[](){return GetCurrentThreadId();};services.context=transport_context;
    services.probe=[](hud_time::CalendarSample& sample,std::string& e){return calendar.read_host_probe(sample,e);};
    services.apply=[](uint64_t sender,const hud_time::CalendarSample& sample,std::string& e){return calendar.receive(sender,sample,e);};
    services.suspend_calendar=[](const std::string& reason,std::string& e){return calendar.suspend(reason,e);};
    services.send=[](uint8_t channel,const uint8_t* bytes,uint32_t size,uint64_t target,std::string& e){
        auto& sidecar=net::SidecarClient::instance();
        if(!sidecar.send(channel,bytes,size,target)){e=sidecar.failure_reason();if(e.empty())e="local transport refused Calendar packet";return false;}
        e.clear();return true;
    };
    services.register_handler=[](uint8_t channel,hud_time::transport::Handler handler){
        std::string cause;if(!net::SidecarClient::instance().claim_handler(channel,std::move(handler),cause))throw std::runtime_error(cause);
        LOG_INFO("[HudCalendar] Exclusive transport handler claimed channel={}",channel);
    };
    if(!transport.initialize(config.game_thread,std::move(services),error))return false;
    initialized=true;error.clear();return true;
}
bool hooks_enabled(std::string& error) {
    if(!initialized||!panel_hook_created||!formatter_hook_created||!original_panel||!original_formatter){error="calendar required enabled hook trampoline missing";return false;}
    return calendar.set_hook_mask(3,error);
}
void tick() {
    if(!initialized)return;
    hud_time::CalendarContext current;std::string error;
    if(!context(nullptr,current,error)){observe(0,"context",error);return;}
    observe(0,"context",{});
    const auto state=calendar.status();
    if(!current.active) {
        if(state.bound||transport.status().bound){const bool stopped=suspend("current Host Calendar authority unavailable",error);observe(4,"suspend",stopped?std::string{}:error);}
        return;
    }
    if(!state.bound&&!state.locked&&current.epoch!=bind_epoch_attempted) {
        bind_epoch_attempted=current.epoch;
        if(!calendar.bind(error)){observe(1,"bind",error);return;}
        if(!transport.bind(error)){std::string retired;if(!calendar.suspend("Calendar transport binding failed",retired))observe(4,"bind retirement",retired);observe(1,"bind",error);return;}
        observe(1,"bind",{});LOG_INFO("[HudCalendar] Bound epoch={} time_revision={} client={}",current.epoch,current.time_revision,current.client);
    }
    if(!transport.tick(GetTickCount64(),error))observe(4,"transport tick",error);
    else observe(4,"transport tick",{});
}
bool suspend(const std::string& reason,std::string& error) {
    if(!initialized){error="hud_calendar_runtime_not_initialized";return false;}
    return transport.suspend(reason,error);
}
nlohmann::json status_json() {
    const auto state=calendar.status();const auto wire=transport.status();
    hud_time::CalendarSample sample;std::string sample_error;const bool available=calendar.read_sample(sample,sample_error);
    return {{"initialized",initialized.load()},{"bound",state.bound},{"locked",state.locked},{"hook_mask",state.hook_mask},
        {"epoch",state.epoch},{"revision",state.revision},{"error",state.error},{"bind_epoch_attempted",bind_epoch_attempted},
        {"probes",state.probes},{"received",state.received},{"rejected",state.rejected},{"duplicates",state.duplicate},
        {"panel_original_attempted",state.panel_original_attempted},{"formatter_original_attempted",state.formatter_original_attempted},
        {"formatter_substituted",state.formatter_substituted},{"formatter_returned",state.formatter_returned},
        {"formatted_epoch",state.formatted_epoch},{"formatted_revision",state.formatted_revision},{"formatted_time_revision",state.formatted_time_revision},
        {"formatted_fields",fields(state.formatted_fields)},{"sample_available",available},{"sample_error",sample_error},
        {"sample",{{"epoch",sample.epoch},{"revision",sample.revision},{"time_revision",sample.time_revision},{"fields",fields(sample.fields)}}},
        {"transport",{{"initialized",wire.initialized},{"bound",wire.bound},{"locked",wire.locked},{"pending",wire.pending},
            {"suspend_pending",wire.suspend_pending},{"epoch",wire.epoch},{"retired_epoch",wire.retired_epoch},
            {"received",wire.received},{"duplicates",wire.duplicates},{"superseded",wire.superseded},{"rejected",wire.rejected},
            {"probed",wire.probed},{"send_attempted",wire.send_attempted},{"send_accepted",wire.send_accepted},
            {"apply_attempted",wire.apply_attempted},{"apply_confirmed",wire.apply_confirmed},{"applied_revision",wire.applied_revision},
            {"last_sent_revision",wire.last_sent_revision},{"future_waits",wire.future_waits},{"failures",wire.failures},
            {"unbound_skipped",wire.unbound_skipped},{"error",wire.error},{"waiting",wire.waiting}}}};
}
}
