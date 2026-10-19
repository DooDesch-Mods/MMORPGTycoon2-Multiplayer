struct HistoryOperation {
    action::Binding binding;action::Request request,poses;action::Evidence evidence;
    std::vector<move_kiss::LocalBinding> before;uintptr_t inverse_graph=0;
    bool confirmed=false,retired=false;std::string cause;
};
std::map<std::string,HistoryOperation> history_operations;

bool history_intent(const action::Binding& binding,const action::Request& request,action::SelectionMoveIntent& out,std::string& e){
    out={};const auto found=operations.find(request.target_action_uuid);
    if((request.action!=action::Action::UndoBuilding&&request.action!=action::Action::RedoBuilding)||
       request.canonical_record_key!="selection:"+request.target_action_uuid||found==operations.end()||
       found->second.binding!=binding||found->second.request.snapshot_uuid!=request.snapshot_uuid||
       !found->second.confirmed||!found->second.retired){e="Selection history lacks confirmed source in current World";return false;}
    const bool undo=request.action==action::Action::UndoBuilding;
    for(const auto& row:found->second.request.selection_move.rows)
        out.rows.push_back({row.key,undo?row.after:row.before,undo?row.before:row.after});
    if(!action::valid_selection_move(out)){e="Selection history complete original poses invalid";return false;}
    return true;
}

action::Outcome perform_history(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);uint64_t revision=0;action::SelectionMoveIntent intent;
    if(!r||!r->ready||GetCurrentThreadId()!=r->thread||
       !contract::pending(typed_action_runtime::controller_status(),request,revision,e)||
       bool(authority)==Config::instance().is_host||!history_intent(binding,request,intent,e)){
        if(e.empty())e="Selection history exact pending action/role unavailable";return action::Outcome::RejectedNoEffect;}
    if(history_operations.contains(request.action_uuid)){e="Selection history already attempted; no replay";return action::Outcome::Unknown;}
    const auto& source=operations.at(request.target_action_uuid);
    auto before=source.before;action::Ledger money;
    if(!ledger(money,e))return action::Outcome::RejectedNoEffect;
    for(auto& row:before)row.ledger=money;
    auto poses=source.request;poses.selection_move=intent;
    auto expected=poses;for(auto& row:expected.selection_move.rows)row.after=row.before;
    uintptr_t source_graph=0;action::SelectionMoveState current_before;action::Ledger checked;
    if(!building_history_slice::graph_for(request.action,request.target_action_uuid,source_graph,e)||
       !current(*r,binding,expected,before,source_graph,current_before,checked,e))return action::Outcome::RejectedNoEffect;
    action::Evidence evidence;evidence.before=evidence.after=money;evidence.selection_move_before=current_before;
    evidence.selection_move_after=action::SelectionMoveState{};
    for(const auto& row:intent.rows)evidence.selection_move_after->rows.push_back(row.after);
    evidence.undo_confirmed=true;
    if(authority&&(authority->before!=money||authority->after!=money||
       authority->selection_move_before!=evidence.selection_move_before||authority->selection_move_after!=evidence.selection_move_after||
       !authority->undo_confirmed||authority->selection_confirmed||authority->target_retired)){
        e="Selection history Client complete before/effect differs from Host";return action::Outcome::RejectedNoEffect;}
    auto& op=history_operations.emplace(request.action_uuid,HistoryOperation{binding,request,poses,evidence,before}).first->second;
    auto unknown=[&](const std::string& why){e=op.cause=why;return action::Outcome::Unknown;};
    const auto outcome=building_history_slice::perform(request.action,request.target_action_uuid,e);
    if(outcome!=action::Outcome::Confirmed){op.cause=e;return outcome;}
    const auto inverse=request.action==action::Action::UndoBuilding?action::Action::RedoBuilding:action::Action::UndoBuilding;
    if(!building_history_slice::graph_for(inverse,request.target_action_uuid,op.inverse_graph,e))return unknown(e);
    // Native history moves the whole graph. Verify every pose before refreshing
    // the registry, then refresh all rows before its complete census runs.
    for(size_t i=0;i<before.size();++i){native::BuildingState actual;
        if(!scenery_kiss::read_action_prop(before[i].local_uid,actual,e))return unknown(e);
        if(world_state::building_state(actual)!=intent.rows[i].after)return unknown("Selection history original full pose differs index="+std::to_string(i));}
    if(!refresh_targets(poses,before,e))return unknown(e);
    action::SelectionMoveState after;action::Ledger after_money;
    if(!current(*r,binding,poses,before,op.inverse_graph,after,after_money,e))return unknown(e);
    if(after!=op.evidence.selection_move_after||after_money!=money)return unknown("Selection history actual complete result differs");
    op.confirmed=true;return action::Outcome::Confirmed;
}

bool observe_history(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);const auto found=history_operations.find(request.action_uuid);
    if(!r||GetCurrentThreadId()!=r->thread||found==history_operations.end()||found->second.binding!=binding||
       found->second.request!=request||!found->second.confirmed){e="Selection history observation lacks exact confirmed operation";return false;}
    const auto& op=found->second;action::SelectionMoveState actual;action::Ledger money;
    if(!current(*r,binding,op.poses,op.before,op.inverse_graph,actual,money,e))return false;
    if(actual!=op.evidence.selection_move_after||money!=op.evidence.after){e="Selection history complete result changed before ACK";return false;}
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}

bool retire_history(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence value;if(!observe_history(binding,request,value,e))return false;
    auto& op=history_operations.at(request.action_uuid);
    if(op.retired){e="Selection history already retired";return false;}
    op.retired=true;return true;
}
