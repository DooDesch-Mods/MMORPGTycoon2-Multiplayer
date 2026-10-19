#pragma once
#include "intervention_actor_state.h"

namespace mt2mp::game::interventions::actors {
// Status values can change the derived native attribute maxima. Complete that
// group before copying current values, and never create or replace native Items.
template<class Backend,class Receipt>
bool adopt_saved(Backend& backend,const Row& first,const Row& target,Receipt& receipt,std::string& error){
    for(size_t n=0;n<target.status_effects.size();++n)
        if(!backend.status_effect(first,n,target.status_effects[n],receipt,error))return false;
    if(!backend.maxima(first,target.maxima,error))return false;
    for(size_t n=0;n<target.abilities.size();++n)
        if(!backend.item(first,n,target.abilities[n],receipt,error))return false;
    return true;
}
}
