#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace mt2mp::sync {

// Parsed game command captured via hardware breakpoint on EditManager
struct GameCommand {
    enum Type : uint8_t {
        UNKNOWN = 0,
        PLACE = 1,        // "Place Tavern:base"
        DESTROY = 2,      // "Single Destroy" / "Destroy Building"
        MOVE = 3,         // "Performing move of N objects"
        SUBEDIT = 4,      // "Multi Destroy" etc.
        SYSTEM = 5,       // "NewMapData", "Init", etc.
    };

    Type type = UNKNOWN;
    std::string action;        // raw action string (e.g. "Place Tavern:base")
    std::string entity_type;   // parsed entity type (e.g. "Tavern")
    std::string variant;       // parsed variant (e.g. "base", "orc")
    float x = 0, y = 0, z = 0; // world position (valid for PLACE)
    bool has_position = false;
    uint64_t source_steam_id = 0; // who sent this command
};

// Initialize: install hardware breakpoints on undo_msg_* LEAs
bool init();

// Set a callback that fires whenever a USER game action is captured.
// Called from MAIN THREAD (deferred from the VEH context).
using CommandCallback = std::function<void(const GameCommand& cmd)>;
void set_command_callback(CommandCallback cb);

// Call from MT2MP_Tick on the main thread to process queued commands
void process_pending_commands();

// Shutdown: remove breakpoints
void shutdown();

// Parse an action string into typed components
// "Place Tavern:base" → type=PLACE, entity="Tavern", variant="base"
GameCommand parse_action(const std::string& action_str);

} // namespace mt2mp::sync
