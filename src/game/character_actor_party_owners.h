#pragma once
#include "party_saved_identity_runtime.h"
#include <map>
#include <set>

namespace mt2mp::game::character_actor_party_owners {
inline bool append(const party_identity::Scope& current,
                   const std::vector<party_saved_identity_runtime::Current>& parties,
                   std::map<uintptr_t,std::string>& owners,std::string& error) {
    const auto fail=[&](const char* cause){error=cause;return false;};
    if(!party_identity::valid(current))return fail("Character Actor Party owners require a completed current World");
    std::map<uintptr_t,std::string> additions;
    std::set<std::string> keys;
    std::set<uint64_t> uids;
    for(const auto& party:parties){
        if(party.binding.scope!=current)return fail("Character Actor Party alias belongs to another World lifetime");
        if(party.binding.retired)return fail("Character Actor Party alias is retired");
        if(!party.binding.pointer||!party.binding.uid||party.member.pointer!=party.binding.pointer||party.member.uid!=party.binding.uid)
            return fail("Character Actor Party copied member and actual full-UID binding differ");
        if(!party_identity::valid_key(party.key))return fail("Character Actor Party lacks an actual saved or original-birth canonical key");
        if(!keys.insert(party.key).second||!uids.insert(party.binding.uid).second||
           !additions.emplace(party.binding.pointer,party.key).second)
            return fail("Character Actor Party copied census repeats a key, pointer or full UID");
        const auto prior=owners.find(party.binding.pointer);
        if(prior!=owners.end()&&prior->second!=party.key)
            return fail("Character Actor Party pointer aliases another canonical owner");
    }
    owners.insert(additions.begin(),additions.end());
    error.clear();
    return true;
}
}
