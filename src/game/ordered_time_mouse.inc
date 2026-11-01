namespace {
bool ordered_current(ordered_time_input::Current& owner,uint64_t& sequence,uint64_t& revision,bool& captured,
                     building_mixed_identity::opened_file::CurrentWorld& world,std::string& e){
    if(GetCurrentThreadId()!=game_thread)return fail(e,"ordered_time_mouse_wrong_game_thread");
    native_time_input::Context guard;
    if(!io.context(guard,e))return false;
    if(!guard.owned_test||!guard.overlay_hidden||!guard.no_pending_operation)
        return fail(e,"ordered_time_mouse_owned_runtime_overlay_or_pending_guard_failed");
    if(!typed_action_runtime::read_ordered_time_input(owner,sequence,revision,captured,e)||
       !native_open_runtime::read_current_world(world,e))return false;
    if(!owner.ordered||owner.context.snapshot_uuid!=world.uuid||owner.binding.native_epoch!=world.world_epoch||
       owner.binding.native_generation!=world.generation)
        return fail(e,"ordered_time_mouse_current_session_native_origin_differs");
    e.clear();return true;
}
bool ordered_button(const button::Request& q,button::Binding& out,std::string& e){
    ordered_time_input::Current owner;uint64_t sequence=0,revision=0;bool captured=false;
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!ordered_current(owner,sequence,revision,captured,world,e))return false;
    if(!pending||!q.ordered||owner.pending||captured||owner.context!=pending->ordered_owner.context||
       owner.binding!=pending->ordered_owner.binding||owner.context.session!=pending->session_epoch||
       sequence==UINT64_MAX||sequence+1!=pending->expected_sequence||revision!=q.revision||
       world.uuid!=q.snapshot_uuid||world.world_epoch!=q.epoch||world.generation!=q.generation)
        return fail(e,"ordered_time_mouse_request_or_idle_owner_differs");
    hud_time::ActivationBinding current;
    if(!reader->button_binding(q.scale,current,e))return false;
    if(!current.matched)return fail(e,"ordered_time_mouse_hud_item_not_matched");
    if(current.world!=reinterpret_cast<uintptr_t>(world.state))return fail(e,"ordered_time_mouse_hud_GameState_owner_differs");
    if(!current.visible)return fail(e,"ordered_time_mouse_clock_panel_hidden");
    if(!current.enabled)return fail(e,"ordered_time_mouse_button_disabled");
    out={};out.image=image;out.world=reinterpret_cast<uintptr_t>(world.world);out.panel=current.panel;out.slot=current.slot;
    out.button=current.button;out.item=current.item;out.type=current.type;
    out.item_proxy=current.item_proxy;out.item_proxy_target=current.item+0x20;
    out.type_proxy=current.type_proxy;out.type_proxy_target=current.type+0x38;
    out.epoch=world.world_epoch;out.generation=world.generation;out.revision=revision;out.ordered=true;
    out.game_thread=game_thread;out.role=owner.context.role==action_session::Role::Host?button::Role::Host:button::Role::Client;
    out.scale=current.scale;out.button_enabled=current.enabled;out.native_item_state=current.native_item_state;
    return button::validate_binding(q,out,e);
}
bool ordered_observe(json& result,std::string& e){
    ordered_time_input::Current owner;uint64_t sequence=0,revision=0;bool captured=false;
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!ordered_current(owner,sequence,revision,captured,world,e))return false;
    native::ClockState clock;if(!native::read_clock(clock,e))return false;
    result={{"ordered",true},{"session",owner.context.session},{"snapshot_uuid",world.uuid},{"world_epoch",world.world_epoch},
        {"generation",world.generation},{"local_peer",owner.context.local_peer},{"request_sequence",sequence},{"revision",revision},
        {"pending",owner.pending},{"captured",captured},{"selected_scale",clock.time_scale},{"pause_scale",clock.pause_scale}};
    hud_time::ActivationBinding pause;
    if(!reader->button_binding(0,pause,e))return false;
    const bool same_world=pause.world==reinterpret_cast<uintptr_t>(world.state);
    const std::string cause=!pause.matched?"ordered_time_mouse_hud_item_not_matched":
        !same_world?"ordered_time_mouse_hud_GameState_owner_differs":
        !pause.visible?"ordered_time_mouse_clock_panel_hidden":
        !pause.enabled?"ordered_time_mouse_button_disabled":"";
    result["pause_button"]={{"matched",pause.matched},{"world_matches",same_world},
        {"visible",pause.visible},{"enabled",pause.enabled},{"ready",cause.empty()},{"cause",cause}};
    e.clear();return true;
}
bool ordered_probe(const button::Request& q,uint64_t session,uint64_t expected,json& result,std::string& e){
    auto& job=jobs.try_emplace(q.token).first->second;job.request=q;job.expected_sequence=expected;job.session_epoch=session;
    const auto reject=[&](){job.terminal=true;job.error=e;result=format(job);return false;};
    uint64_t sequence=0,revision=0;bool captured=false;
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!ordered_current(job.ordered_owner,sequence,revision,captured,world,e))return reject();
    pending=&job;
    struct Clear {~Clear(){pending=nullptr;}} clear;
    bool invoked=false;RootServices services;native::ClockState before,after_clock;
    try{
        job.before=io.hook_counts();
        if(!native::read_clock(before,e))return reject();
        invoked=button::invoke(q,services,job.native,e);
        job.after=io.hook_counts();
        ordered_time_input::Current after;
        if(invoked){
            if(!ordered_current(after,sequence,revision,captured,world,e)||!native::read_clock(after_clock,e))invoked=false;
            else if(after_clock.time_scale!=before.time_scale||after_clock.pause_scale!=before.pause_scale||!captured||after.context!=job.ordered_owner.context||after.binding!=job.ordered_owner.binding||
                sequence==UINT64_MAX||sequence+1!=expected||revision!=q.revision||
                job.after.attempted!=job.before.attempted+1||job.after.matched!=job.before.matched+1||
                job.after.suppressed!=job.before.suppressed+1||job.after.original!=job.before.original||
                job.after.caller!=hud_time::Activation::mouse_caller_rva){
                invoked=false;e="ordered_time_mouse_native_return_without_exact_retained_capture";
            }
        }
    }catch(const std::exception& cause){invoked=false;e=std::string("ordered_time_mouse_invocation_exception: ")+cause.what();}
     catch(...){invoked=false;e="ordered_time_mouse_invocation_unknown_exception";}
    if(!invoked){
        if(job.native.native_attempted)job.native.phase=button::Phase::Unknown;
        if(e.empty())e="ordered_time_mouse_invocation_failed_without_cause";
        return reject();
    }
    job.admitted=true;job.sequence=expected;result=format(job);result["state"]="captured_waiting_for_ordered_commit";
    e.clear();return true;
}
bool ordered_receipt(Job& job,json& result,std::string& e){
    if(job.terminal){result=format(job);e=job.error;return job.confirmed;}
    ordered_time_input::Current owner;uint64_t sequence=0,revision=0;bool captured=false;
    building_mixed_identity::opened_file::CurrentWorld world;
    const auto reject=[&](){job.terminal=true;job.error=e;result=format(job);return false;};
    if(!ordered_current(owner,sequence,revision,captured,world,e))return reject();
    if(owner.context!=job.ordered_owner.context||owner.binding!=job.ordered_owner.binding)
        return fail(e,"ordered_time_mouse_confirmation_owner_changed"),reject();
    auto receipt=typed_action_runtime::ordered_execution_status();
    if(!receipt.value("available",false)||receipt.value("action",std::string())!="Time"||
       receipt.value("origin_peer",uint64_t(0))!=owner.context.local_peer||receipt.value("request_sequence",uint64_t(0))!=job.sequence){
        result=format(job);e.clear();return true;
    }
    job.ordered_receipt=receipt;
    if(receipt.value("session",uint64_t(0))!=job.session_epoch||receipt.value("snapshot_uuid",std::string())!=job.request.snapshot_uuid||
       receipt.value("observed_revision",UINT64_MAX)!=job.request.revision||receipt.value("intent",-1)!=int(intent(job.request.scale))||
       !receipt.value("confirmed",false)||!receipt.value("cause",std::string()).empty())
        return fail(e,"ordered_time_mouse_exact_execution_failed_or_differs"),reject();
    if(!receipt.value("committed_tick",uint64_t(0))){result=format(job);e.clear();return true;}
    if(receipt.at("committed_tick")!=receipt.at("execution_tick")||receipt.at("after").at("selected")!=job.request.scale||
       receipt.at("before").at("pause")!=receipt.at("after").at("pause"))
        return fail(e,"ordered_time_mouse_committed_clock_effect_differs"),reject();
    job.confirmed=job.terminal=true;job.error.clear();result=format(job);e.clear();return true;
}
}
