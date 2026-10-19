#pragma once
#include "group_reader.h"
#include "group_transition.h"
namespace mt2mp::game::group_native {
struct Receipt {
    bool attempted=false,selection_bound=false,original_entered=false,original_returned=false,selection_restored=false;
    bool provisional_created=false,provisional_cleaned=false;
    group_reader::Handle provisional;
    uintptr_t graph=0;
    size_t original_calls=0;
    std::string cleanup_cause;
};
// The whole original GroupSelection/UngroupSelection owns member ordering and
// its single graph. A new Group is explicitly rooted before selecting it so
// the original existing-Group branch never consults the local camera Shard.
template<class Port>bool execute(Port& port,const action_session::GroupIntent& intent,const group_reader::Local& before,
                                group_reader::Local& after,Receipt& receipt,std::string& e,const std::string& created_key={}){
    if(receipt.attempted){e="Group native transaction already attempted; no replay";return false;}
    group_transition::Plan plan;if(!group_transition::plan(before.state,intent,plan,e,created_key)||!port.ready(before,e))return false;
    std::vector<uintptr_t> targets;for(const auto& key:intent.selected){const auto found=before.handles.find(key);
        if(found==before.handles.end()){e="Group selected canonical full binding absent";return false;}targets.push_back(found->second.object);}
    receipt.attempted=true;
    auto cleanup=[&](){std::string why;
        if(!receipt.original_entered&&receipt.provisional_created){
            if(port.cleanup_empty(receipt.provisional,why))receipt.provisional_cleaned=true;
            else receipt.cleanup_cause=why;
        }
        why.clear();if(port.restore(before,after,plan,receipt.original_returned,why))receipt.selection_restored=true;
        else{if(!receipt.cleanup_cause.empty())receipt.cleanup_cause+="; ";receipt.cleanup_cause+=why;}
        if(!receipt.cleanup_cause.empty()){
            if(e.empty())e=receipt.cleanup_cause;
            else e+="; cleanup: "+receipt.cleanup_cause;
        }
    };
    if(plan.create){
        if(!port.create(before,plan,receipt.provisional,e)){receipt.provisional_created=receipt.provisional.object!=0;cleanup();return false;}
        receipt.provisional_created=true;targets.insert(targets.begin(),receipt.provisional.object);
    }
    if(!port.select(targets,e)){cleanup();return false;}receipt.selection_bound=true;
    receipt.original_entered=true;++receipt.original_calls;
    if(!port.original(intent.kind,e)){cleanup();return false;}receipt.original_returned=true;
    if(!port.read_after(before,after,e)||!group_transition::effect(before.state,after.state,intent,e,created_key)||
       !port.inverse(before,after,plan,receipt.provisional,receipt.graph,e)){cleanup();return false;}
    cleanup();return receipt.selection_restored&&receipt.cleanup_cause.empty();
}
}
