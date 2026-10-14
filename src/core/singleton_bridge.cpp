#include "mt2mp/core/singleton_bridge.h"
#include "mt2mp/hooks/vtable_hook.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/hooks/pattern_scanner.h"
#include "mt2mp/core/logger.h"

#include <windows.h>
#include <cstring>

// Must be in a separate function without C++ objects for SEH compat
static uintptr_t safe_read_ptr(uintptr_t addr) {
    uintptr_t result = 0;
    __try {
        result = *reinterpret_cast<uintptr_t*>(addr);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        result = 0;
    }
    return result;
}

namespace mt2mp::core {

// ── Static trampoline for coreGame::Update vtable hook ──────────
// On x64 Windows, both MinGW and MSVC use the same calling convention:
// RCX = this, XMM1 = float timeStep (or RDX for integer args)
// For virtual void Update(float timeStep): RCX=this, XMM1=timeStep

static SingletonBridge* g_bridge = nullptr;
using GameUpdateFn = void(*)(void* this_ptr, float timeStep);
static GameUpdateFn g_orig_game_update = nullptr;

static void game_update_detour(void* this_ptr, float timeStep) {
    if (g_orig_game_update) {
        g_orig_game_update(this_ptr, timeStep);
    }
    if (g_bridge) {
        g_bridge->on_game_update(this_ptr, timeStep);
    }
}

// ── Singleton Bridge Implementation ──────────────────────────────

SingletonBridge& SingletonBridge::instance() {
    static SingletonBridge inst;
    return inst;
}

bool SingletonBridge::init() {
    if (initialized_) return true;
    g_bridge = this;

    LOG_INFO("[BRIDGE] Initializing SingletonBridge...");

    // Step 1: Find key function addresses via string scanning
    bool found_any = find_key_functions();

    // Step 2: Try to find the coreGame instance
    bool found_game = find_game_instance();

    // Step 3: Try to find vsSingletonManager
    find_singleton_manager();

    if (found_any) {
        initialized_ = true;
        LOG_INFO("[BRIDGE] Initialized (game_instance={}, fn_save={}, fn_load={}, fn_edit_begin={}, fn_edit_end={})",
            game_instance_ ? "found" : "null",
            fn_save_ ? "found" : "null",
            fn_load_binary_ ? "found" : "null",
            fn_edit_begin_ ? "found" : "null",
            fn_edit_end_ ? "found" : "null");
    } else {
        LOG_WARN("[BRIDGE] Could not find any key functions");
    }

    return initialized_;
}

bool SingletonBridge::find_key_functions() {
    int found = 0;

    // ModeInGame::Save - called when the game autosaves
    fn_save_ = reinterpret_cast<void*>(
        hooks::find_function_by_string("!! MMO save:"));
    if (fn_save_) {
        LOG_INFO("[BRIDGE] fn_save at 0x{:X}", reinterpret_cast<uintptr_t>(fn_save_));
        found++;
    }

    // ModeInGame::LoadBinary - loads a compressed save file
    fn_load_binary_ = reinterpret_cast<void*>(
        hooks::find_function_by_string("Loading compressed binary save file"));
    if (fn_load_binary_) {
        LOG_INFO("[BRIDGE] fn_load_binary at 0x{:X}", reinterpret_cast<uintptr_t>(fn_load_binary_));
        found++;
    }

    // EditManager::BeginEdit
    fn_edit_begin_ = reinterpret_cast<void*>(
        hooks::find_function_by_string("==> EDIT BEGINS:"));
    if (fn_edit_begin_) {
        LOG_INFO("[BRIDGE] fn_edit_begin at 0x{:X}", reinterpret_cast<uintptr_t>(fn_edit_begin_));
        found++;
    }

    // EditManager::EndEdit
    fn_edit_end_ = reinterpret_cast<void*>(
        hooks::find_function_by_string("==> EDIT COMPLETE"));
    if (fn_edit_end_) {
        LOG_INFO("[BRIDGE] fn_edit_end at 0x{:X}", reinterpret_cast<uintptr_t>(fn_edit_end_));
        found++;
    }

    // coreGame::Init - used to locate the game instance
    fn_game_init_ = reinterpret_cast<void*>(
        hooks::find_function_by_string("Initialising game"));
    if (fn_game_init_) {
        LOG_INFO("[BRIDGE] fn_game_init at 0x{:X}", reinterpret_cast<uintptr_t>(fn_game_init_));
        found++;
    }

    // Additional strings to try:
    // "Starting Game:" → coreGame::StartTimer()
    // "Stopping Game:" → coreGame::StopTimer()
    // "LOAD BEGINS" → LoadBinary start marker
    // "LOAD ENDS" → LoadBinary end marker
    // "set mode:" → game mode switching

    return found > 0;
}

bool SingletonBridge::find_game_instance() {
    // Strategy: Find the `core::s_game` static pointer by scanning for
    // code that references fn_game_init_ (coreGame::Init).
    //
    // In core::Go(), there's: s_game->Init()
    // This compiles to: load s_game, then call Init through vtable.
    // But the derived class overrides Init, so the vtable won't contain
    // our fn_game_init_ directly.
    //
    // Alternative strategy: Scan .rdata for ANY vtable that has function
    // pointers in .text, then scan .data/.bss for objects pointing to those
    // vtables. Filter by checking if the vtable has the right number of
    // entries and the Init entry calls a function containing our known string.
    //
    // Simplest strategy: coreGame::Init calls vsLog("Initialising game...").
    // The derived class Init ALSO calls the base Init (via base::Init()).
    // So the derived Init should CALL fn_game_init_. Find a function in .text
    // that CALLs fn_game_init_, that's likely the derived Init override.
    // Then find a vtable containing THAT function.

    if (!fn_game_init_) {
        LOG_WARN("[BRIDGE] Cannot find game instance without fn_game_init");
        return false;
    }

    auto text_sec = hooks::get_text_section();
    if (!text_sec.base) return false;

    // Build list of candidate Init functions to search for in vtables:
    // 1) fn_game_init_ itself (if the derived class doesn't override Init)
    // 2) Any function that CALLs fn_game_init_ (derived Init override)
    std::vector<uintptr_t> init_candidates;
    init_candidates.push_back(reinterpret_cast<uintptr_t>(fn_game_init_));

    const uint8_t* text = reinterpret_cast<const uint8_t*>(text_sec.base);
    for (size_t i = 0; i < text_sec.size - 5; ++i) {
        if (text[i] != 0xE8) continue;
        int32_t disp = *reinterpret_cast<const int32_t*>(text + i + 1);
        uintptr_t call_target = text_sec.base + i + 5 + disp;
        if (call_target == reinterpret_cast<uintptr_t>(fn_game_init_)) {
            uintptr_t caller = hooks::find_function_start(text_sec.base + i);
            if (caller && caller != reinterpret_cast<uintptr_t>(fn_game_init_)) {
                init_candidates.push_back(caller);
                LOG_INFO("[BRIDGE] Found derived Init candidate at 0x{:X}", caller);
            }
        }
    }

    uintptr_t derived_init = 0;
    if (init_candidates.size() > 1) {
        derived_init = init_candidates[1]; // First caller found
    }

    // Prioritize derived Init (if found) - that's what the actual game object uses
    if (init_candidates.size() > 1) {
        std::swap(init_candidates[0], init_candidates[1]);
    }

    LOG_INFO("[BRIDGE] Have {} Init candidates to search in vtables", init_candidates.size());
    for (auto c : init_candidates)
        LOG_INFO("[BRIDGE]   Candidate: 0x{:X}", c);

    // Diagnostic: raw scan .rdata for fn_game_init_ as a pointer value
    {
        HMODULE exe_diag = GetModuleHandleA(nullptr);
        auto dos_diag = reinterpret_cast<IMAGE_DOS_HEADER*>(exe_diag);
        auto nt_diag = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(exe_diag) + dos_diag->e_lfanew);
        auto sec_diag = IMAGE_FIRST_SECTION(nt_diag);
        uintptr_t base_diag = reinterpret_cast<uintptr_t>(exe_diag);
        for (int si = 0; si < nt_diag->FileHeader.NumberOfSections; ++si) {
            if (strncmp(reinterpret_cast<const char*>(sec_diag[si].Name), ".rdata", 6) != 0) continue;
            uintptr_t rs = base_diag + sec_diag[si].VirtualAddress;
            uintptr_t re = rs + sec_diag[si].Misc.VirtualSize;
            int found_count = 0;
            for (uintptr_t a = rs; a + 8 <= re; a += 8) {
                uintptr_t v = *reinterpret_cast<uintptr_t*>(a);
                for (auto c : init_candidates) {
                    if (v == c) {
                        LOG_INFO("[BRIDGE] DIAG: Init candidate 0x{:X} found at .rdata+0x{:X} (abs 0x{:X})",
                            c, a - rs, a);
                        // Show surrounding values
                        for (int d = -3; d <= 3; ++d) {
                            uintptr_t sa = a + d * 8;
                            if (sa >= rs && sa + 8 <= re) {
                                uintptr_t sv = *reinterpret_cast<uintptr_t*>(sa);
                                bool in_text = sv >= text_sec.base && sv < text_sec.base + text_sec.size;
                                LOG_INFO("[BRIDGE] DIAG:   [{}] = 0x{:X} {}", d, sv, in_text ? "(text)" : "");
                            }
                        }
                        found_count++;
                    }
                }
            }
            if (found_count == 0) {
                LOG_WARN("[BRIDGE] DIAG: NO Init candidates found in .rdata at all!");
            }
            break;
        }
    }

    // Step 2: Scan .rdata for a vtable containing any Init candidate
    HMODULE exe = GetModuleHandleA(nullptr);
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(exe);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<uint8_t*>(exe) + dos->e_lfanew);
    auto sections = IMAGE_FIRST_SECTION(nt);
    int num_sections = nt->FileHeader.NumberOfSections;
    uintptr_t base = reinterpret_cast<uintptr_t>(exe);

    // Collect ALL vtable candidates that contain any Init function
    struct VtableCandidate { uintptr_t vtable_start; uintptr_t matched_init; int init_offset; };
    std::vector<VtableCandidate> vtable_candidates;

    for (int s = 0; s < num_sections; ++s) {
        auto& sec = sections[s];
        if (strncmp(reinterpret_cast<const char*>(sec.Name), ".rdata", 6) != 0)
            continue;

        uintptr_t sec_start = base + sec.VirtualAddress;
        uintptr_t sec_end = sec_start + sec.Misc.VirtualSize;

        for (uintptr_t addr = sec_start; addr + sizeof(void*) <= sec_end; addr += sizeof(void*)) {
            uintptr_t val = *reinterpret_cast<uintptr_t*>(addr);

            bool is_match = false;
            for (uintptr_t candidate : init_candidates) {
                if (val == candidate) { is_match = true; break; }
            }
            if (!is_match) continue;

            // GCC/MinGW Itanium ABI vtable layout:
            //   vtable[-2] = offset_to_top (typically 0)
            //   vtable[-1] = typeinfo pointer (in .rdata)
            //   vtable[0]  = first virtual function (in .text)
            //   vtable[1]  = second virtual function (in .text)
            //   ...
            // Our match is at some vtable[N]. Check that entries AFTER it
            // are also .text pointers (at least 2 more).
            bool looks_like_vtable = true;
            for (int check = 1; check <= 3; ++check) {
                uintptr_t ca = addr + check * sizeof(void*);
                if (ca + sizeof(void*) > sec_end) { looks_like_vtable = false; break; }
                uintptr_t fp = *reinterpret_cast<uintptr_t*>(ca);
                if (fp < text_sec.base || fp >= text_sec.base + text_sec.size) { looks_like_vtable = false; break; }
            }
            if (!looks_like_vtable) continue;

            // Walk back to find the first function pointer in this vtable.
            // Stop when we hit a non-.text pointer (the typeinfo/offset_to_top entries).
            uintptr_t vt_start = addr;
            while (vt_start >= sec_start + sizeof(void*)) {
                uintptr_t prev = *reinterpret_cast<uintptr_t*>(vt_start - sizeof(void*));
                if (prev < text_sec.base || prev >= text_sec.base + text_sec.size) break;
                vt_start -= sizeof(void*);
            }
            int init_off = (int)((addr - vt_start) / sizeof(void*));
            vtable_candidates.push_back({vt_start, val, init_off});
            LOG_INFO("[BRIDGE] Found vtable candidate at 0x{:X} (Init at offset {}, func=0x{:X})",
                vt_start, init_off, val);
        }
    }

    LOG_INFO("[BRIDGE] Found {} vtable candidates total", vtable_candidates.size());
    if (vtable_candidates.empty()) {
        LOG_WARN("[BRIDGE] Could not find any vtable containing Init");
        return false;
    }

    // Step 3: Try each vtable candidate - scan .data and .bss for instances
    for (auto& vtc : vtable_candidates) {
        uintptr_t vtable_addr = vtc.vtable_start;
        LOG_INFO("[BRIDGE] Trying vtable 0x{:X} (Init at offset {})...", vtable_addr, vtc.init_offset);

    int scan_count = 0;
    int deref_count = 0;
    for (int s = 0; s < num_sections; ++s) {
        auto& sec = sections[s];
        if (strncmp(reinterpret_cast<const char*>(sec.Name), ".data", 5) != 0 &&
            strncmp(reinterpret_cast<const char*>(sec.Name), ".bss", 4) != 0)
            continue;

        uintptr_t sec_start = base + sec.VirtualAddress;
        uintptr_t sec_end = sec_start + sec.Misc.VirtualSize;
        LOG_INFO("[BRIDGE] Scanning section '{}' at 0x{:X}-0x{:X} ({} KB)",
            reinterpret_cast<const char*>(sec.Name), sec_start, sec_end,
            (sec_end - sec_start) / 1024);

        for (uintptr_t addr = sec_start; addr + sizeof(void*) <= sec_end; addr += sizeof(void*)) {
            uintptr_t ptr_val = *reinterpret_cast<uintptr_t*>(addr);
            if (ptr_val == 0) continue;
            if (ptr_val < 0x10000) continue; // Skip small values (not pointers)
            scan_count++;

            // Safely dereference the pointer (SEH-protected)
            uintptr_t obj_vptr = safe_read_ptr(ptr_val);
            if (obj_vptr == 0) continue;
            deref_count++;

            if (obj_vptr == vtable_addr) {
                game_instance_ = reinterpret_cast<void*>(ptr_val);
                LOG_INFO("[BRIDGE] Found coreGame instance at 0x{:X} (static ptr at 0x{:X})",
                    ptr_val, addr);

                auto entries = hooks::VtableHook::dump_vtable(game_instance_, 12);
                for (size_t i = 0; i < entries.size(); ++i) {
                    const char* label = "";
                    if (entries[i] == derived_init) label = " [derived Init]";
                    else if (entries[i] == reinterpret_cast<uintptr_t>(fn_game_init_)) label = " [base Init]";
                    LOG_INFO("[BRIDGE]   vtable[{}] = 0x{:X}{}", i, entries[i], label);
                }
                return true;
            }
        }
    }

    LOG_INFO("[BRIDGE] vtable 0x{:X}: scanned {} ptrs, {} deref'd, no match",
             vtable_addr, scan_count, deref_count);
    } // end for vtc

    LOG_WARN("[BRIDGE] Could not find coreGame instance via vtable scan");
    return false;
}

bool SingletonBridge::find_singleton_manager() {
    // Try to find vsSingletonManager by scanning for its RTTI string.
    // MinGW RTTI name for vsSingletonManager* would be something like
    // "P20vsSingletonManager" or "20vsSingletonManager".
    // We search for "vsSingletonManager" as a substring in .rdata.

    HMODULE exe = GetModuleHandleA(nullptr);
    if (!exe) return false;

    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(exe);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<uint8_t*>(exe) + dos->e_lfanew);
    auto sections = IMAGE_FIRST_SECTION(nt);
    int num_sections = nt->FileHeader.NumberOfSections;
    uintptr_t base = reinterpret_cast<uintptr_t>(exe);

    for (int s = 0; s < num_sections; ++s) {
        auto& sec = sections[s];
        if (strncmp(reinterpret_cast<const char*>(sec.Name), ".rdata", 6) != 0)
            continue;

        uintptr_t sec_start = base + sec.VirtualAddress;
        size_t sec_size = sec.Misc.VirtualSize;

        // Search for "vsSingletonManager" string
        const char* needle = "vsSingletonManager";
        size_t needle_len = strlen(needle);
        const uint8_t* data = reinterpret_cast<const uint8_t*>(sec_start);

        for (size_t i = 0; i + needle_len <= sec_size; ++i) {
            if (memcmp(data + i, needle, needle_len) == 0) {
                LOG_INFO("[BRIDGE] Found 'vsSingletonManager' RTTI string at 0x{:X}",
                    sec_start + i);
                // TODO: Follow the RTTI chain to find s_instance
                // For now, just log that we found it
                break;
            }
        }
        break;
    }

    // Finding the actual s_instance pointer requires following RTTI structures
    // or finding the RegisterSingleton calls. This is complex and can be
    // deferred - the vtable-based approach works without it.
    return singleton_manager_ != nullptr;
}

void* SingletonBridge::get_singleton(const char* rtti_name) {
    // TODO: Once singleton_manager_ is found, call GetSingleton on it
    // For now, return nullptr
    (void)rtti_name;
    return nullptr;
}

bool SingletonBridge::hook_game_update(std::function<void(float)> callback) {
    update_callback_ = std::move(callback);

    // If we have the game instance, use vtable hooking (best approach)
    if (game_instance_) {
        if (fn_game_init_) {
            int init_index = hooks::VtableHook::find_entry(
                game_instance_, fn_game_init_, 32);
            if (init_index >= 0) {
                update_vtable_index_ = init_index + 2;
            } else {
                update_vtable_index_ = 4;
            }
        } else {
            update_vtable_index_ = 4;
        }

        void* detour = reinterpret_cast<void*>(&game_update_detour);
        if (hooks::VtableHook::hook(game_instance_, update_vtable_index_,
                                      detour, &orig_game_update_)) {
            g_orig_game_update = reinterpret_cast<GameUpdateFn>(orig_game_update_);
            LOG_INFO("[BRIDGE] coreGame::Update hooked via vtable[{}]", update_vtable_index_);
            return true;
        }
    }

    // Fallback: use MinHook on the base coreGame::Update function directly.
    // From the base class vtable (found during init), Update() is 2 entries
    // after Init(). We read the address from the vtable in .rdata.
    if (fn_game_init_) {
        // We stored the vtable location during find_game_instance.
        // Re-derive it: scan .rdata for fn_game_init_ and read offset +2 entry.
        HMODULE exe = GetModuleHandleA(nullptr);
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(exe);
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
            reinterpret_cast<uint8_t*>(exe) + dos->e_lfanew);
        auto sections = IMAGE_FIRST_SECTION(nt);
        uintptr_t base = reinterpret_cast<uintptr_t>(exe);
        auto text_sec = hooks::get_text_section();

        for (int s = 0; s < nt->FileHeader.NumberOfSections; ++s) {
            if (strncmp(reinterpret_cast<const char*>(sections[s].Name), ".rdata", 6) != 0)
                continue;
            uintptr_t rs = base + sections[s].VirtualAddress;
            uintptr_t re = rs + sections[s].Misc.VirtualSize;
            for (uintptr_t a = rs; a + 8 <= re; a += 8) {
                if (*reinterpret_cast<uintptr_t*>(a) == reinterpret_cast<uintptr_t>(fn_game_init_)) {
                    // Found Init in vtable. Read Update = Init + 2 entries
                    uintptr_t update_addr = *reinterpret_cast<uintptr_t*>(a + 2 * sizeof(void*));
                    if (update_addr >= text_sec.base && update_addr < text_sec.base + text_sec.size) {
                        LOG_INFO("[BRIDGE] Found Update at 0x{:X} (vtable Init+2)", update_addr);

                        // Try MinHook on this function
                        void* target = reinterpret_cast<void*>(update_addr);
                        if (hooks::create_hook(target,
                                reinterpret_cast<void*>(&game_update_detour),
                                &orig_game_update_)) {
                            g_orig_game_update = reinterpret_cast<GameUpdateFn>(orig_game_update_);
                            LOG_INFO("[BRIDGE] coreGame::Update hooked via MinHook at 0x{:X}", update_addr);
                            return true;
                        } else {
                            LOG_WARN("[BRIDGE] MinHook failed on Update at 0x{:X}", update_addr);
                        }
                    }
                    break;
                }
            }
            break;
        }
    }

    LOG_WARN("[BRIDGE] Could not hook game Update (no instance, MinHook failed)");
    return false;
}

void SingletonBridge::on_game_update(void* this_ptr, float dt) {
    // Capture game instance on first call
    if (!game_instance_) {
        game_instance_ = this_ptr;
        LOG_INFO("[BRIDGE] Captured coreGame instance from Update: 0x{:X}",
            reinterpret_cast<uintptr_t>(this_ptr));

        // Dump vtable for debugging
        auto entries = hooks::VtableHook::dump_vtable(game_instance_, 12);
        for (size_t i = 0; i < entries.size(); ++i) {
            LOG_INFO("[BRIDGE]   vtable[{}] = 0x{:X}", i, entries[i]);
        }
    }
    if (update_callback_) {
        update_callback_(dt);
    }
}

void SingletonBridge::unhook_game_update() {
    if (game_instance_ && orig_game_update_ && update_vtable_index_ >= 0) {
        hooks::VtableHook::unhook(game_instance_, update_vtable_index_, orig_game_update_);
        orig_game_update_ = nullptr;
        g_orig_game_update = nullptr;
        update_callback_ = nullptr;
        LOG_INFO("[BRIDGE] coreGame::Update unhooked");
    }
}

} // namespace mt2mp::core
