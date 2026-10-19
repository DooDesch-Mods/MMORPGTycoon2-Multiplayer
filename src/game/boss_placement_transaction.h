#pragma once
#include "boss_placement_reader.h"

namespace mt2mp::game::boss_placement_transaction {
struct Receipt {
    bool attempted=false,purchase_returned=false,purchased=false,region_returned=false,add_returned=false,confirmed=false,unknown=false;
    uintptr_t returned_boss=0;
    std::string cause;
};
// Preserve the native Cursor's core order. Purchase does not call AddBoss.
// Once a native call is attempted, an incomplete receipt cannot be replayed.
template<class Backend>
bool execute(Backend& backend,const action_session::BossPlacementIntent& intent,Receipt& receipt,std::string& error){
    if(receipt.attempted||receipt.confirmed||receipt.unknown)return npc_quest_targets::fail(error,"Boss original transaction was already attempted; no replay");
    if(!boss_placement_reader::valid(intent))return npc_quest_targets::fail(error,"Boss native transaction intent is invalid");
    if(!backend.prepare(intent,error))return false;
    auto failed=[&](){receipt.unknown=receipt.attempted;receipt.cause=error.empty()?"Boss original native transaction failed without a cause":error;error=receipt.cause;return false;};
    receipt.attempted=true;
    bool paid=false;if(!backend.purchase(intent,paid,error))return failed();receipt.purchase_returned=true;
    if(!paid){error="Boss original Purchase rejected affordability";return failed();}receipt.purchased=true;
    uintptr_t region=0;if(!backend.region(intent,region,error)||!region){if(error.empty())error="Boss original RegionAt returned no Region after Purchase";return failed();}receipt.region_returned=true;
    if(!backend.add(region,intent,receipt.returned_boss,error)||!receipt.returned_boss){if(error.empty())error="Boss original AddBoss returned no Boss after Purchase";return failed();}receipt.add_returned=true;
    if(!backend.confirm(receipt.returned_boss,error))return failed();
    receipt.confirmed=true;error.clear();return true;
}
}
