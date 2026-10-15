bool client_ui(const Request& request,json& result,Progress*& phase,std::string& key,std::string& e){
    const bool is_show=request.operation=="client_show";
    Pin p;if(!pin(request,p,e,true)||!before(p,e))return false;
    if(p.development!=request.expected_development)return fail(e,"startingpoint Client fixture expected development differs");
    action_session::Context context;action_session::Binding binding;
    if(!action_session::selected_session().current(context,e)||context.role!=action_session::Role::Client||
       !typed_action_runtime::network_binding(context.snapshot_uuid,binding,e))return fail(e,e.empty()?"startingpoint Client fixture current session differs":e);
    live_host_state::NativePort economy(context,binding,nullptr);live_host_state::Pin ledger_before,ledger_after;
    if(!economy.read(ledger_before,e))return false;
    json progress_before;if(!tutorial_replication::read_progress(progress_before,e))return false;
    key=request.origin_uuid+":"+std::to_string(request.epoch)+":"+std::to_string(request.generation)+":client";
    phase=&worlds[key];if(phase->locked)return fail(e,phase->cause);
    if((is_show&&phase->shown)||(!is_show&&phase->committed))return fail(e,"startingpoint Client fixture phase consumed; no replay");
    if(is_show){if(!development_window(p,e))return false;}
    else{
        bool completed=false;const auto code=complete_state(p.image,p.lounge,completed);
        if(code)return fail(e,"startingpoint Client completion getter exception="+std::to_string(code));
        if(!completed)return fail(e,"startingpoint Client current project not natively complete");
        if(!commit_button(p,e))return false;
    }
    Pin again;if(!pin(request,again,e,true)||!same(p.world,again.world)||p.lounge!=again.lounge||p.project!=again.project||
       p.development!=again.development||p.tutorial!=again.tutorial||!same_inventory_owner(p,again)||p.items!=again.items)
        return fail(e,e.empty()?"startingpoint Client owners changed before UI call":e);
    if(is_show){if(!development_window(again,e)||p.window!=again.window)return fail(e,e.empty()?"startingpoint Client Development owner changed":e);}
    else if(!commit_button(again,e)||p.window!=again.window||p.button!=again.button)return fail(e,e.empty()?"startingpoint Client Commit button changed":e);
    if(!economy.read(ledger_after,e)||ledger_after!=ledger_before)return fail(e,e.empty()?"startingpoint Client ledger changed before UI call":e);
    action_session::Context current;if(!action_session::selected_session().current(current,e)||current!=context)return fail(e,e.empty()?"startingpoint Client common Context changed":e);
    result["operation"]=request.operation;result["token"]=request.token;result["before"]=p.development;
    result["tutorial_before"]=p.tutorial;result["host_completion_confirmed"]=false;result["submission_confirmed"]=false;result["submitted"]=false;
    result["attempted"]=true;result["unknown"]=true;phase->locked=true;phase->cause="startingpoint Client UI attempt outstanding; no replay";receipts[request.token]=result;
    const auto code=is_show?show(p.image,p.window):activate(p.image,p.button);
    result["exception_code"]=code;result["returned"]=code==0;
    json progress;std::string id;Pin after;
    if(code)e="startingpoint Client original UI exception="+std::to_string(code);
    else if(!tutorial_replication::read_progress(progress,e)){result["progress_readback_failed"]=true;}
    else if(!is_show&&!client_submission(progress_before,progress,id,e)){result["submission_readback_failed"]=true;}
    else if(!pin(request,after,e,true,id,&context,&binding)){result["readback_failed"]=true;}
    else if(!same(p.world,after.world)||p.lounge!=after.lounge||p.project!=after.project||p.development!=after.development||
            p.tutorial!=after.tutorial||!same_inventory_owner(p,after)||p.items!=after.items)e="startingpoint Client UI changed local project/inventory/tutorial before Host order";
    else if(!economy.read(ledger_after,e)||ledger_after!=ledger_before){if(e.empty())e="startingpoint Client UI changed full native ledger";}
    else if(!action_session::selected_session().current(current,e)||current!=context){if(e.empty())e="startingpoint Client UI changed common Context";}
    else if(is_show){
        uint8_t visible=0;
        if(!development_window(after,e)||!at(after.window,0x45f,visible,e)){result["ui_readback_failed"]=true;}
        else if(after.window!=p.window||visible!=1)e="startingpoint Client Development Show not visible on same owner";
        else phase->shown=true;
    }else{
        phase->committed=true;result["submission_confirmed"]=true;result["submitted"]=true;result["action_uuid"]=id;
        result["confirmation"]="intent_submission";
    }
    if(e.empty()){
        phase->locked=false;phase->cause.clear();result["confirmed"]=true;result["unknown"]=false;
        result["after"]=after.development;result["tutorial_after"]=after.tutorial;
    }else{phase->cause=e;result["cause"]=e;}
    receipts[request.token]=result;return e.empty();
}
