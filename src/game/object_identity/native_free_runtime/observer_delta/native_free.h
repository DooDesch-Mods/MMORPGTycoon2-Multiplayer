#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace mt2mp::native_free {
using DeleteFn=void(*)(void*);
using SizedFn=void(*)(void*,uint64_t);
using FreeFn=void(*)(void*);
inline constexpr uint32_t required_hooks=3;
struct Permit {
    uint64_t call=0,allocation_generation=0,world_generation=0,retirement_receipt=0;
    void* holder=nullptr;
    uint64_t bytes=56;
    bool root_binding_confirmed=false;
    bool operator==(const Permit&)const=default;
};
enum class Phase {Absent,Prepared,Attempted,Confirmed,Unknown};
struct Receipt {
    Permit permit;
    Phase phase=Phase::Absent;
    uint64_t delete_attempted=0,delete_returned=0,sized_attempted=0,sized_returned=0,free_attempted=0,free_returned=0;
    uintptr_t sized_origin=0,free_origin=0,fault_address=0;
    uint32_t seh_code=0;
    std::array<char,512> cause{};
    bool cause_truncated=false;
};
struct Diagnostics {
    virtual ~Diagnostics()=default;
    virtual void error(const char* operation,const char* original_cause,uint32_t seh_code)noexcept=0;
};
struct Availability {
    bool main_thread=false,enabled=false,active=false,in_delete=false,unknown_locked=false;
    uint32_t seh_code=0;
    uintptr_t fault_address=0;
    std::array<char,512> cause{};
};
class Observer {
public:
    Observer(uint32_t main_thread,Diagnostics& diagnostics):main_thread_(main_thread),diagnostics_(diagnostics){}
    Observer(const Observer&)=delete;
    Observer& operator=(const Observer&)=delete;
    bool publish_disabled(SizedFn trampoline,FreeFn imported_original)noexcept;
    bool confirm_hooks(uint32_t actual_mask)noexcept;
    bool enable()noexcept;
    bool disable()noexcept;
    bool begin(const Permit&)noexcept;
    // This boundary owns only the current D0 call, not holder or heap storage.
    bool invoke_delete(DeleteFn,const Permit&)noexcept;
    void sized_event(void* pointer,uint64_t bytes,uintptr_t actual_return_origin);
    void free_event(void* pointer,uintptr_t actual_return_origin);
    Receipt observe(const Permit&)const noexcept;
    bool resync(uint64_t actual_new_world_generation,bool old_callbacks_retired)noexcept;
    Availability availability()const noexcept;
    bool enabled()const noexcept{return enabled_.load(std::memory_order_acquire);}
    uint64_t original_inflight()const noexcept{return original_inflight_.load(std::memory_order_acquire);}
    // Called only by the POD native bridges and their continue-search SEH filters.
    void original_failed(const char*,uint32_t,uintptr_t,bool owned)noexcept;
    void sized_returned(bool owned)noexcept;
    void free_returned(bool owned)noexcept;
    void original_scope_ended()noexcept{original_inflight_.fetch_sub(1,std::memory_order_acq_rel);}
private:
    uint32_t main_thread_=0;
    Diagnostics& diagnostics_;
    std::atomic<SizedFn> sized_{nullptr};
    std::atomic<FreeFn> free_{nullptr};
    std::atomic<bool> enabled_{false};
    std::atomic<uint64_t> original_inflight_{0};
    uint32_t hook_mask_=0;
    bool active_=false,in_delete_=false,unknown_lock_=false;
    uint64_t last_call_=0,last_world_=0;
    uintptr_t expected_sized_origin_=0,expected_free_origin_=0;
    Receipt current_;
    void unknown(const char*,uint32_t=0,uintptr_t=0)noexcept;
    static void cause(Receipt&,const char*)noexcept;
};
extern "C" void mt2mp_free_delete_bridge(DeleteFn,void*,uintptr_t* expected_return);
extern "C" void mt2mp_free_sized_bridge(SizedFn,void*,uint64_t,uintptr_t* expected_return);
}
