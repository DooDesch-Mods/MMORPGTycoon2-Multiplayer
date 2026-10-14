#include "mt2mp/hooks/hw_breakpoint.h"
#include "mt2mp/core/logger.h"

#include <windows.h>
#include <tlhelp32.h>
#include <mutex>
#include <array>

namespace mt2mp::hooks {

struct HwBpEntry {
    uintptr_t address = 0;
    HwBpCallback callback;
    const char* name = nullptr;
    bool active = false;
};

static std::array<HwBpEntry, 4> g_breakpoints;
static PVOID g_veh_handle = nullptr;
static std::mutex g_bp_mutex;

// Apply debug registers to a single thread
static bool apply_dr_to_thread(HANDLE hThread) {
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    if (!GetThreadContext(hThread, &ctx)) return false;

    // Set DR0-DR3 addresses
    ctx.Dr0 = g_breakpoints[0].active ? g_breakpoints[0].address : 0;
    ctx.Dr1 = g_breakpoints[1].active ? g_breakpoints[1].address : 0;
    ctx.Dr2 = g_breakpoints[2].active ? g_breakpoints[2].address : 0;
    ctx.Dr3 = g_breakpoints[3].active ? g_breakpoints[3].address : 0;

    // DR7: enable/disable individual breakpoints
    // Bits: L0=local DR0, L1=local DR1, L2=local DR2, L3=local DR3
    // Condition: 00=execute, 01=write, 11=read/write
    // Length: 00=1 byte
    ctx.Dr7 = 0;
    if (g_breakpoints[0].active) ctx.Dr7 |= (1 << 0);  // L0
    if (g_breakpoints[1].active) ctx.Dr7 |= (1 << 2);  // L1
    if (g_breakpoints[2].active) ctx.Dr7 |= (1 << 4);  // L2
    if (g_breakpoints[3].active) ctx.Dr7 |= (1 << 6);  // L3
    // Conditions: all execute (00), length all 1-byte (00) - default bits are fine

    return SetThreadContext(hThread, &ctx) != 0;
}

// Apply debug registers to ALL threads in the process
static void apply_dr_to_all_threads() {
    DWORD pid = GetCurrentProcessId();
    DWORD tid = GetCurrentThreadId();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    THREADENTRY32 te = {};
    te.dwSize = sizeof(te);

    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID != pid) continue;

            HANDLE hThread = OpenThread(
                THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME,
                FALSE, te.th32ThreadID);
            if (!hThread) continue;

            if (te.th32ThreadID != tid) {
                SuspendThread(hThread);
            }

            apply_dr_to_thread(hThread);

            if (te.th32ThreadID != tid) {
                ResumeThread(hThread);
            }

            CloseHandle(hThread);
        } while (Thread32Next(snap, &te));
    }

    CloseHandle(snap);
}

// VEH handler for hardware breakpoints
static LONG WINAPI hw_bp_veh_handler(EXCEPTION_POINTERS* ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    uintptr_t addr = reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress);

    // Check which DR triggered
    DWORD dr6 = (DWORD)ep->ContextRecord->Dr6;

    for (int i = 0; i < 4; ++i) {
        if (!(dr6 & (1 << i))) continue;
        if (!g_breakpoints[i].active) continue;
        if (g_breakpoints[i].address != addr) continue;

        // Clear DR6 condition flag
        ep->ContextRecord->Dr6 &= ~(1 << i);

        // Build context for callback
        HwBpContext ctx = {};
        ctx.dr_index = i;
        ctx.rip = addr;
        ctx.rcx = ep->ContextRecord->Rcx;
        ctx.rdx = ep->ContextRecord->Rdx;
        ctx.r8 = ep->ContextRecord->R8;
        ctx.r9 = ep->ContextRecord->R9;
        ctx.rsp = ep->ContextRecord->Rsp;
        ctx.rbp = ep->ContextRecord->Rbp;
        ctx.rax = ep->ContextRecord->Rax;
        ctx.rbx = ep->ContextRecord->Rbx;
        ctx.rsi = ep->ContextRecord->Rsi;
        ctx.rdi = ep->ContextRecord->Rdi;
        ctx.r10 = ep->ContextRecord->R10;
        ctx.r11 = ep->ContextRecord->R11;
        ctx.r12 = ep->ContextRecord->R12;
        ctx.r13 = ep->ContextRecord->R13;
        ctx.r14 = ep->ContextRecord->R14;
        ctx.r15 = ep->ContextRecord->R15;

        // Fire callback
        if (g_breakpoints[i].callback) {
            g_breakpoints[i].callback(ctx);
        }

        // Re-enable the breakpoint (RF flag resumes without re-triggering)
        ep->ContextRecord->EFlags |= (1 << 16); // Resume Flag

        return EXCEPTION_CONTINUE_EXECUTION;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

bool hw_bp_init() {
    if (g_veh_handle) return true;

    g_veh_handle = AddVectoredExceptionHandler(1, hw_bp_veh_handler);
    if (!g_veh_handle) {
        LOG_ERROR("[HWBP] Failed to register VEH handler");
        return false;
    }

    LOG_INFO("[HWBP] Hardware breakpoint system initialized");
    return true;
}

bool hw_bp_set(int dr_index, uintptr_t address, HwBpCallback callback, const char* name) {
    if (dr_index < 0 || dr_index > 3) {
        LOG_ERROR("[HWBP] Invalid DR index: {}", dr_index);
        return false;
    }

    {
        std::lock_guard lock(g_bp_mutex);
        g_breakpoints[dr_index].address = address;
        g_breakpoints[dr_index].callback = std::move(callback);
        g_breakpoints[dr_index].name = name;
        g_breakpoints[dr_index].active = true;
    }

    apply_dr_to_all_threads();

    LOG_INFO("[HWBP] DR{} set on 0x{:X} ({})", dr_index, address, name ? name : "unnamed");
    return true;
}

bool hw_bp_clear(int dr_index) {
    if (dr_index < 0 || dr_index > 3) return false;

    {
        std::lock_guard lock(g_bp_mutex);
        g_breakpoints[dr_index].active = false;
        g_breakpoints[dr_index].address = 0;
        g_breakpoints[dr_index].callback = nullptr;
        g_breakpoints[dr_index].name = nullptr;
    }

    apply_dr_to_all_threads();

    LOG_INFO("[HWBP] DR{} cleared", dr_index);
    return true;
}

void hw_bp_shutdown() {
    for (int i = 0; i < 4; ++i) {
        if (g_breakpoints[i].active) {
            hw_bp_clear(i);
        }
    }

    if (g_veh_handle) {
        RemoveVectoredExceptionHandler(g_veh_handle);
        g_veh_handle = nullptr;
    }

    LOG_INFO("[HWBP] Hardware breakpoint system shut down");
}

} // namespace mt2mp::hooks
