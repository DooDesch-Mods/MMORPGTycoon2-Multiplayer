#pragma once
#include "observer_delta/native_free.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

namespace mt2mp::native_free_runtime {
struct ModuleReceipt {
    uintptr_t base=0;
    std::array<wchar_t,1024> path{};
    std::array<char,65> sha256{};
    std::array<uint32_t,4> version{};
    uint64_t bytes=0;
    uintptr_t held_file=0;
    uint32_t version_error=0;
    bool pinned=false,version_present=false;
};
struct ExportReceipt {
    uintptr_t resolved=0,owner=0;
    uint32_t export_rva=0;
    std::array<char,256> forwarder{};
    bool forwarded=false,committed_executable_image=false;
};
struct HeapModuleReceipt {
    uint32_t process=0;
    ModuleReceipt imported_crt,malloc_owner,free_owner;
    ExportReceipt malloc_export,free_export;
    bool actual_exports=false;
};
struct WorldReceipt {uint64_t epoch=0,generation=0;uintptr_t world=0,map=0;uint32_t thread=0;};
enum class Phase:uint8_t {Idle,Prepared,Enabled,Retired,Failed,Unknown};
struct Receipt {
    Phase phase=Phase::Idle;
    uint32_t process=0,main_thread=0,hook_mask=0,win32_error=0,seh_code=0;
    int32_t minhook_status=0;
    ModuleReceipt executable,self;
    HeapModuleReceipt heap;
    WorldReceipt world;
    uintptr_t image=0,malloc_iat=0,free_iat=0,sized_target=0,sized_trampoline=0;
    uint64_t entry_inflight=0,original_inflight=0;
    bool originals_published=false,iat_protection_restored=false,trampoline_route=false;
    bool crt_confirmed=false,hooks_confirmed=false,native_game_proven=false;
    std::array<char,512> cause{};
};
struct JournalEntry {
    uint64_t ticket=0;
    uint32_t thread=0,seh_code=0;
    std::array<char,64> operation{};
    std::array<char,256> original_cause{};
    enum class Level:uint8_t {Error,Warn,Info};Level level=Level::Error;
};
class Journal final:public native_free::Diagnostics {
public:
    void error(const char*,const char*,uint32_t)noexcept override;
    // Root formats these POD records only outside the hooks.
    size_t drain(JournalEntry*,size_t capacity,uint64_t& dropped)noexcept;
    bool original_failure()const noexcept{return original_failure_.load(std::memory_order_acquire);}
private:
    struct Slot {std::atomic<uint8_t> state{0};JournalEntry data;};
    std::array<Slot,64> slots_;
    std::atomic<uint64_t> tickets_{0},dropped_{0};
    std::atomic<bool> original_failure_{false};
};
// Uses actual Windows module/export/file services, without game calls or test receipts.
bool read_heap_module_receipt(HeapModuleReceipt&,std::string&);
namespace detail {
struct ExchangeReceipt {bool attempted=false,exchanged=false,protection_restored=false;uint32_t win32_error=0,seh_code=0,previous_protection=0;uintptr_t observed=0;};
ExchangeReceipt replace_import_once(uintptr_t aligned_slot,uintptr_t expected,uintptr_t replacement)noexcept;
bool verify_live_bytes(uintptr_t,const unsigned char*,size_t,std::string&);
}
enum class HolderKind:uint8_t {Prop,Building};
struct ProbeReceipt {
    WorldReceipt world;
    uint64_t allocation_generation=0,call=0;
    uint32_t factory_attempted=0,factory_returned=0,constructor_confirmed=0,seh_code=0;
    native_free::Receipt deletion;
    bool confirmed=false,native_game_proven=false;
    std::array<char,512> cause{};
};
class Runtime {
public:
    static Runtime& instance()noexcept;
    Runtime(const Runtime&)=delete;Runtime& operator=(const Runtime&)=delete;
    // Called on the actual loaded-world Mainthread, after global MinHook init.
    bool prepare_and_enable(std::string&);
    bool read_actual_receipt(Receipt&,std::string&);
    bool reserve_holder_generation(uint64_t&,std::string&);
    bool next_free_invocation(uint64_t&,std::string&);
    native_free::Observer* actual_observer()noexcept;
    bool probe_null_holder(HolderKind,ProbeReceipt&,std::string&);
    // Stops new installed routes but retains originals and pinned storage for late entries.
    bool retire_routes(std::string&);
    // Root core shutdown must check this before global hooks::cleanup/MH_Uninitialize.
    bool can_reclaim_global_minhook(std::string&)const;
    Receipt status()const noexcept;
    Journal& journal()noexcept{return journal_;}
    void enter()noexcept{entry_inflight_.fetch_add(1,std::memory_order_acq_rel);}
    void leave()noexcept{entry_inflight_.fetch_sub(1,std::memory_order_acq_rel);}
    native_free::Observer* published_observer()noexcept{return published_.load(std::memory_order_acquire);}
private:
    Runtime()=default;
    Journal journal_;std::optional<native_free::Observer> observer_;
    std::atomic<native_free::Observer*> published_{nullptr};
    std::atomic<uint64_t> entry_inflight_{0};
    std::atomic<uint32_t> bound_thread_{0};
    Receipt receipt_;uint64_t next_holder_=0,next_call_=0;
    void* private_probe_holder_=nullptr;uint64_t private_probe_generation_=0;
    bool hook_created_=false,sized_visible_=false,iat_visible_=false,probe_busy_=false;uint32_t original_iat_protection_=0;
    bool fail(Phase,const char*,std::string&,uint32_t win32=0,int32_t mh=0,uint32_t seh=0)noexcept;
    bool actual_world(WorldReceipt&,std::string&);
    bool routes(std::string&);
};
}
