#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
using namespace mt2mp::action_session;
inline void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline const std::string owner_key="action:33333333-3333-4333-8333-333333333333";
inline const std::string target_key="action:44444444-4444-4444-8444-444444444444";
inline NpcQuestIntent intent(){NpcQuestIntent v;v.region=v.destination_region=67;v.family=QuestDestinationFamily::DirectMonster;
 v.destination_key=target_key;v.action=6;v.kills=1;v.monster_type=0;
 auto& r=v.before;r.sequence_id=0;r.type=1;r.name="Fetch supplies";r.description="Existing authored text";
 r.action=2;r.destination_key="action:55555555-5555-4555-8555-555555555555";r.xp_reward=23;r.gold_reward=7;
 r.accepted={{4,3},2,9};r.completed={{1,2},1,4};r.rating={.5f,.25f,.75f,true};return v;}
inline NpcQuestState before(){NpcQuestState v;v.record=intent().before;v.count=3;v.completed=1;v.all_sha256=std::string(64,'a');v.global_random={1,2,3,4};return v;}
inline NpcQuestState after(NpcQuestState v){const auto i=intent();v.record.destination_key=i.destination_key;v.record.action=i.action;v.record.monsters_to_kill=i.kills;
 v.record.monster_type=i.monster_type;v.record.name="Slay Gnashbone";v.all_sha256=std::string(64,'b');if(v.undo_count<20)++v.undo_count;++v.global_random[0];return v;}
inline Message request(){Message m;m.session=5;auto& r=m.request;r.action=Action::SetNpcQuestDestination;r.peer=2;r.sequence=1;
 r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;r.canonical_record_key=owner_key;r.npc_quest=intent();return m;}
inline Message result(){auto m=request();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
 m.evidence.quest_before=before();m.evidence.quest_after=after(before());m.evidence.undo_confirmed=m.evidence.retirement_confirmed=true;return m;}
