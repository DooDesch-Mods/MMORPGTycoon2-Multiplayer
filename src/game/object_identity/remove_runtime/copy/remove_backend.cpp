#include "remove_backend.h"
#include <algorithm>
#include <set>

namespace mt2mp::build_actions::private_native {
namespace {
struct Busy {bool& b;explicit Busy(bool& v):b(v){b=true;}~Busy(){b=false;}};
bool same(const RemoveContext& a,const RemoveContext& b){
    return a.epoch==b.epoch&&a.thread==b.thread&&a.world==b.world&&a.selection==b.selection&&a.target==b.target;
}
bool valid_context(const RemoveContext& c,bool require_live_target=true){return c.exact_build&&c.main_thread&&c.active_world&&c.saves_idle&&(!require_live_target||c.lease_current);}
std::vector<uintptr_t> survivors(const std::vector<uintptr_t>& a,const std::set<uintptr_t>& retired){
    std::vector<uintptr_t> result;for(auto p:a)if(!retired.contains(p))result.push_back(p);return result;
}
}
bool RemoveBackend::should_forward_clear(const RemoveContext& c,uintptr_t receiver,uintptr_t caller){
    return permit_.should_forward_clear(c,receiver,caller,memory_);
}
RemoveResult RemoveBackend::execute(uint64_t sequence,const RemoveContext& context){
    RemoveResult result;result.sequence=sequence;
    auto fail=[&](const std::string& error){result.error=error.empty()?"remove_service_failed_without_cause":error;result.unknown_locked=unknown_||result.attempted;unknown_=result.unknown_locked;return result;};
    if(busy_)return fail("remove_backend_reentrant");
    if(sequence==high_watermark_&&sequence){
        if(same(context,last_context_))return last_;
        return fail("remove_backend_conflicting_duplicate");
    }
    if(unknown_)return fail("remove_backend_unknown_locked");
    if(!sequence||sequence<=high_watermark_)return fail("remove_backend_old_sequence");
    if(high_watermark_&&sequence!=high_watermark_+1)return fail("remove_backend_sequence_gap");
    Busy busy(busy_);std::string error;bool permit_started=false,observation_started=false;
    auto finish=[&](){high_watermark_=sequence;last_context_=context;last_=result;return result;};
    auto reject=[&](const std::string& cause){result=fail(cause);return finish();};
    try {
        if(!destroy_||!highlights_)return reject("remove_native_entrypoint_missing");
        if(hook_mask_.load()!=15)return reject("remove_required_hooks_unavailable");
        RemoveContext current;
        if(!services_.current(current,error)||!same(context,current)||!valid_context(current))return reject(error.empty()?"remove_context_changed":error);
        UndoWitness undo_before;if(!read_undo(memory_,image_,undo_before,error))return reject(error);
        if(undo_before.active_graph)return reject("remove_wait_existing_undo_transaction");
        if(!permit_.begin(image_,current,memory_,error))return reject(error);
        permit_started=true;const auto before=permit_.before();
        std::set<uintptr_t> unique{context.target};
        for(const auto& list:before.lists)unique.insert(list.begin(),list.end());
        if(before.focused)unique.insert(before.focused);
        std::vector<uintptr_t> objects(unique.begin(),unique.end());std::vector<PropLease> leases;
        if(!services_.retain(current,objects,leases,error)){permit_.finish(false,false,error);return reject(error);}
        result.retained=leases.size();std::set<uintptr_t> held;std::set<uint64_t> tokens;
        for(const auto& lease:leases){
            if(!lease.object||!lease.token||!held.insert(lease.object).second||!tokens.insert(lease.token).second){permit_.finish(false,false,"remove_lease_set_invalid");return reject("remove_lease_set_invalid");}
            Life life=Life::Unknown;
            if(!services_.inspect(current,lease,0,life,error)||life!=Life::Alive){permit_.finish(false,false,error);return reject(error.empty()?"remove_lease_not_alive":error);}
        }
        if(held!=unique){permit_.finish(false,false,"remove_lease_coverage_mismatch");return reject("remove_lease_coverage_mismatch");}
        SelectionWitness fresh;SelectionCounts counts;
        if(!read_selection(memory_,image_,context.selection,fresh,counts,error)||fresh!=before){permit_.finish(false,false,error);return reject(error.empty()?"remove_selection_changed_before_call":error);}
        if(!services_.current(current,error)||!same(context,current)||!valid_context(current)){permit_.finish(false,false,error);return reject(error.empty()?"remove_context_changed_before_call":error);}
        if(!services_.observe_begin(current,error)){permit_.finish(false,false,error);return reject(error);}
        observation_started=true;
        if(hook_mask_.load()!=15){permit_.finish(false,false,"remove_hook_mask_lost_before_call");permit_started=false;uintptr_t discarded=0;std::string end_error;const bool ended=services_.observe_end(discarded,end_error);observation_started=false;return reject(ended?"remove_hook_mask_lost_before_call":"remove_hook_mask_lost_before_call; observe_end: "+end_error);}
        result.attempted=true;
        const auto call=call_destroy(destroy_,context.selection,context.target);
        result.attempted=call.attempted;result.returned=call.returned;result.seh_code=call.seh_code;
        const auto permit=permit_.finish(call.attempted,call.returned,call.error);permit_started=false;
        uintptr_t graph=0;
        const bool observed=services_.observe_end(graph,error);observation_started=false;
        if(hook_mask_.load()!=15)return reject("remove_hook_mask_lost_after_call");
        if(!call.returned||permit.unknown||!observed||!graph)return reject(!call.error.empty()?call.error:(!permit.error.empty()?permit.error:error));
        if(!services_.current(current,error)||!same(context,current)||!valid_context(current,false))return reject(error.empty()?"remove_context_changed_after_call":error);
        UndoWitness undo_after;GraphWitness graph_after;
        if(!read_undo(memory_,image_,undo_after,error)||!read_graph(memory_,image_,graph,graph_after,error)||!validate_undo_commit(undo_before,undo_after,graph_after,error))return reject(error);
        result.undo_confirmed=true;std::set<uintptr_t> retired;
        for(const auto& lease:leases){
            Life life=Life::Unknown;if(!services_.inspect(current,lease,graph,life,error)||life==Life::Unknown)return reject(error.empty()?"remove_lease_state_unknown":error);
            if(life==Life::RetiredByThisCommit)retired.insert(lease.object);
        }
        result.retired=retired.size();result.target_retired=retired.contains(context.target);
        if(!result.target_retired)return reject("remove_target_not_retired");
        SelectionWitness after;
        if(!read_selection(memory_,image_,context.selection,after,counts,error))return reject(error);
        SelectionWitness expected=before;
        for(auto& list:expected.lists)list=survivors(list,retired);
        expected.focused=after.focused;expected.lists[3]=after.lists[3];
        if(after!=expected||retired.contains(after.focused))return reject("remove_surviving_selection_changed");
        if(before.focused&&!retired.contains(before.focused)&&after.focused!=before.focused)return reject("remove_surviving_focus_changed");
        const auto highlight=survivors(before.lists[3],retired);
        if(after.lists[3]!=highlight){
            for(const auto& lease:leases){
                if(retired.contains(lease.object))continue;
                Life life=Life::Unknown;
                if(!services_.inspect(current,lease,graph,life,error)||life!=Life::Alive)return reject(error.empty()?"remove_survivor_changed_before_restore":error);
            }
            if(!services_.current(current,error)||!same(context,current)||!valid_context(current,false))return reject(error.empty()?"remove_context_changed_before_restore":error);
            const auto restored=call_highlights(highlights_,context.selection,image_+0x1748c50,highlight);
            if(!restored.returned){result.seh_code=restored.seh_code;return reject(restored.error);}
        }
        if(!read_selection(memory_,image_,context.selection,after,counts,error))return reject(error);
        expected.lists[3]=highlight;expected.focused=after.focused;
        if(after!=expected||retired.contains(after.focused))return reject("remove_final_selection_mismatch");
        result.selection_confirmed=true;result.confirmed=true;return finish();
    }catch(const std::exception& x){error=std::string("remove_backend_exception: ")+x.what();}
    catch(...){error="remove_backend_unknown_exception";}
    if(permit_started)permit_.finish(result.attempted,result.returned,error);
    if(observation_started){
        uintptr_t graph=0;std::string cleanup;
        try {if(!services_.observe_end(graph,cleanup))error+="; observe_end: "+cleanup;}
        catch(const std::exception& x){error+=std::string("; observe_end exception: ")+x.what();}
        catch(...){error+="; observe_end unknown exception";}
    }
    return reject(error);
}
}
