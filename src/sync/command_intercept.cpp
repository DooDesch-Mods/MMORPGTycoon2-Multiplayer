#include "mt2mp/sync/command_intercept.h"
#include "mt2mp/hooks/hw_breakpoint.h"
#include "mt2mp/hooks/pattern_scanner.h"
#include "mt2mp/core/logger.h"

#include <windows.h>
#include <cstring>
#include <atomic>
#include <mutex>
#include <vector>

namespace mt2mp::sync {

static CommandCallback g_callback;
static std::mutex g_queue_mutex;
static std::vector<GameCommand> g_pending_commands;

void set_command_callback(CommandCallback cb) {
    g_callback = std::move(cb);
}

GameCommand parse_action(const std::string& action) {
    GameCommand cmd;
    cmd.action = action;

    if (action.empty()) {
        cmd.type = GameCommand::UNKNOWN;
        return cmd;
    }

    // Detect type from prefix
    if (action.compare(0, 6, "Place ") == 0) {
        cmd.type = GameCommand::PLACE;
        // "Place Tavern:base" → entity="Tavern", variant="base"
        std::string rest = action.substr(6);
        size_t colon = rest.find(':');
        if (colon != std::string::npos) {
            cmd.entity_type = rest.substr(0, colon);
            cmd.variant = rest.substr(colon + 1);
        } else {
            cmd.entity_type = rest;
        }
    } else if (action.compare(0, 8, "Destroy ") == 0 ||
               action == "Single Destroy" ||
               action == "Multi Destroy") {
        cmd.type = GameCommand::DESTROY;
    } else if (action.compare(0, 10, "Performing") == 0) {
        cmd.type = GameCommand::MOVE;
    } else if (action == "NewMapData" || action == "Init" ||
               action == "MapApplyPatches" ||
               action.find("PostLoad") != std::string::npos ||
               action.find("NavMeshTask") != std::string::npos) {
        cmd.type = GameCommand::SYSTEM;
    } else {
        cmd.type = GameCommand::SUBEDIT;
    }

    return cmd;
}

// Helper: read a std::string object from a register value (MinGW libstdc++ layout)
static bool read_std_string(uintptr_t obj_ptr, std::string& out) {
    out.clear();
    if (obj_ptr < 0x10000) return false;

    char buf[512] = {};
    size_t len_actual = 0;
    bool ok = false;

    __try {
        char* data = *(char**)obj_ptr;
        size_t len = *(size_t*)(obj_ptr + 8);
        if (data && len > 0 && len <= 511) {
            for (size_t i = 0; i < len; i++) {
                uint8_t c = (uint8_t)data[i];
                if (c < 0x09 || c >= 0x7F) { len = i; break; }
                buf[i] = c;
            }
            len_actual = len;
            buf[len] = 0;
            ok = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }

    if (ok && len_actual > 0) {
        out.assign(buf, len_actual);
    }
    return ok;
}

// Helper: safely read a float from memory
static bool read_float(uintptr_t addr, float& out) {
    out = 0;
    __try {
        out = *(float*)addr;
        // Reject NaN/Inf
        uint32_t bits = *(uint32_t*)addr;
        if ((bits & 0x7F800000) == 0x7F800000) return false;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Hardware breakpoint callback - runs in VEH context, must be FAST and SAFE
// We just READ data and queue it; processing happens on main thread tick.
static void on_edit_breakpoint(const hooks::HwBpContext& ctx) {
    // Read action string from R8 (std::string object)
    std::string action;
    if (!read_std_string(ctx.r8, action)) {
        // R8 wasn't a valid std::string - try RBX as fallback
        if (!read_std_string(ctx.rbx, action)) return;
    }

    if (action.empty()) return;

    GameCommand cmd = parse_action(action);

    // Only queue interesting USER actions (PLACE, DESTROY, MOVE)
    // Skip SYSTEM and SUBEDIT noise
    if (cmd.type != GameCommand::PLACE &&
        cmd.type != GameCommand::DESTROY &&
        cmd.type != GameCommand::MOVE) {
        return;
    }

    // For PLACE, read position from EditManager (RCX+0x94)
    if (cmd.type == GameCommand::PLACE && ctx.rcx > 0x10000) {
        float x = 0, y = 0, z = 0;
        if (read_float(ctx.rcx + 0x94, x) &&
            read_float(ctx.rcx + 0x98, y) &&
            read_float(ctx.rcx + 0x9C, z)) {
            // Sanity check: typical world coords are -50000 to +50000
            if (x > -50000 && x < 50000 && z > -50000 && z < 50000) {
                cmd.x = x; cmd.y = y; cmd.z = z;
                cmd.has_position = true;
            }
        }
    }

    // Queue for main thread processing (Steam API not safe in VEH)
    {
        std::lock_guard<std::mutex> lock(g_queue_mutex);
        g_pending_commands.push_back(std::move(cmd));
    }
}

// Called from main thread tick to process queued commands
void process_pending_commands() {
    std::vector<GameCommand> commands;
    {
        std::lock_guard<std::mutex> lock(g_queue_mutex);
        commands.swap(g_pending_commands);
    }

    for (const auto& cmd : commands) {
        // Log the captured command
        if (cmd.type == GameCommand::PLACE && cmd.has_position) {
            LOG_INFO("[CMD] PLACE \"{}\" variant=\"{}\" pos=({:.1f},{:.1f},{:.1f})",
                cmd.entity_type, cmd.variant, cmd.x, cmd.y, cmd.z);
        } else if (cmd.type == GameCommand::DESTROY) {
            LOG_INFO("[CMD] DESTROY \"{}\"", cmd.action);
        } else if (cmd.type == GameCommand::MOVE) {
            LOG_INFO("[CMD] MOVE \"{}\"", cmd.action);
        } else {
            LOG_INFO("[CMD] OTHER \"{}\"", cmd.action);
        }

        if (g_callback) {
            g_callback(cmd);
        }
    }
}

bool init() {
    if (!hooks::hw_bp_init()) return false;

    // Find the LEA that loads "==> EDIT BEGINS: %s" and set BP at +0x0A
    // (the first CALL instruction after the LEA, where R8/RBX hold the std::string args)
    uintptr_t lea = hooks::find_lea_for_string("==> EDIT BEGINS:");
    if (!lea) {
        LOG_ERROR("[CMD] Could not find EDIT BEGINS LEA");
        return false;
    }

    uintptr_t bp_addr = lea + 0x0A;
    if (!hooks::hw_bp_set(0, bp_addr, on_edit_breakpoint, "EDIT_BEGINS")) {
        LOG_ERROR("[CMD] Failed to set HW breakpoint on EDIT BEGINS");
        return false;
    }

    LOG_INFO("[CMD] CommandIntercept initialized (BP at 0x{:X})", bp_addr);
    return true;
}

void shutdown() {
    hooks::hw_bp_clear(0);
}

} // namespace mt2mp::sync
