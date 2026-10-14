#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace mt2mp::subscriber_lifecycle {
using Construct = void(*)(void*);
using PrepareSave = void(*)(const void*, void*);
using SaveRecord = void(*)(const void*, void*, void*);
struct CaptureApi {
    Construct record_construct = nullptr, record_destroy = nullptr;
    Construct context_construct = nullptr, context_destroy = nullptr;
    PrepareSave prepare = nullptr;
    SaveRecord save = nullptr;
};
struct CaptureReceipt {
    bool record_constructed = false, context_constructed = false;
    bool prepare_returned = false, save_returned = false, inspected = false;
    bool record_destroyed = false, context_destroyed = false;
    uint32_t call_exception = 0, record_cleanup_exception = 0, context_cleanup_exception = 0;
};
using InspectRecord = bool(*)(void* user, const void* record, const void* save_context, std::string& cause);
// Inspection must copy the record and link manifest before this scope destroys
// both native owners. No pointer from this callback may enter a wire payload.
bool capture_original(const CaptureApi&, const void* subscriber, InspectRecord,
                      void* user, CaptureReceipt&, std::string& cause);
}
