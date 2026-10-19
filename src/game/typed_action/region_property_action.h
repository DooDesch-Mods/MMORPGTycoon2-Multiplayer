#pragma once
#include "region_action.h"
#include <array>
#include <cstdint>
#include <string>
#include <optional>
#include <algorithm>

namespace mt2mp::action_session {
enum class RegionPropertyKind:uint8_t { Name=1,Type=2,Purchase=3,LevelRange=4, Tint=5,Light=6,Ambient=7,Sky=8,Particles=9,ParticleColor=10,Sale=11 };
struct RegionAppearance {
    std::array<float,4> tint{},light{},ambient{},particle_color{};
    std::array<float,16> sky{};
    int32_t particles=0;
    bool particles_enabled=false;
    bool operator==(const RegionAppearance&) const=default;
};
inline bool appearance_kind(RegionPropertyKind k){return k>=RegionPropertyKind::Tint&&k<=RegionPropertyKind::ParticleColor;}
struct RegionPropertyIntent {
    RegionPropertyKind kind=RegionPropertyKind::Name;
    int32_t id=-1;
    std::string name;
    int32_t type=-1;
    std::array<float,3> hit{};
    std::array<int32_t,2> levels{-1,-1};
    std::array<float,16> appearance_value{};
    bool operator==(const RegionPropertyIntent&) const=default;
};
inline bool appearance_matches(const RegionPropertyIntent& intent,const RegionAppearance& state){
    const std::array<float,4>* color=nullptr;
    switch(intent.kind){
    case RegionPropertyKind::Tint:color=&state.tint;break;
    case RegionPropertyKind::Light:color=&state.light;break;
    case RegionPropertyKind::Ambient:color=&state.ambient;break;
    case RegionPropertyKind::ParticleColor:color=&state.particle_color;break;
    case RegionPropertyKind::Sky:return intent.appearance_value==state.sky;
    case RegionPropertyKind::Particles:return intent.appearance_value[0]==float(state.particles)&&state.particles_enabled==(state.particles!=0);
    default:return false;
    }
    return std::equal(color->begin(),color->end(),intent.appearance_value.begin());
}
struct RegionLevelState {
    std::array<uint64_t,3> counts{};
    std::string roster_sha256,levels_sha256,expected_sha256;
    std::array<uint64_t,4> global_random{};
    bool operator==(const RegionLevelState&) const=default;
};
struct RegionPropertyState {
    RegionState content;
    std::array<uint64_t,4> type_random{};
    std::string name;
    int32_t type=-1,status=-1,level=-1,max_level=-1;
    bool owned=false;
    std::optional<RegionLevelState> actors;
    std::optional<RegionAppearance> appearance;
    uint64_t building_count=0,path_count=0,path_node_count=0,network_node_count=0,network_pipe_count=0;
    bool operator==(const RegionPropertyState&) const=default;
};
}
