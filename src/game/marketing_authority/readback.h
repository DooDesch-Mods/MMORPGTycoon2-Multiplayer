#pragma once
#include "state.h"
#include "subscriber_lifecycle/record_graph.h"
namespace mt2mp::game::marketing_authority {
using Memory=mt2mp::subscriber_lifecycle::Memory;
struct Snapshot {
    uintptr_t marketing=0,hud=0,window=0,clock=0;
    std::vector<uintptr_t> awards,campaigns,views,panes;
    Json values,bindings;
    uint32_t width=0,height=0;
    bool operator==(const Snapshot&)const=default;
};
bool read_snapshot(Memory&,uintptr_t image,uintptr_t state,uintptr_t mode,Snapshot&,std::string&);
bool panes_match(const Snapshot&,std::string&);
Json report(const Snapshot&);
}
