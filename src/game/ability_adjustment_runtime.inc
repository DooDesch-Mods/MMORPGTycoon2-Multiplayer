using ViewSelect=void(*)(void*,void*);
ViewSelect original_view_select=nullptr;uint64_t view_generation=0;
struct ViewTail {bool armed=false,preserve=false;uintptr_t view=0;uint32_t calls=0;};
ViewTail view_tail;
std::map<std::string,uintptr_t> adjustment_views;
void view_select_entry(void* view,void* selected){
    bool imported=false;std::string import_error;
    if(!character_membership::import_presentation(view,selected,uintptr_t(_ReturnAddress()),imported,import_error)){
        fault=true;reject(import_error);return;}
    if(imported)return;
    if(view_tail.armed){
        if(uintptr_t(view)!=view_tail.view||GetCurrentThreadId()!=state.thread||view_tail.calls++){
            fault=true;reject("Ability adjustment scoped original presentation tail receiver/thread/count differs");return;}
        if(view_tail.preserve)return;
    }
    original_view_select(view,selected);
}
bool view_route(Memory& memory,std::string& error){
    if(!original_view_select)return true;
    MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
    if(!quest_list::read_route(state.image+0x689100,receipt,error))return false;
    if(receipt.detour!=uintptr_t(&view_select_entry)||receipt.trampoline!=uintptr_t(original_view_select)||
       receipt.generation!=view_generation||receipt.state!=MT2FW_MT2_ROUTE_ENABLED)
        return fail(error,"Ability adjustment scoped presentation route ownership differs");
    std::array<unsigned char,22> trampoline{};uintptr_t resume=0;
    constexpr std::array<unsigned char,6> prefix{0x55,0x57,0x56,0x53,0x48,0x83};
    // The first complete instructions occupy eight bytes.
    constexpr size_t copied=8;
    if(!memory.copy(uintptr_t(original_view_select),trampoline.data(),trampoline.size(),error))return false;
    std::memcpy(&resume,trampoline.data()+copied+6,8);
    constexpr std::array<unsigned char,6> jump{0xff,0x25,0,0,0,0};
    if(!std::equal(prefix.begin(),prefix.end(),trampoline.begin())||
       !std::equal(receipt.pristine,receipt.pristine+copied,trampoline.begin())||
       !std::equal(jump.begin(),jump.end(),trampoline.begin()+copied)||resume!=state.image+0x689100+copied)
        return fail(error,"Ability adjustment scoped presentation original instruction boundary differs");
    return true;
}
bool read_proposal(Memory& memory,uintptr_t view,ability_adjustment_view::State& out,std::string& error){
    out={};out.view=view;uintptr_t ap=0,outer=0,actual=0,editor=0;
    if(!character_rename::window(memory,0,actual,editor,outer,error)||actual!=view||
       !field(memory,view,0,ap,error)||ap!=state.image+0x1736770||
       !field(memory,view,0x460,out.selected,error)||!field(memory,view,0x470,out.prototype,error)||
       !field(memory,view,0x478,out.cost,error)||!field(memory,view,0x47c,out.index,error)||
       !field(memory,view,0x480,out.delta,error))return fail(error,error.empty()?"Ability adjustment exact current CharacterInfo proposal View differs":error);
    return true;
}
uint32_t write_proposal(const ability_adjustment_view::State& value)noexcept{
    __try{
        std::memcpy(reinterpret_cast<void*>(value.view+0x470),&value.prototype,sizeof value.prototype);
        std::memcpy(reinterpret_cast<void*>(value.view+0x478),&value.cost,sizeof value.cost);
        std::memcpy(reinterpret_cast<void*>(value.view+0x47c),&value.index,sizeof value.index);
        std::memcpy(reinterpret_cast<void*>(value.view+0x480),&value.delta,sizeof value.delta);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t invoke_adjustment(uintptr_t view,bool& returned)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(state.image+0x689310)(reinterpret_cast<void*>(view));returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct AdjustmentCalls {
    Memory& memory;uintptr_t view=0;uint32_t original_calls=0;bool original_returned=false;
    bool read(ability_adjustment_view::State& value,std::string& error){return read_proposal(memory,view,value,error);}
    bool prepare(const ability_adjustment_view::State& value,std::string& error){const auto seh=write_proposal(value);
        return !seh||fail(error,"Ability adjustment owned proposal UI preparation SEH="+std::to_string(seh));}
    bool restore(const ability_adjustment_view::State& value,std::string& error){return prepare(value,error);}
    bool invoke(bool preserve,std::string& error){
        if(view_tail.armed)return fail(error,"Ability adjustment presentation scope is already active");
        view_tail={true,preserve,view,0};++original_calls;const auto seh=invoke_adjustment(view,original_returned);
        const auto calls=view_tail.calls;view_tail={};
        if(seh||!original_returned||calls!=1||fault)return fail(error,"Ability original adjustment/presentation tail SEH="+std::to_string(seh)+" returned="+std::to_string(original_returned)+" tails="+std::to_string(calls)+(cause.empty()?std::string{}:" cause="+cause));
        return true;
    }
};
bool capture_adjustment_impl(void* receiver,void* record,bool& handled,std::string& error){
    handled=false;if(!net::session_wire::connected())return true;
    Memory memory;a::ui_diagnostic::Api api;bool label=false;
    if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return false;
    const auto seh=quest_list::label(api.label_equals,record,"DoAbilityAdjustment",label);
    if(seh)return fail(error,"Ability adjustment label SEH="+std::to_string(seh));if(!label)return true;handled=true;
    const auto controller=typed_action_runtime::controller_status();a::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!ready||fault||pending||replaying||character_rename::pending_ui()||costume_edit::pending_ui()||
       GetCurrentThreadId()!=state.thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||
       !code(memory,error)||!view_route(memory,error)||!typed_action_runtime::network_binding(snapshot,binding,error))
        return fail(error,error.empty()?"Ability adjustment capture requires idle bound Mainthread Controller":error);
    ability_adjustment_view::State proposal;if(!read_proposal(memory,uintptr_t(receiver),proposal,error))return false;
    if(!proposal.prototype)return true;
    if(!proposal.selected||proposal.cost>1||proposal.index<0||!std::isfinite(proposal.delta)||proposal.delta==0)
        return fail(error,"Ability adjustment original selected proposal values differ");
    a::AbilityEditIntent intent;
    if(!field(memory,proposal.selected,0x2c,intent.kind,error)||!field(memory,proposal.selected,0x30,intent.id,error))return false;
    uintptr_t data=0;int32_t count=0;
    if(!field(memory,proposal.selected,0x130,data,error)||!field(memory,proposal.selected,0x138,count,error))return false;
    if(count<0||count>10000||(count&&!data))return fail(error,"Ability adjustment complete selected owner slots differ");
    for(int32_t slot=0;slot<count;++slot){uintptr_t prototype=0;if(!field(memory,data,size_t(slot)*8,prototype,error))return false;
        if(prototype==proposal.prototype){if(intent.slot!=-1)return fail(error,"Ability adjustment prototype has duplicate current owner slots");intent.slot=slot;}}
    if(intent.slot<0)return fail(error,"Ability adjustment proposal is outside the current selected Character owner");
    intent.adjustment=a::AbilityAdjustment{proposal.cost!=0,proposal.index,proposal.delta};Sample before;
    if(!sample(memory,binding,snapshot,intent,before,error)||before.prototype!=proposal.prototype||before.character.selected!=proposal.selected)
        return fail(error,error.empty()?"Ability adjustment complete current owner/prototype differs":error);
    if(!a::ability_adjusted_record(before.value.record,*intent.adjustment,intent.record))return fail(error,"Ability adjustment original cost/effect index or resulting value differs");
    if(!quest_bridge::sha256(record_json(before.value.record).dump(),intent.before_sha256,error))return false;
    pending=Pending{binding,snapshot,intent,uintptr_t(receiver)};++captures;return true;
}
a::Outcome perform_adjustment(Memory& memory,const a::Binding& binding,const a::Request& request,const a::Evidence* authority,const Sample& before,std::string& error){
    const auto& intent=request.ability_edit;a::AbilityRecord expected;
    if(!intent.adjustment||!a::ability_adjusted_record(before.value.record,*intent.adjustment,expected)||expected!=intent.record){error="Ability adjustment exact original proposal result differs";return a::Outcome::RejectedNoEffect;}
    uintptr_t view=0,editor=0,outer=0;
    if(!view_route(memory,error)||!character_rename::window(memory,0,view,editor,outer,error))return a::Outcome::RejectedNoEffect;
    ability_adjustment_view::State saved;if(!read_proposal(memory,view,saved,error))return a::Outcome::RejectedNoEffect;
    const auto captured=adjustment_views.find(request.action_uuid);const bool preserve=captured==adjustment_views.end();
    if(!preserve&&(captured->second!=view||saved.selected!=before.character.selected||saved.prototype!=before.prototype||
       saved.cost!=uint8_t(intent.adjustment->cost)||saved.index!=intent.adjustment->index||saved.delta!=intent.adjustment->delta)){
        error="Ability adjustment initiating native proposal changed before ordered execution";return a::Outcome::RejectedNoEffect;}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){view_tail={};error=op.cause=why.empty()?"Ability adjustment failed without cause":why;return a::Outcome::Unknown;};
    const auto random=authority?authority->ability_edit_before.owner.global_random:before.value.owner.global_random;
    op.seed_attempted=true;const auto seed=action_runtime::seed(state.image,random);
    if(seed)return unknown("Ability adjustment Host RNG adoption SEH="+std::to_string(seed));
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.before,error)||op.before.value.owner.global_random!=random)
        return unknown(error.empty()?"Ability adjustment Host RNG/readback differs":error);
    op.evidence.before=op.before.character.money;op.evidence.ability_edit_before=op.before.value;
    auto prepared=saved;prepared.prototype=before.prototype;prepared.cost=intent.adjustment->cost?1:0;prepared.index=intent.adjustment->index;prepared.delta=intent.adjustment->delta;
    AdjustmentCalls calls{memory,view};bool attempted=false;op.prepared=true;
    const bool returned=ability_adjustment_view::run(calls,prepared,preserve,attempted,error);op.attempted=calls.original_calls!=0;op.calls=calls.original_calls;op.returned=calls.original_returned;
    if(!returned)return unknown(error);
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.after,error)||!same_local(op.before,op.after)||!a::ability_edit_transition(intent,op.before.value,op.after.value))
        return unknown(error.empty()?"Ability adjustment original effect/owner/full census/ledger/Undo differs":error);
    op.evidence.after=op.after.character.money;op.evidence.ability_edit_after=op.after.value;
    if(authority){auto exact=*authority;exact.retirement_confirmed=false;if(op.evidence!=exact)return unknown("Ability adjustment Client original effect differs from Host");}
    op.confirmed=true;return a::Outcome::Confirmed;
}
