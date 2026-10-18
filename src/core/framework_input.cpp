#ifdef MT2MP_FRAMEWORK_PLUGIN
#include "mt2mp/core/framework_input.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/hooks/hook_input.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_version.h>
#include <Windows.h>
#include <cstring>
#include <exception>
#include <mutex>

namespace mt2mp::framework_input {
namespace {
static_assert(sizeof(SDL_Event) <= MT2FW_MT2_EVENT_COPY_BYTES);
struct LastErrorGuard {
    DWORD value = GetLastError();
    ~LastErrorGuard() { SetLastError(value); }
};
std::recursive_mutex gate;
MT2FW_MT2_OwnershipV1 service{};
State state;
bool dispatching = false;
bool subscribing = false;

void reject(const std::string& cause) {
    ++state.rejected;
    state.healthy = false;
    if (state.error.empty()) {
        state.error = cause;
        LOG_ERROR("[FrameworkInput] passive observer rejected cause={} invocation={} thread={} required_thread={} event_bytes={} required_bytes={} sdl_version={} required_version={}; no replay",
                  cause, state.last_received_invocation, state.last_thread, state.owner_thread,
                  state.event_bytes, sizeof(SDL_Event), state.sdl_version, SDL_VERSION);
    }
}

void MT2FW_CALL observed(void* user, const MT2FW_MT2_PolledEvent* call) {
    LastErrorGuard preserve;
    std::lock_guard lock(gate);
    ++state.received;
    if (user != &state || !state.subscribed || !state.healthy || subscribing) {
        ++state.inactive_callbacks;
        return;
    }
    if (!call || call->size < sizeof(MT2FW_MT2_PolledEvent)) {
        reject("input_callback_envelope_invalid");
        return;
    }
    state.event_bytes = call->event_bytes;
    state.sdl_version = call->sdl_version;
    state.last_thread = call->thread;
    state.last_received_invocation = call->invocation;
    if (!call->event || call->event_bytes != sizeof(SDL_Event)) {
        reject("input_event_copy_size_invalid");
        return;
    }
    if (call->sdl_version != SDL_VERSION) {
        reject("input_sdl_compile_version_mismatch");
        return;
    }
    if (call->thread != state.owner_thread || GetCurrentThreadId() != state.owner_thread) {
        reject("input_owner_thread_mismatch");
        return;
    }
    if (!call->invocation || call->invocation <= state.last_invocation) {
        reject("input_invocation_not_increasing");
        return;
    }
    if (dispatching) {
        reject("input_observer_reentered");
        return;
    }
    state.last_invocation = call->invocation;
    SDL_Event event{};
    // No borrowed event or pointer payload is stored past this callback.
    std::memcpy(&event, call->event, sizeof(event));
    dispatching = true;
    try {
        hooks::forward_framework_input_event(&event);
        ++state.forwarded;
        if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) ++state.key_events;
        if (event.type >= SDL_EVENT_MOUSE_MOTION && event.type <= SDL_EVENT_MOUSE_WHEEL) ++state.mouse_events;
    } catch (const std::exception& problem) {
        reject(std::string("input_existing_observer_failed: ") + problem.what());
    } catch (...) {
        reject("input_existing_observer_failed: non-standard exception");
    }
    dispatching = false;
}
}

bool bind(const MT2FW_MT2_OwnershipV1& ownership, uint32_t thread, std::string& error) {
    LastErrorGuard preserve;
    std::lock_guard lock(gate);
    if (state.subscribed || subscribing) { error = "input_subscription_already_owned"; return false; }
    if (!thread || GetCurrentThreadId() != thread) { error = "input_bind_owner_thread_mismatch"; return false; }
    if (ownership.size < sizeof(MT2FW_MT2_OwnershipV1) || ownership.version != MT2FW_MT2_OWNERSHIP_VERSION ||
        !ownership.context || !ownership.input_subscribe || !ownership.input_unsubscribe) {
        error = "input_ownership_extension_incomplete";
        return false;
    }
    const auto generation = state.generation + 1;
    state = {};
    state.generation = generation;
    state.owner_thread = thread;
    service = ownership;
    subscribing = true;
    const auto token = service.input_subscribe(service.context, &observed, &state);
    subscribing = false;
    if (!token) {
        state.error = "input_subscribe_returned_zero";
        error = state.error;
        service = {};
        return false;
    }
    state.subscription = token;
    state.subscribed = state.healthy = true;
    error.clear();
    return true;
}

bool unbind(std::string& error) {
    LastErrorGuard preserve;
    std::lock_guard lock(gate);
    if (!state.subscribed) { error.clear(); return true; }
    if (GetCurrentThreadId() != state.owner_thread) { error = "input_unbind_owner_thread_mismatch"; return false; }
    if (dispatching || subscribing) { error = "input_unbind_callback_inflight"; return false; }
    state.healthy = false;
    const auto result = service.input_unsubscribe(service.context, state.subscription);
    if (result != MT2FW_MT2_OK) {
        error = "input_unsubscribe_not_confirmed result=" + std::to_string(static_cast<int>(result));
        if (state.error.empty()) state.error = error;
        return false;
    }
    state.subscribed = false;
    state.subscription = 0;
    service = {};
    error.clear();
    return true;
}

bool ready() {
    LastErrorGuard preserve;
    std::lock_guard lock(gate);
    return state.subscribed && state.healthy;
}

State read() {
    LastErrorGuard preserve;
    std::lock_guard lock(gate);
    return state;
}
}
#endif
