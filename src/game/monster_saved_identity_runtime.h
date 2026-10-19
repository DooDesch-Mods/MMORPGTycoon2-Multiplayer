#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include "monster_saved_identity.h"
#include "dungeon_scene_identity_runtime.h"

namespace mt2mp::game::monster_saved_identity_runtime {
using PropertyCall = bool(*)(const void*, void*, void*, void*);
using Ticket = uint64_t;
bool initialize(std::string& error);
// Actual root Building seal supplies the complete explicit template/Instance classification.
bool stage_scenes(dungeon_scene_identity_runtime::Census, std::string& error);
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
bool prepare_load(const std::filesystem::path& native_path,const std::string& id,const std::string& sha,uint64_t bytes,std::string& error);
bool write_companion(const std::string& id,const std::string& sha,uint64_t bytes,const std::filesystem::path& native_path,std::string& error);
Ticket save_before(const void* property, void* manager, void* writer, void* context) noexcept;
void save_after(Ticket, const void* property, void* manager, void* writer, void* context, bool original_result) noexcept;
Ticket load_before(const void* property, void* manager, void* reader, void* context) noexcept;
void load_after(Ticket, const void* property, void* manager, void* reader, void* context, bool original_result) noexcept;
void children_before(uintptr_t caller_rva, void* reader) noexcept;
void children_after(uintptr_t caller_rva, void* reader, int32_t count) noexcept;
void registered(uintptr_t caller_rva, void* context, int32_t id, void* object) noexcept;
void original_failed(Ticket, const std::string& cause) noexcept;
}
