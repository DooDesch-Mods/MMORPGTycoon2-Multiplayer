#pragma once
#include "../intervention_actor_state.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace mt2mp::action_session {
struct BossPlacementIntent {
    int32_t type=-1,region=-1;
    std::array<uint32_t,10> pose{};
    // The native release point chooses the Region independently of the pose.
    std::array<uint32_t,3> point{};
    std::string character_sha;
    bool operator==(const BossPlacementIntent&)const=default;
};
struct BossPlacementState {
    std::array<uint64_t,4> random{};
    std::array<int32_t,8> release{};
    std::string untouched_sha,record_sha;
    std::optional<game::interventions::actors::Row> created;
    bool operator==(const BossPlacementState&)const=default;
};
}
