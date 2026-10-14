#pragma once
#include "time_runtime.h"
#include <functional>
#include <memory>

namespace mt2mp::game::time_integration {
namespace runtime=time_runtime;
namespace ui=shared_time;
namespace wire=time_session;
inline constexpr uint8_t channel=8;
inline constexpr size_t packet_size=64;
inline constexpr uintptr_t activate_rva=ui::Bridge::activate_rva;
inline constexpr uintptr_t update_rva=ui::Bridge::update_rva;
struct SessionInputs {
    bool host=false,session_connected=false,transport_connected=false;
    uint64_t local_peer=0,epoch=0;
    uint8_t peer_mask=0;
};
using Handler=std::function<void(uint64_t,const uint8_t*,uint32_t)>;
struct SidecarServices {
    std::function<uint32_t()> thread_id;
    std::function<bool(SessionInputs&,std::string&)> read_session;
    std::function<bool(uint8_t,const uint8_t*,uint32_t,uint64_t)> send;
    std::function<std::string()> failure_reason;
    std::function<void(uint8_t,Handler)> register_handler;
    std::function<void(const char*,const char*,const std::string&,uint64_t)> log;
};
struct Status {
    bool initialization_attempted=false,initialized=false;
    bool registration_attempted=false,handler_registered=false;
    uint64_t receive_attempted=0,receive_accepted=0,receive_failed=0;
    uint64_t tick_attempted=0,forced_suspends=0,error_repeats=0;
    uint64_t reentrant_rejected=0;
    std::string last_reentrant_operation;
    std::string last_operation,last_error;
    runtime::Status runtime{};
};
class Adapter {
public:
    Adapter();
    ~Adapter();
    Adapter(const Adapter&)=delete;
    Adapter& operator=(const Adapter&)=delete;
    bool initialize(const ui::Config&,const SidecarServices&,std::string&);
    bool initialize_with_backend(const ui::Config&,const SidecarServices&,const runtime::Native&,std::string&);
    bool set_hook_mask(uint32_t,std::string&);
    bool tick(std::string&);
    wire::Report receive(uint64_t authenticated_sender,const uint8_t*,size_t);
    wire::Report submit(wire::Intent);
    bool read_presentation(ui::Presentation&,std::string&);
    bool observe_receipt(uint64_t request_peer,uint64_t sequence,wire::Message&,bool& available,std::string&);
    ui::HookResult on_activate(void* type,void* item,uintptr_t caller_rva);
    ui::HookResult on_update(void* type,void* item);
    bool suspend(const std::string&,std::string&);
    bool status(Status&,std::string&);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool initialize_impl(const ui::Config&,const SidecarServices&,const runtime::Native*,std::string&);
};
// One process-lifetime adapter owns the production Runtime and Channel8 callback.
Adapter& instance();
bool initialize(const ui::Config&,std::string&);
}
