#pragma once

#include <cstdint>
#include <string>

namespace mt2mp::hooks {

// Initialize MinHook
bool init();

// Cleanup MinHook
void cleanup();

// Create a hook at target, redirecting to detour. Stores original in *original.
bool create_hook(void* target, void* detour, void** original);

// Create hook by pattern scan
bool create_hook_by_pattern(const std::string& pattern, void* detour, void** original, const char* name);

// Create hook by string reference (find function containing string)
bool create_hook_by_string(const char* search_string, void* detour, void** original, const char* name);

// Create hook on an imported function (IAT hook via MinHook)
bool create_hook_by_import(const char* module_name, const char* func_name, void* detour, void** original);

// Enable all created hooks
bool enable_all();

// Disable all hooks
bool disable_all();

// Install specific hook groups
bool install_renderer_hooks();
bool install_input_hooks();
bool install_edit_manager_hooks();
bool install_game_hooks();
bool install_save_hooks();
bool install_shard_hooks();
bool install_clock_hooks();

} // namespace mt2mp::hooks
