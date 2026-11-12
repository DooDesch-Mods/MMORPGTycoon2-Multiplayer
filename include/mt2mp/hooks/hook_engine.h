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

// Enable all created hooks
bool enable_all();

// Disable all hooks
bool disable_all();

} // namespace mt2mp::hooks
