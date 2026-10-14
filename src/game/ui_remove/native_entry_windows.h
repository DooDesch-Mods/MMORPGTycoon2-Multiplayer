#pragma once
#include "native_contract.h"
#include <atomic>

namespace mt2mp::action_session::ui_capture {
struct Publication {
    Capture* capture=nullptr;Memory* memory=nullptr;
    uint32_t game_thread=0;uintptr_t image=0,trampoline=0;
    std::atomic<uint8_t> last_disposition{uint8_t(Disposition::Rejected)};
    std::atomic<uint8_t> last_origin{uint8_t(Origin::Foreign)};
    std::atomic<uint64_t> inflight{0};
    std::atomic<bool> last_original_attempted{false},last_original_returned{false};
    void (*entry_error)(const std::string& cause)=nullptr;
};
DestroyProps entry_address()noexcept;
// Root's one shared MinHook owner creates disabled, validates/publishes this owner,
// then enables the target and verifies actual route. No hook API is called here.
bool publish_disabled(Publication&,std::string_view actual_exe_sha,std::string& cause);
bool enabled_route(Publication&,std::string& cause);
// Portable bounded entry dispatch also accepts explicit current thread for guards.
void dispatch_entry(Publication&,void* manager,NativePropArray*,uintptr_t native_return_slot,uint32_t current_thread);
}
