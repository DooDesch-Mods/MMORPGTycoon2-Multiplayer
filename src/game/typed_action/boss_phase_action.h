#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::action_session {
enum class BossPhaseKind:uint8_t {Reorder=0,Name=1,Health=2,Vfx=3,Color=4,RemoveAbility=5,AddAbility=6,Create=7,Destroy=8};
struct BossPhaseIntent {
    int32_t character_id=-1,phase=-1,boundary=-1;
    BossPhaseKind kind=BossPhaseKind::Reorder;
    std::string text;int32_t health=0;std::array<float,4> color{};
    int32_t element=-1;std::vector<int32_t> ability_indices;
    bool operator==(const BossPhaseIntent&)const=default;
};
struct BossPhaseRow {
    std::string name,vfx;
    int32_t health=0;
    std::array<float,4> color{};
    std::vector<int32_t> abilities;
    bool operator==(const BossPhaseRow&)const=default;
};
struct BossTypePhases {
    int32_t id=-1,rename_day=0;
    std::string name,definition;
    bool deleted=false,edited=false,released=false;
    std::vector<std::string> ability_names;
    std::vector<BossPhaseRow> phases;
    bool operator==(const BossTypePhases&)const=default;
};
struct BossPhaseState {
    std::vector<BossTypePhases> bosses;
    int32_t release_edits=0;std::array<float,4> default_color{};
    bool operator==(const BossPhaseState&)const=default;
};
}
