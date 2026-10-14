#pragma once
#include "../npc_quest_state.h"
#include <array>
#include <cstdint>
#include <string>

namespace mt2mp::action_session {
struct NpcIntent {
    int32_t region=-1;
    uint8_t type=0;
    int32_t character_slot=-1,character_type=-1;
    std::array<float,3> position{};
    std::array<float,4> rotation{0,0,0,1};
    std::array<float,3> scale{1,1,1};
    bool operator==(const NpcIntent&) const = default;
};
struct NpcActor {
    std::string name;
    int32_t type=0,character_slot=-1,character_type=-1,level=-1,quest_count=-1;
    std::array<float,3> position{};
    std::array<float,4> rotation{};
    std::array<float,3> scale{};
    std::vector<npc_quest_records::Record> quests;
    bool operator==(const NpcActor&) const = default;
};
struct NpcState {
    uint64_t count=0,undo_count=0;
    std::string roster_sha256;
    std::array<uint64_t,4> global_random{};
    NpcActor created;
    std::string quest_targets_sha256;
    bool operator==(const NpcState&) const = default;
};
}
