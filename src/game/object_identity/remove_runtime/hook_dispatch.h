#pragma once
#include "copy/remove_native_contract.h"
#include <atomic>

namespace mt2mp::remove_runtime {
namespace native=build_actions::private_native;
struct HookListener {
    virtual ~HookListener()=default;
    virtual bool forward_clear(uintptr_t receiver,uintptr_t caller)noexcept=0;
    virtual void open_returned(uintptr_t caller,uintptr_t graph)noexcept=0;
    virtual void lock_enter(uintptr_t caller,uintptr_t graph)noexcept=0;
    virtual void lock_returned(uintptr_t graph,bool returned)noexcept=0;
    virtual void destructor_enter(uintptr_t prop)noexcept=0;
    virtual void original_failed(const char* operation,const char* cause,uint32_t seh)noexcept=0;
};
// The installed wrappers retain this table and its listener for process lifetime.
class HookDispatch {
public:
    explicit HookDispatch(HookListener& l):listener_(l){}
    bool publish_disabled(const native::RemoveEntrypoints&)noexcept;
    bool published()const noexcept{return published_.load(std::memory_order_acquire);}
    uint64_t inflight()const noexcept{return inflight_.load(std::memory_order_acquire);}
    void clear(void*,uintptr_t caller);
    void* open(const void*,uintptr_t caller);
    void lock(void*,uintptr_t caller);
    void destructor(void*);
    void enter()noexcept{inflight_.fetch_add(1,std::memory_order_acq_rel);}
    void leave()noexcept{inflight_.fetch_sub(1,std::memory_order_acq_rel);}
    HookListener& listener()noexcept{return listener_;}
    native::RemoveEntrypoints originals()const noexcept{return originals_;}
private:
    HookListener& listener_;native::RemoveEntrypoints originals_{};
    std::atomic<bool> published_{false};std::atomic<uint64_t> inflight_{0};
};
}
