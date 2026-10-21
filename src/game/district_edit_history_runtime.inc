namespace {
std::map<std::string,Operation> history_operations;
bool history_request(const action::Request& request){
    return (request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&
        request.canonical_record_key=="district-edit:"+request.target_action_uuid;
}
bool history_source(const action::Binding& binding,const action::Request& request,Operation*& out,std::string& error){
    const auto found=operations.find(request.target_action_uuid);
    if(!history_request(request)||found==operations.end()||found->second.binding!=binding||
       found->second.request.snapshot_uuid!=request.snapshot_uuid||!found->second.confirmed||!found->second.retired||!found->second.graph){
        error="District history lacks its exact confirmed retired original edit";return false;}
    out=&found->second;return true;
}
bool history_result(const action::DistrictState& current,const history::Stored& stored,const action::DistrictState& after){
    auto expected=history::project(current,stored);
    if(expected==after)return true;
    if(!expected.manual_title_inside&&expected.flags[11]){expected.manual_title_inside=true;expected.flags[11]=0;}
    return expected==after;
}
}
action::Outcome perform_history(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& error){
    auto* runtime=published.load(std::memory_order_acquire);Operation* original=nullptr;uint64_t revision=0;
    if(!runtime||!runtime->ready||runtime->thread!=GetCurrentThreadId()||!installed||
       request.snapshot_uuid!=net::current_snapshot_id()||bool(authority)==Config::instance().is_host||
       !history_source(binding,request,original,error)||!contract::pending(typed_action_runtime::controller_status(),request,revision,error)||!code(*runtime,error)){
        if(error.empty())error="District history exact current request/role/Controller unavailable";return action::Outcome::RejectedNoEffect;}
    if(history_operations.contains(request.action_uuid)){error="District history original already attempted; no replay";return action::Outcome::Unknown;}
    action::DistrictState before;action::Ledger money;witness::UndoWitness undo;uintptr_t graph=0;history::Stored stored;
    if(!district_text::sample(*runtime,binding,original->target,before,money,undo,error)||
       !building_history_slice::graph_for(request.action,request.target_action_uuid,graph,error)||
       !history::graph(runtime->memory,runtime->image,graph,stored,error))return action::Outcome::RejectedNoEffect;
    const auto retained=original->inverse.find(graph);
    if(retained==original->inverse.end()||retained->second!=stored){error="District history original inverse payload no longer matches its retained proof";return action::Outcome::RejectedNoEffect;}
    if(authority&&(!action::valid_district_property_effect(request,*authority)||authority->district_before!=before||authority->before!=money)){
        error="District history Client complete state/Ledger differs from Host";return action::Outcome::RejectedNoEffect;}
    action::Evidence predicted;predicted.before=predicted.after=money;predicted.district_before=before;
    predicted.district_after=history::project(before,stored);predicted.undo_confirmed=true;
    if(!action::district_history_wire_fits(request,predicted,error))return action::Outcome::RejectedNoEffect;
    auto& operation=history_operations.emplace(request.action_uuid,Operation{binding,request,original->target}).first->second;
    operation.evidence.before=money;operation.evidence.district_before=before;
    auto unknown=[&](const std::string& why){error=operation.cause=why;return action::Outcome::Unknown;};
    operation.attempted=true;
    if(building_history_slice::perform(request.action,request.target_action_uuid,error)!=action::Outcome::Confirmed)return unknown(error);
    operation.returned=true;
    action::DistrictState after;action::Ledger after_money;witness::UndoWitness after_undo;
    const auto opposite=request.action==action::Action::UndoBuilding?action::Action::RedoBuilding:action::Action::UndoBuilding;
    history::Stored inverse;
    if(!district_text::sample(*runtime,binding,original->target,after,after_money,after_undo,error)||
       !building_history_slice::graph_for(opposite,request.target_action_uuid,operation.graph,error)||
       !history::graph(runtime->memory,runtime->image,operation.graph,inverse,error))return unknown(error);
    if(inverse!=history::stored(stored.uid,stored.region,stored.root,before)||!history_result(before,stored,after)||after_money!=money)
        return unknown("District original history complete projection/Ledger/inverse differs");
    original->inverse[operation.graph]=inverse;operation.inverse.emplace(operation.graph,std::move(inverse));
    operation.undo=std::move(after_undo);operation.evidence.after=after_money;operation.evidence.district_after=after;operation.evidence.undo_confirmed=true;
    if(!action::valid_district_property_effect(request,operation.evidence)||
       (authority&&(authority->district_after!=after||authority->after!=after_money)))return unknown("District history original result differs from Host");
    operation.confirmed=true;return action::Outcome::Confirmed;
}
bool observe_history(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error){
    auto* runtime=published.load(std::memory_order_acquire);Operation* original=nullptr;const auto found=history_operations.find(request.action_uuid);
    if(!runtime||found==history_operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||
       !history_source(binding,request,original,error)){if(error.empty())error="District history observation lacks its exact confirmed operation";return false;}
    const auto& operation=found->second;action::DistrictState current;action::Ledger money;witness::UndoWitness undo;history::Stored inverse;
    uintptr_t graph=0;const auto opposite=request.action==action::Action::UndoBuilding?action::Action::RedoBuilding:action::Action::UndoBuilding;
    if(!district_text::sample(*runtime,binding,operation.target,current,money,undo,error)||
       !building_history_slice::graph_for(opposite,request.target_action_uuid,graph,error)||
       !history::graph(runtime->memory,runtime->image,graph,inverse,error))return false;
    if(current!=operation.evidence.district_after||money!=operation.evidence.after||undo!=operation.undo||graph!=operation.graph||inverse!=operation.inverse.at(graph)){
        error="District history complete current state/Undo/inverse changed before ACK";return false;}
    out=operation.evidence;out.retirement_confirmed=operation.retired;return true;
}
bool retire_history(const action::Binding& binding,const action::Request& request,std::string& error){
    action::Evidence current;if(!observe_history(binding,request,current,error))return false;
    auto& operation=history_operations.at(request.action_uuid);
    if(operation.retired){error="District history already retired";return false;}operation.retired=true;return true;
}
