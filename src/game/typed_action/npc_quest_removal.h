#pragma once
#include "monster_quest_retirement.h"

namespace mt2mp::action_session {
struct NpcQuestRemovalProof {
    MonsterQuestRetirement quests;
    std::string references_before,references_after;
    bool operator==(const NpcQuestRemovalProof&) const = default;
};
}
