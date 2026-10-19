#pragma once
#include "intervention_actor_state.h"

namespace mt2mp::game::interventions::actors {
// The backend owns the physical actor, native lifecycle receipt and each
// point-of-use guard. This sequence never calls a free-running Actor update.
template<class Backend,class Receipt>
bool adopt(Backend& backend,const State& target,Receipt& receipt,std::string& error){
    State before;if(!backend.sample(before,error)||!preflight(before,target,error))return false;
    for(size_t i=0;i<target.size();++i)if(target[i].target.family==Family::Npc&&before[i].pose!=target[i].pose){error="Actor Frame NPC pose differs from the same final Economy frame key="+target[i].target.key;return false;}
    const auto failed=[&](){receipt.unknown=receipt.attempted;return false;};
    if(!backend.begin(before,target,receipt,error))return failed();
    for(size_t i=0;i<target.size();++i){const auto& first=before[i];const auto& wanted=target[i];
        if(!backend.lifecycle(first,wanted,receipt,error))return failed();
        const auto* old_monster=std::get_if<ZoneMonster>(&first.lifecycle);const auto* new_monster=std::get_if<ZoneMonster>(&wanted.lifecycle);
        if(old_monster&&old_monster->ready&&!new_monster->ready&&!backend.ready(first,wanted,false,receipt,error))return failed();
        if(first.on_map&&!wanted.on_map&&!backend.on_map(first,wanted,false,receipt,error))return failed();
        if(first.pose!=wanted.pose&&!backend.transform(first,wanted,receipt,error))return failed();
        if(!backend.scalars(first,wanted,receipt,error))return failed();
        if(!first.on_map&&wanted.on_map&&!backend.on_map(first,wanted,true,receipt,error))return failed();
        if(old_monster&&!old_monster->ready&&new_monster->ready&&!backend.ready(first,wanted,true,receipt,error))return failed();
        // Native Play resets a stopped clip. Stop aliases first, then Play and
        // apply the authoritative raw signed time, pause and speed.
        for(bool playing:{false,true})for(size_t semantic=0;semantic<animation_slots;++semantic){
            if(bool(wanted.clips[semantic].time)!=playing||first.clips[semantic]==wanted.clips[semantic])continue;
            if(!backend.animation(first,wanted,semantic,receipt,error))return failed();}
        if(!backend.present(first,wanted,receipt,error))return failed();
    }
    State after;if(!backend.sample(after,error))return failed();
    if(after!=target){error="Actor Frame complete native attributes/lifecycle/pose/animation readback differs";return failed();}
    if(!backend.finish(before,after,receipt,error))return failed();
    error.clear();return true;
}
}
