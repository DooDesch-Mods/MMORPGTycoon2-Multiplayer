namespace group_object {
bool retire(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence actual;if(!observe(binding,request,actual,e))return false;auto& op=operations.at(request.action_uuid);
    if(op.retired)return q::fail(e,"Group action already retired");
    if(!building_history_slice::remember_group_object(request.action_uuid,op.graph,e))return false;op.retired=true;return true;
}
bool targets(const action::Binding& binding,const action::Request& request,bool& move_kind,bool& restoring,std::vector<std::string>& out,std::string& e){
    out.clear();restoring=false;
    if(history_request(request)){Operation* source=nullptr;if(!history_source(binding,request,source,e))return false;
        move_kind=moving(source->request);restoring=!move_kind&&request.action==action::Action::UndoBuilding;out=source->request.group_object.members;return true;}
    if((request.action!=action::Action::MoveGroup&&request.action!=action::Action::RemoveGroup)||!action::valid_group_object(moving(request),request.group_object))
        return q::fail(e,"Group current shared target request is invalid");
    move_kind=moving(request);out=request.group_object.members;return true;
}
bool publication(const action::Binding& binding,const action::Request& request,action::GroupObjectState& before,action::GroupObjectState& after,std::string& e){
    const action::Evidence* current=nullptr;
    if(history_request(request)){const auto found=history_operations.find(request.action_uuid);
        if(found==history_operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)return q::fail(e,"Group history publication lacks its confirmed native operation");current=&found->second.evidence;
    }else{const auto found=operations.find(request.action_uuid);
        if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)return q::fail(e,"Group publication lacks its confirmed native operation");current=&found->second.evidence;}
    if(!current->group_object_before||!current->group_object_after)return q::fail(e,"Group publication lacks its complete native state pair");
    before=*current->group_object_before;after=*current->group_object_after;return true;
}
void retire_session(){
    if(captured)reject("Group Move cancelled after its source session retired");captured.reset();
    for(auto it=operations.begin();it!=operations.end();){
        if(it->second.attempted&&!it->second.confirmed){trim(it->second.before);trim(it->second.after);auto node=operations.extract(it++);quarantined.insert(std::move(node));}
        else it=operations.erase(it);
    }
    for(auto it=history_operations.begin();it!=history_operations.end();){
        if(it->second.attempted&&!it->second.confirmed){trim(it->second.after);auto node=history_operations.extract(it++);history_quarantined.insert(std::move(node));}
        else it=history_operations.erase(it);
    }
}
json status(){
    json rows=json::object(),histories=json::object();
    for(const auto& [id,op]:operations)rows[id]={{"group_key",op.request.group_object.key},{"attempted",op.attempted},{"returned",op.returned},{"selection_restored",op.selection_restored},
        {"confirmed",op.confirmed},{"retired",op.retired},{"original_calls",op.original_calls},{"graph",op.graph},{"cause",op.cause},{"cleanup_cause",op.cleanup_cause}};
    for(const auto& [id,op]:history_operations)histories[id]={{"source_action_uuid",op.request.target_action_uuid},{"attempted",op.attempted},{"returned",op.returned},
        {"selection_restored",op.selection_restored},{"confirmed",op.confirmed},{"retired",op.retired},{"graph",op.graph},{"inverse",op.inverse},{"cause",op.cause},{"cleanup_cause",op.cleanup_cause}};
    return {{"pending",pending()},{"captures",captures},{"submitted",submissions},{"rejected",rejections},{"cause",capture_cause},{"operations",std::move(rows)},
        {"quarantined",quarantined.size()},{"history",std::move(histories)},{"history_quarantined",history_quarantined.size()}};
}
}
