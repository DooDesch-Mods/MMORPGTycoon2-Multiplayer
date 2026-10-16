#pragma once
#include "state.h"
#include "../npc_quest_state.h"
#include <array>
namespace mt2mp::game::bug_authority {
struct BugIdentity {
    ParentKind parent_kind=ParentKind::Building;
    std::string parent_key;
    Birth birth;
    bool operator==(const BugIdentity&)const=default;
};
struct JobRow {
    BugIdentity bug;
    // Snapshot uses the witnessed serialized linkId, never Job.raw_id or the
    // current queue ordinal. Later births use the existing publication/action.
    Birth birth;
    int32_t type=7,developer_id=-1,region_id=-1;
    bool valid=true;
    std::array<float,3> position{};
    bool operator==(const JobRow&)const=default;
};
struct ParentVisual {
    ParentKind kind=ParentKind::Building;
    std::string key;
    int32_t state=2;
    float bandwidth=0;
    bool user_set=false,tint_override=false;
    std::array<float,4> tint{};
    bool operator==(const ParentVisual&)const=default;
};
struct JobCounter {
    npc_quest_records::DailyCount value;
    int32_t highest=0,day=0;
    bool operator==(const JobCounter&)const=default;
};
struct WorkState {
    std::vector<JobRow> jobs;
    std::vector<ParentVisual> parents;
    JobCounter created,fixed;
    bool operator==(const WorkState&)const=default;
};
}
