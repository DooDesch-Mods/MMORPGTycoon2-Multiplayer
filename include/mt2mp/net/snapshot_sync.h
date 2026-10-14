#pragma once
#include <cstdint>
#include <string>
#include <filesystem>
#include <chrono>
#include <stdexcept>
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::net {
inline constexpr uint32_t snapshot_data_idle_timeout_seconds=60;
inline constexpr uint32_t snapshot_native_load_timeout_seconds=900;
inline constexpr uint32_t snapshot_native_capture_timeout_seconds=900;
struct SnapshotLoadWatch {
    using Clock=std::chrono::steady_clock;
    bool active=false,observed=false,completed=false;
    int32_t active_mode=-1,requested_mode=-1;
    uint64_t registrations=0,progress_events=0;
    Clock::time_point started{},last_progress{},finished{};
    void begin(Clock::time_point now=Clock::now()) {
        *this={}; active=true; started=last_progress=now;
    }
    // Only native mode changes or additional registrations count as progress.
    // Progress does not extend the absolute load deadline: blocked native calls have no guaranteed polling cadence.
    bool observe(int32_t mode,int32_t requested,uint64_t registered,Clock::time_point now=Clock::now()) {
        if(!active) throw std::logic_error("native load observation before deadline begin");
        if(observed&&registered<registrations) throw std::runtime_error("native load registrations regressed actual="+std::to_string(registered)+" required>="+std::to_string(registrations));
        const bool changed=!observed||mode!=active_mode||requested!=requested_mode||registered>registrations;
        if(changed) { observed=true; active_mode=mode; requested_mode=requested; registrations=registered; ++progress_events; last_progress=now; }
        return changed;
    }
    void finish(Clock::time_point now=Clock::now()) { completed=true; finished=now; }
    int64_t elapsed_ms(Clock::time_point now=Clock::now()) const {
        return active?std::chrono::duration_cast<std::chrono::milliseconds>((completed?finished:now)-started).count():0;
    }
    int64_t progress_idle_ms(Clock::time_point now=Clock::now()) const {
        return active?std::chrono::duration_cast<std::chrono::milliseconds>((completed?finished:now)-last_progress).count():0;
    }
    bool expired(Clock::time_point now=Clock::now()) const {
        return active&&!completed&&now-started>=std::chrono::seconds(snapshot_native_load_timeout_seconds);
    }
    std::string timeout_cause(Clock::time_point now=Clock::now()) const {
        return "snapshot native load observation timeout elapsed_ms="+std::to_string(elapsed_ms(now))+" required<"+
            std::to_string(snapshot_native_load_timeout_seconds)+"s active_mode="+std::to_string(active_mode)+
            " requested_mode="+std::to_string(requested_mode)+" registrations="+std::to_string(registrations);
    }
};
void initialize_snapshot_sync();
void tick_snapshot_sync();
bool distribute_snapshot(const std::string& id,const std::string& digest,const std::filesystem::path& path,std::string& error,const std::string& world_name="MPTestSession");
std::string snapshot_sync_status();
std::string current_snapshot_id();
// Snapshot epoch only; native adapters must also verify their bound world receivers.
uint64_t current_world_epoch();
// Suspend snapshot authority before a local native load replaces its world.
void invalidate_snapshot_world(const std::string& reason);
// Read only on the native game thread while the incoming normal load is pending.
std::string pending_snapshot_load_id();
nlohmann::json snapshot_progress();
#ifdef MT2MP_SNAPSHOT_TEST
namespace testing { void advance_snapshot_clock(uint32_t seconds); }
#endif
}
