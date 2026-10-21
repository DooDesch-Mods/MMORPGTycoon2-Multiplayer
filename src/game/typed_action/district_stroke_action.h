#pragma once
#include "district_action.h"
#include <map>

namespace mt2mp::action_session {
enum class DistrictStrokeKind:uint8_t {Paint=1,Erase=2,Delete=3};
struct DistrictStrokeStamp {
    uint8_t route=0;
    TerrainShape shape;
    bool operator==(const DistrictStrokeStamp&)const=default;
};
struct DistrictStrokeIntent {
    DistrictStrokeKind kind=DistrictStrokeKind::Paint;
    int32_t region=-1,geo=0;
    std::string selected;
    std::array<uint32_t,3> press{},finish{};
    std::array<uint32_t,4> color{},water_color{};
    bool change_color=false,change_water_color=false;
    TerrainShape clip;
    std::vector<DistrictStrokeStamp> stamps;
    bool operator==(const DistrictStrokeIntent&)const=default;
};
struct DistrictStrokeState {
    uint32_t count=0;
    std::string values_sha;
    bool operator==(const DistrictStrokeState&)const=default;
};
struct DistrictStrokeEffect {
    DistrictStrokeState before,after;
    std::string inverse_sha;
    uint32_t inverse_operations=0;
    bool operator==(const DistrictStrokeEffect&)const=default;
};
}
