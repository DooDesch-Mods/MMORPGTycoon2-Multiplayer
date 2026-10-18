#pragma once
#include "ui_remove/ui_capture.h"
#include "vendor/nlohmann/json.hpp"
#include <functional>

namespace mt2mp::game::ui_remove_runtime {
bool enable(std::string&);
#ifdef MT2MP_FRAMEWORK_PLUGIN
bool framework_retire(std::string&);
#endif
bool enabled()noexcept;
bool requested()noexcept;
bool make_replay(const std::string& snapshot,const std::string& key,const std::string& action,
    action_session::ui_capture::ReplayPurpose,action_session::ui_capture::Replay&,std::string&);
bool with_replay(const action_session::ui_capture::Replay&,const std::function<bool(std::string&)>&,std::string&);
bool can_reclaim_global_minhook(std::string&);
nlohmann::json status();
bool probe(const nlohmann::json&,nlohmann::json&,std::string&);
}
