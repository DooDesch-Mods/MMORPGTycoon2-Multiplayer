#include "save_scope.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <exception>

namespace mt2mp::subscriber_lifecycle {
namespace {
uint32_t call(SaveScopeCall fn, void* mode) {
    __try { fn(mode); return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
bool callback(SaveScopeBody fn, void* user, std::string& e) {
    try { return fn(user, e); }
    catch(const std::exception& ex) { e = std::string("save scope callback exception: ") + ex.what(); }
    catch(...) { e = "save scope callback unknown C++ exception"; }
    return false;
}
}
bool with_save_scope(const SaveScopeApi& api, void* mode, SaveScopeBody body,
                     SaveScopeBody exit_guard, void* user, SaveScopeReceipt& r, std::string& e) {
    r = {}; e.clear();
    if (!api.enter || !api.leave || !mode || !body || !exit_guard) { e = "save scope API/receiver missing"; return false; }
    r.enter_attempted = true;
    r.enter_exception = call(api.enter, mode);
    if (r.enter_exception) {
        r.unknown = true; e = "native SaveScope Increment exception=" + std::to_string(r.enter_exception); return false;
    }
    r.enter_returned = true; r.body_attempted = true;
    const bool body_ok = callback(body, user, r.body_cause);
    if (!body_ok && r.body_cause.empty()) r.body_cause = "save scope body rejected without cause";
    r.exit_guard_confirmed = callback(exit_guard, user, r.cleanup_cause);
    if (!r.exit_guard_confirmed) {
        r.unknown = true;
        if (r.cleanup_cause.empty()) r.cleanup_cause = "save scope exit owner guard rejected without cause";
    } else {
        r.leave_attempted = true; r.leave_exception = call(api.leave, mode);
        r.leave_returned = r.leave_exception == 0;
        if (!r.leave_returned) { r.unknown = true; r.cleanup_cause = "native SaveScope Decrement exception=" + std::to_string(r.leave_exception); }
    }
    e = r.body_cause;
    if (!r.cleanup_cause.empty()) { if (!e.empty()) e += "; cleanup: "; e += r.cleanup_cause; }
    return body_ok && r.leave_returned;
}
}
