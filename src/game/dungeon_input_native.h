#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::game::dungeon_input_native {
enum class Tool:uint8_t {Door,Key,Monster,Paint,Treasure};
struct Receipt {
    bool entered=false,purchased=false,created=false,edge_bound=false,graph_created=false;
    bool paint_saved=false,painted=false,paint_finished=false,committed=false,confirmed=false,unknown=false;
    bool edit_opened=false,end_attempted=false,edit_closed=false;
    uintptr_t object=0,graph=0;uint32_t steps=0;std::string cause,cleanup_cause;
};
// Each Port method is one fixed original explicit-owner operation. The outer
// ordered action retains the receipt and owns the complete scene/ledger proof.
template<class Port>bool apply(Port& port,Tool tool,Receipt& receipt,std::string& error){
    if(receipt.entered){error="Dungeon input original operation already entered; replay is forbidden";return false;}
    if(uint8_t(tool)>uint8_t(Tool::Treasure)){error="Dungeon input tool is outside original Door/Key/Monster/Paint/Treasure";return false;}
    if(!port.preflight(tool,error))return false;
    receipt.entered=true;
    const auto cleanup=[&]{if(receipt.edit_opened&&!receipt.end_attempted){receipt.end_attempted=true;std::string cause;
        if(!port.end(cause))receipt.cleanup_cause=cause.empty()?"Dungeon input original EndEdit failed without cause":cause;
        else receipt.edit_closed=true;}};
    const auto failed=[&]{receipt.cause=error.empty()?"Dungeon input original call failed without cause":error;receipt.unknown=true;
        cleanup();error=receipt.cause;if(!receipt.cleanup_cause.empty())error+="; cleanup: "+receipt.cleanup_cause;return false;};
    if(tool==Tool::Door||tool==Tool::Key||tool==Tool::Treasure){++receipt.steps;
        if(!port.purchase(error))return failed();receipt.purchased=true;}
    ++receipt.steps;if(!port.begin(receipt.edit_opened,error))return failed();
    if(tool==Tool::Paint){
        ++receipt.steps;if(!port.graph(receipt.graph,error)||!receipt.graph)return failed();receipt.graph_created=true;
        ++receipt.steps;if(!port.snapshot_paint(receipt.graph,error))return failed();receipt.paint_saved=true;
        ++receipt.steps;if(!port.paint(error))return failed();receipt.painted=true;
        ++receipt.steps;if(!port.finish_paint(error))return failed();receipt.paint_finished=true;
    }else{
        ++receipt.steps;if(!port.create(tool,receipt.object,error)||!receipt.object)return failed();receipt.created=true;
        if(tool==Tool::Door){++receipt.steps;if(!port.edge(receipt.object,error))return failed();receipt.edge_bound=true;}
        ++receipt.steps;if(!port.graph(receipt.graph,error)||!receipt.graph)return failed();receipt.graph_created=true;
        // Door always records the original cash inverse, including a free Door.
        // Key records it only for a nonzero cost; Monster has no cash inverse.
        if(tool==Tool::Door||tool==Tool::Treasure){++receipt.steps;if(!port.cash(receipt.graph,error))return failed();}
        ++receipt.steps;if(!port.destroy_inverse(receipt.graph,receipt.object,error))return failed();
        if(tool==Tool::Key&&port.cost()!=0){++receipt.steps;if(!port.cash(receipt.graph,error))return failed();}
    }
    ++receipt.steps;if(!port.commit(receipt.graph,error))return failed();receipt.committed=true;
    cleanup();if(!receipt.cleanup_cause.empty()){error=receipt.cleanup_cause;return failed();}
    if(!port.readback(receipt,error))return failed();receipt.confirmed=true;receipt.unknown=false;error.clear();return true;
}
}
