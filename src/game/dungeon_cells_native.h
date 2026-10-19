#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::game::dungeon_cells_native {
enum class Edit:uint8_t {Close,Open,Theme,Colors};
struct Cell {int32_t index=-1;Edit edit=Edit::Close;std::string theme;bool operator==(const Cell&)const=default;};
struct Receipt {
    bool entered=false,started=false,finished=false,committed=false,active=false,closed=false;
    bool active_cleanup_attempted=false,end_cleanup_attempted=false,marker_cleanup_attempted=false;
    uintptr_t graph=0;uint32_t calls=0,changes=0,drains=0;std::string cause,cleanup_cause;
};
// Port calls are the exact explicit-Dungeon native functions. Failure after
// entry is an unknown result: retain the original graph and never replay it.
template<class Port>bool apply(Port& port,const std::vector<Cell>& cells,Receipt& receipt,std::string& error){
    if(receipt.entered){error="Dungeon cell native transaction already entered; replay is forbidden";return false;}
    if(cells.empty()){error="Dungeon cell native transaction has no changed cells";return false;}
    if(!port.ready(cells,error))return false;
    receipt.entered=true;
    const auto cleanup=[&]{
        std::string cause;
        if(receipt.active&&!receipt.active_cleanup_attempted){receipt.active_cleanup_attempted=true;
            if(!port.active(0,cause)){receipt.cleanup_cause="clear original active graph: "+cause;}
            else receipt.active=false;
        }
        if(receipt.started&&!receipt.end_cleanup_attempted){receipt.end_cleanup_attempted=true;cause.clear();
            if(!port.end(cause)){if(!receipt.cleanup_cause.empty())receipt.cleanup_cause+="; ";receipt.cleanup_cause+="end original edit: "+cause;}
            else receipt.closed=true;
        }
        if(!receipt.marker_cleanup_attempted){receipt.marker_cleanup_attempted=true;cause.clear();
            if(!port.release_marker(cause)){if(!receipt.cleanup_cause.empty())receipt.cleanup_cause+="; ";receipt.cleanup_cause+="release original edit marker: "+cause;}}
    };
    const auto failed=[&]{receipt.cause=error.empty()?"Dungeon cell native operation failed without cause":error;cleanup();error=receipt.cause;
        if(!receipt.cleanup_cause.empty())error+="; cleanup: "+receipt.cleanup_cause;return false;};
    if(!port.start(receipt.graph,receipt.started,error))return failed();
    if(!receipt.graph){error="Original Dungeon _StartEdit returned no owned graph";return failed();}
    receipt.active=true;if(!port.active(receipt.graph,error))return failed();
    for(const auto& cell:cells){++receipt.calls;bool changed=false;
        if(!port.cell(cell,changed,error))return failed();
        if(changed){++receipt.changes;++receipt.drains;if(!port.drain(receipt.graph,error))return failed();}
    }
    if(!receipt.changes){error="Dungeon original cell transaction completed without a change";return failed();}
    if(!port.finish(error))return failed();receipt.finished=true;
    receipt.active_cleanup_attempted=true;if(!port.active(0,error))return failed();receipt.active=false;
    cleanup();
    if(!receipt.cleanup_cause.empty()){error=receipt.cleanup_cause;return false;}
    if(!port.commit(receipt.graph,error))return failed();receipt.committed=true;
    error.clear();return true;
}
}
