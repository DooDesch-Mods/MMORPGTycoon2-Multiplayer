#pragma once
#include "remove_permit.h"
#include "undo_witness.h"

namespace mt2mp::build_actions::private_native {
struct GraphObservation {
    uintptr_t graph=0;
    bool active=false,in_lock=false,open_returned=false,lock_returned=false,complete=false,failed=false;
    uint64_t foreign=0,worker=0;
    std::string error;
};
// Hooks always call their original. These observations never select native behavior.
class RemoveObserver {
public:
    bool begin(const RemoveContext&,const std::vector<uintptr_t>& retained,std::string&);
    void undo_open_returned(const RemoveContext&,uintptr_t caller_rva,uintptr_t graph);
    void graph_lock_enter(const RemoveContext&,uintptr_t caller_rva,uintptr_t graph,Memory&,uintptr_t image);
    void prop_destructor_enter(const RemoveContext&,uintptr_t prop,Memory&,uintptr_t image);
    void graph_lock_returned(const RemoveContext&,uintptr_t graph,bool returned,Memory&,uintptr_t image);
    bool finish(uintptr_t& graph,std::string&);
    bool observed_destruction(uintptr_t prop)const;
    const GraphObservation& status()const noexcept{return state_;}
private:
    struct Member {uintptr_t object=0;bool destructor_entered=false;};
    RemoveContext bound_{};GraphObservation state_{};std::vector<Member> members_;bool busy_=false;
    bool scope(const RemoveContext&);
    void fail(const std::string&);
};
}
