namespace world_path_runtime {
struct History {
    action::Binding binding;
    action::Request request;
    action::Evidence evidence;
    Sample after;
    bool attempted=false,confirmed=false,retired=false;
    std::string cause;
};
std::map<std::string,History> history;
bool owns_history(const std::string& source){return operations.contains(source);}
action::Outcome perform_history(const action::Binding& binding,const action::Request& request,
                                const action::Evidence* authority,std::string& error){
    auto* r=runtime(error);
    if(!r||!capture.ready||GetCurrentThreadId()!=r->thread||
       (request.action!=action::Action::UndoBuilding&&request.action!=action::Action::RedoBuilding)||
       request.canonical_record_key!="path:"+request.target_action_uuid){
        if(error.empty())error="Wall/Pass history requires an active original Path Controller";
        return action::Outcome::RejectedNoEffect;}
    if(bool(authority)==Config::instance().is_host){
        error="Wall/Pass history authority role differs";return action::Outcome::RejectedNoEffect;}
    uint64_t revision=0;
    if(!ui_move_runtime::contract::pending(typed_action_runtime::controller_status(),request,revision,error))
        return action::Outcome::RejectedNoEffect;
    if(history.contains(request.action_uuid)){
        error="Wall/Pass history already attempted; no replay";return action::Outcome::Unknown;}
    const auto found=operations.find(request.target_action_uuid);
    if(found==operations.end()||!found->second.confirmed||!found->second.retired||
       found->second.binding!=binding||found->second.request.snapshot_uuid!=request.snapshot_uuid){
        error="Wall/Pass history lacks confirmed retired source in this World";
        return action::Outcome::RejectedNoEffect;}
    const auto& source=found->second;
    const bool undo_direction=request.action==action::Action::UndoBuilding;
    const Sample& expected_before=undo_direction?source.after:source.before;
    const Sample& expected_after=undo_direction?source.before:source.after;
    const int64_t original_cash=int64_t(source.evidence.after.cash)-source.evidence.before.cash;
    const int64_t inverse_cash=undo_direction?-original_cash:original_cash;
    if(inverse_cash<INT32_MIN||inverse_cash>INT32_MAX){
        error="mixed Path history original cash inverse exceeds native integer";
        return action::Outcome::RejectedNoEffect;}
    action::Ledger money_after;
    const auto& intent=source.request.world_path;
    region::LocalView view;Sample before;action::Ledger ledger;uintptr_t graph_pointer=0;
    if(!region::read(*r,intent.start_region,0,view,error)||
       !region::bound(view,binding,request.snapshot_uuid,error)||
       !world_path_sample(*r,request.snapshot_uuid,intent.start_region,intent.end_region,before,error)||
       !region::ledger(ledger,error)||
       !building_history_slice::graph_for(request.action,request.target_action_uuid,graph_pointer,error))
        return action::Outcome::RejectedNoEffect;
    if(!mixed::same(before.mixed,expected_before.mixed,error)){
        error="Wall/Pass history complete current topology, Records or ledger differs from original source";
        return action::Outcome::RejectedNoEffect;}
    if(!building_history_slice::cash_effect(ledger,int32_t(inverse_cash),money_after,error))
        return action::Outcome::RejectedNoEffect;
    if(authority&&(authority->before!=ledger||authority->after!=money_after||!authority->world_path_before||
                   *authority->world_path_before!=before.wire)){
        error="Wall/Pass history Client before state differs from Host";
        return action::Outcome::RejectedNoEffect;}
    History value;value.binding=binding;value.request=request;value.evidence.before=ledger;
    value.evidence.world_path_before=before.wire;value.attempted=true;
    auto& operation=history.emplace(request.action_uuid,std::move(value)).first->second;
    const auto result=building_history_slice::perform(request.action,request.target_action_uuid,error);
    if(result!=action::Outcome::Confirmed){operation.cause=error;
        return authority?action::Outcome::Unknown:result;}
    uintptr_t map=0;
    if(!region::at(*r,r->image,0x13df8c0,map,error)){
        operation.cause=error;return action::Outcome::Unknown;}
    mixed_path_native::Census after_census;
    if(!mixed_path_native::read_world(r->memory,r->image,map,view.world.root,after_census,error)||
       !owner.keys.restore(expected_after.mixed.graph,after_census,error)||
       !world_path_sample(*r,request.snapshot_uuid,intent.start_region,intent.end_region,operation.after,error)||
       !region::ledger(operation.evidence.after,error)){
        operation.cause=error;return action::Outcome::Unknown;}
    if(!mixed::same(operation.after.mixed,expected_after.mixed,error)||operation.evidence.after!=money_after){
        error=operation.cause="Wall/Pass original Undo/Redo did not restore complete source topology, Records or ledger";
        return action::Outcome::Unknown;}
    uintptr_t inverse=0;
    const auto direction=undo_direction?action::Action::RedoBuilding:action::Action::UndoBuilding;
    if(!building_history_slice::graph_for(direction,request.target_action_uuid,inverse,error)){
        operation.cause=error;return action::Outcome::Unknown;}
    operation.evidence.world_path_after=operation.after.wire;
    operation.evidence.undo_confirmed=true;
    if(!action::valid_world_path_effect(request,operation.evidence)){
        error=operation.cause="Wall/Pass typed history effect rejected original complete readback";
        return action::Outcome::Unknown;}
    if(authority&&(authority->after!=operation.evidence.after||
                   authority->world_path_after!=operation.evidence.world_path_after)){
        error=operation.cause="Wall/Pass history Client complete after differs from Host";
        return action::Outcome::Unknown;}
    operation.confirmed=true;return action::Outcome::Confirmed;
}
bool observe_history(const action::Binding& binding,const action::Request& request,
                     action::Evidence& out,std::string& error){
    auto* r=runtime(error);const auto found=history.find(request.action_uuid);
    if(!r||GetCurrentThreadId()!=r->thread||found==history.end()||
       found->second.binding!=binding||found->second.request!=request||!found->second.confirmed){
        error="Wall/Pass history observation lacks exact confirmed operation";return false;}
    const auto source=operations.find(request.target_action_uuid);
    if(source==operations.end())return fail(error,"Wall/Pass history original source retired");
    const auto& intent=source->second.request.world_path;
    Sample current;action::Ledger ledger;
    if(!world_path_sample(*r,request.snapshot_uuid,intent.start_region,intent.end_region,current,error)||
       !region::ledger(ledger,error))return false;
    if(current.wire!=found->second.after.wire||ledger!=found->second.evidence.after)
        return fail(error,"Wall/Pass history complete state changed before ACK");
    out=found->second.evidence;out.retirement_confirmed=found->second.retired;return true;
}
bool retire_history(const action::Binding& binding,const action::Request& request,std::string& error){
    action::Evidence actual;if(!observe_history(binding,request,actual,error))return false;
    auto& operation=history.at(request.action_uuid);
    if(operation.retired)return fail(error,"Wall/Pass history already retired");
    operation.retired=true;return true;
}
void retire_world(){
    capture.local_wall=0;
    capture.before.reset();capture.captured.reset();capture.pending.reset();
    owner=Owner{};
    operations.clear();history.clear();
    capture.last_action.clear();capture.cause.clear();
}
}
