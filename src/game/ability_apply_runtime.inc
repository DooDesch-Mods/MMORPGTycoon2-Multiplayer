bool same_apply_local(const Sample& before,const Sample& after,uintptr_t leased_proxy){
    if(!character_rename::same_local(before.character,after.character)||before.prototype!=after.prototype||before.uid!=after.uid||
       after.proxy!=leased_proxy||(before.proxy&&before.proxy!=after.proxy)||before.local.size()!=after.local.size())return false;
    for(size_t i=0;i<before.local.size();++i){const auto& old=before.local[i];const auto& now=after.local[i];
        if(old[0]!=now[0]||old[1]!=now[1]||(old[0]==before.prototype?now[2]!=leased_proxy:old[2]!=now[2]))return false;}
    return true;
}
struct ApplyCalls {
    Memory& memory;Operation& op;uintptr_t outer=0;
    ability_apply_lease::Slots saved,working;
    std::array<unsigned char,0x608> saved_outer{};
    a::AbilityRecord saved_record;
    uintptr_t clone=0,proxy=0;bool constructed=false,acquired=false,borrow_attempted=false;
    bool prepare(std::string& error){
        if(!apply_slots(memory,outer,saved,error)||!memory.copy(outer,saved_outer.data(),saved_outer.size(),error)||
           (saved.draft&&!read_record(memory,saved.draft,saved_record,error)))return false;
        auto seh=apply_clone(op.before.prototype,clone,constructed);
        if(seh)return fail(error,"Ability Apply native draft construction SEH="+std::to_string(seh));
        seh=apply_acquire_proxy(op.before.prototype,proxy,acquired);
        if(seh)return fail(error,"Ability Apply native weak lease acquisition SEH="+std::to_string(seh));
        seh=ability_edit::prepare(clone,op.request.ability_edit.record);
        if(seh)return fail(error,"Ability Apply working draft preparation SEH="+std::to_string(seh));
        seh=simplify(clone);if(seh)return fail(error,"Ability Apply working draft Simplify SEH="+std::to_string(seh));
        a::AbilityRecord record;
        if(!read_record(memory,clone,record,error)||record!=op.request.ability_edit.record||record.effects.empty())
            return fail(error,error.empty()?"Ability Apply working draft differs from captured canonical positive-effect record":error);
        Sample prepared;
        if(!sample(memory,op.binding,op.request.snapshot_uuid,op.request.ability_edit,prepared,error)||
           !same_apply_local(op.before,prepared,proxy)||prepared.value!=op.before.value)
            return fail(error,error.empty()?"Ability Apply draft/lease preparation changed canonical gameplay":error);
        working=saved;working.draft=clone;working.live=op.before.prototype;working.proxy=proxy;working.owner=op.before.character.selected;
        op.prepared=true;return true;
    }
    bool borrow(std::string& error){
        ability_apply_lease::Slots current;
        if(!apply_slots(memory,outer,current,error)||current!=saved)return fail(error,error.empty()?"Ability Apply editor binding changed before borrow":error);
        borrow_attempted=true;const auto seh=apply_write_slots(working);
        if(seh)return fail(error,"Ability Apply borrowed binding write SEH="+std::to_string(seh));
        if(!apply_slots(memory,outer,current,error)||current!=working)return fail(error,error.empty()?"Ability Apply borrowed binding readback differs":error);
        return true;
    }
    bool invoke(std::string& error){
        if(apply_tail.armed||replaying)return fail(error,"Ability Apply presentation/original scope already active");
        a::ui_diagnostic::Api api;a::ui_diagnostic::Window target;target.window=outer;
        if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return false;
        api.command=reinterpret_cast<Command>(state.image+ui_code.rva);api.destroy_label="Apply";
        a::ui_diagnostic::Receipt receipt;op.attempted=true;replaying=true;replay_receiver=outer;original_calls=0;
        apply_tail={true,outer,working.live,0,0,false};
        const bool returned=a::ui_diagnostic::process_calls().command(api,target,receipt,error);
        op.calls=original_calls;op.returned=receipt.ui_command_returned;
        op.presentation_calls=apply_tail.setup;op.apply_event_calls=apply_tail.event;
        const bool tail_failed=apply_tail.failed;apply_tail={};replaying=false;replay_receiver=0;
        return (returned&&op.returned&&op.calls==1&&receipt.record_cleanup_confirmed&&op.presentation_calls==1&&op.apply_event_calls==1&&!tail_failed&&!fault)||
            fail(error,error.empty()?"Ability Apply receipt differs calls="+std::to_string(op.calls)+" setup="+std::to_string(op.presentation_calls)+
                " event40="+std::to_string(op.apply_event_calls)+" required=1/1/1 returned="+std::to_string(op.returned)+
                " command_cleanup="+std::to_string(receipt.record_cleanup_confirmed)+(tail_failed?" cause="+cause:""):error);
    }
    bool cleanup(std::string& error){
        if(apply_tail.armed){op.calls=original_calls;op.presentation_calls=apply_tail.setup;op.apply_event_calls=apply_tail.event;}
        apply_tail={};replaying=false;replay_receiver=0;bool ok=true;
        const auto issue=[&](const std::string& why){ok=false;if(!error.empty())error+="; ";error+=why;};
        if(borrow_attempted){
            if(op.attempted){ability_apply_lease::Slots active;std::string why;
                if(!apply_slots(memory,outer,active,why)){
                    constructed=false;acquired=false;issue(why+"; Ability Apply working ownership unreadable; resources retained");
                }else {
                    if(active.draft!=clone){constructed=false;issue("Ability Apply working draft ownership changed; destruction refused");}
                    if(active.proxy!=proxy){acquired=false;issue("Ability Apply working weak ownership changed; release refused");}
                    if(active.live!=working.live||active.owner!=working.owner)issue("Ability Apply borrowed live/owner changed during original");
                }
            }
            const auto seh=apply_write_slots(saved);
            if(seh)issue("Ability Apply editor binding restore SEH="+std::to_string(seh));
            ability_apply_lease::Slots actual;std::string why;
            if(!apply_slots(memory,outer,actual,why)||actual!=saved)issue(why.empty()?"Ability Apply restored binding readback differs":why);
            op.ui_restored=!seh&&actual==saved;
            // A failed restore may leave the receiver owning the working pointers.
            if(!op.ui_restored)return false;
        }else op.ui_restored=true;
        if(clone){
            if(!constructed)issue("Ability Apply incomplete native draft construction retained; destruction unsafe");
            else {const auto seh=apply_destroy(clone,op.before.prototype,op.before.uid);if(seh)issue("Ability Apply native draft identity/destruction failed code="+std::to_string(seh));else {clone=0;op.apply_draft_destroyed=true;}}
        }
        if(acquired){const auto seh=apply_release(proxy);if(seh)issue("Ability Apply weak lease release SEH="+std::to_string(seh));else {acquired=false;op.apply_lease_released=true;}}
        if(borrow_attempted){std::array<unsigned char,0x608> actual{};std::string why;
            if(!memory.copy(outer,actual.data(),actual.size(),why)||actual!=saved_outer)issue(why.empty()?"Ability Apply original editor object changed after restoration":why);
            a::AbilityRecord actual_record;
            if(saved.draft&&(!read_record(memory,saved.draft,actual_record,why)||actual_record!=saved_record))issue(why.empty()?"Ability Apply unrelated local unsaved draft changed":why);
        }
        return ok;
    }
};
a::Outcome perform_apply(Memory& memory,const a::Binding& binding,const a::Request& request,const Sample& before,std::string& error){
    if(apply_tail.armed||replaying){error="Ability Apply original/presentation scope is already active";return a::Outcome::RejectedNoEffect;}
    if(!ordered_scope(binding,request,error)||request.ability_edit.adjustment||request.ability_edit.record.effects.empty()||!apply_route(memory,error))
        return a::Outcome::RejectedNoEffect;
    uintptr_t outer=0,live=0,draft=0,owner=0;
    if(!window(memory,outer,live,draft,owner,error))return a::Outcome::RejectedNoEffect;
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;op.before=before;op.ordered=true;
    op.evidence.before=before.character.money;op.evidence.ability_edit_before=before.value;
    ApplyCalls calls{memory,op,outer};const bool returned=ability_apply_lease::run(calls,error);
    if(!returned){op.cause=error;return a::Outcome::Unknown;}
    // Native draft destruction can modify Release; only this final sample is evidence.
    if(!sample(memory,binding,request.snapshot_uuid,request.ability_edit,op.after,error)||!same_apply_local(before,op.after,calls.proxy)||
       !a::ability_edit_transition(request.ability_edit,before.value,op.after.value)){
        error=op.cause=error.empty()?"Ability Apply final canonical effect/owner/census/Ledger/Undo differs":error;return a::Outcome::Unknown;}
    op.evidence.after=op.after.character.money;op.evidence.ability_edit_after=op.after.value;op.confirmed=true;return a::Outcome::Confirmed;
}
