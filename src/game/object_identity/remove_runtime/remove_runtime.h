#pragma once
#include "hook_dispatch.h"
#include "operation_state.h"
#include "retirement_journal.h"
#include "copy/remove_services.h"
#include "../native_free_runtime/native_free_runtime.h"
#include <memory>

namespace mt2mp::remove_runtime {
namespace host=build_actions::private_native::host;
enum class RuntimePhase {Idle,Prepared,Enabled,Failed,Unknown};
struct InstallationReceipt {
    RuntimePhase phase=RuntimePhase::Idle;
    uint32_t main_thread=0,hook_mask=0,created_mask=0;uintptr_t image=0;
    std::array<uintptr_t,4> targets{},trampolines{};
    std::array<int32_t,4> create_status{-999,-999,-999,-999},enable_status{-999,-999,-999,-999};
    std::array<bool,4> create_attempted{},enable_attempted{};
    bool originals_published=false;
    uint64_t inflight=0;std::string cause;
};
class Runtime;
class ActualHostServices final:public host::RequiredHostServices {
public:
    ActualHostServices(Runtime& r,uint64_t sequence):runtime_(r),sequence_(sequence){}
    bool installation(bool&,uint32_t&,bool&,std::string&)override;
    bool retirement(const lease::PrivateLease&,const lease::Retirement&,std::string&)override;
    bool allocation_generation(uint64_t&,std::string&)override;
    bool free_invocation(uint64_t&,std::string&)override;
private:Runtime& runtime_;uint64_t sequence_=0;
};
class Runtime final:private HookListener {
public:
    static Runtime& instance();
    Runtime(const Runtime&)=delete;Runtime& operator=(const Runtime&)=delete;
    bool prepare_and_enable(std::string&);
    // Root resolves a stable NativeSnapshot binding before this private Mainthread call.
    OperationStatus execute(const PrivateRequest&);
    bool retire_completed(uint64_t sequence,std::string&);
    bool bind_root_retirement_service(RootBindingRetirement&,std::string&);
    const RetirementJournal* private_retirement_status(uint64_t sequence)const;
    bool read_actual_hook_mask(uint32_t&,std::string&);
    bool read_owned_undo_open(uintptr_t image,native::UndoOpenFn&,std::string&);
    InstallationReceipt status()const;
    const OperationStatus* operation_status(uint64_t)const;
    bool can_reclaim_global_minhook(std::string&)const;
    native_free_runtime::Runtime& free_runtime()noexcept{return native_free_runtime::Runtime::instance();}
private:
    friend class ActualHostServices;
    Runtime();~Runtime()=default;
    struct PoolLog final:lease::Diagnostics {
        void error_unretired_native_ownership(size_t,bool)noexcept override;
    } pool_log_;
    struct Operation;
    host::ProcessMemory memory_;host::NativeOrigin origin_;
    host::LookupApi lookup_;native::RemoveEntrypoints entries_;
    std::string sha_;InstallationReceipt installation_;HookDispatch dispatch_;
    OperationLedger ledger_;std::map<uint64_t,std::unique_ptr<Operation>> operations_;
    Operation* active_=nullptr;bool busy_=false;
    RootBindingRetirement* root_retirement_=nullptr;Operation* retiring_=nullptr;uint64_t next_retirement_=0;
    std::atomic<uint32_t> main_thread_{0};std::atomic<bool> original_fault_{false};
    std::array<char,512> hook_error_{};
    bool fail(RuntimePhase,const std::string&,std::string&);
    void observation_failed(const char*)noexcept;
    bool callback_operation()const noexcept;
    bool validate_own_retirement(uint64_t,const lease::PrivateLease&,const lease::Retirement&,std::string&);
    bool forward_clear(uintptr_t,uintptr_t)noexcept override;
    void open_returned(uintptr_t,uintptr_t)noexcept override;
    void lock_enter(uintptr_t,uintptr_t)noexcept override;
    void lock_returned(uintptr_t,bool)noexcept override;
    void destructor_enter(uintptr_t)noexcept override;
    void original_failed(const char*,const char*,uint32_t)noexcept override;
};
}
