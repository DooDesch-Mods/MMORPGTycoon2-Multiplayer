#pragma once
#include <array>
#include <optional>
#include "../bug_authority/job_state.h"
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::game::developer_staff::construction {
struct ActorPose {
    int32_t id=-1,region_id=-1;
    std::array<float,4> orientation{0,0,0,1};
    std::array<float,3> scale{1,1,1};
    bool operator==(const ActorPose&)const=default;
};
// The canonical Building key comes from snapshot admission or ordered placement.
// Its local Job occurrence is retained at that boundary, never discovered from
// a later queue index or an untrusted native Job ID.
struct JobState {
    std::string building_key;
    bool present=false,active=false;
    int32_t developer_id=-1;
    bool operator==(const JobState&)const=default;
};
struct State {
    std::vector<ActorPose> actors;
    std::vector<JobState> jobs;
    std::optional<bug_authority::WorkState> work;
    bool operator==(const State&)const=default;
};
struct LocalJob {
    std::string building_key;
    uintptr_t job=0;
    uint64_t building_uid=0;
    bool target_alive=true;
    bool retired=false;
    uintptr_t building=0,building_proxy=0,region=0;
    bool target_restored=false;
    bool operator==(const LocalJob&)const=default;
};
}
