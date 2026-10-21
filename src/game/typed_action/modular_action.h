#pragma once
#include "duplicate_action.h"
#include "scenery_stroke_action.h"
#include "../subscriber_lifecycle/record_graph.h"
#include "vendor/nlohmann/json.hpp"
#include <optional>
namespace mt2mp::action_session {
struct ModularIntent {
    uint8_t mode=0;int32_t region=-1;std::string target,before_record_sha,theme;
    std::array<uint32_t,3> position{};std::array<uint32_t,10> pose{};std::array<uint32_t,6> clearance{};
    nlohmann::json layout=nlohmann::json::array(),plan=nlohmann::json::array();
    bool operator==(const ModularIntent&)const=default;
};
using ModularState=SceneryStrokeState;
struct ModularEffect {
    std::vector<DuplicateObject> removed,created;
    std::vector<std::array<DuplicateObject,2>> changed;
    std::vector<DuplicateCash> cash;
    std::string untouched_sha;
    std::array<std::string,4> dependency_sha;
    std::vector<std::string> related_buildings;
    bool operator==(const ModularEffect&)const=default;
};
}
