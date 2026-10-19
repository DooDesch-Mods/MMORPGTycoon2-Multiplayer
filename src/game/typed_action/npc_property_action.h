#pragma once
#include "npc_action.h"
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
inline bool npc_property_unchanged(const NpcPropertyState& before,const NpcPropertyIntent& intent){
    switch(intent.kind){
    case NpcPropertyKind::Name:return before.actor.created.name==intent.name;
    case NpcPropertyKind::OnlineState:return before.online==intent.online;
    case NpcPropertyKind::RaiseLevel:return before.command_level>=before.region_max;
    case NpcPropertyKind::LowerLevel:return before.command_level<=before.region_min;
    }
    return true;
}
}
