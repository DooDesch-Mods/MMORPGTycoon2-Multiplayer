#pragma once
#include "selection_witness.h"

namespace mt2mp::build_actions::private_native {
struct UndoWitness {
    uintptr_t receiver=0,active_graph=0;
    std::vector<uintptr_t> undo,redo;
    bool operator==(const UndoWitness&)const=default;
};
struct GraphWitness {
    uintptr_t graph=0;
    int32_t nodes=0,operations=0,pending_delete=0;
    bool locked=false;
    bool operator==(const GraphWitness&)const=default;
};
bool read_undo(Memory&,uintptr_t image,UndoWitness&,std::string&);
bool read_graph(Memory&,uintptr_t image,uintptr_t graph,GraphWitness&,std::string&);
bool validate_undo_commit(const UndoWitness& before,const UndoWitness& after,
                          const GraphWitness&,std::string&);
}
