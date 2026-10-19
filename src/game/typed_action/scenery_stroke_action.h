#pragma once
#include "terrain_action.h"
#include "duplicate_action.h"

namespace mt2mp::action_session {
enum class SceneryStrokeKind:uint8_t {Paint=1,Erase=2};
using SceneryStrokePose=std::array<uint32_t,10>;
struct SceneryStrokeStamp {
    TerrainShape shape;
    std::vector<SceneryStrokePose> dots;
    bool operator==(const SceneryStrokeStamp&)const=default;
};
struct SceneryStrokeIntent {
    SceneryStrokeKind kind=SceneryStrokeKind::Paint;
    int32_t region=-1,mode=0;
    std::string type,variant;
    bool replace=false,random_variant=false;
    std::vector<SceneryStrokeStamp> stamps;
    std::array<uint32_t,6> bounds{};
    std::array<uint32_t,3> finish_position{};
    bool operator==(const SceneryStrokeIntent&)const=default;
};
struct SceneryStrokeChange {
    DuplicateObject before,after;
    bool operator==(const SceneryStrokeChange&)const=default;
};
struct SceneryStrokeState {
    std::string props_sha,bugs_sha;
    uint64_t bug_count=0;
    std::array<uint64_t,4> random{},region_random{};
    std::array<int32_t,8> release{};
    bool operator==(const SceneryStrokeState&)const=default;
};
struct SceneryStrokeEffect {
    std::vector<DuplicateObject> removed,created;
    std::vector<SceneryStrokeChange> changed;
    std::string untouched_sha;
    bool operator==(const SceneryStrokeEffect&)const=default;
};
}
