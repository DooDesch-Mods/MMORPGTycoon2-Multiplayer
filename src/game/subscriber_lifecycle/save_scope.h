#pragma once
#include <cstdint>
#include <string>

namespace mt2mp::subscriber_lifecycle {
using SaveScopeCall = void(*)(void*);
using SaveScopeBody = bool(*)(void*, std::string&);
struct SaveScopeApi { SaveScopeCall enter = nullptr, leave = nullptr; };
struct SaveScopeReceipt {
    bool enter_attempted = false, enter_returned = false, body_attempted = false;
    bool exit_guard_confirmed = false, leave_attempted = false, leave_returned = false;
    bool unknown = false;
    uint32_t enter_exception = 0, leave_exception = 0;
    std::string body_cause, cleanup_cause;
};
bool with_save_scope(const SaveScopeApi&, void* mode, SaveScopeBody body,
                     SaveScopeBody exit_guard, void* user, SaveScopeReceipt&, std::string&);
}
