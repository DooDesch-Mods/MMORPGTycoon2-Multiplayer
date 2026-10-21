#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
using namespace mt2mp::action_session;
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
inline Ledger money(){Ledger value;value.cash=value.record_cash=10000;value.flat_day=value.record_day=28;value.record_present=1;return value;}
inline CharacterMembershipIntent intent(CharacterMembershipCommand command=CharacterMembershipCommand::CreateVariant){return {command,1,0,command==CharacterMembershipCommand::CreateVariant?"Wolf 3":"",std::string(64,'a'),command==CharacterMembershipCommand::Import?"789c010203":""};}
inline CharacterMembershipState before(bool vacant=false){CharacterMembershipState value;value.counts={0,4,0,0,0};if(vacant)value.first_deleted[1]=1;
    value.selected={1,0,-1,false,"Wolf",{2,3}};value.manager_sha=std::string(64,'a');
    value.family_sha=value.others_sha=value.users_sha=value.quests_sha=value.economy_sha=value.monsters_sha=value.bosses_sha=std::string(64,'b');
    value.global_random={1,2,3,4};value.undo_count=2;return value;}
inline CharacterMembershipState after(const CharacterMembershipIntent& command,CharacterMembershipState value){
    value.manager_sha=std::string(64,'c');value.family_sha=std::string(64,'d');
    if(command.command==CharacterMembershipCommand::CreateVariant){const auto kind=size_t(command.kind);
        const auto id=value.first_deleted[kind]<0?value.counts[kind]++:value.first_deleted[kind];value.first_deleted[kind]=-1;
        value.created=CharacterMembershipRow{command.kind,id,command.id,false,command.name,{}};value.selected.variants.push_back(id);value.global_random[0]++;
    }else if(command.command==CharacterMembershipCommand::Import){value.selected.name="Imported Wolf";value.import_art_sha=value.import_release_sha=std::string(64,'e');}
    else {value.retired_definitions=value.selected.variants;value.retired_definitions.push_back(command.id);std::sort(value.retired_definitions.begin(),value.retired_definitions.end());
        value.selected.deleted=true;value.selected.parent=-1;value.selected.variants.clear();value.first_deleted[size_t(command.kind)]=0;value.undo_count=4;}
    return value;
}
inline Message request(CharacterMembershipCommand command=CharacterMembershipCommand::CreateVariant){Message value;value.session=5;auto& r=value.request;
    r.action=Action::EditCharacterMembership;r.peer=2;r.sequence=1;r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;r.canonical_record_key="character:1:0";r.character_membership=intent(command);return value;}
inline Message result(CharacterMembershipCommand command=CharacterMembershipCommand::CreateVariant,bool vacant=false){auto value=request(command);
    value.kind=Kind::HostResult;value.revision=1;value.required_mask=3;value.outcome=Outcome::Confirmed;value.evidence.retirement_confirmed=true;value.evidence.before=value.evidence.after=money();
    value.evidence.character_membership_before=before(vacant);
    if(command==CharacterMembershipCommand::Import)value.evidence.character_membership_before.import_art_sha=value.evidence.character_membership_before.import_release_sha=std::string(64,'b');value.evidence.character_membership_after=after(value.request.character_membership,value.evidence.character_membership_before);return value;}
