#pragma once
#include "character_rename_action.h"
#include <vector>
#include <optional>
namespace mt2mp::action_session {
struct AbilityEffectRecord {
    std::array<int32_t,3> integers{};std::array<float,2> values{};int32_t count=0;
    bool operator==(const AbilityEffectRecord&)const=default;
};
struct AbilityRecord {
    std::array<std::string,8> text{};float item_value=0;int32_t item_kind=0;bool item_flag=false;
    std::array<int32_t,5> selectors{};std::array<float,4> color{};std::array<float,3> timing{};
    std::array<bool,5> flags{};std::array<float,2> final_values{};int32_t level=0;bool final_flag=false;
    std::vector<AbilityEffectRecord> costs,effects;
    bool operator==(const AbilityRecord&)const=default;
};
struct AbilityAdjustment {
    bool cost=false;int32_t index=-1;float delta=0;
    bool operator==(const AbilityAdjustment&)const=default;
};
struct AbilityEditIntent {
    int32_t kind=-1,id=-1,slot=-1;std::string before_sha256;AbilityRecord record;std::optional<AbilityAdjustment> adjustment;
    bool operator==(const AbilityEditIntent&)const=default;
};
struct AbilityEditState {
    CharacterRenameState owner;AbilityRecord record;std::string all_sha256;uint32_t count=0;
    bool operator==(const AbilityEditState&)const=default;
};
inline std::string ability_key(const AbilityEditIntent& v){return "character:"+std::to_string(v.kind)+":"+std::to_string(v.id)+"/ability/"+std::to_string(v.slot);}
inline bool ability_adjusted_record(const AbilityRecord& before,const AbilityAdjustment& change,AbilityRecord& after){
    if(change.index<0||!std::isfinite(change.delta)||change.delta==0)return false;
    const auto& current=change.cost?before.costs:before.effects;
    if(size_t(change.index)>=current.size())return false;
    const auto value=current[size_t(change.index)].values[0]+change.delta;
    if(!std::isfinite(value))return false;
    const auto& selected=current[size_t(change.index)];
    if(value!=0&&((selected.integers[0]<1||selected.integers[0]>3)&&selected.integers[0]!=7))return false;
    if(value!=0&&(selected.integers[2]==0?value<0:value>0))return false;
    after=before;auto& rows=change.cost?after.costs:after.effects;
    if(value==0)rows.erase(rows.begin()+change.index);
    else rows[size_t(change.index)].values[0]=value;
    return true;
}
inline bool ability_edit_transition(const AbilityEditIntent& intent,const AbilityEditState& before,const AbilityEditState& after){
    if(intent.adjustment){AbilityRecord expected;
        return before.count>0&&before.count==after.count&&before.owner==after.owner&&
            ability_adjusted_record(before.record,*intent.adjustment,expected)&&expected==intent.record&&after.record==expected&&
            before.all_sha256!=after.all_sha256;}
    if(after.record!=intent.record||before.count!=after.count||before.count==0||
       before.owner.name!=after.owner.name||before.owner.definition_name!=after.owner.definition_name||
       before.owner.count!=after.owner.count||before.owner.day!=after.owner.day||before.owner.undo_count!=after.owner.undo_count||
       before.owner.attributes!=after.owner.attributes||before.owner.speed!=after.owner.speed||
       before.owner.boss_health!=after.owner.boss_health||before.owner.boss_phases!=after.owner.boss_phases||
       before.owner.released!=after.owner.released)return false;
    for(size_t i=0;i<8;++i){const auto delta=int64_t(after.owner.release_edits[i])-before.owner.release_edits[i];
        if(i==7?(delta<0||uint64_t(delta)>before.owner.count):delta!=0)return false;}
    return before.record==intent.record||before.all_sha256!=after.all_sha256;
}
}
