#include "mt2mp/hooks/vtable_hook.h"
#include "mt2mp/core/logger.h"

#include <windows.h>
#include <cstring>

namespace mt2mp::hooks {

void** VtableHook::get_vptr(void* object) {
    if (!object) return nullptr;
    // The vptr is the first pointer-sized value in the object
    return *reinterpret_cast<void***>(object);
}

void* VtableHook::read_vtable(void* object, int vtable_index) {
    void** vptr = get_vptr(object);
    if (!vptr) return nullptr;
    return vptr[vtable_index];
}

std::vector<uintptr_t> VtableHook::dump_vtable(void* object, int count) {
    std::vector<uintptr_t> entries;
    void** vptr = get_vptr(object);
    if (!vptr) return entries;

    for (int i = 0; i < count; ++i) {
        // Safety: check the address is readable
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(vptr + i, &mbi, sizeof(mbi)) == 0) break;
        if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
            break;
        entries.push_back(reinterpret_cast<uintptr_t>(vptr[i]));
    }
    return entries;
}

int VtableHook::find_entry(void* object, void* function_addr, int max_search) {
    void** vptr = get_vptr(object);
    if (!vptr) return -1;

    for (int i = 0; i < max_search; ++i) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(vptr + i, &mbi, sizeof(mbi)) == 0) break;
        if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
            break;
        if (vptr[i] == function_addr)
            return i;
    }
    return -1;
}

bool VtableHook::set_page_writable(void* addr, size_t size, uint32_t& old_protect) {
    DWORD old_prot = 0;
    if (!VirtualProtect(addr, size, PAGE_READWRITE, &old_prot)) {
        LOG_ERROR("[VTABLE] VirtualProtect(RW) failed at 0x{:X}: error {}",
            reinterpret_cast<uintptr_t>(addr), GetLastError());
        return false;
    }
    old_protect = old_prot;
    return true;
}

bool VtableHook::restore_page_protect(void* addr, size_t size, uint32_t old_protect) {
    DWORD dummy = 0;
    return VirtualProtect(addr, size, old_protect, &dummy) != 0;
}

bool VtableHook::hook(void* object, int vtable_index, void* detour, void** original) {
    if (!object || !detour || !original) {
        LOG_ERROR("[VTABLE] hook: null argument");
        return false;
    }

    void** vptr = get_vptr(object);
    if (!vptr) {
        LOG_ERROR("[VTABLE] hook: could not read vptr");
        return false;
    }

    void** slot = &vptr[vtable_index];

    // Save the original function pointer
    *original = *slot;

    // Make the vtable page writable
    uint32_t old_protect = 0;
    if (!set_page_writable(slot, sizeof(void*), old_protect)) {
        return false;
    }

    // Replace the vtable entry
    *slot = detour;

    // Restore original page protection
    restore_page_protect(slot, sizeof(void*), old_protect);

    LOG_INFO("[VTABLE] Hooked vtable[{}]: 0x{:X} → 0x{:X}",
        vtable_index,
        reinterpret_cast<uintptr_t>(*original),
        reinterpret_cast<uintptr_t>(detour));

    return true;
}

bool VtableHook::unhook(void* object, int vtable_index, void* original) {
    if (!object || !original) return false;

    void** vptr = get_vptr(object);
    if (!vptr) return false;

    void** slot = &vptr[vtable_index];

    uint32_t old_protect = 0;
    if (!set_page_writable(slot, sizeof(void*), old_protect)) {
        return false;
    }

    *slot = original;

    restore_page_protect(slot, sizeof(void*), old_protect);

    LOG_INFO("[VTABLE] Unhooked vtable[{}]: restored 0x{:X}",
        vtable_index, reinterpret_cast<uintptr_t>(original));

    return true;
}

} // namespace mt2mp::hooks
