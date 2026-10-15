bool client_staff(const Request& request,json& result,Progress*& phase,std::string& key,std::string& e){
    const bool is_show=request.operation=="client_staff_show";
    Pin p;if(!pin(request,p,e,true))return false;
    if(p.development!=request.expected_development)return fail(e,"startingpoint Client Staff expected development differs");
    tutorial_development::StartingpointSnapshot snapshot;json diagnostic;
    if(!begin_snapshot(p,snapshot,diagnostic,e)||diagnostic!=p.development||!staff_baseline(p,snapshot,e))
        return fail(e,e.empty()?"startingpoint Client Staff private baseline differs":e);
    if(!staff_ui_code(p.image,e))return false;
    action_session::Context context;action_session::Binding binding;
    if(!action_session::selected_session().current(context,e)||context.role!=action_session::Role::Client||
       !typed_action_runtime::network_binding(context.snapshot_uuid,binding,e))return fail(e,e.empty()?"startingpoint Client Staff session differs":e);
    live_host_state::NativePort economy(context,binding,nullptr);live_host_state::Pin ledger_before,ledger_after;
    native::SimulationState simulation_before,simulation_after;
    if(!economy.read(ledger_before,e)||!native::read_simulation_state(simulation_before,e))return false;
    int32_t available=0;if(!at(ledger_before.business,0x4c,available,e))return false;
    json measured;json progress_before;if(!tutorial_replication::read_progress(progress_before,e))return false;
    key=request.origin_uuid+":"+std::to_string(request.epoch)+":"+std::to_string(request.generation)+":client_staff";
    phase=&worlds[key];if(phase->locked)return fail(e,phase->cause);
    if((is_show&&phase->shown)||(!is_show&&phase->begun))return fail(e,"startingpoint Client Staff phase consumed; no replay");
    StaffUI ui;
    if(is_show){if(!staff_tabs(p,ui,e,true))return false;}
    else if(!staff_ready(p,snapshot,available,ui,measured,e))return false;
    Pin again;tutorial_development::StartingpointSnapshot stable;json stable_diagnostic;StaffUI stable_ui;
    if(!pin(request,again,e,true)||!begin_snapshot(again,stable,stable_diagnostic,e)||!same_research_native(p,again,snapshot,stable))
        return fail(e,e.empty()?"startingpoint Client Staff owners changed before UI call":e);
    if(is_show){if(!staff_tabs(again,stable_ui,e,true))return false;}
    else if(!staff_ready(again,stable,available,stable_ui,measured,e))return false;
    if(p.window!=again.window||ui!=stable_ui)return fail(e,"startingpoint Client Staff UI owner changed before call");
    if(!economy.read(ledger_after,e)||ledger_before!=ledger_after||!native::read_simulation_state(simulation_after,e)||
       begin_ledger(simulation_before)!=begin_ledger(simulation_after))return fail(e,e.empty()?"startingpoint Client Staff ledger changed before call":e);
    action_session::Context current;if(!action_session::selected_session().current(current,e)||current!=context)
        return fail(e,e.empty()?"startingpoint Client Staff common Context changed":e);
    result.update({{"operation",request.operation},{"token",request.token},{"before",p.development},{"tutorial_before",p.tutorial},
        {"desired_workers",is_show?0:1},{"ledger_before",begin_ledger(simulation_before)},{"host_completion_confirmed",false},
        {"submitted",false},{"submission_confirmed",false},{"selection_confirmed",false},{"attempted",true},{"unknown",true}});
    phase->locked=true;phase->cause="startingpoint Client Staff UI attempt outstanding; no replay";receipts[request.token]=result;
    DWORD code=0;
    result["selection_attempted"]=false;result["activation_attempted"]=false;
    if(is_show){
        code=show(p.image,p.window);result["exception_code"]=code;result["returned"]=code==0;
        if(code)e="startingpoint Client Staff original Show exception="+std::to_string(code);
        else{
            Pin shown;tutorial_development::StartingpointSnapshot shown_snapshot;json shown_diagnostic;StaffUI shown_ui;
            if(!pin(request,shown,e,true)||!begin_snapshot(shown,shown_snapshot,shown_diagnostic,e)||!same_research_native(p,shown,snapshot,shown_snapshot)||
               !staff_tabs(shown,shown_ui,e)||shown.window!=p.window||shown_ui.tabs!=ui.tabs||shown_ui.view!=ui.view)
                {if(e.empty())e="startingpoint Client Staff owners changed during Show";}
            else{code=staff_page(p.image,shown_ui.tabs);result["page_exception"]=code;result["returned"]=code==0;
                if(code)e="startingpoint Client Staff original SetPage exception="+std::to_string(code);}
        }
    }else{
        result["activation_attempted"]=true;
        code=activate(p.image,ui.button);result["exception_code"]=code;result["returned"]=code==0;
        if(code)e="startingpoint Client Staff original Activate exception="+std::to_string(code);
    }
    json progress;std::string id;Pin after;tutorial_development::StartingpointSnapshot after_snapshot;json after_diagnostic;StaffUI after_ui;
    if(e.empty()&&!tutorial_replication::read_progress(progress,e))result["progress_readback_failed"]=true;
    if(e.empty()&&!is_show&&!staff_submission(progress_before,progress,id,e))result["submission_readback_failed"]=true;
    if(e.empty()&&(!pin(request,after,e,true,id,&context,&binding)||!begin_snapshot(after,after_snapshot,after_diagnostic,e)))result["readback_failed"]=true;
    if(e.empty()&&!same_research_native(p,after,snapshot,after_snapshot))e="startingpoint Client Staff changed local row/catalogue/inventory/tutorial before Host order";
    if(e.empty()&&(!economy.read(ledger_after,e)||ledger_after!=ledger_before||!native::read_simulation_state(simulation_after,e)||
       begin_ledger(simulation_after)!=begin_ledger(simulation_before))){if(e.empty())e="startingpoint Client Staff changed full native ledger";}
    if(e.empty()&&(!action_session::selected_session().current(current,e)||current!=context)){if(e.empty())e="startingpoint Client Staff changed common Context";}
    if(e.empty()&&is_show){
        std::string requested;
        if(!staff_tabs(after,after_ui,e)||after.window!=p.window||after_ui.tabs!=ui.tabs||after_ui.view!=ui.view||!text(after_ui.view,0x3e8,requested,e))
            {if(e.empty())e="startingpoint Client Staff requested page owner changed";}
        else if(requested!="{dev_tab_staff}")e="startingpoint Client Staff page request differs actual="+requested;
        else result["page_requested"]=true;
    }
    if(e.empty()&&!is_show&&(!staff_ready(after,after_snapshot,available,after_ui,measured,e,1)||after.window!=p.window||after_ui!=ui))
        {if(e.empty())e="startingpoint Client Staff selected UI owner changed";}
    if(e.empty()){
        phase->locked=false;phase->cause.clear();if(is_show)phase->shown=true;else phase->begun=true;
        result["confirmed"]=true;result["unknown"]=false;
        if(!is_show)result["ui_after"]=measured;
        result["after"]=after.development;result["tutorial_after"]=after.tutorial;result["ledger_after"]=begin_ledger(simulation_after);
        if(!is_show){result["submitted"]=true;result["submission_confirmed"]=true;result["action_uuid"]=id;result["confirmation"]="intent_submission";}
    }else{phase->cause=e;result["cause"]=e;}
    receipts[request.token]=result;return e.empty();
}

bool client_staff_state(const Request& request,json& result,std::string& e){
    Pin p;tutorial_development::StartingpointSnapshot snapshot;json diagnostic,ui;StaffUI owners;
    if(!pin(request,p,e,true)||!begin_snapshot(p,snapshot,diagnostic,e)||!staff_baseline(p,snapshot,e)||!staff_ui_code(p.image,e))return false;
    if(p.development!=request.expected_development||diagnostic!=p.development)return fail(e,"startingpoint Staff state expected development differs");
    action_session::Context context,current;if(!action_session::selected_session().current(context,e))return false;
    action_session::Binding binding;if(!typed_action_runtime::network_binding(context.snapshot_uuid,binding,e))return false;
    live_host_state::NativePort economy(context,binding,nullptr);live_host_state::Pin ledger_before,ledger_after;
    int32_t available=0;if(!economy.read(ledger_before,e)||!at(ledger_before.business,0x4c,available,e))return false;
    if(!staff_measure(p,snapshot,available,owners,ui,e)){result["ui"]=ui;return false;}
    Pin after;tutorial_development::StartingpointSnapshot after_snapshot;json after_diagnostic,after_ui;StaffUI after_owners;
    if(!pin(request,after,e,true)||!begin_snapshot(after,after_snapshot,after_diagnostic,e)||!staff_measure(after,after_snapshot,available,after_owners,after_ui,e))return false;
    if(!economy.read(ledger_after,e)||ledger_before!=ledger_after)return fail(e,e.empty()?"startingpoint Staff state ledger changed":e);
    if(!same_research_native(p,after,snapshot,after_snapshot)||owners!=after_owners||ui!=after_ui||p.window!=after.window)
        return fail(e,"startingpoint Staff state owners/presentation changed during read");
    if(!action_session::selected_session().current(current,e)||current!=context)return fail(e,e.empty()?"startingpoint Staff state common Context changed":e);
    result={{"operation",request.operation},{"token",request.token},{"attempted",false},{"returned",true},{"confirmed",true},{"unknown",false},
        {"read_only",true},{"ready",ui.at("ready")},{"cause",ui.at("cause")},{"ui",ui}};
    const auto key=request.origin_uuid+":"+std::to_string(request.epoch)+":"+std::to_string(request.generation)+":client_staff";
    const auto prior=worlds.find(key);result["phase_locked"]=prior!=worlds.end()&&prior->second.locked;
    if(prior!=worlds.end()&&prior->second.locked)result["phase_cause"]=prior->second.cause;
    return true;
}
