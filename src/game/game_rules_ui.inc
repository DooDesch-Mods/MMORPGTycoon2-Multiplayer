namespace {
struct RulesUi {uintptr_t window=0,checkbox=0;bool visible=false,ready=false;};
bool rules_ui_find(Runtime& r,const rules::Pin& pin,RulesUi& out,std::string& e){
    out={};uintptr_t data=0;int32_t count=0,capacity=0;if(!at(r,r.image,0x1a3c478,data,e)||!at(r,r.image,0x1a3c480,count,e)||!at(r,r.image,0x1a3c484,capacity,e))return false;
    if(count<0||count>1024||capacity<count||capacity>65536||(!data&&count))return fail(e,"Design UI registry extent invalid");std::vector<uintptr_t> windows(size_t(count),0);if(count&&!r.memory.copy(data,windows.data(),windows.size()*sizeof(uintptr_t),e))return false;
    for(auto window:windows){uintptr_t klass=0;if(!at(r,window,0,klass,e))return false;if(klass!=r.image+0x16ff0a0)continue;
        std::string name;uintptr_t logic=1;uint8_t shown=0;if(!text(r,window+0x1b0,name,256,e)||!at(r,window,0x318,logic,e)||!at(r,window,0x45f,shown,e))return false;
        if(name!="design_combat"||logic||shown>1)continue;if(out.window)return fail(e,"Design UI has multiple Combat windows");out.window=window;out.visible=shown!=0;
    }
    if(!out.window||!out.visible)return true;
    std::vector<uintptr_t> pending{out.window};std::set<uintptr_t> seen;
    while(!pending.empty()){const auto pane=pending.back();pending.pop_back();if(!seen.insert(pane).second||seen.size()>512)return fail(e,"Design UI child cycle or extent invalid");
        uintptr_t klass=0;if(!at(r,pane,0,klass,e))return false;
        if(klass==r.image+0x16d98c0){uintptr_t value=0,owner=0,scalar=0;if(!at(r,pane,0x3c8,value,e))return false;
            if(value){if(!ap(r,value,0x1749610,e)||!at(r,value,8,owner,e))return false;if(owner==pin.design+0x1c0){std::string name;
                if(!text(r,value+0x18,name,32,e)||name!="delayForToons"||!at(r,value,0x38,scalar,e)||scalar!=owner+0x1c)return fail(e,e.empty()?"Design UI Respawn binding differs":e);
                if(out.checkbox)return fail(e,"Design UI has multiple Respawn delay controls");out.checkbox=pane;
            }}
        }
        if(!service_ui_children(r,pane,klass,out.window,pending,e))return false;
    }
    if(!out.checkbox)return true;out.ready=true;uintptr_t pane=out.checkbox;std::set<uintptr_t> parents;
    while(pane!=out.window){uint8_t shown=0;uintptr_t parent=0,owner=0;if(!pane||!parents.insert(pane).second||parents.size()>32||!at(r,pane,0x69,shown,e)||shown>1||!at(r,pane,0x278,parent,e)||!at(r,pane,0x280,owner,e)||owner!=out.window)return fail(e,e.empty()?"Design UI visible ancestor binding differs":e);out.ready=out.ready&&shown==1;pane=parent;}
    uint8_t clickable=0;if(!at(r,out.checkbox,0x174,clickable,e)||clickable>1)return fail(e,e.empty()?"Design UI checkbox clickable flag invalid":e);out.ready=out.ready&&clickable==1;return true;
}
uint32_t rules_ui_toggle(uintptr_t image,uintptr_t checkbox,bool desired,bool& returned)noexcept{
    __try{reinterpret_cast<void(*)(void*,bool)>(image+0x6a1780)(reinterpret_cast<void*>(checkbox),desired);returned=true;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool rules_ui_fixture(const nlohmann::json& args,nlohmann::json& out,std::string& e){
    out={{"execution_ack",false},{"ui_attempted",false},{"captured",false},{"unknown",false}};auto* r=published.load(std::memory_order_acquire);std::string token;
    try{
        if(!r||!r->ready||GetCurrentThreadId()!=r->thread||!Config::instance().local_test)return fail(e,"Design UI requires owned ready local-test Mainthread");
        bool inside=false;if(!typed_action_runtime::original_go_admission(inside,e)||!inside)return fail(e,e.empty()?"Design UI requires actual original Go":e);
        a::Context context;a::Binding binding;bool ordered=false;
        if(!a::selected_session().current(context,e)||!rules_capture_binding(context.snapshot_uuid,binding,ordered,e)||!ordered||Config::instance().is_host!=(context.role==a::Role::Host))return fail(e,e.empty()?"Design UI current ordered role/Binding differs":e);
        Local local;rules::Pin pin;a::GameRulesProof before;a::Ledger money;if(!rules_proof(*r,context,binding,local,pin,before,money,e))return false;
        RulesUi ui;if(!rules_ui_find(*r,pin,ui,e))return false;
        if(args.is_object()&&args.size()==1&&args.value("command",std::string{})=="Read"){
            out={{"read_only",true},{"snapshot_uuid",context.snapshot_uuid},{"session",context.session},{"local_peer",context.local_peer},{"epoch",binding.native_epoch},{"generation",binding.native_generation},{"window_present",ui.window!=0},{"window_visible",ui.visible},{"checkbox_ready",ui.ready},{"proof",rules::encode(before)},{"ledger",a::management_ledger_observation(money)},{"ui_proofs",design_rules.ui_proofs}};return true;
        }
        if(!args.is_object()||args.size()!=6||!args.contains("command")||!args.at("command").is_string()||!args.contains("token")||!args.at("token").is_string()||!args.contains("snapshot_uuid")||!args.at("snapshot_uuid").is_string()||!args.contains("epoch")||!args.at("epoch").is_number_unsigned()||!args.contains("generation")||!args.at("generation").is_number_unsigned()||!args.contains("expected_delay")||!args.at("expected_delay").is_boolean())return fail(e,"Design UI requires exact six typed fields");
        const auto command=args.at("command").get<std::string>();token=args.at("token").get<std::string>();
        if((command!="Show"&&command!="ToggleDelay")||!monster_target_key::uuid(token)||!monster_target_key::uuid(args.at("snapshot_uuid").get<std::string>()))return fail(e,"Design UI command/UUID invalid");
        if(design_rules.ui_proofs.contains(token)){out=design_rules.ui_proofs.at(token);return fail(e,"Design UI token consumed; no replay");}
        const auto controller=typed_action_runtime::controller_status();
        if(design_rules.ui_unknown||design_rules.ui_proofs.size()>=64||r->replay||r->pending||policy_pending()||rules_pending()||intervention_pending()||control_input::runtime::pending()||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))return fail(e,"Design UI requires idle unlocked owners");
        if(args.at("snapshot_uuid").get<std::string>()!=context.snapshot_uuid||args.at("epoch").get<uint64_t>()!=binding.native_epoch||args.at("generation").get<uint64_t>()!=binding.native_generation||args.at("expected_delay").get<bool>()!=before.state.respawn_delay||!ui.window)return fail(e,"Design UI actual identity/value/window differs");
        if(!rules_routes(*r,e)||!policy_route_code(*r,e))return false;
        if(command=="Show"){if(!hud_time_runtime::verify_window_show_hook(r->image+0x81ec20,e)||!loan_ui_body(*r,0x6bf990,467,"e41f9ed9d4a7af627ff4b5ad82faad352928e50a5928e5b1eb33c835c697fb37",e))return false;}
        else if(!ui.visible||!ui.ready)return fail(e,"Design UI actual Respawn delay control is not visible and clickable");
        out["token"]=token;out["command"]=command;out["ui_attempted"]=true;out["unknown"]=true;design_rules.ui_unknown=true;design_rules.ui_proofs.emplace(token,out);
        const auto captures=r->captures;bool ok=false;
        if(command=="Show"){const auto exception=developer_staff::original_show(r->image,ui.window);out["exception"]=exception;out["ui_returned"]=exception==0;ok=!exception&&r->captures==captures&&!design_rules.pending;}
        else{bool returned=false;const auto exception=rules_ui_toggle(r->image,ui.checkbox,!before.state.respawn_delay,returned);out["exception"]=exception;out["ui_returned"]=returned;
            ok=!exception&&returned&&r->captures==captures+1&&design_rules.pending&&design_rules.pending->ordered&&design_rules.pending->context==context&&design_rules.pending->binding==binding&&design_rules.pending->intent.command==rules::Command::RespawnDelay&&design_rules.pending->intent.before==before.state&&design_rules.pending->intent.after.respawn_delay!=before.state.respawn_delay;
            if(ok){out["captured"]=true;out["action_uuid"]=design_rules.pending->id;}
        }
        Local final;rules::Pin final_pin;a::GameRulesProof after;a::Ledger after_money;RulesUi retained;a::Context final_context;a::Binding final_binding;bool final_ordered=false;
        if(!rules_proof(*r,context,binding,final,final_pin,after,after_money,e)||after!=before||after_money!=money||final.pin!=local.pin||final_pin!=pin||!a::selected_session().current(final_context,e)||final_context!=context||!rules_capture_binding(context.snapshot_uuid,final_binding,final_ordered,e)||final_binding!=binding||!final_ordered||!rules_ui_find(*r,pin,retained,e)||retained.window!=ui.window||!retained.visible||(command=="ToggleDelay"&&retained.checkbox!=ui.checkbox))ok=false;
        if(!ok){if(e.empty())e="Design original UI call/capture changed gameplay or lacks exact readback";out["cause"]=e;design_rules.ui_proofs[token]=out;return false;}
        out["unknown"]=false;design_rules.ui_unknown=false;design_rules.ui_proofs[token]=out;e.clear();return true;
    }catch(const std::exception& cause){e=std::string("Design UI exception: ")+cause.what();}catch(...){e="Design UI nonstandard exception";}
    if(r&&design_rules.ui_proofs.contains(token)){design_rules.ui_unknown=true;out["unknown"]=true;out["cause"]=e;design_rules.ui_proofs[token]=out;}return false;
}
