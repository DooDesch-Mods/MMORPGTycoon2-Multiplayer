#pragma once
#include "mt2mp/game/native_engine.h"
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::game::building_capture_identity {
using SaveArrayFn = void(*)(const void*, void*, void*);
using Ticket = uint64_t;
enum class SealResult { Pending, Confirmed, Failed };
struct Binding {
    int32_t link_id = 0;
    uint64_t local_uid = 0;
    std::string record_key;
    native::BuildingState building;
};
struct Status {
    std::string state = "idle", capture_uuid, snapshot_sha256, error;
    std::string scope = "main_saved_building_arrays";
    uint64_t session_epoch = 0, snapshot_bytes = 0, wrapper_calls = 0, wrapper_returns = 0, readback_calls = 0;
    size_t inflight_wrappers = 0, arrays_verified = 0, foreign_writers_skipped = 0;
    size_t contexts_seen = 0, staged_buildings = 0, native_array_buildings = 0, readbacks_confirmed = 0, final_readbacks_checked = 0;
    size_t unique_main_arrays = 0, root_buildings = 0, dungeon_buildings = 0, dungeon_instance_buildings = 0;
    size_t building_limit = 4096, context_limit = 8, inflight_limit = 256, array_limit = 4096;
    uint32_t game_thread_id = 0;
    std::vector<uint32_t> observed_thread_ids;
    bool file_confirmed = false, bindings_confirmed = false, full_coop_proven = false;
    bool world_witness_bound = false, world_witness_current = false;
    uint64_t world_witness_checks = 0;
    std::vector<Binding> buildings;
};
bool initialize(std::string& error);
// This read is also used while the owned capture seal holds its mutex.
bool confirm_current_receivers(void* game,void* mode,void* state,void* world,void* map,std::string& error);
void* capture_entrypoint();
bool begin(const std::string& capture_uuid, uint64_t session_epoch, std::string& error, const std::string& world_name="MPTestSession", bool bind_snapshot_identities=true);
// Hook invokes original exactly once between these callbacks, even if the ticket is zero.
Ticket capture_before(const void* array, void* writer, void* save_context) noexcept;
void capture_after(Ticket ticket, const void* array, void* writer, void* save_context) noexcept;
void original_failed(Ticket ticket, const std::string& cause) noexcept;
// Only the game thread can seal the caller's verified owned file receipt.
SealResult seal(const std::string& capture_uuid, const std::string& verified_sha256,
                uint64_t verified_bytes, uint64_t current_session_epoch, Status& result, std::string& error);
void cancel(const std::string& reason);
Status status();
struct StreamOwner {
    std::string capture_uuid;
    uintptr_t stream=0,consumer=0,file=0,task=0;
};
// Shares the already armed capture filename and original writer lifetime guard.
bool stream_owner(void* writer,StreamOwner& result,bool& own,std::string& error);

#ifdef MT2MP_CAPTURE_IDENTITY_TEST
namespace testing {
using Contains = bool(*)(void*, void*, int32_t&, std::string&);
using Typed = bool(*)(void*, std::string&);
using Readback = bool(*)(uint64_t, void*, native::BuildingState&, std::string&);
void backend(uintptr_t image_base, Contains contains, Typed typed, Readback readback);
}
#endif
}
