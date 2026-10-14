#pragma once
#include "vendor/nlohmann/json.hpp"
#include <string>
namespace mt2mp::game::lobby_prepare {
bool prepare(const std::string& selected_file,std::string& error);
bool prepare_current(std::string& error);
bool start(std::string& error);
bool tick(std::string& error);
nlohmann::json status();
}

namespace mt2mp::game::lobby_prepare { bool leave(std::string&); }
