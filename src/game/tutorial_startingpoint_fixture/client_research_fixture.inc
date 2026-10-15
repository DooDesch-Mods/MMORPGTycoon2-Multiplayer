bool same_research_native(const Pin& a,const Pin& b,const tutorial_development::StartingpointSnapshot& x,
                          const tutorial_development::StartingpointSnapshot& y){
    return same(a.world,b.world)&&a.lounge==b.lounge&&a.project==b.project&&a.development==b.development&&
        a.tutorial==b.tutorial&&same_inventory_owner(a,b)&&a.items==b.items&&x.creation==y.creation&&
        x.row==y.row&&x.tech==y.tech&&x.record==y.record&&x.project==y.project;
}
bool client_research(const Request& request,json& result,Progress*& phase,std::string& key,std::string& e){
    const bool is_show=request.operation=="client_research_show";
    Pin p;if(!pin(request,p,e,true)||!before_begin(p.development,p.project,e))return false;
    if(p.development!=request.expected_development)return fail(e,"startingpoint Client Research expected development differs");
    tutorial_development::StartingpointSnapshot snapshot;json diagnostic;
    if(!begin_snapshot(p,snapshot,diagnostic,e)||diagnostic!=p.development||snapshot.project)
        return fail(e,e.empty()?"startingpoint Client Research private baseline differs":e);
    if(!research_code(p.image,e))return false;
    int32_t price=0;if(!at(snapshot.tech,0xac,price,e))return false;
    if(price<0)return fail(e,"startingpoint Client Research native price negative");
    action_session::Context context;action_session::Binding binding;
    if(!action_session::selected_session().current(context,e)||context.role!=action_session::Role::Client||
       !typed_action_runtime::network_binding(context.snapshot_uuid,binding,e))return fail(e,e.empty()?"startingpoint Client Research session differs":e);
    live_host_state::NativePort economy(context,binding,nullptr);live_host_state::Pin ledger_before,ledger_after;
    native::SimulationState simulation_before,simulation_after;
    if(!economy.read(ledger_before,e)||!native::read_simulation_state(simulation_before,e))return false;
    json progress_before;if(!tutorial_replication::read_progress(progress_before,e))return false;
    key=request.origin_uuid+":"+std::to_string(request.epoch)+":"+std::to_string(request.generation)+":client_research";
    phase=&worlds[key];if(phase->locked)return fail(e,phase->cause);
    if((is_show&&phase->shown)||(!is_show&&phase->begun))return fail(e,"startingpoint Client Research phase consumed; no replay");
    ResearchUI ui;
    if(is_show){if(!research_tabs(p,ui,e,true))return false;}
    else if(!research_selection(p,snapshot,ui,e,false,false,true))return false;
    Pin again;tutorial_development::StartingpointSnapshot stable;json stable_diagnostic;ResearchUI stable_ui;
    if(!pin(request,again,e,true)||!begin_snapshot(again,stable,stable_diagnostic,e)||!same_research_native(p,again,snapshot,stable))
        return fail(e,e.empty()?"startingpoint Client Research owners changed before UI call":e);
    if(is_show){if(!research_tabs(again,stable_ui,e,true))return false;}
    else if(!research_selection(again,stable,stable_ui,e,false,false,true))return false;
    if(p.window!=again.window||ui!=stable_ui)return fail(e,"startingpoint Client Research UI owner changed before call");
    if(!economy.read(ledger_after,e)||ledger_before!=ledger_after||!native::read_simulation_state(simulation_after,e)||
       begin_ledger(simulation_before)!=begin_ledger(simulation_after))return fail(e,e.empty()?"startingpoint Client Research ledger changed before call":e);
    action_session::Context current;if(!action_session::selected_session().current(current,e)||current!=context)
        return fail(e,e.empty()?"startingpoint Client Research common Context changed":e);
    result.update({{"operation",request.operation},{"token",request.token},{"before",p.development},{"tutorial_before",p.tutorial},
        {"price",price},{"ledger_before",begin_ledger(simulation_before)},{"host_completion_confirmed",false},
        {"submitted",false},{"submission_confirmed",false},{"selection_confirmed",false},{"attempted",true},{"unknown",true}});
    phase->locked=true;phase->cause="startingpoint Client Research UI attempt outstanding; no replay";receipts[request.token]=result;
    DWORD code=0;
    result["selection_attempted"]=false;result["activation_attempted"]=false;
    if(is_show){
        code=show(p.image,p.window);result["exception_code"]=code;result["returned"]=code==0;
        if(code)e="startingpoint Client Research original Show exception="+std::to_string(code);
        else{
            Pin shown;tutorial_development::StartingpointSnapshot shown_snapshot;json shown_diagnostic;ResearchUI shown_ui;
            if(!pin(request,shown,e,true)||!begin_snapshot(shown,shown_snapshot,shown_diagnostic,e)||!same_research_native(p,shown,snapshot,shown_snapshot)||
               !research_tabs(shown,shown_ui,e)||shown.window!=p.window||shown_ui.tabs!=ui.tabs||shown_ui.view!=ui.view)
                {if(e.empty())e="startingpoint Client Research owners changed during Show";}
            else{code=research_page(p.image,shown_ui.tabs);result["page_exception"]=code;result["returned"]=code==0;
                if(code)e="startingpoint Client Research original SetPage exception="+std::to_string(code);}
        }
    }else{
        result["selection_attempted"]=true;
        code=research_select(p.image,ui.grid,ui.row);result["selection_exception"]=code;result["returned"]=code==0;
        if(code)e="startingpoint Client Research original SelectRow exception="+std::to_string(code);
        else{
            Pin selected;tutorial_development::StartingpointSnapshot selected_snapshot;json selected_diagnostic;ResearchUI selected_ui;
            if(!pin(request,selected,e,true)||!begin_snapshot(selected,selected_snapshot,selected_diagnostic,e)||!same_research_native(p,selected,snapshot,selected_snapshot)||
               !research_selection(selected,selected_snapshot,selected_ui,e,true,true,true)||selected.window!=p.window||selected_ui!=ui)
                {if(e.empty())e="startingpoint Client Research selection changed native owners";}
            else if(!economy.read(ledger_after,e)||ledger_after!=ledger_before||!native::read_simulation_state(simulation_after,e)||
                    begin_ledger(simulation_before)!=begin_ledger(simulation_after)){if(e.empty())e="startingpoint Client Research selection changed full ledger";}
            else if(!action_session::selected_session().current(current,e)||current!=context){if(e.empty())e="startingpoint Client Research selection changed common Context";}
            else{result["selection_confirmed"]=true;result["activation_attempted"]=true;
                code=activate(p.image,ui.button);result["exception_code"]=code;result["returned"]=code==0;
                if(code)e="startingpoint Client Research original Activate exception="+std::to_string(code);}
        }
    }
    json progress;std::string id;Pin after;tutorial_development::StartingpointSnapshot after_snapshot;json after_diagnostic;ResearchUI after_ui;
    if(e.empty()&&!tutorial_replication::read_progress(progress,e))result["progress_readback_failed"]=true;
    if(e.empty()&&!is_show&&!research_submission(progress_before,progress,id,e))result["submission_readback_failed"]=true;
    if(e.empty()&&(!pin(request,after,e,true,id,&context,&binding)||!begin_snapshot(after,after_snapshot,after_diagnostic,e)))result["readback_failed"]=true;
    if(e.empty()&&!same_research_native(p,after,snapshot,after_snapshot))e="startingpoint Client Research changed local row/catalogue/inventory/tutorial before Host order";
    if(e.empty()&&(!economy.read(ledger_after,e)||ledger_after!=ledger_before||!native::read_simulation_state(simulation_after,e)||
       begin_ledger(simulation_after)!=begin_ledger(simulation_before))){if(e.empty())e="startingpoint Client Research changed full native ledger";}
    if(e.empty()&&(!action_session::selected_session().current(current,e)||current!=context)){if(e.empty())e="startingpoint Client Research changed common Context";}
    if(e.empty()&&is_show){
        std::string requested;
        if(!research_tabs(after,after_ui,e)||after.window!=p.window||after_ui.tabs!=ui.tabs||after_ui.view!=ui.view||!text(after_ui.view,0x3e8,requested,e))
            {if(e.empty())e="startingpoint Client Research requested page owner changed";}
        else if(requested!="{dev_tab_tech}")e="startingpoint Client Research page request differs actual="+requested;
        else result["page_requested"]=true;
    }
    if(e.empty()&&!is_show&&(!research_selection(after,after_snapshot,after_ui,e,true,true,true)||after.window!=p.window||after_ui!=ui))
        {if(e.empty())e="startingpoint Client Research selected UI owner changed";}
    if(e.empty()){
        phase->locked=false;phase->cause.clear();if(is_show)phase->shown=true;else phase->begun=true;
        result["confirmed"]=true;result["unknown"]=false;
        if(!is_show){result["grid_row"]=after_ui.row;result["catalogue_index"]=after_ui.catalogue;}
        result["after"]=after.development;result["tutorial_after"]=after.tutorial;result["ledger_after"]=begin_ledger(simulation_after);
        if(!is_show){result["submitted"]=true;result["submission_confirmed"]=true;result["action_uuid"]=id;result["confirmation"]="intent_submission";}
    }else{phase->cause=e;result["cause"]=e;}
    receipts[request.token]=result;return e.empty();
}

bool client_research_state(const Request& request,json& result,std::string& e){
    Pin p;tutorial_development::StartingpointSnapshot snapshot;json diagnostic,ui;ResearchUI owners;
    if(!pin(request,p,e,true)||!before_begin(p.development,p.project,e)||!begin_snapshot(p,snapshot,diagnostic,e))return false;
    if(p.development!=request.expected_development||diagnostic!=p.development)return fail(e,"startingpoint Research state expected development differs");
    action_session::Context context,current;if(!action_session::selected_session().current(context,e))return false;
    if(!research_measure(p,snapshot,owners,ui,e)){result["ui"]=ui;return false;}
    Pin after;tutorial_development::StartingpointSnapshot after_snapshot;json after_diagnostic,after_ui;ResearchUI after_owners;
    if(!pin(request,after,e,true)||!begin_snapshot(after,after_snapshot,after_diagnostic,e)||!research_measure(after,after_snapshot,after_owners,after_ui,e))return false;
    if(!same_research_native(p,after,snapshot,after_snapshot)||owners!=after_owners||ui!=after_ui||p.window!=after.window)
        return fail(e,"startingpoint Research state owners/presentation changed during read");
    if(!action_session::selected_session().current(current,e)||current!=context)return fail(e,e.empty()?"startingpoint Research state common Context changed":e);
    result={{"operation",request.operation},{"token",request.token},{"attempted",false},{"returned",true},{"confirmed",true},{"unknown",false},
        {"read_only",true},{"ready",ui.at("ready")},{"cause",ui.at("cause")},{"ui",ui}};
    const auto key=request.origin_uuid+":"+std::to_string(request.epoch)+":"+std::to_string(request.generation)+":client_research";
    const auto prior=worlds.find(key);result["phase_locked"]=prior!=worlds.end()&&prior->second.locked;
    if(prior!=worlds.end()&&prior->second.locked)result["phase_cause"]=prior->second.cause;
    return true;
}
