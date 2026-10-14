#pragma once
#include "monetary.h"
namespace mt2mp::game::live_host_state::day::economy {
bool encode(const State&,std::vector<uint8_t>&,std::string&);
bool decode(const uint8_t*,size_t,State&,std::string&);
}
