namespace modular {
namespace {
bool history_request(const action::Request& request){return (request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&request.canonical_record_key=="modular:"+request.target_action_uuid;}
struct History {
    action::Binding binding;action::Request request;action::Evidence evidence;Sample after;
    uintptr_t graph=0,inverse=0;bool attempted=false,returned=false,confirmed=false,retired=false,selection_restored=false;
    std::map<uint64_t,Scenery> inverse_scenery;std::map<uint64_t,Building> inverse_buildings;std::set<uint64_t> inverse_destroy;
    std::vector<action::DuplicateCash> inverse_cash;std::optional<Building> inverse_layout;std::string cause,cleanup_cause;
    Dependencies dependency_before,dependency_after;std::optional<DependencyPlan> dependency_inverse;
};
std::map<std::string,History> histories,history_quarantined;
bool source_for(const action::Binding& binding,const action::Request& request,Port*& out,std::string& e){
    const auto found=operations.find(request.target_action_uuid);
    if(!history_request(request)||found==operations.end()||found->second.binding!=binding||found->second.request.snapshot_uuid!=request.snapshot_uuid||!found->second.confirmed||!found->second.retired||!found->second.port)return q::fail(e,"Modular history lacks its confirmed retired original source");
    out=found->second.port.get();return true;
}
bool content_equal(action::ModularState actual,const action::ModularState& required){actual.random=required.random;actual.region_random=required.region_random;return actual==required;}
bool authority_seed(Runtime& r,Sample& before,const action::Evidence* authority,std::string& e){
    if(!authority)return true;uint32_t fault=0;{live_host_state::apply_timing::PageReadScope boundary(live_host_state::apply_timing::PageBoundary::OriginalCall);fault=scenery_stroke_calls::seed(before.props.random,before.region_random,*authority->modular_before);}
    if(fault)return q::fail(e,"Modular authority RNG seed SEH="+std::to_string(fault)+"; no replay");
    std::array<uint64_t,4> global{},regional{};if(!q::at(r.memory,before.props.random,0,global,e)||!q::at(r.memory,before.region_random,0,regional,e)||global!=authority->modular_before->random||regional!=authority->modular_before->region_random)return q::fail(e,e.empty()?"Modular authority RNG seed readback differs":e);
    before.state.random=global;before.state.region_random=regional;before.props.state.random=global;return true;
}
bool evidence_equal(const action::Evidence& actual,const action::Evidence& expected){return actual.before==expected.before&&actual.after==expected.after&&actual.modular_before==expected.modular_before&&actual.modular_after==expected.modular_after&&actual.modular_result==expected.modular_result&&actual.target_retired==expected.target_retired;}
bool wire_fits(const action::Request& request,const action::Evidence& evidence,std::string& e){auto owned=std::make_unique<action::Message>();auto& message=*owned;message.kind=action::Kind::HostResult;message.session=1;message.revision=1;message.required_mask=15;message.request=request;message.outcome=action::Outcome::Confirmed;message.evidence=evidence;message.evidence.retirement_confirmed=true;std::vector<uint8_t> bytes;return action::encode(message,bytes,e);}
bool result_current(Runtime& r,const Port& source,const Sample& expected,Sample& actual,std::string& e){return module_sample(r,source.binding,source.request.modular.region,source.aliases,actual,e)&&scenery_stroke::same_binding(expected,actual)&&actual.state==expected.state&&actual.props.money==expected.props.money&&actual.props.undo==expected.props.undo||q::fail(e,e.empty()?"Modular complete result/Undo changed before acknowledgement":e);}
action::Outcome perform_history(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);Port* source=nullptr;uint64_t revision=0;
    if(!r||!installed||!r->ready||r->thread!=GetCurrentThreadId()||request.snapshot_uuid!=net::current_snapshot_id()||bool(authority)==Config::instance().is_host||!source_for(binding,request,source,e)||!contract::pending(typed_action_runtime::controller_status(),request,revision,e)){if(e.empty())e="Modular history exact pending owner/role is unavailable";return action::Outcome::RejectedNoEffect;}
    if(histories.contains(request.action_uuid)||history_quarantined.contains(request.action_uuid)){e="Modular history already attempted; no replay";return action::Outcome::Unknown;}
    if(authority&&!action::valid_modular_effect(request,*authority)){e="Modular history Host evidence is invalid";return action::Outcome::RejectedNoEffect;}
    const bool undo=request.action==action::Action::UndoBuilding;const bool prefab=source->request.modular.mode==0;Sample before;uintptr_t graph=0;
    if(!code(*r,e)||!module_sample(*r,binding,source->request.modular.region,source->aliases,before,e)||!building_history_slice::graph_for(request.action,request.target_action_uuid,graph,e))return action::Outcome::RejectedNoEffect;
    if(authority&&(!content_equal(before.state,*authority->modular_before)||before.props.money!=authority->before)){e="Modular history Client complete before state differs from Host";return action::Outcome::RejectedNoEffect;}
    Dependencies dependency_before;if(!dependencies(*r,binding,before,&source->after_dependencies.quests,dependency_before,e))return action::Outcome::RejectedNoEffect;
    std::optional<DependencyPlan> plan;std::optional<Building> layout_before;std::map<uint64_t,Scenery> makes;std::map<uint64_t,Building> make_buildings;std::set<uint64_t> destroys;uintptr_t destroyed=0;
    const auto cash=undo?source->inverse_cash:scenery_stroke_history::opposite(source->inverse_cash);
    if(!prefab){const auto expected=source->layout_graphs.find(graph);Building actual;const auto handle=before.props.handles.find(source->request.modular.target);
        if(expected==source->layout_graphs.end()||handle==before.props.handles.end()||handle->second.uid!=source->target_uid||!module_graph(*r,graph,source->target_uid,expected->second,e)||!duplicate_edit::history_building_value(*r,handle->second.object,actual,e))return action::Outcome::RejectedNoEffect;layout_before=std::move(actual);
    }else{
        if(source->buildings.size()!=1){e="Modular Prefab source lacks its one original Building";return action::Outcome::RejectedNoEffect;}const auto born=source->buildings.begin()->first;
        if(undo){const auto handle=before.props.handles.find(source->aliases.at(born));Building actual;if(handle==before.props.handles.end()||handle->second.uid!=born||!duplicate_edit::history_building_value(*r,handle->second.object,actual,e))return action::Outcome::RejectedNoEffect;destroyed=handle->second.object;source->buildings[born]=std::move(actual);makes=source->removed;destroys.insert(born);plan.emplace();if(!dependency_plan(before,born,dependency_before,*plan,e))return action::Outcome::RejectedNoEffect;}
        else{if(before.props.objects.contains(source->aliases.at(born))||!remove_diagnostics::history_target_absent(request.snapshot_uuid,source->aliases.at(born),born,e))return action::Outcome::RejectedNoEffect;
            make_buildings=source->buildings;for(auto& [uid,saved]:source->removed){const auto actual=before.scenery.find(uid);if(actual==before.scenery.end()||!scenery_stroke_history::same_inputs(actual->second,saved)){e="Modular Redo current Scenery owner/inputs differ";return action::Outcome::RejectedNoEffect;}saved=actual->second;destroys.insert(uid);}}
        if(!undo){if(!source->dependency_history){e="Modular Redo lacks complete original dependency receipts";return action::Outcome::RejectedNoEffect;}plan=source->dependency_history;plan->seen_services.clear();plan->seen_quests.clear();}
        auto related=[&](uintptr_t node,uintptr_t ap,std::string& why){return plan&&!undo&&dependency_node(*r,node,ap,*plan,why);};
        if(!prefab_graph(*r,graph,makes,make_buildings,destroys,cash,e,related)||(!undo&&!dependency_graph(*r,graph,*plan,e)))return action::Outcome::RejectedNoEffect;
    }
    std::vector<duplicate_native::Handle> selection;std::array<uint8_t,9> filter{};uintptr_t camera=0;if(!duplicate_edit::history_local_selection(*r,before.props,selection,filter,camera,e))return action::Outcome::RejectedNoEffect;
    duplicate_edit::HistorySource known;if(prefab)for(const auto& [uid,key]:source->aliases){(void)key;known.known.emplace(uid,duplicate_history::Known{});}
    auto& op=histories.emplace(request.action_uuid,History{binding,request}).first->second;op.graph=graph;op.dependency_before=dependency_before;
    auto unknown=[&](){op.cause=e.empty()?"Modular history failed without its original cause":e;std::string cleanup;if(!op.selection_restored){if(duplicate_edit::history_restore(*r,before.props,selection,filter,camera,known,cleanup))op.selection_restored=true;else{op.cleanup_cause=cleanup;op.cause+="; cleanup: "+cleanup;}}e=op.cause;return action::Outcome::Unknown;};
    op.attempted=true;if(!authority_seed(*r,before,authority,e))return unknown();op.evidence.before=before.props.money;op.evidence.modular_before=before.state;
    if(building_history_slice::perform(request.action,request.target_action_uuid,e)!=action::Outcome::Confirmed)return unknown();op.returned=true;
    const auto opposite=undo?action::Action::RedoBuilding:action::Action::UndoBuilding;
    if(!building_history_slice::graph_for(opposite,request.target_action_uuid,op.inverse,e))return unknown();
    if(!prefab){op.inverse_layout=layout_before;if(!module_graph(*r,op.inverse,source->target_uid,*op.inverse_layout,e))return unknown();source->layout_graphs[op.inverse]=*op.inverse_layout;}
    else{
        const auto born=source->buildings.begin()->first;
        if(undo){op.inverse_buildings=source->buildings;for(const auto& [uid,saved]:source->removed){(void)saved;op.inverse_destroy.insert(uid);}if(!remove_diagnostics::retire_undone_building(request.snapshot_uuid,source->aliases.at(born),born,e))return unknown();}
        else{op.inverse_scenery=source->removed;op.inverse_destroy.insert(born);if(!remove_diagnostics::restore_redone_modular(request.snapshot_uuid,source->aliases.at(born),born,e))return unknown();}
        op.inverse_cash=scenery_stroke_history::opposite(cash);
        if(undo){plan->seen_services.clear();plan->seen_quests.clear();}auto related=[&](uintptr_t node,uintptr_t ap,std::string& why){return plan&&undo&&dependency_node(*r,node,ap,*plan,why);};
        if(!prefab_graph(*r,op.inverse,op.inverse_scenery,op.inverse_buildings,op.inverse_destroy,op.inverse_cash,e,related)||(undo&&!dependency_graph(*r,op.inverse,*plan,e)))return unknown();if(undo)op.dependency_inverse=plan;
    }
    host::WorldState fresh;duplicate_reader::Census census;auto find=[&](uint64_t uid,uintptr_t& object,std::string& why){return group_edit::find(*r,uid,object,why);};
    if(!world(*r,fresh,e)||!host::same_world(before.props.world.origin,fresh.origin)||!duplicate_reader::census(r->memory,r->image,uintptr_t(fresh.origin.map),fresh.root,find,census,e))return unknown();
    std::set<uint64_t> alive;std::map<uint64_t,std::string> prior,restorable;
    for(const auto& [object,row]:census.objects){(void)object;if(row.family==Family::Scenery)alive.insert(row.handle.uid);}
    for(const auto& [uid,saved]:before.scenery){const auto key=std::find_if(before.props.handles.begin(),before.props.handles.end(),[&](const auto& row){return row.second.uid==uid&&row.second.object==saved.object;});if(key==before.props.handles.end()){e="Modular history prior Scenery has no canonical owner";return unknown();}prior.emplace(uid,key->first);}
    for(const auto& [uid,saved]:source->removed){(void)saved;restorable.emplace(uid,source->aliases.at(uid));}
    auto retire_alias=[&](const auto& ids,auto& keys,std::string& why){return remove_diagnostics::retire_region_scenery(request.snapshot_uuid,request.action_uuid,ids,keys,why);};
    auto restore_alias=[&](uint64_t uid,const std::string& key,std::string& why){uintptr_t object=0;Scenery actual;const auto expected=source->removed.find(uid);return expected!=source->removed.end()&&find(uid,object,why)&&scenery_make_history::live(r->memory,r->image,object,uid,before.props.world.root,before.region,find,actual,why)&&(scenery_stroke_history::same_inputs(actual,expected->second)||q::fail(why,"Modular restored Scenery full original inputs differ"))&&remove_diagnostics::restore_redone_building(request.snapshot_uuid,key,uid,why);};
    auto read=[&](std::string& why){return module_sample(*r,binding,source->request.modular.region,source->aliases,op.after,why)&&scenery_stroke::same_binding(before,op.after);};scenery_alias_order::Receipt settled;
    if(!scenery_alias_order::capture(prior,alive,restorable,retire_alias,restore_alias,read,settled,e))return unknown();
    if(!duplicate_edit::history_restore(*r,before.props,selection,filter,camera,known,e))return unknown();op.selection_restored=true;
    if(!dependencies(*r,binding,op.after,&dependency_before.quests,op.dependency_after,e))return unknown();
    if(prefab){if(!dependency_result(*r,undo,*plan,dependency_before,op.dependency_after,destroyed,e))return unknown();}else if(dependency_before.sha!=op.dependency_after.sha){e="Modular layout history changed an unrelated complete Service/Quest witness";return unknown();}
    action::ModularEffect effect;if(!value::difference(before.props.objects,op.after.props.objects,scenery_stroke::digest,effect,e))return unknown();effect.cash=prefab?cash:std::vector<action::DuplicateCash>{};dependency_effect(dependency_before,op.dependency_after,plan?plan->related:std::set<std::string>{},effect);
    op.evidence.after=op.after.props.money;op.evidence.modular_after=op.after.state;op.evidence.modular_result=std::move(effect);op.evidence.undo_confirmed=true;op.evidence.selection_confirmed=true;
    if(!action::valid_modular_effect(request,op.evidence)||!wire_fits(request,op.evidence,e)){if(e.empty())e="Modular history complete original effect is invalid";return unknown();}
    if(authority&&!evidence_equal(op.evidence,*authority)){e="Modular history actual complete result differs from Host";return unknown();}
    Sample final;if(!result_current(*r,*source,op.after,final,e))return unknown();op.after=std::move(final);source->after_dependencies=op.dependency_after;if(prefab)source->dependency_history=plan;op.confirmed=true;return action::Outcome::Confirmed;
}
}
}
