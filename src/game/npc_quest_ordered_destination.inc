bool ordered_scope(const a::Binding& binding,const a::Request& request,std::string& e){
    if(request.action!=a::Action::SetNpcQuestDestination||!a::ordered_quest_destination(request.npc_quest))
        return fail(e,"Ordered Quest destination requires a same-Region Building target");
    a::Context context;if(!a::selected_session().current(context,e))return false;
    if(Config::instance().is_host!=(context.role==a::Role::Host))return fail(e,"Ordered Quest destination actual role differs");
    return a::tick_commands::authority::matches(context,binding,request,e);
}
bool execution_admission(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& e){
    if(a::tick_commands::authority::active()){
        if(authority)return fail(e,"Ordered Quest destination forbids Host result state and RNG adoption");
        return ordered_scope(binding,request,e);
    }
    return !typed_action_runtime::controller_status().value("ordered",false)||fail(e,"Ordered Quest destination requires current consumed command scope");
}
bool capture_binding(const std::string& snapshot,a::Binding& binding,std::string& e){
    const auto controller=typed_action_runtime::controller_status();
    if(controller.value("ordered",false)){
        bool inside=false;if(!typed_action_runtime::original_go_admission(inside,e))return false;
        if(inside){
            if(!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))
                return fail(e,"network admission requires idle unlocked Controller");
            return typed_action_runtime::read_binding(snapshot,binding,e);
        }
    }
    return typed_action_runtime::network_binding(snapshot,binding,e);
}
bool capture_context(const std::string& snapshot,const a::Binding& binding,std::optional<a::Context>& out,std::string& e){
    out.reset();if(!typed_action_runtime::controller_status().value("ordered",false))return true;
    a::Context context;a::Binding actual;
    if(!a::selected_session().current(context,e)||context.snapshot_uuid!=snapshot||
       Config::instance().is_host!=(context.role==a::Role::Host)||!capture_binding(snapshot,actual,e)||actual!=binding)
        return fail(e,e.empty()?"Ordered Quest destination captured Session/Binding differs":e);
    out=context;return true;
}
bool current_capture(const Pending& saved,std::string& e){
    if(!saved.context)return true;
    std::optional<a::Context> actual;
    return capture_context(saved.snapshot,saved.binding,actual,e)&&(actual==saved.context||fail(e,"Ordered Quest destination captured Session changed"));
}
bool complete_effect(bool ordered,const a::Binding& binding,const a::Request& request,const qr::Witness& witness,
                     a::NpcQuestState& value,json& refs,std::string& targets,std::string& e){
    return !ordered||quest_list::authoring_effect(binding,request.snapshot_uuid,request.npc_quest.region,witness,refs,targets,value.all_sha256,e);
}
bool protected_effect(const Operation& op,const d::Sample& sample,const json& refs,const std::string& targets,std::string& e){
    if(!op.ordered)return true;
    auto protected_state=sample.state;protected_state.global_random=op.sample.state.global_random;
    return (refs==op.refs_before&&targets==op.targets_before&&protected_state==op.sample.state&&sample.release_edits==op.sample.release_edits)||
        fail(e,"Ordered Quest destination changed active references, target pool, Region state/RNG or release edits");
}
