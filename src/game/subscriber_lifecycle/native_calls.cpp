#include "native_calls.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace mt2mp::subscriber_lifecycle {
namespace {
uint32_t load_seh(ArrayLoad fn, void* array, void* record, void* context, CallReceipt* out) {
    __try { out->attempted = true; fn(array, record, context); out->returned = true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
uint32_t resolve_seh(Resolve fn, void* context, CallReceipt* out) {
    __try { out->attempted = true; fn(context); out->returned = true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
uint32_t notify_seh(Notify fn, void* manager, void* subscriber, CallReceipt* out) {
    __try { out->attempted = true; fn(manager, subscriber); out->returned = true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
uint32_t property_seh(PropertyLoad fn, const void* property, void* manager, void* record, void* context, CallReceipt* out) {
    __try { out->attempted = true; out->native_rejected = !fn(property, manager, record, context); out->returned = true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
bool result(const CallReceipt& out, const char* operation, std::string& error) {
    if (out.seh) { error = std::string(operation) + " SEH=" + std::to_string(out.seh) + "; effects unknown, do not replay"; return false; }
    if (!out.returned) { error = std::string(operation) + " did not return; effects unknown, do not replay"; return false; }
    if (out.native_rejected) { error = std::string(operation) + " rejected after entry; effects unknown, do not replay"; return false; }
    error.clear(); return true;
}
}
bool load_original(const Api& api, void* array, void* record, void* context, CallReceipt& out, std::string& error) {
    out = {};
    if (!api.array_load || !array || !record || !context) { error = "native Subscriber ArrayLoad arguments incomplete"; return false; }
    out.seh = load_seh(api.array_load, array, record, context, &out);
    return result(out, "native Subscriber ArrayLoad", error);
}
bool resolve_original(const Api& api, void* context, CallReceipt& out, std::string& error) {
    out = {};
    if (!api.resolve || !context) { error = "native Subscriber ContextResolve arguments incomplete"; return false; }
    out.seh = resolve_seh(api.resolve, context, &out);
    return result(out, "native Subscriber ContextResolve", error);
}
bool classify_original(const Api& api, void* manager, void* subscriber, bool online, CallReceipt& out, std::string& error) {
    out = {}; const auto fn = online ? api.login : api.logout;
    if (!fn || !manager || !subscriber) { error = "native Subscriber Notify arguments incomplete"; return false; }
    out.seh = notify_seh(fn, manager, subscriber, &out);
    return result(out, "native Subscriber Notify", error);
}
bool next_id_original(const Api& api, const void* property, void* manager, void* record, void* context, CallReceipt& out, std::string& error) {
    out = {};
    if (!api.int_load || !property || !manager || !record || !context) { error = "native nextSubscriberId property arguments incomplete"; return false; }
    out.seh = property_seh(api.int_load, property, manager, record, context, &out);
    return result(out, "native nextSubscriberId Property.Load", error);
}
bool load_scalar_property(PropertyLoad load, const void* property, void* object, void* record, CallReceipt& out, std::string& error) {
    out = {};
    if (!load || !property || !object || !record) { error = "native scalar Property.Load arguments incomplete"; return false; }
    out.seh = property_seh(load, property, object, record, nullptr, &out);
    return result(out, "native scalar Property.Load", error);
}
}
