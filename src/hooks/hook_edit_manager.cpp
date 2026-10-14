#include "mt2mp/hooks/hook_edit_manager.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/hooks/pattern_scanner.h"
#include "mt2mp/core/logger.h"
#include <windows.h>

namespace mt2mp::hooks {

static void* orig_begin_edit = nullptr;
static void* orig_end_edit = nullptr;

static bool g_edit_in_progress = false;
static EditBeginCallback g_on_edit_begin;
static EditEndCallback g_on_edit_end;

// On x64 Windows, MinGW and MSVC use the same calling convention.
// We use a naked-style detour that preserves all registers perfectly.
// The function signature doesn't matter - we just pass through all args.

// Minimal safe detour: use the raw function pointer type.
// void return, takes this + unknown args (all passed through in registers).
using RawFn = void(*)(void*, void*, void*, void*, void*, void*);

static volatile long g_edit_begin_count = 0;
static volatile long g_edit_end_count = 0;

static void detour_begin_edit(void* a1, void* a2, void* a3, void* a4, void* a5, void* a6) {
    // Increment counter atomically (simplest possible side effect)
    InterlockedIncrement(&g_edit_begin_count);
    g_edit_in_progress = true;

    // Call original
    reinterpret_cast<RawFn>(orig_begin_edit)(a1, a2, a3, a4, a5, a6);

    // Log AFTER original returns (safer - function state is consistent)
    // Use a simple file write to avoid any logger issues
    static bool first_log = true;
    if (first_log) {
        first_log = false;
        FILE* f = fopen("mt2mp_edit_hook.log", "w");
        if (f) { fprintf(f, "EDIT HOOK FIRED! count=%ld a1=%p a2=%p\n", g_edit_begin_count, a1, a2); fclose(f); }
    }

    long cnt = g_edit_begin_count;
    LOG_INFO(">>> EDIT BEGIN #{} [this=0x{:X}]", cnt,
        reinterpret_cast<uintptr_t>(a1));

    if (g_on_edit_begin) {
        g_on_edit_begin("edit");
    }
}

static void detour_end_edit(void* a1, void* a2, void* a3, void* a4, void* a5, void* a6) {
    // Call original first
    reinterpret_cast<RawFn>(orig_end_edit)(a1, a2, a3, a4, a5, a6);

    InterlockedIncrement(&g_edit_end_count);
    g_edit_in_progress = false;

    long ecnt = g_edit_end_count;
    LOG_INFO("<<< EDIT END #{}", ecnt);

    if (g_on_edit_end) {
        g_on_edit_end();
    }
}

void set_edit_begin_callback(EditBeginCallback cb) { g_on_edit_begin = std::move(cb); }
void set_edit_end_callback(EditEndCallback cb) { g_on_edit_end = std::move(cb); }

bool is_edit_in_progress() { return g_edit_in_progress; }
const char* current_edit_reason() { return ""; }

// ── INT3 debug hook: proves the function is actually called ────
static void* g_int3_target = nullptr;
static uint8_t g_int3_saved_byte = 0;
static volatile long g_int3_hit_count = 0;

static LONG WINAPI int3_handler(EXCEPTION_POINTERS* ep) {
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress);
        uintptr_t target = reinterpret_cast<uintptr_t>(g_int3_target);
        if (addr == target) {
            InterlockedIncrement(&g_int3_hit_count);
            // Restore original byte and continue (one-shot)
            DWORD old; VirtualProtect(g_int3_target, 1, PAGE_EXECUTE_READWRITE, &old);
            *reinterpret_cast<uint8_t*>(g_int3_target) = g_int3_saved_byte;
            VirtualProtect(g_int3_target, 1, old, &old);
            // Write proof file
            FILE* f = fopen("mt2mp_int3_proof.log", "a");
            if (f) { fprintf(f, "INT3 HIT at 0x%llX count=%ld RCX=%llX RDX=%llX\n",
                (unsigned long long)addr, g_int3_hit_count,
                (unsigned long long)ep->ContextRecord->Rcx,
                (unsigned long long)ep->ContextRecord->Rdx); fclose(f); }
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

bool install_edit_manager_hooks() {
    LOG_INFO("[EDIT_HOOK] Installing BeginEdit hook, detour at 0x{:X}",
        reinterpret_cast<uintptr_t>(&detour_begin_edit));

    // STRATEGY: Use INT3 breakpoint to PROVE the function is called at our address.
    // If mt2mp_int3_proof.log is created, the function IS called and we know the address is right.
    // Then the MinHook issue is something else (trampoline, relay, etc.)
    uintptr_t fn_addr = find_function_by_string("==> EDIT BEGINS:");
    if (fn_addr) {
        LOG_INFO("[EDIT_HOOK] Placing INT3 at 0x{:X} to prove function is called", fn_addr);
        g_int3_target = reinterpret_cast<void*>(fn_addr);
        g_int3_saved_byte = *reinterpret_cast<uint8_t*>(fn_addr);
        AddVectoredExceptionHandler(1, int3_handler);

        DWORD old_prot;
        VirtualProtect(g_int3_target, 1, PAGE_EXECUTE_READWRITE, &old_prot);
        *reinterpret_cast<uint8_t*>(g_int3_target) = 0xCC; // INT3
        VirtualProtect(g_int3_target, 1, old_prot, &old_prot);

        LOG_INFO("[EDIT_HOOK] INT3 placed. Original byte: 0x{:02X}. Waiting for hit...", g_int3_saved_byte);
    } else {
        LOG_WARN("[EDIT_HOOK] Could not find EDIT BEGINS function");
    }

    // NO MinHook for now - pure INT3 test only.
    // MinHook would overwrite the INT3 byte, so we skip it for this test.

    return fn_addr != 0;
}

} // namespace mt2mp::hooks
