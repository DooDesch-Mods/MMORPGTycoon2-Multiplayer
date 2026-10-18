#include "hud_time_runtime.h"
#include "hud_time_reader_process.h"
#include "hud_time_writer_process.h"
#include "time_integration.h"
#include "session_provider.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/snapshot_sync.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <windows.h>

namespace mt2mp::game::hud_time_runtime {
namespace {
hud_time::Writer writer;
std::atomic<bool> initialized=false;
bool show_hook_created=false;
using Show=void(*)(void*,bool,bool);
Show original_show=nullptr;
uintptr_t show_target=0;
uint32_t show_thread=0;
uint64_t bind_epoch_attempted=0;
std::atomic<uint64_t> read_calls=0,read_microseconds=0,max_read_microseconds=0;
struct Observation {std::string reason;uint64_t repeats=0;};
std::array<Observation,5> observations;
std::mutex observation_mutex;
void observe(size_t index,const char* operation,const std::string& error) noexcept {
    try {
    std::lock_guard lock(observation_mutex);
    auto& entry=observations.at(index);
    if(error.empty()) {
        if(!entry.reason.empty()) LOG_INFO("[HudTime] {} recovered previous={} repeats={}",operation,entry.reason,entry.repeats);
        entry={};return;
    }
    if(entry.reason==error){++entry.repeats;return;}
    entry={error,1};LOG_ERROR("[HudTime] {} failed cause={}",operation,error);
    } catch(const std::exception& cause) {
        OutputDebugStringA("error: HUD observation logging failed; native OriginalOnce continues. cause=");
        OutputDebugStringA(cause.what());
    } catch(...) {
        OutputDebugStringA("error: HUD observation logging failed; native OriginalOnce continues. cause=non-standard exception");
    }
}
bool authority(void*,hud_time::Authority& out,std::string& error) {
    out={};
    if(!initialized){error="hud_time_runtime_not_initialized";return false;}
    time_integration::Status current;
    if(!time_integration::instance().status(current,error))return false;
    const auto& runtime=current.runtime;
    const auto& applied=runtime.controller.applied;
    out.epoch=runtime.epoch;out.revision=applied.revision;
    out.selected_scale=applied.clock.selected;out.pause_scale=applied.clock.pause;
    const bool active=runtime.phase==time_runtime::Phase::Active&&runtime.hook_mask==3&&
        !runtime.presentation_uncertain&&!runtime.controller.write_locked&&applied.present&&!applied.uncertain&&
        out.epoch!=0&&out.epoch==net::current_world_epoch()&&net::session_wire::connected()&&net::pending_snapshot_load_id().empty();
    if(!active){error.clear();return true;}
    action_session::Context session;
    if(!action_session::selected_session().current(session,error))return false;
    if(!session.session||session.session!=net::session_wire::session_id()||session.snapshot_uuid.empty()||session.snapshot_uuid!=net::current_snapshot_id()||
       session.host_peer!=1||session.local_peer<1||session.local_peer>4||session.local_peer!=net::session_wire::local_peer()||
       session.peer_mask!=net::session_wire::peer_mask()||(session.peer_mask&0xf0)||!(session.peer_mask&1u)||
       !(session.peer_mask&(1u<<(session.local_peer-1)))||
       (session.role!=action_session::Role::Host&&session.role!=action_session::Role::Client)||
       (session.role==action_session::Role::Host)!=(session.local_peer==1)){
        error="HUD Time actual Session/slot/membership differs";return false;
    }
    out.client=session.role==action_session::Role::Client;out.available=out.client;
    error.clear();return true;
}
void show(void* window,bool visible,bool immediate) {
    const auto plan=writer.on_show(window,visible);
    observe(2,"show plan",plan.error);
    try {original_show(window,plan.visible,immediate);}
    catch(const std::exception& error){LOG_ERROR("[HudTime] OriginalShow failed cause={}; no replay",error.what());throw;}
    catch(...){LOG_ERROR("[HudTime] OriginalShow failed cause=non-standard exception; no replay");throw;}
}
}
bool initialize(const hud_time::Config& config,std::string& error) {
    if(initialized){error="hud_time_runtime_already_initialized";return false;}
    const hud_time::WriterServices services{nullptr,authority,hud_time::process_store_item_state,hud_time::process_call_clock};
    if(!writer.initialize(config,hud_time::process_read_services(),services,error))return false;
    if(!hooks::create_hook(reinterpret_cast<void*>(config.image+0x81ec20),reinterpret_cast<void*>(&show),reinterpret_cast<void**>(&original_show))) {
        error="hud_time_WindowShow_hook_creation_failed";return false;
    }
    show_target=config.image+0x81ec20;show_thread=config.game_thread;
    show_hook_created=true;initialized=true;return true;
}
bool verify_window_show_hook(uintptr_t target,std::string& error) {
    if(!initialized||!show_hook_created||!original_show||target!=show_target||writer.status().hook_mask!=7){
        error="hud_time_owned_Show_hook_not_ready";return false;
    }
    if(GetCurrentThreadId()!=show_thread){error="hud_time_owned_Show_hook_wrong_thread";return false;}
    const auto reads=hud_time::process_read_services();
    std::array<unsigned char,13> entry{};
    if(!reads.read(reads.user,target,entry.data(),entry.size(),error))return false;
    constexpr std::array<unsigned char,8> tail{0xb9,0x77,0x03,0,0,0,0x75,0x23};
    if(entry[0]!=0xe9||!std::equal(tail.begin(),tail.end(),entry.begin()+5)){
        error="hud_time_owned_Show_entry_differs";return false;
    }
    int32_t displacement=0;std::memcpy(&displacement,entry.data()+1,sizeof(displacement));
    const auto relay=uintptr_t(int64_t(target)+5+displacement);
    const auto trampoline=reinterpret_cast<uintptr_t>(original_show);
    // MinHook x64 keeps its absolute relay inside the same 64-byte trampoline slot.
    if(relay<trampoline||relay-trampoline>50){error="hud_time_owned_Show_relay_outside_own_slot";return false;}
    std::array<unsigned char,14> jump{};
    if(!reads.read(reads.user,relay,jump.data(),jump.size(),error))return false;
    constexpr std::array<unsigned char,6> indirect{0xff,0x25,0,0,0,0};
    uintptr_t destination=0;std::memcpy(&destination,jump.data()+6,sizeof(destination));
    if(!std::equal(indirect.begin(),indirect.end(),jump.begin())||destination!=reinterpret_cast<uintptr_t>(&show)){
        error="hud_time_owned_Show_relay_target_differs";return false;
    }
    error.clear();return true;
}
bool hooks_enabled(bool time_update_hook,bool clock_scope_hook,std::string& error) {
    if(!initialized||!show_hook_created||!original_show||!time_update_hook||!clock_scope_hook) {
        error="hud_time_required_hook_trampoline_missing";return false;
    }
    return writer.set_hook_mask(7,error);
}
void tick() {
    if(!initialized)return;
    hud_time::Authority current;std::string error;
    if(!authority(nullptr,current,error)){observe(0,"authority",error);return;}
    observe(0,"authority",{});
    const auto state=writer.status();
    if(!current.available) {
        if(state.bound) {
            const bool suspended=writer.suspend("current Host time authority unavailable",error);
            observe(3,"suspend",suspended?std::string{}:error);
        }
        return;
    }
    if(state.bound||state.locked||current.epoch==bind_epoch_attempted)return;
    bind_epoch_attempted=current.epoch;
    const bool bound=writer.bind(error);
    observe(1,"bind",bound?std::string{}:error);
    if(bound)LOG_INFO("[HudTime] Client HUD bound epoch={} revision={} hook_mask=7",current.epoch,current.revision);
}
bool suspend(const std::string& reason,std::string& error) {
    if(!initialized){error="hud_time_runtime_not_initialized";return false;}
    return writer.suspend(reason,error);
}
hud_time::UpdateResult on_update(void* type,void* item) {
    if(!initialized)return {};
    const auto started=std::chrono::steady_clock::now();
    auto result=writer.on_update(type,item);
    const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count();
    ++read_calls;read_microseconds+=uint64_t(elapsed);
    auto previous=max_read_microseconds.load();
    while(uint64_t(elapsed)>previous&&!max_read_microseconds.compare_exchange_weak(previous,uint64_t(elapsed))) {}
    observe(4,"item update",result.error);return result;
}
void clock_update(void* receiver,float dt,hud_time::OriginalClockUpdate original) {
    if(!initialized){original(receiver,dt);return;}
    std::string error;
    if(!writer.clock_update(receiver,dt,original,error))observe(3,"ClockUpdate OriginalOnce",error);
    else observe(3,"ClockUpdate OriginalOnce",{});
}
nlohmann::json status_json() {
    const auto state=writer.status();
    return {{"initialized",initialized.load()},{"bound",state.bound},{"locked",state.locked},{"hook_mask",state.hook_mask},
        {"epoch",state.epoch},{"revision",state.revision},{"error",state.error},{"item_attempted",state.item_attempted},
        {"item_confirmed",state.item_confirmed},{"item_failed",state.item_failed},{"item_forwarded",state.item_forwarded},
        {"show_replaced",state.show_replaced},{"show_forwarded",state.show_forwarded},{"clock_entered",state.clock_entered},
        {"clock_forwarded",state.clock_forwarded},{"bind_epoch_attempted",bind_epoch_attempted},
        {"update_calls",read_calls.load()},{"update_microseconds",read_microseconds.load()},{"max_update_microseconds",max_read_microseconds.load()}};
}
}
