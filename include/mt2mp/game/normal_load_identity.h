#pragma once

#include "mt2mp/game/native_engine.h"
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::game::normal_load_identity {

using AddObjectFn = void(*)(void*, int32_t, void*);

struct Binding {
    int32_t link_id = 0;
    uint64_t local_uid = 0;
    std::string record_key;
    native::BuildingState building;
};

struct Status {
    std::string state = "idle";
    std::string snapshot_sha256;
    std::string snapshot_uuid;
    std::string error;
    std::string classification_policy = "native_checked";
    bool negative_cache_enabled = false;
    uint64_t registrations = 0;
    uint64_t registration_checks = 0;
    uint64_t building_registrations = 0;
    uint64_t nonbuilding_registrations = 0;
    uint64_t building_replacements = 0;
    uint64_t building_drops = 0;
    uint64_t negative_cache_hits = 0;
    uint64_t negative_cache_misses = 0;
    uint64_t rtti_fallbacks = 0;
    uint64_t negative_cache_invalidations = 0;
    size_t negative_cache_entries = 0;
    size_t negative_cache_peak_entries = 0;
    size_t negative_cache_bytes = 0;
    size_t negative_cache_byte_limit = 131072;
    size_t negative_cache_limit = 512;
    std::string rtti_fallback_reason;
    uint64_t failed_rtti_memo_hits = 0;
    uint64_t failed_rtti_memo_overflows = 0;
    uint64_t rtti_graph_attempts = 0;
    size_t failed_rtti_memo_entries = 0;
    size_t failed_rtti_memo_peak_entries = 0;
    size_t failed_rtti_memo_limit = 512;
    // Counts and limits cover retained typed building slots and their nonempty contexts.
    size_t reference_count = 0;
    size_t context_count = 0;
    size_t peak_reference_count = 0;
    size_t peak_context_count = 0;
    size_t reference_limit = 4096;
    size_t context_limit = 64;
    bool buildings_only = true;
    bool all_references_retained = false;
    // Timing samples cover the observer body under its mutex, excluding original AddObject and lock wait.
    uint64_t observer_sample_count = 0;
    uint64_t observer_sample_total_ns = 0;
    uint64_t observer_sample_max_ns = 0;
    uint32_t observer_sample_stride = 1024;
    size_t rejected_buildings = 0;
    bool load_pending = false;
    bool scenery_supported = false;
    bool characters_supported = false;
    std::vector<Binding> buildings;
};

// Call after native::initialize and before patching the original AddObject bytes.
bool initialize(std::string& error);
void* entrypoint();
// The caller owns the verified MPTest snapshot receipt and normal load request.
bool begin(const std::string& snapshot_sha256, const std::string& snapshot_uuid, std::string& error);
// Invoke after the original AddObject(context, id, object) exactly once.
void observe_registered(void* context, int32_t link_id, void* object) noexcept;
// Invoke on the native game thread after the normal world activation.
bool finalize(Status& result, std::string& error);
Status status();
void cancel(const std::string& reason);
void shutdown();
// Ordinal must come from the SHA-validated RecordV2 index for this snapshot.
bool resolve_record(const std::string& snapshot_sha256, int32_t link_id, uint32_t record_ordinal,
                    Binding& result, std::string& error);

#ifdef MT2MP_IDENTITY_TEST
namespace testing {
using Registration = bool(*)(void*, int32_t, void*, uint64_t&, std::string&);
using ReadBuilding = bool(*)(uint64_t, native::BuildingState&, std::string&);
void backend(Registration registration, ReadBuilding read_building);
// The fixture owns a readonly synthetic image and its exact RTTI roots.
void metadata_image(void* base, size_t bytes, uintptr_t class_vptr, uintptr_t si_vptr,
                    uintptr_t vmi_vptr, uintptr_t source_typeinfo, uintptr_t building_typeinfo);
}
#endif

} // namespace mt2mp::game::normal_load_identity
