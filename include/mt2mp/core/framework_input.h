#pragma once
#ifdef MT2MP_FRAMEWORK_PLUGIN
#include <native_ownership.h>
#include <cstdint>
#include <string>

namespace mt2mp::framework_input {
struct State {
    bool subscribed = false;
    bool healthy = false;
    uint32_t owner_thread = 0;
    uint32_t event_bytes = 0;
    uint32_t sdl_version = 0;
    uint64_t subscription = 0;
    uint64_t generation = 0;
    uint64_t last_invocation = 0;
    uint64_t last_received_invocation = 0;
    uint64_t last_thread = 0;
    uint64_t received = 0;
    uint64_t forwarded = 0;
    uint64_t key_events = 0;
    uint64_t mouse_events = 0;
    uint64_t rejected = 0;
    uint64_t inactive_callbacks = 0;
    std::string error;
};

// Lifecycle calls use the actual Framework callback thread. Failed retirement
// retains its subscription so Core cannot unload an unresolved callback owner.
bool bind(const MT2FW_MT2_OwnershipV1& ownership, uint32_t thread, std::string& error);
bool unbind(std::string& error);
bool ready();
State read();
}
#endif
