#pragma once
#include "live_host_state/day_rollover/economy/monetary.h"
namespace mt2mp::game::subscriber_retirement {
using Remove=void(*)(void*,void*);
bool initialize(uintptr_t,std::string&);
void* entrypoint();
void observe(void*,void*,Remove);
bool bind(const live_host_state::day::economy::Owners&,std::string&);
bool facts(std::vector<std::pair<int32_t,int32_t>>&,std::string&);
bool apply(int32_t,int32_t,nlohmann::json&,std::string&);
}
