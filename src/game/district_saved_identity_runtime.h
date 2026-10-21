#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "district_saved_identity.h"
#include "district_lifecycle.h"

namespace mt2mp::game::district_saved_identity_runtime {
// Current Districts retain their saved or confirmed original-action identity.
// Every member is checked against the current manager and full UID registry.
using Current = district_saved_identity::Current;
bool current_snapshot_sha(std::string& out, std::string& error);
// Only the enclosing pure District owner reader opts into its metadata lifetime.
bool read_current(std::vector<Current>&, std::string& error, bool share_metadata=false);
bool resolve(const std::string& key, Current&, std::string& error);
bool bind_local(uintptr_t object, uint64_t uid, std::string& key, std::string& error);

struct Edit {
    uint64_t token=0;
    std::string operation,snapshot;
    std::map<int32_t,district_lifecycle::Roster> before;
    bool operator==(const Edit&)const=default;
};
struct Preview {
    Edit edit;
    std::map<int32_t,district_lifecycle::Change> changes;
    bool operator==(const Preview&)const=default;
};
// Original mutation runs between prepare and preview, outside the registry lock.
// Publication repeats the complete raw census after the caller proves its effect.
bool prepare_edit(const std::string& operation,Edit&,std::string& error);
bool preview_edit(const Edit&,const std::map<int32_t,std::map<uint64_t,std::string>>& restored,Preview&,std::string& error);
bool publish_edit(const Preview&,std::string& error);
bool cancel_edit(const Edit&,std::string& error);


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
