namespace {
path_history_graph::Snapshot history_snapshot(const Graph& value){return {value.path_uids,value.node_uids,value.rows};}
bool history_state(region::Runtime&,const Graph& value,const path_history_graph::Snapshot& order,action::PathState& out,std::string& e){
    json rows;
    if(!path_history_graph::rebase(history_snapshot(value),order,rows,e))return false;
    out=value.state;out.cost=0;return region::sha256(rows.dump(),out.graph_sha256,e);
}
struct HistoryOperation {
    action::Binding binding;action::Request request;action::Evidence evidence;
    path_history_graph::Snapshot order;int32_t region_id=-1;uintptr_t inverse_graph=0;
    bool confirmed=false,retired=false;std::string cause;
};
std::map<std::string,HistoryOperation> history_operations;
}

action::Outcome perform_history(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    if(world_path_runtime::owns_history(request.target_action_uuid))
        return world_path_runtime::perform_history(binding,request,authority,e);
    auto* r=runtime(e);uint64_t revision=0;
    if(!r||!path.ready){if(e.empty())e="Path history native runtime unavailable";return action::Outcome::RejectedNoEffect;}
    if(GetCurrentThreadId()!=r->thread){e="Path history Mainthread differs";return action::Outcome::RejectedNoEffect;}
    if(bool(authority)==Config::instance().is_host){e="Path history authority/role differs";return action::Outcome::RejectedNoEffect;}
    if(!ui_move_runtime::contract::pending(typed_action_runtime::controller_status(),request,revision,e))return action::Outcome::RejectedNoEffect;
    if(history_operations.contains(request.action_uuid)){e="Path history already attempted; no replay";return action::Outcome::Unknown;}
    const auto found=path.operations.find(request.target_action_uuid);
    if((request.action!=action::Action::UndoBuilding&&request.action!=action::Action::RedoBuilding)||
       request.canonical_record_key!="path:"+request.target_action_uuid||found==path.operations.end()){
        e="Path history original source/direction/key missing";return action::Outcome::RejectedNoEffect;}
    const auto& source=found->second;
    if(source.binding!=binding||source.request.snapshot_uuid!=request.snapshot_uuid||!source.confirmed||!source.retired||
       !source.history_before||!source.history_after||!source.history_graph||
       (source.request.action!=action::Action::PlacePath&&source.request.action!=action::Action::RemovePath)){
        e="Path history lacks confirmed retired source in this World";return action::Outcome::RejectedNoEffect;}
    const bool undo_direction=request.action==action::Action::UndoBuilding;
    const auto& old_order=undo_direction?*source.history_after:*source.history_before;
    const auto& new_order=undo_direction?*source.history_before:*source.history_after;
    const int32_t id=source.request.action==action::Action::RemovePath?source.request.path_delete.region:source.request.path.region;
    const int32_t price=source.evidence.path_after.cost;
    if(price<0){e="Path history original cash amount is negative";return action::Outcome::RejectedNoEffect;}
    const bool destroying=(source.request.action==action::Action::PlacePath)==undo_direction;
    action::Ledger money,expected_money;region::LocalView view;Graph current;uintptr_t global=0,graph_pointer=0;
    action::Evidence evidence;
    if(!region::read(*r,id,0,view,e)||!region::bound(view,binding,request.snapshot_uuid,e)||
       !graph(*r,view,current,e)||!region::ledger(money,e)||!terrain::terrain_state(*r,view,evidence.terrain_before,global,e)||
       !building_history_slice::graph_for(request.action,request.target_action_uuid,graph_pointer,e)||
       !building_history_slice::cash_effect(money,destroying?price:-price,expected_money,e))return action::Outcome::RejectedNoEffect;
    if(!history_state(*r,current,old_order,evidence.path_before,e))return action::Outcome::RejectedNoEffect;
    json expected_rows,actual_rows;
    if(!path_history_graph::rebase(old_order,old_order,expected_rows,e)||
       !path_history_graph::rebase(history_snapshot(current),old_order,actual_rows,e))return action::Outcome::RejectedNoEffect;
    if(actual_rows!=expected_rows){e="Path history complete current topology differs from original source";return action::Outcome::RejectedNoEffect;}
    evidence.before=money;evidence.region_before=view.state;
    if(authority&&(authority->before!=money||authority->after!=expected_money||
       authority->path_before!=evidence.path_before||!action::same_region_content(view.state,authority->region_before)||
       !action::same_terrain_content(evidence.terrain_before,authority->terrain_before))){
        e="Path history Client topology/Region/Terrain/full Ledger BEFORE differs from Host";return action::Outcome::RejectedNoEffect;}
    HistoryOperation value;value.binding=binding;value.request=request;value.region_id=id;value.order=new_order;value.evidence=evidence;
    if(authority){value.evidence.region_before=authority->region_before;value.evidence.terrain_before=authority->terrain_before;}
    auto& op=history_operations.emplace(request.action_uuid,std::move(value)).first->second;
    auto unknown=[&](const std::string& cause){e=op.cause=cause;return action::Outcome::Unknown;};
    if(authority&&!native_error(terrain::seed(view.random,global,*authority),"Path history authority RNG seed",e))return unknown(e);
    const auto outcome=building_history_slice::perform(request.action,request.target_action_uuid,e);
    if(outcome!=action::Outcome::Confirmed){op.cause=e;return authority?unknown(e):outcome;}
    const auto inverse=undo_direction?action::Action::RedoBuilding:action::Action::UndoBuilding;
    region::LocalView after;Graph result;uintptr_t after_global=0;
    if(!building_history_slice::graph_for(inverse,request.target_action_uuid,op.inverse_graph,e)||
       !region::read(*r,id,view.region,after,e)||!region::bound(after,binding,request.snapshot_uuid,e)||
       !graph(*r,after,result,e)||!history_state(*r,result,new_order,op.evidence.path_after,e)||
       !terrain::terrain_state(*r,after,op.evidence.terrain_after,after_global,e)||!region::ledger(op.evidence.after,e))return unknown(e);
    if(global!=after_global)return unknown("Path history global RNG owner changed");
    if(!path_history_graph::rebase(new_order,new_order,expected_rows,e)||
       !path_history_graph::rebase(history_snapshot(result),new_order,actual_rows,e))return unknown(e);
    if(actual_rows!=expected_rows||op.evidence.after!=expected_money)return unknown("Path history original complete topology or cash inverse differs");
    op.evidence.region_after=after.state;op.evidence.path_after.cost=price;op.evidence.undo_confirmed=true;
    if(!action::path_history_effect(request,op.evidence,e))return unknown(e);
    if(authority&&(op.evidence.after!=authority->after||op.evidence.path_after!=authority->path_after||
       op.evidence.region_after!=authority->region_after||op.evidence.terrain_after!=authority->terrain_after))
        return unknown("Path history Client complete original result differs from Host");
    op.confirmed=true;return action::Outcome::Confirmed;
}

bool observe_history(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    if(world_path_runtime::owns_history(request.target_action_uuid))
        return world_path_runtime::observe_history(binding,request,out,e);
    auto* r=runtime(e);const auto found=history_operations.find(request.action_uuid);
    if(!r||GetCurrentThreadId()!=r->thread||found==history_operations.end()||found->second.binding!=binding||
       found->second.request!=request||!found->second.confirmed){e="Path history observation lacks exact confirmed operation";return false;}
    const auto& op=found->second;region::LocalView view;Graph actual;action::Evidence current;uintptr_t global=0;
    if(!region::read(*r,op.region_id,0,view,e)||!region::bound(view,binding,request.snapshot_uuid,e)||!graph(*r,view,actual,e)||
       !history_state(*r,actual,op.order,current.path_after,e)||!terrain::terrain_state(*r,view,current.terrain_after,global,e)||
       !region::ledger(current.after,e))return false;
    current.path_after.cost=op.evidence.path_after.cost;
    current.terrain_after.global_random=op.evidence.terrain_after.global_random;
    if(view.state!=op.evidence.region_after||current.path_after!=op.evidence.path_after||
       current.terrain_after!=op.evidence.terrain_after||current.after!=op.evidence.after){e="Path history complete result changed before ACK";return false;}
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}

bool retire_history(const action::Binding& binding,const action::Request& request,std::string& e){
    if(world_path_runtime::owns_history(request.target_action_uuid))
        return world_path_runtime::retire_history(binding,request,e);
    action::Evidence actual;if(!observe_history(binding,request,actual,e))return false;
    auto& op=history_operations.at(request.action_uuid);
    if(op.retired){e="Path history already retired";return false;}
    op.retired=true;return true;
}
