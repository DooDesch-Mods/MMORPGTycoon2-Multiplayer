#pragma once
#include "typed_action/duplicate_action.h"
#include <string>
#include <vector>
namespace mt2mp::game::duplicate_native {
struct Handle {uintptr_t object=0;uint64_t uid=0;bool operator==(const Handle&)const=default;};
struct Receipt {
    bool attempted=false,charged=false,edit_open=false,graph_created=false,graph_committed=false;
    bool selection_restored=false,edit_end_attempted=false,edit_closed=false;
    uintptr_t graph=0;
    size_t cash_inverses=0,duplicate_calls=0,duplicate_returns=0,destroy_inverses=0;
    std::vector<Handle> created;
    std::string cleanup_cause;
};
// This is the original single Duplicate transaction with explicit source
// cursor position. Native Duplicate leaves retain Building layout/plan/art
// behavior; one graph owns all cash and destroy inverses.
template<class Port>bool execute(Port& port,const action_session::DuplicateIntent& intent,
                                const std::vector<Handle>& sources,Receipt& receipt,std::string& error){
    auto fail=[&](const std::string& e){error=e;return false;};
    if(receipt.attempted)return fail("Duplicate native transaction already attempted; no replay");
    if(intent.rows.empty()||sources.size()!=intent.rows.size())return fail("Duplicate complete source count differs");
    int64_t total=0;
    for(size_t i=0;i<sources.size();++i){if(!sources[i].object||!sources[i].uid||intent.rows[i].cost<0)
            return fail("Duplicate original source/full UID/cost invalid index="+std::to_string(i));
        total+=intent.rows[i].cost;if(total>INT32_MAX)return fail("Duplicate complete original cost exceeds int32");}
    if(!port.ready(intent,sources,int32_t(total),error))return false;
    receipt.attempted=true;
    auto cleanup=[&](){std::string why;
        if(port.restore_selection(why))receipt.selection_restored=true;else receipt.cleanup_cause=why;
        if(receipt.edit_open&&!receipt.edit_end_attempted){receipt.edit_end_attempted=true;why.clear();if(port.end_edit(why))receipt.edit_closed=true;else{
            if(!receipt.cleanup_cause.empty())receipt.cleanup_cause+="; ";receipt.cleanup_cause+=why;}}
        if(!receipt.cleanup_cause.empty()){if(error.empty())error=receipt.cleanup_cause;else error+="; cleanup: "+receipt.cleanup_cause;}
    };
    // Once a native payment/creation can have happened, no retry or synthetic
    // inverse is safe. The caller retains the partial receipt as Unknown.
    if(!port.charge(int32_t(total),error)){cleanup();return false;}receipt.charged=true;
    if(!port.begin_edit(receipt.edit_open,error)){cleanup();return false;}
    if(!port.create_graph(receipt.graph,error)){cleanup();return false;}receipt.graph_created=true;
    if(intent.rows.size()>9){
        if(!port.cash_inverse(receipt.graph,int32_t(total),intent.cash_position,error)){cleanup();return false;}++receipt.cash_inverses;
    }else for(const auto& row:intent.rows){const std::array<uint32_t,3> position{row.after[4],row.after[5],row.after[6]};
        if(!port.cash_inverse(receipt.graph,row.cost,position,error)){cleanup();return false;}++receipt.cash_inverses;
    }
    for(size_t i=0;i<sources.size();++i){Handle created;++receipt.duplicate_calls;
        if(!port.duplicate(sources[i],intent.rows[i],created,error)){if(created.object)receipt.created.push_back(created);cleanup();return false;}
        ++receipt.duplicate_returns;receipt.created.push_back(created);
        if(!created.object||!created.uid){fail("Duplicate original returned absent object/full UID index="+std::to_string(i));cleanup();return false;}
        if(!port.destroy_inverse(receipt.graph,created,error)){cleanup();return false;}++receipt.destroy_inverses;
    }
    if(!port.select_created(receipt.created,error)||!port.release_points(intent,receipt.created,error)||
       !port.commit_graph(receipt.graph,receipt.created,error)){cleanup();return false;}receipt.graph_committed=true;
    if(receipt.edit_open){receipt.edit_end_attempted=true;if(!port.end_edit(error)){cleanup();return false;}receipt.edit_closed=true;}
    if(!port.readback(intent,sources,receipt,error)){cleanup();return false;}
    cleanup();return receipt.selection_restored&&(!receipt.edit_open||receipt.edit_closed)&&receipt.cleanup_cause.empty();
}
}
