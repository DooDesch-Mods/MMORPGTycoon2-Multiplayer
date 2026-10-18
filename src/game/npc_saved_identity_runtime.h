#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "npc_saved_identity.h"

namespace mt2mp::game::npc_saved_identity_runtime {
// Current saved NPCs only. Missing members are omitted without dereferencing
// their retained addresses. The active snapshot and completed world are checked.
using Current = npc_saved_identity::Current;
bool current_snapshot_sha(std::string& out, std::string& error);
// Only the enclosing pure NPC owner reader opts into its metadata lifetime.
bool read_current(std::vector<Current>&, std::string& error, bool share_metadata=false);
bool resolve(const std::string& key, Current&, std::string& error);
bool bind_local(uintptr_t object, uint64_t uid, std::string& key, std::string& error);

using PropertyCall = bool(*)(const void*, void*, void*, void*);
using Ticket = uint64_t;
bool initialize(std::string& error);
void* entrypoint(bool save);
bool confirm_hooks(std::string& error);
// BuildingCapture supplies its freshly checked world before any NativeOpen load.
struct CaptureWorld {
    void* game = nullptr; void* mode = nullptr; void* state = nullptr;
    void* world = nullptr; void* map = nullptr;
};
bool arm_host(const std::string& uuid, const CaptureWorld&, std::string& error);
bool seal_host(const std::string& uuid, const std::string& sha, const CaptureWorld&, std::string& error);
bool arm_client(const std::string& uuid, const std::string& sha, std::string& error);
bool seal_client(std::string& error);
void cancel(const std::string& cause);
Ticket save_before(const void* property, void* manager, void* writer, void* context) noexcept;
void save_after(Ticket, const void* property, void* manager, void* writer, void* context, bool original_result) noexcept;
Ticket load_before(const void* property, void* manager, void* reader, void* context) noexcept;
void load_after(Ticket, const void* property, void* manager, void* reader, void* context, bool original_result) noexcept;
void children_before(uintptr_t caller_rva, void* reader) noexcept;
void children_after(uintptr_t caller_rva, void* reader, int32_t count) noexcept;
void registered(uintptr_t caller_rva, void* context, int32_t id, void* object) noexcept;
void original_failed(Ticket, const std::string& cause) noexcept;
}
