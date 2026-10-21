#pragma once
#include "dungeon_gizmo_control.h"
#include "duplicate_native.h"
namespace mt2mp::game::dungeon_gizmo_control_native {
struct Receipt {
    bool attempted=false,confirmed=false,unknown=false;
    uintptr_t graph=0;duplicate_native::Receipt duplicate;
    std::string cause;
};
// A control uses one existing original transaction. Partial original effects
// retain their receipt and cannot be retried as a fresh operation.
template<class Port>bool execute(Port& port,const dungeon_gizmo_control::Intent& intent,Receipt& receipt,std::string& error){
    if(receipt.attempted){error="Gizmo control already entered; replay forbidden";return false;}
    if(!dungeon_gizmo_control::valid(intent)||!port.preflight(error))return false;
    receipt.attempted=true;
    const auto failed=[&]{receipt.unknown=true;receipt.cause=error.empty()?"Gizmo control original failed without cause":error;error=receipt.cause;return false;};
    using Command=dungeon_gizmo_control::Command;
    if(intent.command==Command::Move){if(!port.move(receipt.graph,error))return failed();}
    else if(intent.command==Command::Destroy){if(!port.destroy(receipt.graph,error))return failed();}
    else{
        action_session::DuplicateIntent original;original.cash_position=intent.cash_position;
        for(const auto& row:intent.rows){action_session::DuplicateTarget target;
            target.key=row.key;target.after=row.after;target.cost=row.cost;original.rows.push_back(std::move(target));}
        if(!duplicate_native::execute(port,original,port.sources,receipt.duplicate,error))return failed();
        receipt.graph=receipt.duplicate.graph;
    }
    if(!receipt.graph||!port.complete(receipt.graph,error))return failed();
    receipt.confirmed=true;error.clear();return true;
}
}
