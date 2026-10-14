#pragma once
#include "hud_time_activation.h"
#include "native_time_input.h"
#include "vendor/nlohmann/json.hpp"
#include <functional>
namespace mt2mp::game::time_mouse_diagnostics {
struct HookCounts {uint64_t attempted=0,matched=0,suppressed=0,original=0;uintptr_t caller=0;};
struct Services {
    std::function<bool(native_time_input::Context&,std::string&)> context;
    std::function<HookCounts()> hook_counts;
};
bool initialize(uintptr_t image,uint32_t thread,hud_time::Reader&,hud_time::Activation&,Services,std::string&);
bool probe(const nlohmann::json&,nlohmann::json&,std::string&);
bool receipt(const nlohmann::json&,nlohmann::json&,std::string&);
bool unresolved();
}
