#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace mt2mp::action_session {
struct RegionIntent {
    int32_t id=-1;
    std::string preset;
    std::array<std::string,4> styles{};
    bool operator==(const RegionIntent&) const=default;
};
struct RegionState {
    int32_t id=-1;
    std::string preset;
    std::array<std::string,4> styles{};
    std::array<uint64_t,4> random{};
    uint64_t scenery_count=0;
    std::string scenery_sha256,grid_sha256;
    bool operator==(const RegionState&) const=default;
};
inline bool same_region_content(RegionState a,RegionState b) {
    a.random={};b.random={};return a==b;
}
}
