#pragma once
#include "object_identity/remove_runtime/copy/selection_witness.h"
#include <algorithm>

namespace mt2mp::game::move_selection_refresh {
using Selection=build_actions::private_native::SelectionWitness;
struct Receipt {
    bool required=false,attempted=false,returned=false,selection_preserved=false;
    uint32_t exception=0;
};
inline bool visible(const Selection& selection,uintptr_t target){
    const auto contains=[&](size_t index){const auto& list=selection.lists[index];return std::find(list.begin(),list.end(),target)!=list.end();};
    return (!selection.hide_selected_outline&&contains(2))||contains(3);
}
template<class Refresh,class Read>
bool retained(const Selection& before,const Selection& after,uintptr_t target,Receipt& receipt,
              Refresh&& refresh,Read&& read,std::string& error){
    if(receipt.attempted){error="move outline refresh already attempted; no replay";return false;}
    receipt.required=visible(before,target)&&visible(after,target);
    if(!receipt.required)return true;
    if(!target||!before.receiver||before.receiver!=after.receiver||after.selection_in_progress||after.queue_mode){
        error="move outline refresh selection boundary differs";return false;
    }
    // Native Move re-adds the same selected prop without rebuilding its cached
    // outline. Refresh only that retained visual, keeping the native selection.
    if(!refresh(receipt,error))return false;
    Selection current;
    if(!read(current,error))return false;
    if(current!=after){error="move outline refresh changed local selection";return false;}
    receipt.selection_preserved=true;return true;
}
}
