#pragma once
#include <cstdint>
#include <array>
#include <string>
#include <filesystem>
#include <chrono>
#include <stdexcept>
#include "vendor/nlohmann/json.hpp"
struct MT2FW_MT2_StepServiceDraftV1;
struct MT2FW_MT2_StepReceipt;
namespace mt2mp::game::ordered_session {class ProofOwner;}
namespace mt2mp::net {
using OrderedClientEntry=bool(*)(std::string& error);
void configure_ordered_client_entry(OrderedClientEntry);
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
// Selection requires the explicit API or registered admitted-lobby entry; Config is not authority.
struct OrderedSnapshotIdentity {
    std::array<uint64_t,5> members{};
    uint64_t lobby=0,session=0,local_peer=0;
    uint8_t peer_mask=0;
    uint32_t region_rng_mode=0;
    std::string snapshot_uuid,sha256,world_name;
    uint64_t bytes=0,seed=0,native_generation=0,world_epoch=0;
    bool seed_present=false,file_verified=false,native_prepared=false,request_attempted=false,loaded=false;
};
bool select_ordered_snapshot(std::string&);
bool ordered_snapshot_selected();
bool retain_ordered_original_cohort(std::string&);
// Only the exact selected native-save copy establishes first-import provenance.
bool select_ordered_snapshot_source(const std::string& id,const std::string& sha,uint64_t bytes,uint32_t region_rng_mode,std::string&);
// Root owns Step/provider unbind and retains both owners through BUSY.
// Null proof is permitted only before either owner has been registered.
bool retire_ordered_snapshot(const game::ordered_session::ProofOwner*,std::string&);
bool prepare_ordered_host_snapshot(std::string&);
// Read the live Framework v3 seed receipt before the one original request.
bool request_ordered_snapshot_load(const MT2FW_MT2_StepServiceDraftV1&,uint64_t lease,std::string&);
bool poll_ordered_snapshot_load(bool& pending,std::string&);
using OrderedInitialWitness=bool(*)(std::array<uint8_t,32>&,nlohmann::json&,std::string&);
bool acknowledge_ordered_bootstrap(const MT2FW_MT2_StepServiceDraftV1&,uint64_t lease,OrderedInitialWitness,bool& ready,std::string&);
// Before a native request this reads the exact pinned file. Loaded=true requires
// NativeOpen's completed local world proof; Host distribution alone cannot supply it.
bool read_ordered_snapshot(OrderedSnapshotIdentity&,std::string&);
// Recovery-local proof only. Normal Step and action readers keep read_ordered_snapshot.
bool read_ordered_snapshot_local(OrderedSnapshotIdentity&,std::string&);
// Read-only initial census admission. Reads the retained live service and exact
// physical peer agreement; unavailable after activation or the first original Go.
bool read_ordered_initial_bootstrap(OrderedSnapshotIdentity&,MT2FW_MT2_StepReceipt&,std::string&);
void initialize_snapshot_sync();
void tick_snapshot_sync();
bool distribute_snapshot(const std::string& id,const std::string& digest,const std::filesystem::path& path,std::string& error,const std::string& world_name="MPTestSession");
std::string snapshot_sync_status();
// End a failed live Host world through the existing snapshot-error channel.
// Queue acknowledgement is distinct from a peer observing the terminal state.
bool fail_host_world(const std::string& expected_snapshot,const std::string& cause,std::string& error);
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
