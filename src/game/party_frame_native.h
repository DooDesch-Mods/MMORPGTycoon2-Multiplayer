#pragma once
#include "live_host_state/party_port.h"
#include <memory>

namespace mt2mp::game::party_frame {
std::unique_ptr<live_host_state::PartyPort> make_native(live_host_state::Port& authority);
}
