#pragma once
#include "character_actor_records.h"
namespace mt2mp::game::intervention_records {
using External=npc_probe::character_actor_records::External;
struct Subscriber {
    npc_probe::character_actor_records::Result record;
};
// Capture the real current manager-owned Subscriber, including its complete
// original Toon/Brain tree. Native UIDs remain private capture guards.
bool subscriber(const action_session::Context&,const action_session::Binding&,int32_t,
                const External&,Subscriber&,std::string&);
bool zone(const action_session::Context&,const action_session::Binding&,uintptr_t,uint64_t,
          const std::string& canonical,const External&,npc_probe::character_actor_records::Result&,std::string&);
}
