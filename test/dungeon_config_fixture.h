#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
using namespace mt2mp::action_session;
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline int32_t previous_count=1,desired_count=4,instance_price=100;inline bool sandbox=false;
inline void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
inline DungeonConfigIntent intent(){return {"action:33333333-3333-4333-8333-333333333333",previous_count,desired_count};}
inline DungeonConfigState before(){DungeonConfigState v;v.desired=previous_count;v.price=instance_price;v.sandbox=sandbox;v.instances={{0,true},{2,false}};v.global_random={11,12,13,14};v.undo_count=2;return v;}
inline DungeonConfigState after(DungeonConfigState v){v.desired=desired_count;++v.global_random[0];return v;}
inline Ledger money(){Ledger v;v.cash=v.record_cash=10000;v.flat_day=v.record_day=28;v.record_present=1;v.record_income=500;v.record_expense=100;v.line_counts[0]=2;v.line_income[0]=-100;return v;}
inline Ledger charged(){auto v=money();const auto cost=std::max(desired_count-previous_count,0)*instance_price;v.cash-=cost;v.record_cash-=cost;v.record_expense+=cost;++v.line_counts[0];v.line_income[0]-=cost;return v;}
inline Message request(){Message m;m.session=5;auto& r=m.request;r.action=Action::ConfigureDungeonInstances;r.peer=2;r.sequence=1;r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;r.canonical_record_key=intent().key;r.dungeon_config=intent();return m;}
inline Message result(){auto m=request();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;m.evidence.before=money();m.evidence.after=charged();m.evidence.dungeon_config_before=before();m.evidence.dungeon_config_after=after(before());m.evidence.retirement_confirmed=true;return m;}
