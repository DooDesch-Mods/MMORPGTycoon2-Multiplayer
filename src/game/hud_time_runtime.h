#pragma once
#include "hud_time_writer.h"
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::hud_time_runtime {
bool initialize(const hud_time::Config&,std::string&);
bool hooks_enabled(bool time_update_hook,bool clock_scope_hook,std::string&);
void tick();
bool suspend(const std::string&,std::string&);
hud_time::UpdateResult on_update(void*,void*);
void clock_update(void*,float,hud_time::OriginalClockUpdate);
nlohmann::json status_json();
bool verify_window_show_hook(uintptr_t target,std::string& error);
}
