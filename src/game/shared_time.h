#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

namespace mt2mp::game::shared_time {

enum class Intent { Pause, Normal, Fast, Fastest, TogglePauseNormal };
enum class Origin { Mouse, Keyboard };
enum class Decision { Forward, Suppress };
struct Context {
    uint64_t epoch = 0;
    bool client = false;
    bool host = false;
};
// Memory callbacks must bound accesses, preserve failures, and never throw.
struct Backend {
    void* user = nullptr;
    bool (*read)(void*, uintptr_t, void*, size_t, std::string&) = nullptr;
    bool (*compare_write)(void*, uintptr_t, int32_t, int32_t, std::string&) = nullptr;
    uint32_t (*thread_id)(void*) = nullptr;
    bool (*context)(void*, Context&, std::string&) = nullptr;
};
struct Config {
    uintptr_t image = 0;
    uint32_t game_thread = 0;
    // Receipt from the existing native resolver after hashing the loaded executable.
    std::string verified_executable_sha256;
};
struct ItemState { float scale = 0; int32_t native_state = 0; bool active = false; };
struct Presentation {
    uint64_t epoch = 0, revision = 0;
    bool host_state_available = false, capture_enabled = false;
    bool local_host = false;
    float selected_scale = 0, pause_scale = 0, effective_scale = 0;
    std::array<ItemState, 4> items{};
};
struct WriteReceipt {
    uint32_t attempted = 0, completed = 0;
    bool partial_or_uncertain = false, readback_confirmed = false;
    Presentation before{}, after{};
};
struct Request {
    uint64_t epoch = 0, sequence = 0, observed_host_revision = 0;
    Intent intent = Intent::Pause;
    Origin origin = Origin::Mouse;
};
struct HookResult {
    Decision decision = Decision::Forward;
    bool matched = false, origin_valid = false;
    bool request_queued = false, presentation_confirmed = false;
    Intent intent = Intent::Pause;
    Origin origin = Origin::Mouse;
    std::string error;
};
struct Counters {
    uint64_t captured = 0, queued = 0, cancelled_on_unbind = 0, queue_full = 0, rejected = 0;
    uint64_t presentation_confirmed = 0, presentation_failed = 0, host_original_fallback = 0;
    std::string last_error;
};

class Bridge {
public:
    Bridge() = default;
    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;
    static constexpr uintptr_t activate_rva = 0x20e6d0;
    static constexpr uintptr_t update_rva = 0x20e8d0;
    // Hooks use void(type*,item*) and void(type*,item*,float), respectively.
    // Initialize before either hook changes the original prologue.
    bool initialize(const Config&, const Backend&, std::string& error);
    bool bind(uint64_t epoch, std::string& error);
    bool unbind(std::string& error);
    bool read(Presentation&, std::string& error);
    bool set_host_state(uint64_t epoch, uint64_t revision, float selected_scale,
                        float pause_scale, std::string& error);
    // Host state validation and capture admission share one callback-free scan.
    bool set_host_state_and_enable_capture(uint64_t epoch, uint64_t revision, float selected_scale,
                                           float pause_scale, uint32_t installed_hook_mask, std::string& error);
    bool write_presentation(WriteReceipt&, std::string& error);
    // Client requires mask3; Host requires Activate bit0 and keeps Update original.
    bool enable_capture(uint32_t installed_hook_mask, std::string& error);
    bool disable_capture(std::string& error);
    // A bound but disabled bridge observes valid origins and keeps Original enabled.
    HookResult on_activate(void* type, void* item, uintptr_t caller_pc);
    HookResult on_update(void* type, void* item);
    bool pop_request(Request&, bool& available, std::string& error);
    bool counters(Counters&, std::string& error);
    static bool resolve(Intent, float host_time_scale, float host_pause_scale,
                        float& target, std::string& error);
private:
    struct Handle { uintptr_t item=0, type=0; float scale=0; int32_t state=0; };
    Backend io_{};
    Config config_{};
    bool ready_=false, bound_=false, capture_=false, host_available_=false, uncertain_=false;
    bool bound_host_=false, observed_host_=false;
    float observed_scale_=0, observed_pause_=0;
    uint64_t epoch_=0, revision_=0, sequence_=0;
    uintptr_t world_=0, player_=0, inventory_=0;
    float selected_=0, pause_=0;
    std::array<Handle,4> handles_{};
    std::deque<Request> queue_;
    Counters counters_{};
    bool thread(std::string&) const;
    bool live(uintptr_t& world, uintptr_t& player, uintptr_t& inventory, std::string&);
    bool scan(std::array<Handle,4>&, bool require_bound, std::string&);
    bool bytes(uintptr_t, void*, size_t, std::string&) const;
    template<class T> bool at(uintptr_t p, size_t offset, T& v, std::string& e) const {
        if (!p || p > UINTPTR_MAX-offset) { e="null_or_overflow_address"; return false; }
        return bytes(p+offset,&v,sizeof(v),e);
    }
    bool vtable(uintptr_t, uintptr_t, const char*, std::string&) const;
    void describe(const std::array<Handle,4>&, Presentation&) const;
    HookResult reject(HookResult, const std::string&);
};

#ifdef _WIN32
// Parent owns context lifetime; its reader supplies the authoritative local epoch/role.
Backend process_backend(void* context_user,
    bool (*context_reader)(void*, Context&, std::string&));
#endif
}
