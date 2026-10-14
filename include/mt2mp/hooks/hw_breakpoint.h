#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace mt2mp::hooks {

// Hardware breakpoint callback. Called when the breakpoint fires.
// Parameters: dr_index, exception context (registers).
// The callback runs in the exception handler context - keep it FAST.
// Queue work for the main thread instead of doing heavy processing.
struct HwBpContext {
    int dr_index;
    uintptr_t rip;  // instruction pointer (the breakpoint address)
    uintptr_t rcx, rdx, r8, r9;  // first 4 integer args
    uintptr_t rsp, rbp, rax, rbx, rsi, rdi;
    uintptr_t r10, r11, r12, r13, r14, r15;
};

using HwBpCallback = std::function<void(const HwBpContext& ctx)>;

// Initialize the hardware breakpoint system.
// Registers the Vectored Exception Handler.
bool hw_bp_init();

// Set a hardware breakpoint on the specified address.
// dr_index: 0-3 (x86-64 has 4 debug registers)
// address: the instruction address to break on
// callback: called when the breakpoint fires
// name: descriptive name for logging
bool hw_bp_set(int dr_index, uintptr_t address, HwBpCallback callback, const char* name);

// Clear a hardware breakpoint.
bool hw_bp_clear(int dr_index);

// Shutdown: remove all breakpoints and VEH.
void hw_bp_shutdown();

} // namespace mt2mp::hooks
