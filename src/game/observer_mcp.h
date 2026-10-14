#pragma once
#include "vendor/nlohmann/json.hpp"
#include <string>
namespace mt2mp::game::node_observer {
bool dispatch(const std::string& operation,const nlohmann::json& args,nlohmann::json& result,std::string& cause);
}
