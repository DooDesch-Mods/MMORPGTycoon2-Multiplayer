#pragma once
#include "placement_adapter.h"
#include <cstddef>

namespace placement_kiss {
using Update = void(__fastcall*)(void*,float);
enum class Capture { PassOriginal, Captured, BlockedKnownCommit };
class NativeBuild {
public:
    bool bind_before_hooks(std::uintptr_t image, std::uint32_t root_main_thread, std::string& cause);
    bool publish_typed_original(Update trampoline, std::string& cause);
    Capture capture_at_update_entry(void* receiver, Request& scalar_output, std::string& cause) const;
    bool consume_captured_intent(void* receiver, const Request& captured_scalars, std::uint32_t& exception, std::string& cause) const;
    bool prepare(const Request&, void* actual_shard, PrivateBuild&, std::string& cause) const;
    bool update_once(PrivateBuild&, std::uint32_t& exception) const;
    bool destroy_once(PrivateBuild&, std::uint32_t& exception) const;
private:
    std::uintptr_t image_{};
    std::uint32_t thread_{};
    Update original_{};
};
}
