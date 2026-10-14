#include "remove_observer.h"
#include <algorithm>

namespace mt2mp::build_actions::private_native {
namespace {struct Busy {bool& b;explicit Busy(bool& v):b(v){b=true;}~Busy(){b=false;}};}
void RemoveObserver::fail(const std::string& error){state_.failed=true;if(state_.error.empty())state_.error=error;}
bool RemoveObserver::scope(const RemoveContext& c){
    if(!state_.active)return false;
    if(c.thread!=bound_.thread||!c.main_thread){++state_.worker;return false;}
    if(c.epoch!=bound_.epoch||c.world!=bound_.world||c.selection!=bound_.selection||c.target!=bound_.target||!c.exact_build||!c.active_world){fail("remove_observer_lifetime_changed");return false;}
    if(busy_){fail("remove_observer_reentrant");return false;}
    return !state_.failed;
}
bool RemoveObserver::begin(const RemoveContext& c,const std::vector<uintptr_t>& retained,std::string& e){
    if(busy_||state_.active){e="remove_observer_busy";return false;}
    if(state_.failed){e="remove_observer_failed_locked";return false;}
    if(!c.main_thread||!c.thread||!c.epoch||!c.world||!c.target||!c.exact_build||!c.active_world){e="remove_observer_context_guard";return false;}
    Busy busy(busy_);
    try {
        std::vector<Member> members;members.reserve(retained.size());
        for(auto p:retained){if(!p||std::any_of(members.begin(),members.end(),[&](const Member& v){return v.object==p;})){e="remove_observer_retained_set_invalid";return false;}members.push_back({p,false});}
        if(std::none_of(members.begin(),members.end(),[&](const Member& v){return v.object==c.target;})){e="remove_observer_target_not_retained";return false;}
        members_=std::move(members);bound_=c;state_={};state_.active=true;return true;
    }catch(const std::exception& x){e=std::string("remove_observer_begin_exception: ")+x.what();return false;}
    catch(...){e="remove_observer_begin_unknown_exception";return false;}
}
void RemoveObserver::undo_open_returned(const RemoveContext& c,uintptr_t caller,uintptr_t graph){
    if(!scope(c))return;
    if(caller!=0x2d444d){++state_.foreign;return;}
    if(state_.open_returned||!graph){fail("remove_observer_open_duplicate_or_null");return;}
    state_.graph=graph;state_.open_returned=true;
}
void RemoveObserver::graph_lock_enter(const RemoveContext& c,uintptr_t caller,uintptr_t graph,Memory& m,uintptr_t image){
    if(!scope(c))return;
    if(caller!=0x53b2c4||graph!=state_.graph){++state_.foreign;return;}
    if(!state_.open_returned||state_.in_lock||state_.lock_returned){fail("remove_observer_lock_order");return;}
    Busy busy(busy_);GraphWitness witness;std::string e;
    if(!read_graph(m,image,graph,witness,e)){fail(e);return;}
    if(witness.locked){fail("remove_observer_graph_already_locked");return;}
    if(state_.failed)return;
    state_.in_lock=true;
}
void RemoveObserver::prop_destructor_enter(const RemoveContext& c,uintptr_t prop,Memory& memory,uintptr_t image){
    if(!scope(c))return;
    if(!state_.open_returned||state_.lock_returned){++state_.foreign;return;}
    const auto it=std::find_if(members_.begin(),members_.end(),[&](const Member& v){return v.object==prop;});
    if(it==members_.end()){++state_.foreign;return;}
    if(!state_.in_lock){
        Busy busy(busy_);uintptr_t active=0;std::string error;
        if(!image||image>UINTPTR_MAX-0x1a3aa80){fail("remove_observer_image_range");return;}
        try {
            if(!memory.copy(image+0x1a3aa80,&active,sizeof(active),error)){fail(error.empty()?"remove_observer_active_graph_unreadable":error);return;}
        }catch(const std::exception& x){fail(std::string("remove_observer_active_graph_exception: ")+x.what());return;}
        catch(...){fail("remove_observer_active_graph_unknown_exception");return;}
        if(active!=state_.graph){fail("remove_observer_destruction_wrong_graph");return;}
        if(state_.failed)return;
    }
    it->destructor_entered=true;
}
void RemoveObserver::graph_lock_returned(const RemoveContext& c,uintptr_t graph,bool returned,Memory& m,uintptr_t image){
    if(!scope(c))return;
    if(graph!=state_.graph){++state_.foreign;return;}
    if(!state_.in_lock){fail("remove_observer_lock_return_without_entry");return;}
    state_.in_lock=false;
    if(!returned){fail("remove_observer_native_lock_unknown");return;}
    Busy busy(busy_);GraphWitness witness;std::string e;
    if(!read_graph(m,image,graph,witness,e)){fail(e);return;}
    if(!witness.locked||witness.pending_delete){fail("remove_observer_lock_not_drained");return;}
    if(state_.failed)return;
    state_.lock_returned=true;
}
bool RemoveObserver::finish(uintptr_t& graph,std::string& e){
    graph=0;
    if(busy_){fail("remove_observer_finish_reentrant");e=state_.error;return false;}
    if(!state_.active){e="remove_observer_not_active";return false;}
    state_.active=false;
    if(!state_.failed&&(!state_.open_returned||!state_.lock_returned||state_.in_lock))fail("remove_observer_incomplete_native_path");
    if(state_.failed){e=state_.error;return false;}
    state_.complete=true;graph=state_.graph;return true;
}
bool RemoveObserver::observed_destruction(uintptr_t prop)const{
    if(!state_.complete||state_.failed)return false;
    return std::any_of(members_.begin(),members_.end(),[&](const Member& v){return v.object==prop&&v.destructor_entered;});
}
}
