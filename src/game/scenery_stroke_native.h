#pragma once
#include "typed_action/scenery_stroke_action.h"
#include <exception>

namespace mt2mp::game::scenery_stroke_native {
struct Receipt {
    bool attempted=false,prepare_entered=false,prepared=false,context_entered=false,edit_open=false;
    bool finish_entered=false,finish_returned=false,graph_committed=false;
    bool context_restored=false,selection_restored=false,cursor_destroyed=false,resources_released=false;
    size_t stamp_calls=0,stamp_returns=0;
    uintptr_t graph=0;
    std::string cleanup_cause;
};
// Each stamp uses the native AddPaint and the original final Finish commits
// their one graph. A partial mutation is retained as Unknown and never replayed.
template<class Port>bool execute(Port& port,const action_session::SceneryStrokeIntent& intent,Receipt& receipt,std::string& error){
    auto fail=[&](const std::string& cause){error=cause;return false;};
    if(receipt.attempted)return fail("Scenery stroke native execution already attempted; no replay");
    if(intent.stamps.empty())return fail("Scenery stroke native execution has no complete stamps");
    if(!port.ready(intent,error))return false;
    auto cleanup=[&](){
        auto step=[&](const char* name,auto&& invoke,bool& done){
            std::string cause;bool okay=false;
            try{okay=invoke(cause);}catch(const std::exception& ex){cause=ex.what();}
            if(okay){done=true;return;}
            if(cause.empty())cause="operation failed without cause";
            if(!receipt.cleanup_cause.empty())receipt.cleanup_cause+="; ";receipt.cleanup_cause+=std::string(name)+": "+cause;
        };
        if(receipt.context_entered)step("native context restore",[&](auto& e){return port.restore_context(e);},receipt.context_restored);
        if(receipt.prepare_entered){
            if(receipt.edit_open&&!receipt.attempted){bool closed=false;
                step("original empty stroke Finish",[&](auto& e){return port.finish_empty(e);},closed);
            }
            step("local selection restore",[&](auto& e){return port.restore_selection(e);},receipt.selection_restored);
            // A cursor that may still own an uncommitted graph must remain
            // available to diagnostics. Do not destroy an uncertain graph.
            if(!receipt.attempted||receipt.finish_returned)step("native cursor cleanup",[&](auto& e){return port.destroy_cursor(e);},receipt.cursor_destroyed);
        }
        step("untransferred native resource cleanup",[&](auto& e){return port.release_resources(e);},receipt.resources_released);
        if(!receipt.cleanup_cause.empty()){
            if(error.empty())error=receipt.cleanup_cause;else error+="; cleanup: "+receipt.cleanup_cause;
        }
    };
    try{
        receipt.prepare_entered=true;if(!port.prepare(intent,error)){cleanup();return false;}receipt.prepared=true;
        // enter_context records its restorable original values before a write.
        if(!port.enter_context(receipt.context_entered,error)){cleanup();return false;}
        if(!port.begin_edit(receipt.edit_open,error)){cleanup();return false;}
        for(size_t index=0;index<intent.stamps.size();++index){
            if(!port.before_stamp(index,error)){cleanup();return false;}
            receipt.attempted=true;++receipt.stamp_calls;
            if(!port.add_paint(index,receipt.graph,error)){cleanup();return false;}++receipt.stamp_returns;
            if(!port.after_stamp(index,receipt.graph,error)){cleanup();return false;}
        }
        if(!port.finish(receipt.graph,receipt.finish_entered,receipt.finish_returned,error)){cleanup();return false;}
        if(!port.readback(receipt.graph,error)){cleanup();return false;}receipt.graph_committed=true;
    }catch(const std::exception& ex){error=std::string("Scenery stroke native execution: ")+ex.what();cleanup();return false;}
    cleanup();return receipt.context_restored&&receipt.selection_restored&&receipt.cursor_destroyed&&receipt.resources_released&&receipt.cleanup_cause.empty();
}
}
