#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/hooks/pattern_scanner.h"
#include "mt2mp/core/logger.h"
#include "../game/object_identity/native_free_runtime/native_free_runtime.h"
#include "../game/object_identity/remove_runtime/remove_runtime.h"
#include "../game/ui_remove_runtime.h"
#include "../game/ui_build_runtime.h"
#include "../game/ui_move_runtime.h"

#include "mt2mp/hooks/minhook_backend.h"
#include <vector>

namespace mt2mp::hooks {

struct HookEntry {
    void* target;
    void* detour;
    void** original;
    const char* name;
    bool enabled;
};

static std::vector<HookEntry> g_hooks;

bool init() {
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        LOG_ERROR("MH_Initialize failed: {}", MH_StatusToString(status));
        return false;
    }
    return true;
}

void cleanup() {
    std::string error;
    if(!game::ui_move_runtime::can_reclaim_global_minhook(error)) {
        LOG_WARN("[UIMove] Global MinHook cleanup retained cause={}",error);
        return;
    }
    if(!game::ui_build_runtime::can_reclaim_global_minhook(error)) {
        LOG_WARN("[UIBuild] Global MinHook cleanup retained cause={}",error);
        return;
    }
    if(!game::ui_remove_runtime::can_reclaim_global_minhook(error)) {
        LOG_WARN("[UIRemove] Global MinHook cleanup retained cause={}",error);
        return;
    }
    if(!remove_runtime::Runtime::instance().can_reclaim_global_minhook(error)) {
        LOG_WARN("[Remove] Global MinHook cleanup retained cause={}",error);
        return;
    }
    if(!native_free_runtime::Runtime::instance().can_reclaim_global_minhook(error)) {
        LOG_WARN("[NativeFree] Global MinHook cleanup retained cause={}",error);
        return;
    }
    if(!disable_all())return;
    const auto result=MH_Uninitialize();
    if(result!=MH_OK&&result!=MH_ERROR_NOT_INITIALIZED) {
        LOG_ERROR("MH_Uninitialize failed: {}",MH_StatusToString(result));
        return;
    }
    g_hooks.clear();
}

bool create_hook(void* target, void* detour, void** original) {
    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK) {
        LOG_ERROR("MH_CreateHook failed: {}", MH_StatusToString(status));
        return false;
    }
    g_hooks.push_back({target, detour, original, "unnamed", false});
    return true;
}

bool create_hook_by_pattern(const std::string& pattern, void* detour, void** original, const char* name) {
    uintptr_t addr = find_pattern(pattern);
    if (!addr) {
        LOG_ERROR("Pattern not found for hook: {}", name);
        return false;
    }
    LOG_INFO("Hook '{}' pattern found at 0x{:X}", name, addr);

    if (!create_hook(reinterpret_cast<void*>(addr), detour, original)) {
        return false;
    }
    g_hooks.back().name = name;
    return true;
}

bool create_hook_by_string(const char* search_string, void* detour, void** original, const char* name) {
    uintptr_t addr = find_function_by_string(search_string);
    if (!addr) {
        LOG_ERROR("Function not found by string for hook: {} (string: \"{}\")", name, search_string);
        return false;
    }
    LOG_INFO("Hook '{}' function found at 0x{:X}", name, addr);

    if (!create_hook(reinterpret_cast<void*>(addr), detour, original)) {
        return false;
    }
    g_hooks.back().name = name;
    return true;
}

bool create_hook_by_import(const char* module_name, const char* func_name, void* detour, void** original) {
    HMODULE mod = GetModuleHandleA(module_name);
    if (!mod) {
        LOG_ERROR("Module not found: {}", module_name);
        return false;
    }

    void* target = reinterpret_cast<void*>(GetProcAddress(mod, func_name));
    if (!target) {
        LOG_ERROR("Function not found: {}!{}", module_name, func_name);
        return false;
    }

    LOG_INFO("Import hook: {}!{} at 0x{:X}", module_name, func_name,
        reinterpret_cast<uintptr_t>(target));

    if (!create_hook(target, detour, original)) {
        return false;
    }
    g_hooks.back().name = func_name;
    return true;
}

bool enable_all() {
    MH_STATUS status = MH_EnableHook(MH_ALL_HOOKS);
    if (status != MH_OK) {
        LOG_ERROR("MH_EnableHook(ALL) failed: {}", MH_StatusToString(status));
        return false;
    }
    for (auto& h : g_hooks) h.enabled = true;
    LOG_INFO("Enabled {} hooks", g_hooks.size());
    return true;
}

bool disable_all() {
    MH_STATUS status = MH_DisableHook(MH_ALL_HOOKS);
    if (status != MH_OK && status != MH_ERROR_NOT_INITIALIZED) {
        LOG_ERROR("MH_DisableHook(ALL) failed: {}", MH_StatusToString(status));
        return false;
    }
    for (auto& h : g_hooks) h.enabled = false;
    return true;
}

} // namespace mt2mp::hooks
