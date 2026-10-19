#pragma once
#include "ability_edit_action.h"
#include "character_membership_action.h"
namespace mt2mp::action_session {
enum class AbilityMembershipCommand:uint8_t {Create,Remove,Dock,Copy};
struct AbilityMembershipIntent {
    AbilityMembershipCommand command=AbilityMembershipCommand::Create;
    int32_t kind=-1,id=-1,slot=-1,level=0,display_slot=-1;
    std::string before_sha256,copy_name;AbilityRecord prefab;
    bool operator==(const AbilityMembershipIntent&)const=default;
};
struct AbilityMembershipSlot {
    AbilityRecord record;int32_t display_slot=-1;
    bool operator==(const AbilityMembershipSlot&)const=default;
};
struct AbilityMembershipState {
    CharacterMembershipState complete;
    std::vector<AbilityMembershipSlot> slots;
    bool operator==(const AbilityMembershipState&)const=default;
};
inline std::string ability_membership_key(const AbilityMembershipIntent& value){
    return "character:"+std::to_string(value.kind)+":"+std::to_string(value.id)+"/abilities";
}
inline bool ability_membership_transition(const AbilityMembershipIntent& intent,const AbilityMembershipState& before,const AbilityMembershipState& after){
    const auto& prior=before.complete;const auto& current=after.complete;
    if(prior.selected.kind!=intent.kind||prior.selected.id!=intent.id||prior.selected.deleted||prior.created||current.created||
       !prior.retired_definitions.empty()||!current.retired_definitions.empty()||prior.selected!=current.selected||
       prior.counts!=current.counts||prior.first_deleted!=current.first_deleted||prior.undo_count!=current.undo_count)return false;
    auto expected=before.slots;
    switch(intent.command){
    case AbilityMembershipCommand::Create:
        if(expected.size()>=12||after.slots.size()!=expected.size()+1||
           !std::equal(expected.begin(),expected.end(),after.slots.begin()))return false;
        return after.slots.back().record.level==(intent.kind==4?1:intent.level)&&after.slots.back().display_slot==-1;
    case AbilityMembershipCommand::Remove:
        if(intent.slot<0||size_t(intent.slot)>=expected.size())return false;
        expected.erase(expected.begin()+intent.slot);return expected==after.slots;
    case AbilityMembershipCommand::Dock:
        if(intent.slot<0||size_t(intent.slot)>=expected.size())return false;
        expected[size_t(intent.slot)].record.level=intent.level;
        expected[size_t(intent.slot)].display_slot=intent.display_slot;return expected==after.slots;
    case AbilityMembershipCommand::Copy:
        if(after.slots.size()!=expected.size()+1||!std::equal(expected.begin(),expected.end(),after.slots.begin()))return false;
        {auto copied=after.slots.back().record;copied.text[0]=intent.prefab.text[0];copied.level=intent.prefab.level;
         return copied==intent.prefab&&after.slots.back().record.text[0]==intent.copy_name&&
             after.slots.back().record.level==intent.level&&after.slots.back().display_slot==-1;}
    }
    return false;
}
}
