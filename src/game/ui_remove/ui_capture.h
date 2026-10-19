#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace mt2mp::action_session::ui_capture {
struct NativePropArray {uintptr_t addresspoint=0;uintptr_t* data=nullptr;int32_t count=0,capacity=0;};
static_assert(sizeof(NativePropArray)==0x18&&offsetof(NativePropArray,data)==8&&offsetof(NativePropArray,count)==0x10);
using DestroyProps=void(*)(void* manager,NativePropArray* native_array);
enum class Origin:uint8_t { Foreign,BuildingWindow,DestroyCursor,OtherWindow,SelectionWindow,NpcWindow,MonsterWindow,NetworkWindow };
enum class Policy:uint8_t { Standalone,AuthorityRequired,Unavailable };
enum class Disposition:uint8_t { ForeignOriginal,StandaloneOriginal,EmptyOriginal,IntentCaptured,Rejected,OwnReplayOriginal };
enum class ReplayPurpose:uint8_t { HostExecution,ConfirmedHostApplication };
struct LocalBinding {
    uintptr_t manager=0,object=0;
    uint64_t uid=0,native_epoch=0,native_generation=0;
    std::string snapshot_uuid,canonical_record_key;
    bool supported_normal_building=false,supported_normal_scenery=false,supported_npc=false,supported_monster=false,supported_network=false,supported_group=false;
    bool operator==(const LocalBinding&)const=default;
};
struct Intent {std::string action_uuid,snapshot_uuid,canonical_record_key;};
struct Replay {ReplayPurpose purpose=ReplayPurpose::HostExecution;Intent intent;LocalBinding binding;};
struct ArrayObservation {uintptr_t addresspoint=0,data=0,object=0;int32_t count=0,capacity=0;};
struct Report {Disposition disposition=Disposition::Rejected;bool original_attempted=false,original_returned=false;std::string cause;};
struct Status {
    uint64_t captured=0,rejected=0,foreign_originals=0,replay_originals=0,suppressed_errors=0;
    bool replay_active=false,replay_consumed=false,replay_returned=false,unknown=false;
    std::string cause;
};
class Services {
public:
    virtual ~Services()=default;
    virtual bool on_game_thread()noexcept=0;
    virtual Policy policy(std::string& cause)=0;
    // Read the existing native array. Never create a native ownership object here.
    virtual bool array(NativePropArray*,ArrayObservation&,std::string& cause)=0;
    // Root reads exact current object+UID, snapshot registry metadata and native world.
    virtual bool resolve(uintptr_t manager,uintptr_t object,LocalBinding&,std::string& cause)=0;
    virtual bool fresh_action_uuid(std::string&,std::string& cause)=0;
    // Same shared typed Controller for Host and Client; no native execute in this adapter.
    virtual bool submit(const Intent&,std::string& cause)=0;
    virtual bool submit_npc(const Intent&,std::string& cause){cause="NPC UI submit service unavailable";return false;}
    virtual bool submit_monster(const Intent&,std::string& cause){cause="Monster UI submit service unavailable";return false;}
    virtual bool submit_network(const Intent&,std::string& cause){cause="Network UI submit service unavailable";return false;}
    virtual bool submit_group(const Intent&,std::string& cause){cause="Group UI submit service unavailable";return false;}
    // Root admits this private scope only from its actual authoritative engine bridge.
    virtual bool authorize_replay(const Replay&,std::string& cause)=0;
    virtual void original(DestroyProps,void*,NativePropArray*)=0;
    virtual void log(const char* level,const char* operation,const std::string& cause)noexcept=0;
    // Must update actual visible Root status/notice, not only a diagnostic log.
    virtual void show_rejection(const std::string& cause)noexcept=0;
};
class Capture {
public:
    explicit Capture(Services& services):services_(services){}
    Capture(const Capture&)=delete;Capture& operator=(const Capture&)=delete;
    bool publish_disabled(DestroyProps,std::string& cause);
    bool set_enabled(bool,std::string& cause);
    Report destroy_props(void* manager,NativePropArray*,Origin);
    // Scope is synchronous and exact-local. A successful callback is still not
    // gameplay confirmation; Root must inspect its existing native receipts.
    bool with_replay(const Replay&,const std::function<bool(std::string&)>& native_call,std::string& cause);
    Status status()const{return status_;}
    void record_entry_failure(const std::string& cause)noexcept;
private:
    Services& services_;DestroyProps original_=nullptr;
    bool enabled_=false,capturing_=false,authorizing_=false;Replay replay_;Status status_;
    std::string last_error_,replay_failure_;
    Report forward(Disposition,void*,NativePropArray*,bool replay=false);
    Report forward_worker(void*,NativePropArray*);
    Report reject(const std::string&);
    void error(const std::string&);
};
}
