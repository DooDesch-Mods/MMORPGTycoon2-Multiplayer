#pragma once
#include "../dungeon_gizmo_reader.h"
#include "../dungeon_gizmo_identity.h"
#include "../dungeon_gizmo_live_state.h"
#include <optional>
#include "../live_host_state/network/node_state.h"
#include <algorithm>

namespace mt2mp::action_session {
enum class DungeonGizmoEdit:uint8_t {Gold,Loot,VendorTrash,Keys,BossKeys,State};
using DungeonGizmoRecord=game::dungeon_gizmo_reader::Record;
struct DungeonGizmoRow {std::string key;DungeonGizmoRecord value;bool operator==(const DungeonGizmoRow&)const=default;};
struct DungeonGizmoScene {std::string key;std::vector<DungeonGizmoRow> rows;bool operator==(const DungeonGizmoScene&)const=default;};
struct DungeonGizmoIntent {
    std::string key,scene,before_sha256;DungeonGizmoEdit edit=DungeonGizmoEdit::Gold;int32_t value=0;bool door=false;
    bool operator==(const DungeonGizmoIntent&)const=default;
};
struct DungeonGizmoState {
    std::string protected_sha256;std::array<uint64_t,4> global_random{};std::vector<DungeonGizmoScene> scenes;
    std::optional<game::dungeon_gizmo_live::State> live;
    std::optional<game::live_host_state::network::State> network;
    bool operator==(const DungeonGizmoState&)const=default;
};
inline int32_t* dungeon_gizmo_value(DungeonGizmoRecord& record,DungeonGizmoEdit edit){
    switch(edit){case DungeonGizmoEdit::Gold:return &record.gold;case DungeonGizmoEdit::Loot:return &record.loot;
    case DungeonGizmoEdit::VendorTrash:return &record.vendor_trash;case DungeonGizmoEdit::Keys:return &record.keys;case DungeonGizmoEdit::BossKeys:return &record.boss_keys;case DungeonGizmoEdit::State:return nullptr;}
    return nullptr;
}
inline bool dungeon_gizmo_project(const DungeonGizmoIntent& intent,const DungeonGizmoState& before,DungeonGizmoState& after){
    after={};if(intent.value<0||intent.value>5)return false;auto expected=before;unsigned matches=0;
    for(auto& scene:expected.scenes)for(auto& row:scene.rows)if(row.key==intent.key){
        if(scene.key!=intent.scene)return false;
        if(intent.edit==DungeonGizmoEdit::State){
            if(!before.live||!before.network||intent.value<1||intent.value>3||(!intent.door&&intent.value==2))return false;
            if(intent.door){row.value.state=intent.value-1;if(intent.value==1)row.value.triggered=false;
                if(intent.value==3){const auto found=std::find_if(before.live->begin(),before.live->end(),[&](const auto& s){return s.key==intent.scene;});
                    if(found==before.live->end())return false;const auto member=std::find_if(found->rows.begin(),found->rows.end(),[&](const auto& r){return r.key==intent.key;});
                    if(member==found->rows.end())return false;if(!member->immediate)row.value.triggered=true;}}
            else row.value.triggered=intent.value==1;
            bool live_match=false;for(auto& s:*expected.live)if(s.key==intent.scene)for(auto& r:s.rows)if(r.key==intent.key){
                if(live_match)return false;live_match=true;r.state=row.value.state;r.triggered=row.value.triggered;
                if(intent.door&&intent.value==1)s.unlocked&=~(uint32_t(1)<<(uint32_t(row.value.lock_id)&31));
                if(intent.door&&intent.value==3&&!r.immediate)s.unlocked|=uint32_t(1)<<(uint32_t(row.value.lock_id)&31);
            }if(!live_match)return false;
        }else{if(intent.door||before.live||before.network)return false;auto* value=dungeon_gizmo_value(row.value,intent.edit);if(!value)return false;*value=intent.value;}
        ++matches;
    }
    if(matches!=1)return false;after=std::move(expected);return true;
}
inline bool dungeon_gizmo_transition(const DungeonGizmoIntent& intent,const DungeonGizmoState& before,const DungeonGizmoState& after){
    DungeonGizmoState expected;if(!dungeon_gizmo_project(intent,before,expected))return false;
    if(intent.edit==DungeonGizmoEdit::State){
        if(!after.live||!after.network)return false;std::string error;if(!game::dungeon_gizmo_live::valid(*after.live,error)||
            !game::dungeon_gizmo_live::same_members(*expected.live,*after.live,error))return false;
        auto portable=*expected.live;
        for(size_t s=0;s<portable.size();++s)for(size_t n=0;n<portable[s].rows.size();++n)if(portable[s].rows[n].key==intent.key){
            const auto& actual=(*after.live)[s].rows[n];
            if(actual.state!=portable[s].rows[n].state||actual.triggered!=portable[s].rows[n].triggered)return false;
            portable[s].rows[n]=actual;
        }
        if(portable!=*after.live)return false;expected.live=after.live;expected.network=after.network;expected.global_random=after.global_random;
    }
    return expected==after;
}
}
