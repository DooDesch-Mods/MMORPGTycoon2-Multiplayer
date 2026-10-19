#pragma once
#include <array>
#include <cstdint>
#include <cmath>
#include <string>
namespace mt2mp::action_session {
struct CharacterRenameIntent {
    int32_t kind=-1,id=-1;std::string before,name;
    uint8_t property=0;float value=0;
    bool operator==(const CharacterRenameIntent&)const=default;
};
struct CharacterRenameState {
    int32_t day=0,rename_day=0;uint32_t count=0;
    std::string name,definition_name,all_sha256,others_sha256;
    bool edited=false,released=false;
    std::array<std::array<float,3>,2> attributes{};float speed=0;int32_t boss_health=0,boss_phases=0;
    uint64_t undo_count=0;std::array<int32_t,8> release_edits{};std::array<uint64_t,4> global_random{};
    bool operator==(const CharacterRenameState&)const=default;
};
inline std::string character_key(const CharacterRenameIntent& v){return "character:"+std::to_string(v.kind)+":"+std::to_string(v.id);}
inline bool character_rename_transition(const CharacterRenameIntent& intent,const CharacterRenameState& before,const CharacterRenameState& after){
    if(intent.property){
        if(before.name!=intent.before||before.name!=after.name||before.definition_name!=after.definition_name||before.released!=after.released||
           before.day!=after.day||before.count!=after.count||before.others_sha256!=after.others_sha256||before.undo_count!=after.undo_count||
           before.boss_phases!=after.boss_phases)return false;
        auto expected=before;
        if(intent.property==4)expected.speed=intent.value;
        else if(intent.property==1&&intent.kind==4){if(expected.boss_phases)expected.boss_health=int32_t(intent.value);}
        else {const auto index=size_t(intent.property-1);if(index>=3)return false;
            expected.attributes[0][index]=intent.value;expected.attributes[1][index]=intent.property==1?intent.value*2.f:intent.value;}
        if(expected.attributes!=after.attributes||expected.speed!=after.speed||expected.boss_health!=after.boss_health)return false;
        for(size_t i=0;i<8;++i){const auto delta=int64_t(after.release_edits[i])-before.release_edits[i];
            if(i==7?(delta<0||uint64_t(delta)>before.count):delta!=0)return false;}
        return true;
    }
    if(before.attributes!=after.attributes||before.speed!=after.speed||before.boss_health!=after.boss_health||before.boss_phases!=after.boss_phases)return false;
    if(before.name!=intent.before||intent.name==intent.before||after.name!=intent.name||after.definition_name!=intent.name||
       !after.edited||after.rename_day!=before.day||before.day!=after.day||before.released!=after.released||before.count!=after.count||
       before.others_sha256!=after.others_sha256||before.all_sha256.empty()||before.all_sha256==after.all_sha256||
       before.undo_count!=after.undo_count)return false;
    for(size_t i=0;i<8;++i)if(int64_t(after.release_edits[i])-before.release_edits[i]!=(i==7&&!before.edited?1:0))return false;
    return true;
}
}
