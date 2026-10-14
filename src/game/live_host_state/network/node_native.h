#pragma once
#include "node_state.h"
#include "live_host_state/state.h"
namespace mt2mp::game::live_host_state::network {
bool read_native(const Pin&,State&,std::string&);
bool preflight_native(live_host_state::Port&,const State&,std::string&);
Receipt apply_native(live_host_state::Port&,const State&);
}
