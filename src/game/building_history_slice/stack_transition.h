#pragma once

#include "object_identity/remove_runtime/copy/undo_witness.h"
#include <algorithm>
#include <string>
#include <vector>

namespace mt2mp::game::building_history_slice {
namespace undo=mt2mp::build_actions::private_native;

enum class Direction {Undo,Redo};

// Check one exact native effect. This does not call the game or retry a mutation.
inline bool transition(Direction direction,uintptr_t expected_top,
                       const undo::UndoWitness& before,const undo::UndoWitness& after,
                       const undo::GraphWitness& inverse,std::string& error) {
    if(!before.receiver||before.receiver!=after.receiver) {
        error="building_history_undo_receiver_changed";return false;
    }
    if(before.active_graph||after.active_graph) {
        error="building_history_active_graph";return false;
    }
    const auto& source_before=direction==Direction::Undo?before.undo:before.redo;
    const auto& source_after=direction==Direction::Undo?after.undo:after.redo;
    const auto& other_before=direction==Direction::Undo?before.redo:before.undo;
    const auto& other_after=direction==Direction::Undo?after.redo:after.undo;
    if(!expected_top||source_before.empty()||source_before.back()!=expected_top) {
        error="building_history_target_not_local_stack_top";return false;
    }
    auto expected_source=source_before;
    expected_source.pop_back();
    if(source_after!=expected_source) {
        error="building_history_source_stack_differs";return false;
    }
    if(!inverse.graph||!inverse.locked||inverse.pending_delete||
       std::find(other_before.begin(),other_before.end(),inverse.graph)!=other_before.end()) {
        error="building_history_inverse_graph_invalid";return false;
    }
    auto expected_other=other_before;
    expected_other.push_back(inverse.graph);
    if(expected_other.size()>20)expected_other.erase(expected_other.begin(),expected_other.end()-20);
    if(other_after!=expected_other) {
        error="building_history_inverse_stack_differs";return false;
    }
    error.clear();return true;
}
}
