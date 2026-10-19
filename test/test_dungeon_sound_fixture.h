#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
using namespace mt2mp::action_session;
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline bool equal_apply=false,sandbox=false;inline int32_t previous_count=1;
inline void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
inline std::array<float,23> light(){std::array<float,23> v;for(size_t i=0;i<v.size();++i)v[i]=float(i+1)*.1f;return v;}
inline int32_t selected_atmosphere=0;
inline DungeonSoundIntent intent(){DungeonSoundIntent v;v.key="action:33333333-3333-4333-8333-333333333333";v.atmosphere=selected_atmosphere;v.before=equal_apply?selected_atmosphere:(selected_atmosphere+1)%6;return v;}
inline DungeonSoundState before(){DungeonSoundState v;v.light=light();v.atmosphere=intent().before;v.dungeon.desired=previous_count;v.dungeon.price=100;v.dungeon.sandbox=sandbox;v.dungeon.instances={{0,true},{2,false}};v.dungeon.global_random={11,12,13,14};v.dungeon.undo_count=2;return v;}
inline DungeonSoundState after(DungeonSoundState v){v.atmosphere=intent().atmosphere;++v.dungeon.global_random[0];return v;}
inline Ledger money(){Ledger v;v.cash=v.record_cash=10000;v.flat_day=v.record_day=28;v.record_present=1;v.record_income=500;v.record_expense=100;v.line_counts[0]=2;v.line_income[0]=-100;return v;}
inline Message request(){Message m;m.session=5;auto& r=m.request;r.action=Action::SetDungeonAtmosphere;r.peer=2;r.sequence=1;r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;r.canonical_record_key=intent().key;r.dungeon_sound=intent();return m;}
inline Message result(){auto m=request();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;m.evidence.before=m.evidence.after=money();m.evidence.dungeon_sound_before=before();m.evidence.dungeon_sound_after=after(before());m.evidence.retirement_confirmed=true;return m;}
