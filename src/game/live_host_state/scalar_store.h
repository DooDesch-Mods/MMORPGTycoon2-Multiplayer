#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::game::live_host_state {
bool compare_fixed_i32(uintptr_t address,int32_t expected,int32_t target,std::string&);
}
