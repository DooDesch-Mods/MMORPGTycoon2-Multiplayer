#pragma once
#include "hud_time_reader.h"
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::hud_calendar_runtime {
bool initialize(const hud_time::Config&,std::string&);
bool hooks_enabled(std::string&);
void tick();
bool suspend(const std::string&,std::string&);
nlohmann::json status_json();
}
