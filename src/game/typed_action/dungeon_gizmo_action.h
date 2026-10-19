#pragma once
#include "../dungeon_gizmo_reader.h"
#include "../dungeon_gizmo_identity.h"

namespace mt2mp::action_session {
enum class DungeonGizmoEdit:uint8_t {Gold,Loot,VendorTrash,Keys,BossKeys};
using DungeonGizmoRecord=game::dungeon_gizmo_reader::Record;
struct DungeonGizmoRow {std::string key;DungeonGizmoRecord value;bool operator==(const DungeonGizmoRow&)const=default;};
struct DungeonGizmoScene {std::string key;std::vector<DungeonGizmoRow> rows;bool operator==(const DungeonGizmoScene&)const=default;};
struct DungeonGizmoIntent {
    std::string key,scene,before_sha256;DungeonGizmoEdit edit=DungeonGizmoEdit::Gold;int32_t value=0;
    bool operator==(const DungeonGizmoIntent&)const=default;
};
struct DungeonGizmoState {
    std::string protected_sha256;std::array<uint64_t,4> global_random{};std::vector<DungeonGizmoScene> scenes;
    bool operator==(const DungeonGizmoState&)const=default;
};
inline int32_t* dungeon_gizmo_value(DungeonGizmoRecord& record,DungeonGizmoEdit edit){
    switch(edit){case DungeonGizmoEdit::Gold:return &record.gold;case DungeonGizmoEdit::Loot:return &record.loot;
    case DungeonGizmoEdit::VendorTrash:return &record.vendor_trash;case DungeonGizmoEdit::Keys:return &record.keys;case DungeonGizmoEdit::BossKeys:return &record.boss_keys;}
    return nullptr;
}
inline bool dungeon_gizmo_project(const DungeonGizmoIntent& intent,const DungeonGizmoState& before,DungeonGizmoState& after){
    after={};if(intent.value<0||intent.value>5)return false;auto expected=before;unsigned matches=0;
    for(auto& scene:expected.scenes)for(auto& row:scene.rows)if(row.key==intent.key){
        if(scene.key!=intent.scene)return false;auto* value=dungeon_gizmo_value(row.value,intent.edit);if(!value)return false;*value=intent.value;++matches;
    }
    if(matches!=1)return false;after=std::move(expected);return true;
}
inline bool dungeon_gizmo_transition(const DungeonGizmoIntent& intent,const DungeonGizmoState& before,const DungeonGizmoState& after){
    DungeonGizmoState expected;return dungeon_gizmo_project(intent,before,expected)&&expected==after;
}
}
