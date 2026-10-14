#pragma once
#include "action_process.h"
#include "remove_permit.h"
#include "undo_witness.h"
#include <atomic>

namespace mt2mp::build_actions::private_native {
struct PropLease {uintptr_t object=0;uint64_t token=0;};
enum class Life {Alive,RetiredByThisCommit,Unknown};
struct RemoveServices {
    virtual ~RemoveServices()=default;
    virtual bool current(RemoveContext&,std::string&)=0;
    // Own native weak holders for every unique selection member and target.
    // Partial allocation ownership stays with the service until explicit retirement.
    virtual bool retain(const RemoveContext&,const std::vector<uintptr_t>&,std::vector<PropLease>&,std::string&)=0;
    virtual bool inspect(const RemoveContext&,const PropLease&,uintptr_t committed_graph,Life&,std::string&)=0;
    // Capture the graph created by this DestroyProp, including nested retirements.
    virtual bool observe_begin(const RemoveContext&,std::string&)=0;
    virtual bool observe_end(uintptr_t& graph,std::string&)=0;
};
struct RemoveResult {
    uint64_t sequence=0;
    bool attempted=false,returned=false,confirmed=false,unknown_locked=false;
    bool target_retired=false,undo_confirmed=false,selection_confirmed=false;
    size_t retained=0,retired=0;
    uint32_t seh_code=0;
    std::string error;
};
class RemoveBackend {
public:
    RemoveBackend(uintptr_t image,Memory& memory,RemoveServices& services,DestroyProp destroy,SetHighlightedProps highlights)
        :image_(image),memory_(memory),services_(services),destroy_(destroy),highlights_(highlights){}
    RemoveResult execute(uint64_t sequence,const RemoveContext&);
    // Bits: ClearSelection, Undo.Open, Graph.Lock, Prop destructor.
    void set_hook_mask(uint8_t mask)noexcept{hook_mask_.store(mask);}
    bool should_forward_clear(const RemoveContext&,uintptr_t receiver,uintptr_t caller_rva);
    const RemoveResult& status()const noexcept{return last_;}
    bool busy()const noexcept{return busy_;}
private:
    uintptr_t image_=0;Memory& memory_;RemoveServices& services_;
    DestroyProp destroy_=nullptr;SetHighlightedProps highlights_=nullptr;
    RemovePermit permit_;bool busy_=false,unknown_=false;
    std::atomic<uint8_t> hook_mask_{0};
    uint64_t high_watermark_=0;RemoveContext last_context_{};RemoveResult last_{};
};
}
