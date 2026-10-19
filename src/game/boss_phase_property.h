#pragma once
#include "typed_action/boss_phase_action.h"
namespace mt2mp::game::boss_phase_property {
template<class Calls>
uint32_t apply(Calls& calls,uintptr_t image,uintptr_t phase,const action_session::BossPhaseIntent& intent,bool& returned){
    using Kind=action_session::BossPhaseKind;
    if(intent.kind==Kind::Reorder)return 0xE0010042;
    auto code=calls.event(image,phase,false);if(code)return code;
    switch(intent.kind){
    case Kind::Name:code=calls.text(image,phase+0x38,intent.text);break;
    case Kind::Health:code=calls.bytes(phase+0x58,&intent.health,sizeof intent.health);break;
    case Kind::Vfx:code=calls.text(image,phase+0x60,intent.text);break;
    case Kind::RemoveAbility:code=calls.remove(image,phase,intent.element);break;
    case Kind::Color:code=calls.bytes(phase+0x80,intent.color.data(),sizeof intent.color);break;
    default:return 0xE0010043;
    }
    if(code)return code;
    code=calls.event(image,phase,true);if(code)return code;
    returned=true;return 0;
}
template<class Calls>
uint32_t add(Calls& calls,uintptr_t image,uintptr_t phase,const std::vector<uintptr_t>& abilities,const action_session::BossPhaseIntent& intent,bool& returned){
    for(auto index:intent.ability_indices){
        if(index<0||size_t(index)>=abilities.size())return 0xE0010044;
        const auto code=calls.add(image,phase,abilities[size_t(index)]);if(code)return code;
    }
    returned=true;return 0;
}
}
