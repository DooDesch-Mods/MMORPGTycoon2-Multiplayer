#pragma once
#include "character_rename_action.h"
#include <vector>
#include <optional>
#include <algorithm>
namespace mt2mp::action_session {
struct CostumePose {
    std::array<float,4> rotation{0,0,0,1};std::array<float,3> position{},scale{1,1,1};
    bool operator==(const CostumePose&)const=default;
};
struct CostumeAttachment {
    std::string name;CostumePose pose;
    bool operator==(const CostumeAttachment&)const=default;
};
struct CostumePart {
    std::string bone,model;CostumePose pose;float model_scale=1;
    std::vector<CostumeAttachment> attachments;std::vector<std::array<float,4>> colors;
    bool operator==(const CostumePart&)const=default;
};
struct CostumeRecord {
    std::string name,skeleton;std::vector<CostumePart> parts;
    bool flag=false;
    bool operator==(const CostumeRecord&)const=default;
};
struct CostumeWeaponIntent {
    std::string category;bool enabled=false;
    bool operator==(const CostumeWeaponIntent&)const=default;
};
struct CostumeEditIntent {
    int32_t kind=-1,id=-1;std::string before_sha256;CostumeRecord record;std::optional<CostumeWeaponIntent> weapon_category;
    bool operator==(const CostumeEditIntent&)const=default;
};
struct CostumeEditState {
    CharacterRenameState owner;CostumeRecord record;std::string all_sha256;uint32_t count=0;std::vector<std::string> weapon_categories;
    bool operator==(const CostumeEditState&)const=default;
};
inline std::string costume_key(const CostumeEditIntent& v){return "character:"+std::to_string(v.kind)+":"+std::to_string(v.id)+"/costume";}
inline bool costume_weapon_after(const std::vector<std::string>& before,const CostumeWeaponIntent& intent,std::vector<std::string>& after){
    after=before;const auto found=std::find(after.begin(),after.end(),intent.category);
    if(intent.enabled){if(found!=after.end())return false;after.push_back(intent.category);}
    else{if(found==after.end())return false;*found=after.back();after.pop_back();}return true;
}
inline bool costume_edit_transition(const CostumeEditIntent& intent,const CostumeEditState& before,const CostumeEditState& after){
    if(intent.weapon_category){std::vector<std::string> expected;
        return before.count>0&&before.count==after.count&&before.owner==after.owner&&before.record==intent.record&&after.record==before.record&&
            costume_weapon_after(before.weapon_categories,*intent.weapon_category,expected)&&expected==after.weapon_categories&&before.all_sha256!=after.all_sha256;}
    if(before.weapon_categories!=after.weapon_categories||after.record!=intent.record||before.count!=after.count||before.count==0||
       before.owner.name!=after.owner.name||before.owner.definition_name!=after.owner.definition_name||
       before.owner.count!=after.owner.count||before.owner.day!=after.owner.day||before.owner.undo_count!=after.owner.undo_count||
       before.owner.attributes!=after.owner.attributes||before.owner.speed!=after.owner.speed||before.owner.boss_health!=after.owner.boss_health||
       before.owner.boss_phases!=after.owner.boss_phases||before.owner.released!=after.owner.released||!after.owner.edited||after.owner.rename_day!=before.owner.day)return false;
    for(size_t i=0;i<8;++i)if(int64_t(after.owner.release_edits[i])-before.owner.release_edits[i]!=(i==7&&!before.owner.edited?1:0))return false;
    return before.record==intent.record||before.all_sha256!=after.all_sha256;
}
}
