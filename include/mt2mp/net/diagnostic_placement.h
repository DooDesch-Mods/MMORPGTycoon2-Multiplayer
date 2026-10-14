#pragma once
#include "vendor/nlohmann/json.hpp"
#include <string>
namespace mt2mp::net {
void initialize_diagnostic_placement();
void tick_diagnostic_placement();
bool begin_diagnostic_placement(const std::string& id,const nlohmann::json& arguments,std::string& error);
bool poll_diagnostic_placement(const std::string& id,bool& complete,nlohmann::json& result,std::string& error);
std::string diagnostic_placement_status();
}
