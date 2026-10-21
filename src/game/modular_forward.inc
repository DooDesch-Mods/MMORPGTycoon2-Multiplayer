namespace modular {
bool targets(const action::Binding& binding,const action::Request& request,std::vector<std::string>& out,std::string& e){out.clear();auto* r=published.load(std::memory_order_acquire);if(!r||r->thread!=GetCurrentThreadId())return q::fail(e,"Modular publication targets require the bound Mainthread");Port* source=nullptr;
    if(history_request(request)&&!source_for(binding,request,source,e))return false;const auto& input=source?source->request.modular:request.modular;Sample current;
    if(!module_sample(*r,binding,input.region,source?source->aliases:std::map<uint64_t,std::string>{},current,e))return false;
    for(const auto& [key,row]:current.props.objects)if((row.family==Family::Scenery&&row.region==input.region)||(row.family==Family::Building&&(input.mode?key==input.target:(source&&std::any_of(source->buildings.begin(),source->buildings.end(),[&](const auto& value){return source->aliases.at(value.first)==key;})))))out.push_back(key);return true;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    if(history_request(request))return perform_history(binding,request,authority,e);
    auto* r=published.load(std::memory_order_acquire);uint64_t revision=0;
    if(!r||!installed||!r->ready||r->thread!=GetCurrentThreadId()||request.action!=action::Action::EditModularBuilding||!action::valid_modular(request.modular)||request.snapshot_uuid!=net::current_snapshot_id()||bool(authority)==Config::instance().is_host||!contract::pending(typed_action_runtime::controller_status(),request,revision,e)){if(e.empty())e="Modular exact pending action/role is unavailable";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)||quarantined.contains(request.action_uuid)){e="Modular original transaction already attempted; no replay";return action::Outcome::Unknown;}
    if(authority&&!action::valid_modular_effect(request,*authority)){e="Modular Host evidence is invalid";return action::Outcome::RejectedNoEffect;}
    Sample before;if(!module_sample(*r,binding,request.modular.region,{},before,e,request.modular.target))return action::Outcome::RejectedNoEffect;
    if(authority&&(!content_equal(before.state,*authority->modular_before)||before.props.money!=authority->before)){e="Modular Client complete objects/bugs/Release/Ledger differ from Host";return action::Outcome::RejectedNoEffect;}
    auto& op=operations.emplace(request.action_uuid,Operation{binding,request}).first->second;
    op.port=std::make_shared<Port>(Port{*r,binding,request,std::move(before),{},op.receipt});auto& port=*op.port;
    // Seed before native preparation. Keep a separate attempt flag because the
    // transaction receipt starts at its first reconstruction operation.
    bool seeded=false;if(authority){seeded=true;if(!authority_seed(*r,port.before,authority,e)){op.receipt.attempted=true;op.cause=e;return action::Outcome::Unknown;}}
    const bool returned=modular_transaction::execute(port,request.modular,op.receipt,e);
    if(!returned){op.receipt.attempted=op.receipt.attempted||seeded;op.cause=e;if(port.no_effect&&op.receipt.cleanup_cause.empty())return action::Outcome::RejectedNoEffect;return op.receipt.attempted?action::Outcome::Unknown:action::Outcome::RejectedNoEffect;}
    op.graph=op.receipt.graph;op.evidence.before=port.before.props.money;op.evidence.after=port.after.props.money;op.evidence.modular_before=port.before.state;op.evidence.modular_after=port.after.state;op.evidence.modular_result=port.effect;op.evidence.undo_confirmed=true;op.evidence.selection_confirmed=port.selection_restored;
    if(!action::valid_modular_effect(request,op.evidence)||!wire_fits(request,op.evidence,e)){op.cause=e.empty()?"Modular original complete effect fails the action contract":e;e=op.cause;return action::Outcome::Unknown;}
    if(authority&&!evidence_equal(op.evidence,*authority)){e=op.cause="Modular actual original complete result differs from Host";return action::Outcome::Unknown;}
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r||r->thread!=GetCurrentThreadId())return q::fail(e,"Modular observe requires the bound Mainthread");
    if(history_request(request)){const auto found=histories.find(request.action_uuid);Port* source=nullptr;if(found==histories.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!source_for(binding,request,source,e))return q::fail(e,e.empty()?"Modular history observe lacks its confirmed exact operation":e);auto& op=found->second;Sample actual;uintptr_t inverse=0;const auto opposite=request.action==action::Action::UndoBuilding?action::Action::RedoBuilding:action::Action::UndoBuilding;
        if(!result_current(*r,*source,op.after,actual,e)||!building_history_slice::graph_for(opposite,request.target_action_uuid,inverse,e)||inverse!=op.inverse)return q::fail(e,e.empty()?"Modular history inverse/top graph changed before acknowledgement":e);
        if(op.inverse_layout){if(!module_graph(*r,inverse,source->target_uid,*op.inverse_layout,e))return false;}else{auto plan=op.dependency_inverse;if(plan){plan->seen_services.clear();plan->seen_quests.clear();}auto related=[&](uintptr_t node,uintptr_t ap,std::string& why){return plan&&dependency_node(*r,node,ap,*plan,why);};if(!prefab_graph(*r,inverse,op.inverse_scenery,op.inverse_buildings,op.inverse_destroy,op.inverse_cash,e,related)||(plan&&!dependency_graph(*r,inverse,*plan,e)))return false;}
        Dependencies deps;if(!dependencies(*r,binding,actual,&op.dependency_after.quests,deps,e)||deps.sha!=op.dependency_after.sha)return q::fail(e,e.empty()?"Modular history complete Service/Quest result changed before acknowledgement":e);
        out=op.evidence;out.retirement_confirmed=op.retired;return true;
    }
    const auto found=operations.find(request.action_uuid);if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.port)return q::fail(e,"Modular observe lacks its confirmed exact operation");auto& op=found->second;auto& port=*op.port;Sample actual;
    if(!result_current(*r,port,port.after,actual,e))return false;
    if(request.modular.mode){if(!module_graph(*r,op.graph,port.target_uid,port.layout_graphs.at(op.graph),e))return false;}
    else{const auto born=port.buildings.begin()->first;if(!prefab_graph(*r,op.graph,port.removed,{},std::set<uint64_t>{born},port.inverse_cash,e))return false;}
    Dependencies deps;if(!dependencies(*r,binding,actual,&port.after_dependencies.quests,deps,e)||deps.sha!=port.after_dependencies.sha)return q::fail(e,e.empty()?"Modular complete Service/Quest result changed before acknowledgement":e);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool publication(const action::Binding& binding,const action::Request& request,action::ModularEffect& out,std::string& e){
    if(history_request(request)){const auto found=histories.find(request.action_uuid);if(found==histories.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.evidence.modular_result)return q::fail(e,"Modular history publication lacks its confirmed exact operation");out=*found->second.evidence.modular_result;return true;}
    const auto found=operations.find(request.action_uuid);if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.evidence.modular_result)return q::fail(e,"Modular publication lacks its confirmed exact operation");out=*found->second.evidence.modular_result;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& e){action::Evidence actual;if(!observe(binding,request,actual,e))return false;
    if(history_request(request)){auto& op=histories.at(request.action_uuid);if(op.retired)return q::fail(e,"Modular history already retired");op.retired=true;return true;}
    auto& op=operations.at(request.action_uuid);if(op.retired)return q::fail(e,"Modular original transaction already retired");if(!building_history_slice::remember_modular(request.action_uuid,op.graph,e))return false;op.retired=true;return true;
}
void tick(){
    if(!net::session_wire::connected()){retire_session();return;}auto* r=published.load(std::memory_order_acquire);const auto controller=typed_action_runtime::controller_status();
    if(queued){std::string e;action::Binding binding;if(!r||queued->snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(queued->snapshot,binding,e)||binding!=queued->binding){reject(e.empty()?"Modular captured input World/snapshot retired before submission":e);queued.reset();}
        else if(!controller.value("started",false)||controller.value("locked",true)){reject("Modular captured input Controller is unavailable");queued.reset();}
        else if(!controller.value("pending",true)){auto input=std::move(*queued);queued.reset();if(!typed_action_runtime::submit_modular(input.id,input.intent,e))reject(e);else ++submitted;return;}}
    if(!r||controller.value("pending",true))return;
    for(auto& [id,view]:presentations){if(view.attempted)continue;const auto op=operations.find(id);if(op==operations.end()||!op->second.confirmed||!op->second.retired)continue;view.attempted=true;std::string e;action::Binding binding;bool own=false;
        if(view.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(view.snapshot,binding,e)||binding!=view.binding||!scenery_kiss::live_cursor(r->memory.process,r->image,view.receiver,own,e)||!own||!q::typed(r->memory,r->image,view.receiver,0x17414c0,e)||!code(*r,e)){view.cause=e.empty()?"Modular source preview owner retired before NewLayout":e;reject(view.cause);continue;}
        uintptr_t layout=0;if(!q::at(r->memory,view.receiver,0x378,layout,e)){view.cause=e;reject(e);continue;}if(layout){calls::Work work;work.op=calls::Op::NewLayout;work.object=view.receiver;if(!call(*r,work,e)){view.cause=e;reject(e);continue;}}view.confirmed=true;
    }
}
void retire_session(){if(queued)reject("Modular captured input cancelled after session retirement");queued.reset();presentations.clear();last_source_records=nullptr;
    for(auto it=operations.begin();it!=operations.end();){if(!it->second.confirmed&&it->second.receipt.attempted&&(!it->second.port||!it->second.port->no_effect)){auto node=operations.extract(it++);quarantined.insert(std::move(node));}else it=operations.erase(it);}
    for(auto it=histories.begin();it!=histories.end();){if(it->second.attempted&&!it->second.confirmed){auto node=histories.extract(it++);history_quarantined.insert(std::move(node));}else it=histories.erase(it);}
}
json status(){json rows=json::object(),history=json::object(),views=json::object();for(const auto& [id,op]:operations)rows[id]={{"action",uint8_t(op.request.action)},{"region",op.request.modular.region},{"mode",op.request.modular.mode},{"attempted",op.receipt.attempted},{"calls",op.receipt.calls},{"returns",op.receipt.returned},{"graph",op.graph},{"graph_committed",op.receipt.graph_committed},{"edit_closed",op.receipt.edit_closed},{"confirmed",op.confirmed},{"retired",op.retired},{"cause",op.cause},{"cleanup_cause",op.receipt.cleanup_cause}};
    for(const auto& [id,op]:operations){rows[id]["target"]=op.request.modular.target;rows[id]["before_record_sha"]=op.request.modular.before_record_sha;
    for(const auto& [id,op]:operations)rows[id]["target_records"]=op.port&&!op.request.modular.target.empty()?op.port->before.props.selected_records:json(nullptr);
        rows[id]["authored"]=op.confirmed&&op.evidence.modular_before&&op.evidence.modular_after&&op.evidence.modular_result?
            value::authored_status(*op.evidence.modular_before,*op.evidence.modular_after,*op.evidence.modular_result):json(nullptr);}
    for(const auto& [id,op]:histories)history[id]={{"attempted",op.attempted},{"returned",op.returned},{"graph",op.graph},{"inverse",op.inverse},{"confirmed",op.confirmed},{"retired",op.retired},{"cause",op.cause},{"cleanup_cause",op.cleanup_cause}};
    for(const auto& [id,op]:histories){history[id]["action"]=uint8_t(op.request.action);history[id]["target_action_uuid"]=op.request.target_action_uuid;
        history[id]["authored"]=op.confirmed&&op.evidence.modular_before&&op.evidence.modular_after&&op.evidence.modular_result?
            value::authored_status(*op.evidence.modular_before,*op.evidence.modular_after,*op.evidence.modular_result):json(nullptr);}
    for(const auto& [id,view]:presentations)views[id]={{"attempted",view.attempted},{"confirmed",view.confirmed},{"cause",view.cause}};
    return {{"enabled",installed},{"pending",pending()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},{"cause",cause},{"operations",std::move(rows)},{"history",std::move(history)},{"source_preview",std::move(views)},{"last_source_records",last_source_records},{"quarantined",quarantined.size()},{"history_quarantined",history_quarantined.size()}};
}
}
