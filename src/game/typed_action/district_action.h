#pragma once
#include "terrain_action.h"
#include <array>
#include <string>
namespace mt2mp::action_session {
enum class DistrictTextKind:uint8_t {Name=1,Description=2};
struct DistrictTextIntent {DistrictTextKind kind=DistrictTextKind::Name;std::string text;bool operator==(const DistrictTextIntent&)const=default;};
struct DistrictTitleIntent {std::array<uint32_t,3> position{};bool operator==(const DistrictTitleIntent&)const=default;};
struct DistrictState {
    int32_t region=-1,geo=-1,constructed_day=0,saved_version=0;
    std::string name,description;
    std::array<uint32_t,10> pose{};
    std::array<uint32_t,33> values{};
    std::array<uint8_t,13> flags{};
    bool manual_title_inside=true;
    TerrainShape shape;
    bool operator==(const DistrictState&)const=default;
};
}
