#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace mt2mp::action_session {
struct BuildingStatusIntent {
    std::string key;int32_t before=-1,state=-1;bool dungeon_ui=false;
    bool operator==(const BuildingStatusIntent&)const=default;
};
struct BuildingStatusState {
    int32_t state=-1,construction=0;float bandwidth=0;bool user_set=false,dungeon=false,dungeon_ready=false;
    uint32_t bugs=0,jobs=0,linked=0,developers=0;
    std::string bugs_sha256,expected_bugs_sha256,pool_sha256;
    uint64_t undo_count=0;std::array<uint64_t,4> global_random{};
    bool operator==(const BuildingStatusState&)const=default;
};
inline bool building_status_transition(const BuildingStatusIntent& intent,const BuildingStatusState& before,const BuildingStatusState& after){
    return before.state==intent.before&&after.state==intent.state&&after.user_set&&after.bandwidth==(intent.state?.5f:0.f)&&
        before.dungeon==after.dungeon&&before.dungeon_ready==after.dungeon_ready&&(!before.dungeon||!intent.state||before.dungeon_ready)&&
        before.construction==after.construction&&(intent.state!=2||intent.dungeon_ui||!before.construction)&&(!intent.dungeon_ui||before.dungeon)&&before.bugs==after.bugs&&before.developers==after.developers&&
        before.expected_bugs_sha256==after.bugs_sha256&&after.expected_bugs_sha256==after.bugs_sha256&&
        (intent.state==2?(before.linked<=before.jobs&&after.jobs==before.jobs-before.linked&&after.linked==0):(before.jobs==after.jobs&&before.linked==after.linked))&&
        before.undo_count==after.undo_count;
}
}
