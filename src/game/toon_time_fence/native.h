#pragma once
#include "normalization.h"
#include "live_host_state/apply_timing.h"
#include "../typed_action/action_session.h"
#include <array>

namespace mt2mp::game::toon_time_fence {
struct Owner {
    action_session::Context context;
    action_session::Binding binding;
    std::array<uintptr_t,5> origin{};
    std::string origin_uuid,origin_sha;
    uint64_t origin_bytes=0;
    uintptr_t subscriber=0,toon=0;
    int32_t subscriber_id=-1;
    bool operator==(const Owner&)const=default;
};
struct Callbacks {
    bool(*owner)(uintptr_t,Owner&,bool&,std::string&)=nullptr;
    void(*publish)(uintptr_t,const Receipt&)=nullptr;
    bool(*client_active)()=nullptr;
};
bool initialize(uintptr_t image,const Callbacks&,std::string&);
bool verify_hook(std::string&);
}
