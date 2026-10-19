#pragma once
#include "../npc_quest_state.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::action_session {
enum class QuestListEdit:uint8_t { List,Title,Kills,Rewards };
struct NpcQuestListIntent {
    int32_t region=-1,remove_slot=-1;
    std::vector<npc_quest_records::Record> before;
    QuestListEdit edit=QuestListEdit::List;
    std::string title;int32_t kills=0,xp=0,gold=0;bool defaults=false;
    bool operator==(const NpcQuestListIntent&)const=default;
};
struct NpcQuestListState {
    std::vector<npc_quest_records::Record> records;
    std::string all_sha256,targets_sha256,references_sha256;
    uint32_t count=0;int32_t completed=0;uint64_t undo_count=0;
    std::array<uint64_t,4> global_random{};
    bool operator==(const NpcQuestListState&)const=default;
};
inline bool npc_quest_list_transition(const NpcQuestListIntent& intent,const NpcQuestListState& before,const NpcQuestListState& after){
    if(intent.remove_slot< -1||before.records!=intent.before||before.targets_sha256!=after.targets_sha256||
       before.all_sha256.empty()||(intent.edit==QuestListEdit::List&&before.all_sha256==after.all_sha256)||before.undo_count>20||
       after.undo_count!=(before.undo_count<20?before.undo_count+1:20))return false;
    if(intent.edit!=QuestListEdit::List){
        if(intent.remove_slot<0||size_t(intent.remove_slot)>=before.records.size()||before.count!=after.count||before.completed!=after.completed||
           before.references_sha256!=after.references_sha256||before.global_random!=after.global_random)return false;
        auto expected=before.records;auto& changed=expected[size_t(intent.remove_slot)];
        switch(intent.edit){
        case QuestListEdit::Title:changed.name=intent.title;break;
        case QuestListEdit::Kills:changed.monsters_to_kill=intent.kills;break;
        case QuestListEdit::Rewards:changed.xp_reward=intent.defaults?-1:intent.xp;changed.gold_reward=intent.defaults?-1:intent.gold;break;
        default:return false;}
        return expected==after.records;
    }
    if(intent.remove_slot==-1){
        if(before.records.size()>4||after.records.size()!=before.records.size()+1||after.count!=before.count+1||
           after.completed!=before.completed||before.references_sha256!=after.references_sha256)return false;
        for(size_t i=0;i<before.records.size();++i)if(before.records[i]!=after.records[i])return false;
        const auto& created=after.records.back();
        return created.sequence_id==int32_t(before.records.size())&&created.type==1&&
            created.accepted==npc_quest_records::DailyCount{}&&created.completed==npc_quest_records::DailyCount{};
    }
    const auto slot=size_t(intent.remove_slot);
    if(slot>=before.records.size()||!before.count||after.count!=before.count-1||after.records.size()+1!=before.records.size()||
       after.completed!=before.completed-(before.records[slot].completed.total>0?1:0))return false;
    for(size_t i=0,j=0;i<before.records.size();++i){if(i==slot)continue;
        auto expected=before.records[i];expected.sequence_id=int32_t(j);if(after.records[j++]!=expected)return false;}
    return true;
}
}
