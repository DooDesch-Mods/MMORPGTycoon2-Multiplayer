#pragma once
#include "typed_action/action_session.h"
#include "vendor/nlohmann/json.hpp"
#include <string>

namespace mt2mp::game::ui_price_runtime {
namespace detail {
enum class HookGate { FirstInstall, Reuse, FailedAttempt, WrongThread };
constexpr HookGate hook_gate(bool attempted,bool ready,bool same_thread) noexcept {
    if(!same_thread)return HookGate::WrongThread;
    if(ready)return attempted?HookGate::Reuse:HookGate::FailedAttempt;
    return attempted?HookGate::FailedAttempt:HookGate::FirstInstall;
}
}
bool enable(std::string&);
void tick();
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,
                                const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
bool query(nlohmann::json&,std::string&);
bool native_ui(const std::string&,int32_t,int32_t,nlohmann::json&,std::string&);
nlohmann::json status();
}
