#pragma once
#include "intervention_actor_read.h"
#include "typed_action/action_session.h"
#include <map>
#include <memory>
namespace mt2mp::game::interventions::actors {
using Locals=std::map<std::string,Local>;
struct Receipt {bool attempted=false,confirmed=false,unknown=false;uint32_t attempted_stores=0,stores=0;std::string cause;};
// Current native memory and renderer leaves only. This does not enter SaveScope
// or call an Actor PrepareSave/Save/Load method.
// Empty samples may share their complete physical pair with an outer read-only bracket.
struct EmptyReadWitness;
using EmptyRead=std::shared_ptr<const EmptyReadWitness>;
bool same_empty_read(const EmptyRead&,const EmptyRead&,std::string&);
bool read_current(const action_session::Context&,const action_session::Binding&,State&,Locals&,std::string&,EmptyRead* empty_pair=nullptr);
bool read_bound(const Local&,Row&,Local&,std::string&);
// The caller first validates its authoritative Frame or confirmed HostResult.
// This is the same physical writer used by Management Frame24, including native
// map membership, pose, lifecycle scalar, clip and complete current readback.
bool adopt_current(const action_session::Context&,const action_session::Binding&,const State&,Receipt&,std::string&);
}
