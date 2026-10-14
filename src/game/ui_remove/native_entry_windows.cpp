#include "native_entry_windows.h"
#include "native_contract_bytes.h"
#include <intrin.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace mt2mp::action_session::ui_capture {
namespace {
std::atomic<Publication*> published{nullptr};
__declspec(noinline) void entry(void* manager,NativePropArray* array) {
    auto* owner=published.load(std::memory_order_acquire);
    if(!owner){RaiseFailFastException(nullptr,nullptr,0);return;}
    const auto return_slot=uintptr_t(_AddressOfReturnAddress());
    owner->inflight.fetch_add(1,std::memory_order_acq_rel);
    __try {dispatch_entry(*owner,manager,array,return_slot,GetCurrentThreadId());}
    __finally {owner->inflight.fetch_sub(1,std::memory_order_acq_rel);}
}
}
DestroyProps entry_address()noexcept{return &entry;}
bool publish_disabled(Publication& owner,std::string_view sha,std::string& cause) {
    if(published.load(std::memory_order_acquire)||!owner.capture||!owner.memory||!owner.entry_error||
       !owner.game_thread||owner.inflight.load(std::memory_order_acquire)||GetCurrentThreadId()!=owner.game_thread){cause="UI Remove owner publication requires fresh exact game-thread services";return false;}
    if(!validate_unpatched(*owner.memory,owner.image,sha,cause)||!validate_trampoline(*owner.memory,owner.image,owner.trampoline,cause))return false;
    if(!owner.capture->publish_disabled(reinterpret_cast<DestroyProps>(owner.trampoline),cause))return false;
    published.store(&owner,std::memory_order_release);cause.clear();return true;
}
bool enabled_route(Publication& owner,std::string& cause) {
    if(published.load(std::memory_order_acquire)!=&owner||GetCurrentThreadId()!=owner.game_thread){cause="UI Remove enabled route owner/thread differs";return false;}
    if(!validate_own_enabled_route(*owner.memory,owner.image,owner.trampoline,uintptr_t(&entry),cause))return false;
    return owner.capture->set_enabled(true,cause);
}
}
