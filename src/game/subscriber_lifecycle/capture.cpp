#include "capture.h"
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
struct alignas(8) Record { std::array<uint8_t, 0x78> data{}; };
struct alignas(8) SaveContext { std::array<uint8_t, 0x10> data{}; };
uint32_t save_seh(const CaptureApi* api, const void* subscriber, Record* record,
                  SaveContext* context, CaptureReceipt* receipt) {
    __try {
        api->record_construct(record); receipt->record_constructed = true;
        api->context_construct(context); receipt->context_constructed = true;
        api->prepare(subscriber, context); receipt->prepare_returned = true;
        api->save(subscriber, record, context); receipt->save_returned = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
uint32_t destroy_seh(Construct fn, void* object, bool* returned) {
    __try { fn(object); *returned = true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
uint32_t inspect_seh(InspectRecord inspect, void* user, const Record* record,
                     const SaveContext* context, bool* result, std::string* cause) {
    __try { *result = inspect(user, record, context, *cause); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
}
bool capture_original(const CaptureApi& api, const void* subscriber, InspectRecord inspect,
                      void* user, CaptureReceipt& receipt, std::string& cause) {
    receipt = {};
    if (!subscriber || !inspect || !api.record_construct || !api.record_destroy ||
        !api.context_construct || !api.context_destroy || !api.prepare || !api.save) {
        cause = "native Subscriber capture binding incomplete"; return false;
    }
    Record record; SaveContext context;
    receipt.call_exception = save_seh(&api, subscriber, &record, &context, &receipt);
    if (!receipt.call_exception && receipt.save_returned)
        receipt.call_exception = inspect_seh(inspect, user, &record, &context, &receipt.inspected, &cause);
    if (receipt.record_constructed)
        receipt.record_cleanup_exception = destroy_seh(api.record_destroy, &record, &receipt.record_destroyed);
    if (receipt.context_constructed)
        receipt.context_cleanup_exception = destroy_seh(api.context_destroy, &context, &receipt.context_destroyed);
    if (receipt.call_exception) cause = "native Subscriber capture/inspect SEH=" + std::to_string(receipt.call_exception);
    else if (!receipt.inspected && cause.empty()) cause = "native Subscriber record inspection not confirmed";
    if (receipt.record_cleanup_exception) cause += "; record cleanup SEH=" + std::to_string(receipt.record_cleanup_exception);
    if (receipt.context_cleanup_exception) cause += "; context cleanup SEH=" + std::to_string(receipt.context_cleanup_exception);
    if (receipt.call_exception || receipt.record_cleanup_exception || receipt.context_cleanup_exception || !receipt.inspected) return false;
    cause.clear(); return true;
}
}
