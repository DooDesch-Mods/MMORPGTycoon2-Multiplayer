#pragma once
#include "npc_action.h"
#include <algorithm>
#include <optional>
namespace mt2mp::action_session {
enum class NpcPropertyKind:uint8_t { Name=1,OnlineState=2,RaiseLevel=3,LowerLevel=4 };
struct NpcPropertyIntent {
    NpcPropertyKind kind=NpcPropertyKind::Name;
    std::string name;
    int32_t online=0;
    bool operator==(const NpcPropertyIntent&) const = default;
};
struct NpcPropertyState {
    NpcState actor;
    int32_t online=0;
    bool force_tint=false;
    std::array<float,4> tint{};
    int32_t command_level=0,region_min=0,region_max=0;
    bool operator==(const NpcPropertyState&) const = default;
};
inline int32_t npc_property_level(const NpcPropertyState& before,NpcPropertyKind kind){
    const int64_t requested=int64_t(before.command_level)+(kind==NpcPropertyKind::RaiseLevel?1:-1);
    return int32_t(std::clamp(requested,int64_t(before.region_min),int64_t(before.region_max)));
}
inline bool npc_property_unchanged(const NpcPropertyState& before,const NpcPropertyIntent& intent){
    switch(intent.kind){
    case NpcPropertyKind::Name:return before.actor.created.name==intent.name;
    case NpcPropertyKind::OnlineState:return before.online==intent.online;
    case NpcPropertyKind::RaiseLevel:
    case NpcPropertyKind::LowerLevel:return before.command_level==npc_property_level(before,intent.kind);
    }
    return true;
}
}
