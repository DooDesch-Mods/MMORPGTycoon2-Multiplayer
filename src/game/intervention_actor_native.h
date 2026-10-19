#pragma once
#include "intervention_actor_state.h"
#include <cstdint>
namespace mt2mp::game::interventions::actors::native {
struct Render {uintptr_t character=0,actor=0;std::array<int32_t,animation_slots> indexes{};bool operator==(const Render&)const=default;};
uint32_t maximum(uintptr_t image,uintptr_t object,int32_t attribute,float& out)noexcept;
uint32_t clip(uintptr_t image,const Render&,int32_t semantic,Clip&)noexcept;
uint32_t clip(uintptr_t image,const Render&,int32_t semantic,const Clip&)noexcept;
uint32_t on_map(uintptr_t image,Family,uintptr_t object,bool value)noexcept;
uint32_t ready(uintptr_t image,uintptr_t object,bool value)noexcept;
uint32_t transform(uintptr_t image,uintptr_t object,const subscriber_motion::Pose&)noexcept;
uint32_t spatial(uintptr_t image,uintptr_t object)noexcept;
uint32_t registered(uintptr_t image,uint64_t uid,uintptr_t& object)noexcept;
uint32_t scene(uintptr_t image,uintptr_t screen,uintptr_t& scene)noexcept;
uint32_t present(uintptr_t image,Family,uintptr_t object,const Render&)noexcept;
uint32_t compare_pointer(uintptr_t address,uintptr_t expected,uintptr_t value,bool& exchanged)noexcept;
}
