#pragma once
#include "dungeon_config_action.h"
namespace mt2mp::action_session {
struct DungeonLightInput {
    std::array<int32_t,4> spinners{};std::array<int32_t,3> sliders{};std::array<std::array<float,4>,4> colors{};
    bool operator==(const DungeonLightInput&)const=default;
};
struct DungeonLightIntent {
    std::string key,before_sha256;DungeonLightInput input;std::array<float,23> light{};
    bool operator==(const DungeonLightIntent&)const=default;
};
struct DungeonLightState {
    DungeonConfigState dungeon;std::array<float,23> light{};
    bool operator==(const DungeonLightState&)const=default;
};
inline bool dungeon_light_transition(const DungeonLightIntent& intent,const DungeonLightState& before,const DungeonLightState& after){
    auto expected=before.dungeon;expected.global_random=after.dungeon.global_random;return after.light==intent.light&&expected==after.dungeon;
}
}
