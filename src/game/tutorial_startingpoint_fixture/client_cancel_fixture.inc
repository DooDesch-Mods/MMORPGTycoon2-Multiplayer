struct CancelCall {
    bool constructed=false,label_confirmed=false,attempted=false,returned=false,accepted=false,cleanup_confirmed=false;
    DWORD fault=0,cleanup_fault=0;
};
void cancel_command(uintptr_t image,uintptr_t window,CancelCall& out){
    initial_release::Record record{};
    constexpr char command[]="CancelTechConfirm";
    __try{
        reinterpret_cast<initial_release::RecordCtor>(image+0x967370)(&record,command);out.constructed=true;
        out.label_confirmed=reinterpret_cast<initial_release::RecordEquals>(image+0x1113d80)(&record,command);
        if(out.label_confirmed){
            out.attempted=true;
            out.accepted=reinterpret_cast<initial_release::UiCommand>(image+0x6c7b70)(reinterpret_cast<void*>(window),&record);
            out.returned=true;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){out.fault=GetExceptionCode();}
    if(out.constructed){
        __try{reinterpret_cast<initial_release::RecordDtor>(image+0x9676a0)(&record);out.cleanup_confirmed=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){out.cleanup_fault=GetExceptionCode();}
    }
}
bool cancel_code(uintptr_t image,std::string& e){
    using namespace initial_release::bytes;
    const Guard guards[]={{0x967370,body_967370,sizeof body_967370},
        {0x9676a0,body_9676a0,sizeof body_9676a0},{0x1113d80,body_1113d80,sizeof body_1113d80}};
    for(const auto& guard:guards){
        std::vector<uint8_t> actual(guard.size);
        if(!memory.copy(image+guard.rva,actual.data(),actual.size(),e))return false;
        if(std::memcmp(actual.data(),guard.data,guard.size))return fail(e,"Bridges Cancel record body differs RVA="+std::to_string(guard.rva));
    }
    return developer_staff::action_runtime::verify_entry(image+0x6c7b70,e);
}
bool cancel_window(Pin& p,std::string& e){
    uint8_t visible=0;
    return development_window(p,e)&&at(p.window,0x45f,visible,e)&&
        (visible==1||fail(e,"Bridges Cancel requires the visible owned DevelopmentWindow"));
}
bool client_cancel(const Request& request,json& result,Progress*& phase,std::string& key,std::string& e){
    Pin p;tutorial_development::StartingpointSnapshot snapshot;json diagnostic;
    if(!pin(request,p,e,true)||!p.ordered)return fail(e,e.empty()?"Bridges Cancel fixture requires ordered mode":e);
    if(!same_expected_development(p.development,request.expected_development)||!begin_snapshot(p,snapshot,diagnostic,e)||diagnostic!=p.development)
        return fail(e,e.empty()?"Bridges Cancel expected native development differs":e);
    general_project::Intent intent;
    if(!expected_project_intent(snapshot,"bridges",general_project::Command::Cancel,intent,e)||!cancel_window(p,e)||!cancel_code(p.image,e))return false;
    action_session::Context context;action_session::Binding binding;bool ordered=false;
    if(!action_session::selected_session().current(context,e)||context.role!=action_session::Role::Client||
       !typed_action_runtime::network_binding_with_mode(context.snapshot_uuid,binding,e,&ordered)||!ordered)
        return fail(e,e.empty()?"Bridges Cancel actual ordered Client binding differs":e);
    live_host_state::NativePort economy(context,binding,nullptr);live_host_state::Pin ledger_before,ledger_after;
    native::SimulationState simulation_before,simulation_after;
    if(!economy.read(ledger_before,e)||!native::read_simulation_state(simulation_before,e))return false;
    const auto captures=general_project_runtime::status().value("captures",uint64_t{});
    key=request.origin_uuid+":"+std::to_string(request.epoch)+":"+std::to_string(request.generation)+":client_cancel:bridges";
    phase=&worlds[key];if(phase->locked)return fail(e,phase->cause);
    if(phase->begun)return fail(e,"Bridges Cancel phase consumed; no replay");
    Pin again;tutorial_development::StartingpointSnapshot stable;json stable_diagnostic;
    if(!pin(request,again,e,true)||!again.ordered||!begin_snapshot(again,stable,stable_diagnostic,e)||
       !same_research_native(p,again,snapshot,stable)||!cancel_window(again,e)||p.window!=again.window)
        return fail(e,e.empty()?"Bridges Cancel owners changed before native command":e);
    action_session::Context current;action_session::Binding current_binding;
    if(!action_session::selected_session().current(current,e)||current!=context||
       !typed_action_runtime::network_binding_with_mode(current.snapshot_uuid,current_binding,e,&ordered)||!ordered||current_binding!=binding||
       !economy.read(ledger_after,e)||ledger_before!=ledger_after||!native::read_simulation_state(simulation_after,e)||
       begin_ledger(simulation_before)!=begin_ledger(simulation_after))
        return fail(e,e.empty()?"Bridges Cancel Context/Binding/full ledger changed before command":e);
    result.update({{"operation",request.operation},{"token",request.token},{"before",p.development},
        {"tutorial_before",p.tutorial},{"ledger_before",begin_ledger(simulation_before)},
        {"attempted",true},{"unknown",true},{"host_completion_confirmed",false}});
    phase->locked=true;phase->cause="Bridges Cancel UI attempt outstanding; no replay";receipts[request.token]=result;
    CancelCall call;cancel_command(p.image,p.window,call);
    result.update({{"constructed",call.constructed},{"label_confirmed",call.label_confirmed},{"command_attempted",call.attempted},
        {"returned",call.returned},{"accepted",call.accepted},{"exception_code",call.fault},
        {"cleanup_confirmed",call.cleanup_confirmed},{"cleanup_exception",call.cleanup_fault}});
    if(call.fault||call.cleanup_fault||!call.constructed||!call.label_confirmed||!call.attempted||!call.returned||!call.accepted||!call.cleanup_confirmed)
        e="Bridges Cancel original DevelopmentWindow command/record cleanup failed; exception="+std::to_string(call.fault)+" cleanup="+std::to_string(call.cleanup_fault);
    std::string id;Pin after;tutorial_development::StartingpointSnapshot after_snapshot;json after_diagnostic;
    if(e.empty()&&!general_project_runtime::observe_pending_ui(context,binding,intent,captures,id,e))result["capture_readback_failed"]=true;
    if(e.empty()&&(!pin(request,after,e,true)||!after.ordered||!begin_snapshot(after,after_snapshot,after_diagnostic,e)||
       !same_research_native(p,after,snapshot,after_snapshot)||!cancel_window(after,e)||p.window!=after.window))
        {if(e.empty())e="Bridges Cancel changed local native state before Host order";}
    if(e.empty()&&(!economy.read(ledger_after,e)||ledger_before!=ledger_after||!native::read_simulation_state(simulation_after,e)||
       begin_ledger(simulation_before)!=begin_ledger(simulation_after)))
        {if(e.empty())e="Bridges Cancel changed full ledger before Host order";}
    if(e.empty()&&(!action_session::selected_session().current(current,e)||current!=context||
       !typed_action_runtime::network_binding_with_mode(current.snapshot_uuid,current_binding,e,&ordered)||!ordered||current_binding!=binding))
        {if(e.empty())e="Bridges Cancel actual Context/Binding changed after command";}
    if(e.empty()){
        phase->locked=false;phase->cause.clear();phase->begun=true;
        result.update({{"confirmed",true},{"unknown",false},{"queued",true},{"submitted",false},
            {"submission_confirmed",false},{"action_uuid",id},{"route","general_project"},{"confirmation","intent_queued"},
            {"after",after.development},{"tutorial_after",after.tutorial},{"ledger_after",begin_ledger(simulation_after)}});
    }else{phase->cause=e;result["cause"]=e;}
    receipts[request.token]=result;return e.empty();
}
