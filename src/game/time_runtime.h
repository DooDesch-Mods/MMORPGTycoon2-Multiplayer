#pragma once
#include "shared_time.h"
#include "time_session.h"
#include <memory>

namespace mt2mp::game::time_runtime {
namespace ui=shared_time;
namespace wire=time_session;
enum class SendResult { Accepted, Backpressure, Failed };
enum class Phase { Uninitialized, Waiting, ReadOnly, Active, Retiring };
struct Session {
    uint64_t epoch=0;
    wire::Role role=wire::Role::Client;
    uint8_t local_peer=0,peer_mask=0;
    bool snapshot_active=false;
};
struct Services {
    void* user=nullptr;
    bool (*session)(void*,Session&,std::string&)=nullptr;
    // Accepted means the transport owns all 64 bytes. No partial acceptance.
    SendResult (*send)(void*,uint8_t,const std::array<uint8_t,64>&,std::string&)=nullptr;
    void (*log)(void*,const char* level,const char* operation,const std::string& cause,uint64_t repeats)=nullptr;
};
struct Native {
    void* user=nullptr;
    ui::Backend memory{};
    bool (*read_clock)(void*,wire::Clock&,std::string&)=nullptr;
    bool (*set_clock)(void*,float,std::string&)=nullptr;
    bool (*client_epoch)(void*,uint64_t&,std::string&)=nullptr;
};
struct Status {
    Phase phase=Phase::Uninitialized;
    uint64_t epoch=0;
    uint32_t hook_mask=0;
    bool pending_outbound=false,presentation_uncertain=false;
    uint64_t sends_accepted=0,sends_waiting=0,sends_failed=0,early_states=0;
    uint64_t native_set_attempted=0,native_set_confirmed=0,native_set_failed=0;
    uint64_t item_stores_attempted=0,item_stores_completed=0,cancelled_intents=0;
    uint64_t last_reason_repeats=0;
    wire::Status controller{};
    std::string last_operation,last_error;
};
class Runtime {
public:
    Runtime();
    ~Runtime();
    Runtime(const Runtime&)=delete;
    Runtime& operator=(const Runtime&)=delete;
    bool initialize(const ui::Config&,const Services&,std::string&);
    // Isolated fixtures inject memory and native callbacks, never a game receiver.
    bool initialize_with_backend(const ui::Config&,const Services&,const Native&,std::string&);
    bool set_hook_mask(uint32_t,std::string&);
    bool tick(std::string&);
    wire::Report submit(wire::Intent);
    wire::Report receive(uint8_t authenticated_sender,const uint8_t*,size_t);
    bool read_presentation(ui::Presentation&,std::string&);
    ui::HookResult on_activate(void* type,void* item,uintptr_t caller_rva);
    ui::HookResult on_update(void* type,void* item);
    bool suspend(const std::string& reason,std::string&);
    bool observe_receipt(uint8_t request_peer,uint64_t sequence,wire::Message&,bool& available,std::string&);
    bool status(Status&,std::string&);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Uses existing native resolver/clock/software APIs and the Windows memory backend.
bool production_backend(const ui::Config&,Native&,std::string&);
}
