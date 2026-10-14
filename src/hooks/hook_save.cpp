#include "mt2mp/hooks/hook_save.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/core/logger.h"

namespace mt2mp::hooks {

static void* orig_save = nullptr;
static void* orig_load_binary = nullptr;
static void* orig_load_begin = nullptr;
static void* orig_load_end = nullptr;
static SaveCallback g_on_save;
static LoadCallback g_on_load_begin;
static LoadCallback g_on_load_end;

using SaveFn = void(__fastcall*)(void* this_ptr);
static void __fastcall detour_save(void* this_ptr) {
    LOG_INFO("ModeInGame::Save triggered");
    if (g_on_save) g_on_save("");
    reinterpret_cast<SaveFn>(orig_save)(this_ptr);
}

using LoadBinaryFn = void(__fastcall*)(void* this_ptr, const char* path);
static void __fastcall detour_load_binary(void* this_ptr, const char* path) {
    LOG_INFO("ModeInGame::LoadBinary - path: \"{}\"", path ? path : "null");
    reinterpret_cast<LoadBinaryFn>(orig_load_binary)(this_ptr, path);
}

using LoadBeginFn = void(__fastcall*)(void* this_ptr);
static void __fastcall detour_load_begin(void* this_ptr) {
    LOG_INFO("LOAD BEGINS");
    if (g_on_load_begin) g_on_load_begin("");
    reinterpret_cast<LoadBeginFn>(orig_load_begin)(this_ptr);
}

using LoadEndFn = void(__fastcall*)(void* this_ptr);
static void __fastcall detour_load_end(void* this_ptr) {
    reinterpret_cast<LoadEndFn>(orig_load_end)(this_ptr);
    LOG_INFO("LOAD ENDS");
    if (g_on_load_end) g_on_load_end("");
}

void set_save_callback(SaveCallback cb) { g_on_save = std::move(cb); }
void set_load_begin_callback(LoadCallback cb) { g_on_load_begin = std::move(cb); }
void set_load_end_callback(LoadCallback cb) { g_on_load_end = std::move(cb); }

bool install_save_hooks() {
    bool ok = true;
    ok &= create_hook_by_string("!! MMO save:", reinterpret_cast<void*>(&detour_save),
        &orig_save, "ModeInGame::Save");
    ok &= create_hook_by_string("Loading compressed binary save file",
        reinterpret_cast<void*>(&detour_load_binary), &orig_load_binary, "ModeInGame::LoadBinary");
    ok &= create_hook_by_string("LOAD BEGINS", reinterpret_cast<void*>(&detour_load_begin),
        &orig_load_begin, "ModeInGame::LoadBegin");
    ok &= create_hook_by_string("LOAD ENDS", reinterpret_cast<void*>(&detour_load_end),
        &orig_load_end, "ModeInGame::LoadEnd");
    return ok;
}

} // namespace mt2mp::hooks
