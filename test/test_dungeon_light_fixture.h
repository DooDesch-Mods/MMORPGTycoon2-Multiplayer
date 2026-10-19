#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
using namespace mt2mp::action_session;
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline bool equal_apply=false,sandbox=false;inline int32_t previous_count=1;
inline void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
inline std::array<float,23> light(){std::array<float,23> v;for(size_t i=0;i<v.size();++i)v[i]=float(i+1)*.1f;return v;}
inline DungeonLightIntent intent(){DungeonLightIntent v;v.key="action:33333333-3333-4333-8333-333333333333";v.before_sha256=std::string(64,'a');v.input.spinners={200,90,100,200};v.input.sliders={5,2,100};for(auto& color:v.input.colors)color={.1f,.2f,.3f,1};v.light=light();if(!equal_apply)for(float& x:v.light)x+=1;return v;}
inline DungeonLightState before(){DungeonLightState v;v.light=light();v.dungeon.desired=previous_count;v.dungeon.price=100;v.dungeon.sandbox=sandbox;v.dungeon.instances={{0,true},{2,false}};v.dungeon.global_random={11,12,13,14};v.dungeon.undo_count=2;return v;}
inline DungeonLightState after(DungeonLightState v){v.light=intent().light;++v.dungeon.global_random[0];return v;}
inline Ledger money(){Ledger v;v.cash=v.record_cash=10000;v.flat_day=v.record_day=28;v.record_present=1;v.record_income=500;v.record_expense=100;v.line_counts[0]=2;v.line_income[0]=-100;return v;}
inline Message request(){Message m;m.session=5;auto& r=m.request;r.action=Action::SetDungeonLighting;r.peer=2;r.sequence=1;r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;r.canonical_record_key=intent().key;r.dungeon_light=intent();return m;}
inline Message result(){auto m=request();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;m.evidence.before=m.evidence.after=money();m.evidence.dungeon_light_before=before();m.evidence.dungeon_light_after=after(before());m.evidence.retirement_confirmed=true;return m;}
