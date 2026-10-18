#pragma once
#include "vendor/nlohmann/json.hpp"
#include <string>
namespace mt2mp::game::scenario_vip_authority {
bool read_alert_owners(nlohmann::json& out,std::string& error);
}
