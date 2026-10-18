#pragma once
#include "../npc_quest_state.h"
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::action_session {
struct MonsterRetiredQuest {
    std::string owner_key;
    npc_quest_records::Record record;
    bool operator==(const MonsterRetiredQuest&)const=default;
};
struct MonsterQuestRetirement {
    std::string before_sha256,after_sha256;
    uint32_t before_count=0,after_count=0;
    int32_t completed_before=0,completed_after=0;
    std::vector<MonsterRetiredQuest> removed;
    bool operator==(const MonsterQuestRetirement&)const=default;
};
}
