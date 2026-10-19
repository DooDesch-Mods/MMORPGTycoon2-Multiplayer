#pragma once
#include "vendor/nlohmann/json.hpp"
#include <string>
namespace mt2mp::game::session_reload {
bool initialize(std::string& error);
bool busy();
bool request_resync(std::string& error);
bool release_for_leave(std::string& error);
void tick();
nlohmann::json status();
}
