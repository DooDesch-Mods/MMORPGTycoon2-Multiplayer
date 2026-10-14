#pragma once
#include "region_action.h"
#include <array>
#include <cstdint>
#include <string>
#include <optional>

namespace mt2mp::action_session {
enum class RegionPropertyKind:uint8_t { Name=1,Type=2,Purchase=3,LevelRange=4 };
struct RegionPropertyIntent {
    RegionPropertyKind kind=RegionPropertyKind::Name;
    int32_t id=-1;
    std::string name;
    int32_t type=-1;
    std::array<float,3> hit{};
    std::array<int32_t,2> levels{-1,-1};
    bool operator==(const RegionPropertyIntent&) const=default;
};
struct RegionLevelState {
    std::array<uint64_t,3> counts{};
    std::string roster_sha256,levels_sha256,expected_sha256;
    std::array<uint64_t,4> global_random{};
    bool operator==(const RegionLevelState&) const=default;
};
struct RegionPropertyState {
    RegionState content;
    std::array<uint64_t,4> type_random{};
    std::string name;
    int32_t type=-1,status=-1,level=-1,max_level=-1;
    bool owned=false;
    std::optional<RegionLevelState> actors;
    uint64_t building_count=0,path_count=0,path_node_count=0,network_node_count=0,network_pipe_count=0;
    bool operator==(const RegionPropertyState&) const=default;
};
}
