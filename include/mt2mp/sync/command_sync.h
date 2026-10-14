#pragma once

namespace mt2mp::sync {

// Wires CommandIntercept ↔ SteamP2P:
// - When local player does a placement → serialize → send via P2P to peers
// - When P2P receives a command → deserialize → log (replay later)
//
// For now: each player executes their OWN placements locally, AND broadcasts them.
// Receivers just log (replay implementation comes next).
bool init_command_sync();
void tick_command_sync();

} // namespace mt2mp::sync
