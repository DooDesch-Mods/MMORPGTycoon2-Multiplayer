#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
using namespace mt2mp::action_session;
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
inline Ledger money(){Ledger value;value.cash=value.record_cash=10000;value.flat_day=value.record_day=28;value.record_present=1;return value;}
inline AbilityRecord record(int32_t id){AbilityRecord value;value.text[0]="Attack "+std::to_string(id);value.level=id;
    value.effects.push_back({{0,1,2},{3,4},5});return value;}
inline AbilityMembershipIntent intent(AbilityMembershipCommand command=AbilityMembershipCommand::Create){
    AbilityMembershipIntent value;value.command=command;value.kind=1;value.id=0;value.before_sha256=std::string(64,'a');
    if(command==AbilityMembershipCommand::Create)value.level=9;
    if(command==AbilityMembershipCommand::Remove)value.slot=1;
    if(command==AbilityMembershipCommand::Dock){value.slot=1;value.level=7;value.display_slot=3;}
    if(command==AbilityMembershipCommand::Copy){value.level=8;value.display_slot=2;value.prefab=record(2);value.copy_name="Attack 2 1";}
    return value;
}
inline AbilityMembershipState before(){AbilityMembershipState value;auto& complete=value.complete;
    complete.counts={0,1,0,0,0};complete.selected={1,0,-1,false,"Wolf",{}};
    complete.manager_sha=std::string(64,'a');complete.family_sha=complete.others_sha=complete.users_sha=complete.quests_sha=
        complete.economy_sha=complete.monsters_sha=complete.bosses_sha=std::string(64,'b');
    complete.global_random={1,2,3,4};complete.undo_count=2;value.slots={{record(1),0},{record(2),1}};return value;
}
inline AbilityMembershipState after(const AbilityMembershipIntent& command,AbilityMembershipState value){
    value.complete.manager_sha=std::string(64,'c');value.complete.family_sha=std::string(64,'d');
    switch(command.command){
    case AbilityMembershipCommand::Create:{auto generated=record(command.level);if(command.kind==4)generated.level=1;value.slots.push_back({generated,-1});break;}
    case AbilityMembershipCommand::Remove:value.slots.erase(value.slots.begin()+command.slot);break;
    case AbilityMembershipCommand::Dock:value.slots[size_t(command.slot)].record.level=command.level;value.slots[size_t(command.slot)].display_slot=command.display_slot;break;
    case AbilityMembershipCommand::Copy:{auto copied=command.prefab;copied.text[0]=command.copy_name;copied.level=command.level;value.slots.push_back({copied,-1});break;}}
    return value;
}
inline Message request(AbilityMembershipCommand command=AbilityMembershipCommand::Create){Message value;value.session=5;auto& r=value.request;
    r.action=Action::EditAbilityMembership;r.peer=2;r.sequence=1;r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;
    r.ability_membership=intent(command);r.canonical_record_key=ability_membership_key(r.ability_membership);return value;}
inline Message result(AbilityMembershipCommand command=AbilityMembershipCommand::Create){auto value=request(command);
    value.kind=Kind::HostResult;value.revision=1;value.required_mask=3;value.outcome=Outcome::Confirmed;
    value.evidence.retirement_confirmed=true;value.evidence.before=value.evidence.after=money();
    value.evidence.ability_membership_before=before();value.evidence.ability_membership_after=after(value.request.ability_membership,before());return value;}
