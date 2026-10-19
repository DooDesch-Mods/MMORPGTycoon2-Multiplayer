#pragma once
#include "dungeon_light_action.h"
namespace mt2mp::action_session {
struct DungeonSoundIntent {std::string key;int32_t before=-1,atmosphere=-1;bool operator==(const DungeonSoundIntent&)const=default;};
struct DungeonSoundState {DungeonConfigState dungeon;std::array<float,23> light{};int32_t atmosphere=0;bool operator==(const DungeonSoundState&)const=default;};
inline bool dungeon_sound_transition(const DungeonSoundIntent& intent,const DungeonSoundState& before,const DungeonSoundState& after){
    auto expected=before;expected.atmosphere=intent.atmosphere;expected.dungeon.global_random=after.dungeon.global_random;return before.atmosphere==intent.before&&expected==after;
}
}
