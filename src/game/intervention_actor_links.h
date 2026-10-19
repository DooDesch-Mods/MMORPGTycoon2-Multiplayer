#pragma once
#include "intervention_actor_state.h"
#include <map>
#include <memory>
namespace mt2mp::game::interventions::actors {
struct ActorLink {Target target;uintptr_t pointer=0;uint64_t uid=0;};
struct AbilityLink {std::string key;uintptr_t pointer=0;uint64_t uid=0;};
struct Links {
    std::map<uintptr_t,ActorLink> actors;
    std::map<uintptr_t,AbilityLink> abilities;
    std::map<std::pair<uint8_t,std::string>,uintptr_t> actor_keys;
    std::map<std::string,uintptr_t> ability_keys;
};
struct ItemLocal {uintptr_t pointer=0,prototype=0,proxy=0;uint64_t uid=0,prototype_uid=0;bool operator==(const ItemLocal&)const=default;};
}
