#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
namespace mt2mp::action_session {struct Binding;struct Request;}
namespace mt2mp::game::remove_diagnostics {
struct CharacterBossRetirement {
    uint64_t ticket=0;
    std::vector<std::string> retired_keys;
};
bool prepare_character_boss_retirement(const action_session::Binding&,const action_session::Request&,
    const std::vector<std::pair<uintptr_t,uint64_t>>&,CharacterBossRetirement&,std::string&);
bool confirm_character_boss_retirement(const action_session::Binding&,const action_session::Request&,
    CharacterBossRetirement&,std::string&);
}
