#pragma once
#include <cstdint>
#include "network_object_identity.h"
#include "network_snapshot_alias.h"
#include <string>
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::game::network_identity {
inline constexpr uint32_t maximum_nodes=65536,maximum_regions=16384,maximum_contexts=64,maximum_pending_arrays=64;
inline constexpr uint32_t default_sample_limit=64,maximum_sample_limit=256;
using CaptureSaveFn=bool(*)(const void* property,const void* manager,void* writer,void* save_context);
// Initialize after native::ready and before installing either original-entry detour.
bool initialize(std::string& error);
void* capture_entrypoint();
// Mainthread only. The capture SHA is unknown until the owned file is complete.
bool begin_host_capture(const std::string& capture_uuid,uint32_t expected_nodes,std::string& error,const std::string& world_name="MPTestSession");
// Parent calls its original CaptureSaveFn exactly once between these observations, even for ticket 0.
uint64_t capture_before(const void* property,const void* manager,void* writer,void* save_context) noexcept;
void capture_after(uint64_t ticket,const void* property,const void* manager,void* writer,void* save_context,bool original_result) noexcept;
void original_failed(uint64_t ticket,const std::string& cause) noexcept;
bool seal_host_capture(const std::string& capture_uuid,const std::string& finished_file_sha256,std::string& error);
bool begin_client_load(const std::string& snapshot_uuid,const std::string& snapshot_sha256,uint32_t expected_nodes,std::string& error);
// Invoke only after the existing AddObject original, without installing a second AddObject detour.
void observe_client_registered(void* context,int32_t link_id,void* object,uint64_t caller_rva=0) noexcept;
bool finalize_client_load(std::string& error);
void cancel_pending(const std::string& reason);
void invalidate_world(const std::string& reason);
using ObjectEntry=network_object_identity::Entry;
bool prepare_aliases(const std::string& uuid,const std::string& sha,const network_snapshot_alias::Manifest&,std::string&);
bool snapshot_aliases(const std::string& uuid,const std::string& sha,network_snapshot_alias::Manifest&,std::string&);
bool begin_original_load(const std::string& uuid,const std::string& sha,std::string&);
bool finalize_original_load(const std::string& uuid,const std::string& sha,std::string&);
bool object_key(uintptr_t object,uint64_t full_uid,std::string&,ObjectEntry&,std::string&);
bool object_target(const std::string& key,ObjectEntry&,std::string&);
bool register_place(const std::string& action_uuid,uintptr_t object,uint64_t full_uid,std::string&);
bool register_places(const std::string& action_uuid,const std::vector<std::pair<uintptr_t,uint64_t>>&,std::string&);
bool retire_object(const std::string& key,uintptr_t object,uint64_t full_uid,std::string&);
bool inspect_history_object(uintptr_t object,uint64_t full_uid,ObjectEntry&,std::string&);
bool reconcile_history(const std::string& action_uuid,const std::map<std::string,ObjectEntry>& before,const std::map<std::string,ObjectEntry>& after,std::string&);
// Stored scalar receipts only. No native calls or process pointers; arrays are sampled before JSON construction.
nlohmann::json status_json(uint32_t node_limit=default_sample_limit);

#ifdef MT2MP_NETWORK_IDENTITY_TEST
namespace testing {
using Contains=bool(*)(void*,const void*,int32_t*);
using Lookup=void*(*)(uint64_t);
using Owns=bool(*)(const void*,void*);
using ContextLookup=void*(*)(void*,int32_t);
void backend(uintptr_t base,Contains contains,Lookup lookup,Owns owns,ContextLookup context_lookup);
uint64_t read_queries();
using SelectorBarrier=void(*)();
void selector_barrier(SelectorBarrier callback);
}
#endif
}
