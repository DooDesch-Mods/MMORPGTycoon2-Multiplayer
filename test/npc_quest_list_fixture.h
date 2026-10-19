#pragma once
#include "typed_action/action_session.h"
#include <stdexcept>
using namespace mt2mp::action_session;
inline void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
inline const std::string owner_key="action:33333333-3333-4333-8333-333333333333";
inline bool removing=false;
inline mt2mp::npc_quest_records::Record record(int32_t slot){
    mt2mp::npc_quest_records::Record v;v.sequence_id=slot;v.type=1;v.name="Existing "+std::to_string(slot);v.description="Original text";
    v.action=2;v.destination_key="action:55555555-5555-4555-8555-555555555555";v.xp_reward=23;v.gold_reward=7;
    v.accepted={{4,3},2,9};v.completed={{1,2},1,4};v.rating={.5f,.25f,.75f,true};return v;
}
inline NpcQuestListState before(){NpcQuestListState v;for(int32_t i=0;i<3;++i)v.records.push_back(record(i));
    v.count=5;v.completed=3;v.all_sha256=std::string(64,'a');v.targets_sha256=std::string(64,'c');v.references_sha256=std::string(64,'d');v.global_random={1,2,3,4};return v;
}
inline NpcQuestListIntent intent(){return {67,removing?0:-1,before().records};}
inline NpcQuestListState after(NpcQuestListState v){
    if(removing){v.records.erase(v.records.begin());for(size_t i=0;i<v.records.size();++i)v.records[i].sequence_id=int32_t(i);--v.count;--v.completed;v.references_sha256=std::string(64,'e');}
    else{auto created=record(int32_t(v.records.size()));created.accepted={};created.completed={};created.rating={};v.records.push_back(created);++v.count;}
    v.all_sha256=std::string(64,'b');if(v.undo_count<20)++v.undo_count;++v.global_random[0];return v;
}
inline Message request(){Message m;m.session=5;auto& r=m.request;r.action=Action::SetNpcQuestList;r.peer=2;r.sequence=1;
    r.action_uuid=action_id;r.snapshot_uuid=snapshot_id;r.canonical_record_key=owner_key;r.npc_quest_list=intent();return m;
}
inline Message result(){auto m=request();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    m.evidence.quest_list_before=before();m.evidence.quest_list_after=after(before());m.evidence.undo_confirmed=m.evidence.retirement_confirmed=true;return m;
}
