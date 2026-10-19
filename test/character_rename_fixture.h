#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
using namespace mt2mp::action_session;
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline int32_t character_kind=0;
inline bool edited_before=false;
inline void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
inline CharacterRenameIntent intent(){return {character_kind,1,"Old name","A new name"};}
inline CharacterRenameState before(){CharacterRenameState v;v.day=28;v.rename_day=20;v.count=13;v.name=v.definition_name="Old name";
    v.all_sha256=std::string(64,'a');v.others_sha256=std::string(64,'b');v.edited=edited_before;v.released=true;v.undo_count=2;
    v.release_edits={1,2,3,4,5,6,7,8};v.global_random={11,12,13,14};return v;}
inline CharacterRenameState after(CharacterRenameState v){v.name=v.definition_name=intent().name;v.rename_day=v.day;
    if(!v.edited)++v.release_edits[7];v.edited=true;v.all_sha256=std::string(64,'c');++v.global_random[0];return v;}
inline Message request(){Message m;m.session=5;auto& r=m.request;r.action=Action::RenameCharacter;r.peer=2;r.sequence=1;
    r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;r.canonical_record_key=character_key(intent());r.character_rename=intent();return m;}
inline Message result(){auto m=request();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    m.evidence.character_rename_before=before();m.evidence.character_rename_after=after(before());m.evidence.retirement_confirmed=true;return m;}
