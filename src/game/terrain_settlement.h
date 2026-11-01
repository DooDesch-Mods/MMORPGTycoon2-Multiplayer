#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

namespace mt2mp::game::terrain_settlement {
struct State {int32_t boxes=0,gpu=0;bool queued=false,plots=false;};
struct Receipt {uint64_t elapsed_ms=0,pumps=0,batches=0,plots=0,publications=0;};
inline bool idle(const State& s){return !s.boxes&&s.gpu==5&&!s.queued&&!s.plots;}
template<class Port>Receipt drain(Port& port){
    Receipt receipt;const auto start=port.now();
    for(;;){
        port.held();const auto state=port.state();receipt.elapsed_ms=port.now()-start;
        if(idle(state))return receipt;
        if(receipt.elapsed_ms>=5000||receipt.batches>=16||receipt.pumps>=100000)
            throw std::runtime_error("Terrain settlement limit: elapsed_ms="+std::to_string(receipt.elapsed_ms)+
                " batches="+std::to_string(receipt.batches)+" pumps="+std::to_string(receipt.pumps)+
                " boxes="+std::to_string(state.boxes)+" gpu="+std::to_string(state.gpu)+" queued="+std::to_string(state.queued)+" plots="+std::to_string(state.plots));
        // Finish a prior GPU publication before merging a new invalidation.
        // Original Plot can enqueue another publication of its own.
        if(state.gpu==5&&!state.queued){
            receipt.plots+=port.plot_queued();
            if(port.state().boxes){port.consume_boxes();++receipt.batches;}
        }
        port.pump();++receipt.pumps;
        port.yield();
    }
}
}
