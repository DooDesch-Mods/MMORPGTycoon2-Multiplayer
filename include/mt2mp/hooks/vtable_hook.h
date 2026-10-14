#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace mt2mp::hooks {

// Generic vtable hooking utility.
// Works across MinGW/MSVC ABI because it replaces pointers in the vtable,
// not code in the function body. On x86-64 Windows both compilers use
// the Microsoft x64 calling convention, so detour functions written in
// MSVC can be called in place of MinGW-compiled virtual methods.

class VtableHook {
public:
    // Replace the function pointer at vtable[index] of `object` with `detour`.
    // Stores the original function pointer in *original.
    // Returns true on success.
    static bool hook(void* object, int vtable_index,
                     void* detour, void** original);

    // Restore the original function pointer at vtable[index].
    static bool unhook(void* object, int vtable_index, void* original);

    // Read the function pointer at vtable[index] without modifying it.
    static void* read_vtable(void* object, int vtable_index);

    // Read the full vtable pointer (vptr) from an object.
    static void** get_vptr(void* object);

    // Dump the first `count` vtable entries for debugging.
    // Returns addresses as hex strings.
    static std::vector<uintptr_t> dump_vtable(void* object, int count);

    // Find a vtable entry that points to a known function address.
    // Useful for verifying vtable layout at runtime.
    // Returns the index, or -1 if not found within max_search entries.
    static int find_entry(void* object, void* function_addr, int max_search = 32);

private:
    static bool set_page_writable(void* addr, size_t size, uint32_t& old_protect);
    static bool restore_page_protect(void* addr, size_t size, uint32_t old_protect);
};

} // namespace mt2mp::hooks
