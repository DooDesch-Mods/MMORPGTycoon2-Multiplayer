#pragma once
#include "typed_action/action_session.h"
#include "vendor/nlohmann/json.hpp"
#include <string>

namespace mt2mp::game::ui_build_runtime {
bool enable(std::string&);
#ifdef MT2MP_FRAMEWORK_PLUGIN
bool framework_retire(std::string&);
#endif
bool enabled() noexcept;
bool validate_scenery_ui_routes(uintptr_t image,uint32_t thread,std::string&);
nlohmann::json status();
bool place(const action_session::Request&,action_session::BuildingState&,nlohmann::json&,std::string&);
bool can_reclaim_global_minhook(std::string&);
}
