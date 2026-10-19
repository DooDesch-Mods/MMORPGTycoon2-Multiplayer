#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <string>

namespace mt2mp::npc_quest_runtime {
struct Value {
    std::array<uint32_t,3> rating_bits{};
    bool rating_set=false;
    int32_t xp_reward=0,gold_reward=0;
    bool operator==(const Value&)const=default;
};
inline bool valid(const Value& value,std::string& error){
    for(auto bits:value.rating_bits)if(!std::isfinite(std::bit_cast<float>(bits))){
        error="Quest runtime rating bits must be finite";return false;
    }
    error.clear();return true;
}
}
