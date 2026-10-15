struct StartPresentation {
    std::string action;World origin;start_intent::Window window;
};
std::optional<StartPresentation> pending_start_presentation;
start_intent::Receipt client_start_receipt;
std::string client_start_presentation_cause;
struct StartCaptureCalls {
    void* receiver;action_session::Context context;action_session::Binding binding;
    tutorial_development::StartingpointSnapshot snapshot;start_intent::Window window;
    bool original(){return original_pay(receiver);}
    bool capture(std::string& e){
        if(!action_session::selected_session().current(context,e))return false;
        if(context.role!=action_session::Role::Client)return fail(e,"Startingpoint StartTech intent requires joined Client");
        if(!typed_action_runtime::network_binding(context.snapshot_uuid,binding,e))return false;
        StartActionCalls reader{context,binding};live_host_state::Pin pin;
        if(!reader.snapshot(snapshot,pin,e)||!reader.preflight(e))return false;
        if(!start_intent::verify_ui(image,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e,developer_staff::action_runtime::verify_entry))return false;
        return start_intent::window(memory,snapshot,reinterpret_cast<uintptr_t>(receiver),window,e);
    }
    bool action_uuid(std::string& id,std::string& e){return intent_uuid(id,e);}
    bool submit(const std::string& id,std::string& e){
        if(!typed_action_runtime::submit_start_startingpoint(id,e))return false;
        pending_start_presentation=StartPresentation{id,snapshot.origin,window};return true;
    }
};
bool pay_current(void* source){
    const auto selected=start_intent::route(load.armed,GetCurrentThreadId()==mainthread,image,
        reinterpret_cast<uintptr_t>(_ReturnAddress()));
    StartCaptureCalls calls{source};start_intent::Receipt receipt;
    const bool returned=start_intent::dispatch(selected,calls,receipt);
    if(receipt.suppressed){
        if(receipt.submitted)LOG_INFO("[Tutorial] Client Startingpoint StartTech submitted action={}",receipt.action_uuid);
        else if(receipt.cause!=client_start_receipt.cause)LOG_WARN("[Tutorial] Client Startingpoint StartTech not submitted cause={}",receipt.cause);
        client_start_receipt=std::move(receipt);
    }
    return returned;
}
bool finish_start_presentation(const std::string& action,const action_session::Context& context,
    const action_session::Binding& binding,bool& attempted,std::string& e){
    attempted=false;
    if(!pending_start_presentation||pending_start_presentation->action!=action)return true;
    const auto pending=*pending_start_presentation;pending_start_presentation.reset();
    StartActionCalls reader{context,binding};tutorial_development::StartingpointSnapshot snapshot;
    live_host_state::Pin before,after;start_intent::Window window;
    if(!reader.snapshot(snapshot,before,e))return false;
    if(!same_world(pending.origin,snapshot.origin)||
       !start_intent::window(memory,snapshot,pending.window.selected,window,e,true)||window!=pending.window)
        return e.empty()?fail(e,"Startingpoint StartTech presentation window/selection changed"):false;
    if(!start_intent::verify_ui(image,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e,developer_staff::action_runtime::verify_entry))return false;
    attempted=true;const auto code=start_intent::native_present(image,window.address,window.tabs);
    if(code)return fail(e,"Startingpoint StartTech native presentation exception="+std::to_string(code));
    tutorial_development::StartingpointSnapshot final;
    if(!reader.snapshot(final,after,e))return false;
    if(before!=after||snapshot.row!=final.row||snapshot.creation!=final.creation||snapshot.project!=final.project)
        return fail(e,"Startingpoint StartTech presentation changed full Economy/project state");
    uintptr_t selected=1;if(!at(window.address,0x610,selected,e))return false;
    return !selected||fail(e,"Startingpoint StartTech original presentation did not clear selection");
}
