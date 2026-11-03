#pragma once
#include "../game_rules_state.h"
namespace mt2mp::action_session {
using GameRulesIntent=game::game_rules::Intent;
using GameRulesProof=game::game_rules::Proof;
inline bool ordered_game_rules(const GameRulesIntent& intent){
    return (intent.command==game::game_rules::Command::RespawnDelay||intent.command==game::game_rules::Command::RespawnSeconds||intent.command==game::game_rules::Command::ApplyXP)&&game::game_rules::valid(intent);
}
}
