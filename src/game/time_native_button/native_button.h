#pragma once
#include <cstdint>
#include <string>

namespace mt2mp::game::time_native_button {
enum class Role:uint8_t {Host,Client};
enum class Phase:uint8_t {RejectedNoNativeEffect,PreparedNativeInvocation,NativeReturnedAwaitingAuthority,Unknown};
struct Request {
    std::string token,snapshot_uuid;
    uint64_t epoch=0,generation=0,revision=0;
    float scale=0;
};
struct Binding {
    uintptr_t image=0,world=0,panel=0,slot=0,button=0,item=0,type=0;
    uintptr_t item_proxy=0,item_proxy_target=0,type_proxy=0,type_proxy_target=0;
    uint64_t epoch=0,generation=0,revision=0;
    uint32_t game_thread=0;
    Role role=Role::Client;
    float scale=0;
    bool button_enabled=false;
    int32_t native_item_state=-1;
    bool operator==(const Binding&)const=default;
};
struct JobAttempt {
    Request request;
    uint64_t sequence=0;
    Binding binding;
};
struct Receipt {
    Phase phase=Phase::RejectedNoNativeEffect;
    bool invocation_reserved=false,native_attempted=false,native_returned=false;
    uint32_t seh_code=0;
    JobAttempt actual_attempt;
    std::string cause;
};
class RootServices {
public:
    virtual ~RootServices()=default;
    // Read the actual Root pending job and reserve its token once in that ledger.
    virtual bool reserve_actual_job(const Request&,JobAttempt&,std::string&)=0;
    // Read actual Session/World/Time readiness and the native Reader's exact current witness.
    virtual bool read_actual_button(const Request&,Binding&,std::string&)=0;
    // Verify Root's exact-process native resolver receipt, not a supplied readiness flag.
    virtual bool read_actual_executable_sha256(std::string&,std::string&)=0;
    // Persist attempted/returned/Unknown in the same Root job, including original causes.
    virtual bool record_actual_attempt(const Receipt&,std::string&)=0;
};
bool validate_binding(const Request&,const Binding&,std::string&);
bool invoke(const Request&,RootServices&,Receipt&,std::string&);
}
