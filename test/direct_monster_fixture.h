#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
using namespace mt2mp::action_session;
inline void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline MonsterIntent intent(){MonsterIntent v;v.region=63;v.character_slot=1;v.character_type=0;v.level=1;v.rank=1;v.tool="item_elitemonster";v.position={8020,70.576828f,8550};return v;}
inline Ledger money(){Ledger v;v.cash=v.record_cash=88400;v.record_present=1;v.flat_day=v.record_day=1;return v;}
inline MonsterState empty(){MonsterState v;v.roster.content_sha256=std::string(64,'a');return v;}
inline MonsterState born(MonsterState v){const auto i=intent();auto& r=v.roster;++r.monsters;++r.undo_count;
 v.actor={r.next_tag,3,i.rank,i.character_slot,i.character_type,i.level,"Deathblood",i.position,i.scale,i.rotation};
 ++r.next_tag;++r.created_monsters;++r.release_edits[6];++r.global_random[0];r.content_sha256=std::string(64,'b');return v;}
inline Message request(){Message m;m.session=5;auto& r=m.request;r.action=Action::PlaceMonster;r.peer=2;r.sequence=1;r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;r.canonical_record_key=std::string("action:")+action_id;r.monster=intent();return m;}
inline Message result(){auto m=request();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;auto& e=m.evidence;e.before=e.after=money();e.monster_before=empty();e.monster_after=born(e.monster_before);e.undo_confirmed=e.retirement_confirmed=true;return m;}
