#pragma once
#include <string>
#include "vendor/nlohmann/json.hpp"
struct MT2FW_Host;
namespace mt2mp::game::coop_cursor::production {
bool bind(const MT2FW_Host*,std::string& error);
void tick();
bool retire(std::string& error);
nlohmann::json status();
}
