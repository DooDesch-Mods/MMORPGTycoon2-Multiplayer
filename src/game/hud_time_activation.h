#pragma once
#include "hud_time_reader.h"
#include "shared_time.h"
#include "time_session.h"
#include <functional>

namespace mt2mp::game::hud_time {
struct ActivationContext {
    uint64_t epoch=0;
    uint8_t local_peer=0,peer_mask=0;
    bool host=false,active=false;
    uint32_t hook_mask=0;
    bool operator==(const ActivationContext&) const=default;
};
struct ActivationServices {
    bool client=false;
    std::function<uint32_t()> thread_id;
    std::function<bool(ActivationContext&,std::string&)> context;
    std::function<time_session::Report(time_session::Intent)> submit;
};
struct ActivationStatus {
    bool initialized=false;
    uint64_t attempted=0,matched=0,submitted=0,rejected=0;
    uint64_t last_sequence=0,last_epoch=0;
    std::string error;
};
class Activation {
public:
    static constexpr uintptr_t mouse_caller_rva=0x58095c;
    bool initialize(uint32_t game_thread,Reader&,ActivationServices,std::string&);
    shared_time::HookResult on_activate(uintptr_t type,uintptr_t item,uintptr_t caller_rva);
    const ActivationStatus& status() const {return state_;}
private:
    Reader* reader_=nullptr;
    ActivationServices io_{};
    uint32_t game_thread_=0;
    bool attempted_=false,busy_=false;
    ActivationStatus state_{};
};
}
