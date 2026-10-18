#pragma once
#include <native_routes.h>
#include <native_ownership.h>
#include <MinHook.h>
#include <map>
#include <string>

namespace mt2mp::hooks::framework {
// A remove permission must include the feature's in-flight and original-retention guards.
using Reclaim = bool(*)(void* user, uintptr_t target, std::string& cause);
using Log = void(*)(void* user, const char* operation, const std::string& cause);

class Backend {
public:
    Backend() = default;
    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;
    bool bind(const MT2FW_MT2_RoutesV1&, uintptr_t image, const char* owner,
              uint32_t game_thread, Reclaim, Log, void* user);
    bool bind_ownership(const MT2FW_MT2_OwnershipV1&);
    MH_STATUS initialize();
    MH_STATUS create(void* target, void* detour, void** original);
    MH_STATUS enable(void* target);
    MH_STATUS disable(void* target);
    MH_STATUS remove(void* target);
    MH_STATUS cleanup();
    bool read_route(void* target, MT2FW_MT2_RouteReceipt&);
    bool verify_code(uintptr_t address, const void* expected, size_t length, std::string& error);
    bool prepare_free(MT2FW_MT2_FreeFn, MT2FW_MT2_FreeReceipt&, std::string& error);
    bool enable_free(MT2FW_MT2_FreeReceipt&, std::string& error);
    bool disable_free(MT2FW_MT2_FreeReceipt&, std::string& error);
    bool read_free(MT2FW_MT2_FreeReceipt&, std::string& error);
    bool remove_free(MT2FW_MT2_FreeReceipt&, std::string& error);
    const std::string& error() const noexcept { return error_; }
    size_t retained() const noexcept;
private:
    struct Entry {
        std::string name;
        uintptr_t detour=0, trampoline=0;
        uint64_t generation=0;
        uint32_t state=MT2FW_MT2_ROUTE_UNKNOWN;
        bool owned=false, blocked=false;
        std::string cause;
    };
    MT2FW_MT2_RoutesV1 api_{};
    MT2FW_MT2_OwnershipV1 ownership_{};
    MT2FW_MT2_FreeReceipt free_{};
    bool ownership_bound_=false, free_attempted_=false, free_blocked_=false;
    std::string free_failure_;
    uintptr_t image_=0;
    uint32_t thread_=0;
    std::string owner_, error_;
    Reclaim reclaim_=nullptr;
    Log log_=nullptr;
    void* user_=nullptr;
    bool bound_=false;
    std::map<uintptr_t,Entry> entries_;
    bool current(const char*);
    MH_STATUS fail(const char*, MH_STATUS, const std::string&, Entry* = nullptr);
    bool receipt(const MT2FW_MT2_RouteReceipt&, uintptr_t, const Entry*, uint32_t) const;
    MH_STATUS enable_one(uintptr_t, Entry&);
    MH_STATUS disable_one(uintptr_t, Entry&);
    bool free_receipt(const MT2FW_MT2_FreeReceipt&, uint32_t state, uintptr_t replacement) const;
    bool free_fail(const char*, const std::string&, std::string& error, bool block);
    bool free_change(const char*, uint32_t state, MT2FW_MT2_Result (MT2FW_CALL *)(void*,MT2FW_MT2_FreeReceipt*), MT2FW_MT2_FreeReceipt&, std::string&);
};

Backend& process_backend();
bool verify_code(uintptr_t address, const void* expected, size_t length, std::string& error);
const char* status_text(MH_STATUS) noexcept;
}
