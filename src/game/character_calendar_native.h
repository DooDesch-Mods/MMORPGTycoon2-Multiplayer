#pragma once
#include "character_calendar_state.h"
#include "subscriber_demand_state.h"
#include "typed_action/action_session.h"
#include <map>
#include <memory>
namespace mt2mp::game::live_host_state {struct Pin;}
namespace mt2mp::game::character_calendar {
bool capture(const action_session::Context&,const action_session::Binding&,State&,std::string&);
bool capture_demands(const action_session::Context&,const action_session::Binding&,const live_host_state::Pin&,
                     State&,subscriber_demand::State&,std::string&);
// A read witness contains only local physical owners for paired comparison.
struct DemandReadWitness;
using DemandRead = std::shared_ptr<const DemandReadWitness>;
// Pair two fresh samples with same_demand_read before consuming their values.
bool capture_demands_once(const action_session::Context&,const action_session::Binding&,const live_host_state::Pin&,
                          State&,subscriber_demand::State&,DemandRead&,std::string&);
bool same_demand_read(const DemandRead&,const State&,const subscriber_demand::State&,
                      const DemandRead&,const State&,const subscriber_demand::State&,std::string&);
bool apply(const action_session::Context&,const action_session::Binding&,const State&,Receipt&,std::string&);
bool demand_keys(const action_session::Context&,const action_session::Binding&,std::map<uintptr_t,std::string>&,std::string&);
bool resolve_demand(const action_session::Context&,const action_session::Binding&,const std::string&,uintptr_t&,std::string&);
}
