#pragma once
#include <cstdint>
#include <string>

namespace mt2mp::subscriber_lifecycle {
inline constexpr uintptr_t array_load_rva = 0xc18440;
inline constexpr uintptr_t context_resolve_rva = 0x8a72b0;
inline constexpr uintptr_t notify_login_rva = 0x2f4330;
inline constexpr uintptr_t notify_logout_rva = 0x2f4060;
inline constexpr uintptr_t lookup_rva = 0x8a7210;
inline constexpr uintptr_t manager_int_load_rva = 0xd3a830;
inline constexpr uintptr_t next_id_property_rva = 0x13cd480;
using ArrayLoad = void(*)(void*, void*, void*);
using Resolve = void(*)(void*);
using Notify = void(*)(void*, void*);
using PropertyLoad = bool(*)(const void*, void*, void*, void*);
struct Api { ArrayLoad array_load = nullptr; Resolve resolve = nullptr; Notify login = nullptr, logout = nullptr; PropertyLoad int_load = nullptr; };
struct CallReceipt {
    bool attempted = false, returned = false;
    uint32_t seh = 0;
    bool native_rejected = false;
};
// These are private primitives, not a complete live replication adapter. Caller
// owns exact-build guards, fresh world/record ownership, and the Unknown lock.
bool load_original(const Api&, void* array, void* record, void* context, CallReceipt&, std::string&);
bool resolve_original(const Api&, void* context, CallReceipt&, std::string&);
bool classify_original(const Api&, void* manager, void* subscriber, bool online, CallReceipt&, std::string&);
bool next_id_original(const Api&, const void* actual_next_id_property, void* manager,
                      void* integer_record, void* context, CallReceipt&, std::string&);
// The caller pins scalar property leaves whose original bodies do not use Context.
bool load_scalar_property(PropertyLoad, const void* property, void* object,
                          void* record, CallReceipt&, std::string&);
}
