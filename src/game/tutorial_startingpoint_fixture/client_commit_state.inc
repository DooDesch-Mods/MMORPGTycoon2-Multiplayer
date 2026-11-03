bool client_commit_state(const Request& request,json& result,std::string& e){
    if(request.operation!="client_bridges_commit_state"&&request.operation!="client_startingpoint_commit_state")
        return fail(e,"Completion read requires an exact supported research operation");
    const std::string tech=!request.tech_key.empty()?request.tech_key:request.operation=="client_startingpoint_commit_state"?"startingpoint":"bridges";
    const std::string label=tech=="startingpoint"?"Starting Points":"Bridges";
    const bool client=!Config::instance().is_host;
    Pin p,after;tutorial_development::StartingpointSnapshot snapshot,stable;json diagnostic,stable_diagnostic;
    if(!pin(request,p,e,client)||!p.ordered)return fail(e,e.empty()?label+" completion read requires actual ordered mode":e);
    action_session::Context context,current;action_session::Binding binding,current_binding;
    if(!action_session::selected_session().current(context,e)||!typed_action_runtime::network_binding(context.snapshot_uuid,binding,e)||
       (p.generic?!begin_snapshot(p,snapshot,diagnostic,e):!tutorial_development::read_research(tech,snapshot,diagnostic,e,true)))return false;
    if(!same(p.world,snapshot.origin)||p.lounge!=snapshot.lounge||p.project!=snapshot.project||!snapshot.row||
       (!p.generic&&!snapshot.row->purchased)||snapshot.row->custom)return fail(e,label+" completion native project/source owner differs");
    bool completed=false;
    if(snapshot.project){
        if(!snapshot.row->project_present||snapshot.row->deployed)return fail(e,label+" completion current project differs");
        const auto code=complete_state(p.image,p.lounge,completed);
        if(code)return fail(e,label+" completion getter exception="+std::to_string(code));
    }else if(!p.generic&&!snapshot.row->deployed)return fail(e,label+" completion has neither current nor deployed project");
    if(!pin(request,after,e,client)||(p.generic?!begin_snapshot(after,stable,stable_diagnostic,e):!tutorial_development::read_research(tech,stable,stable_diagnostic,e,true))||
       !same_research_native(p,after,snapshot,stable)||diagnostic!=stable_diagnostic)
        return fail(e,e.empty()?label+" completion native owners changed during read":e);
    if(!action_session::selected_session().current(current,e)||current!=context||
       !typed_action_runtime::network_binding(current.snapshot_uuid,current_binding,e)||current_binding!=binding)
        return fail(e,e.empty()?label+" completion Context/Binding changed during read":e);
    result={{"operation",request.operation},{"token",request.token},{"attempted",false},{"returned",true},{"confirmed",true},
        {"unknown",false},{"read_only",true},{"implementation_complete",completed},
        {"deployed",snapshot.row->deployed},{"development",stable_diagnostic}};
    if(p.generic)result.update({{"tech_key",tech},{"purchased",snapshot.row->purchased},{"project_proof",snapshot.admission}});
    return true;
}
