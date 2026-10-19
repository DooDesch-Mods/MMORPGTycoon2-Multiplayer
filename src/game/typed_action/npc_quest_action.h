#pragma once
#include "../npc_quest_state.h"
#include <array>
#include <cstdint>
#include <string>

namespace mt2mp::action_session {
enum class QuestDestinationFamily:uint8_t { Building=1,Zone=2,DirectMonster=3,NPC=4 };
struct NpcQuestIntent {
    int32_t region=-1,destination_region=-1;
    QuestDestinationFamily family=QuestDestinationFamily::Building;
    std::string destination_key;
    int32_t action=0,kills=0,monster_type=-1;
    npc_quest_records::Record before;
    bool operator==(const NpcQuestIntent&) const=default;
};
struct NpcQuestState {
    npc_quest_records::Record record;
    std::string all_sha256;
    uint32_t count=0;int32_t completed=0;
    uint64_t undo_count=0;
    std::array<uint64_t,4> global_random{};
    bool operator==(const NpcQuestState&) const=default;
};
inline bool npc_quest_record_transition(const NpcQuestIntent& intent,const npc_quest_records::Record& after){
    auto expected=intent.before;
    expected.destination_key=intent.destination_key;expected.action=intent.action;
    expected.monsters_to_kill=intent.kills;expected.monster_type=intent.monster_type;
    expected.name=after.name;
    return !after.name.empty()&&after==expected&&
        (intent.before.destination_key!=intent.destination_key||intent.before.action!=intent.action||intent.before.monsters_to_kill!=intent.kills);
}
inline bool npc_quest_transition(const NpcQuestIntent& intent,const NpcQuestState& before,const NpcQuestState& after){
    return before.record==intent.before&&npc_quest_record_transition(intent,after.record)&&
        before.count>0&&before.count==after.count&&before.completed==after.completed&&
        before.undo_count<=20&&after.undo_count==(before.undo_count<20?before.undo_count+1:20)&&
        before.all_sha256!=after.all_sha256;
}
}
