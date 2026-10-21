#pragma once
#include "intervention_state.h"
#include "boss_current.h"
#include "monster_zone_read.h"
namespace mt2mp::game::interventions {
struct Actor {Target target;uintptr_t object=0;uint64_t uid=0;bool operator==(const Actor&)const=default;};
struct Zone {std::string key;uintptr_t object=0;uint64_t uid=0;bool operator==(const Zone&)const=default;};
struct Census {
    boss_current::State bosses;
    character_definition_identity::Census definitions;
    character_membership_reader::Users users;
    std::map<uintptr_t,std::string> owners;
    std::vector<Actor> actors;std::vector<Zone> zones;
};
bool actor_census(const action_session::Context&,const action_session::Binding&,Census&,std::string&);
bool census(const action_session::Context&,const action_session::Binding&,Census&,std::string&);
}
