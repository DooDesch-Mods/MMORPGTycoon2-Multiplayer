bool native_remove_ui(const json& args,json& result,std::string& e)try{
    result=json::object();
    if(!args.is_object()||args.size()!=3||args.value("family",std::string{})!="monster"||
       !args.contains("token")||!args.at("token").is_string()||!args.contains("target_action_uuid")||!args.at("target_action_uuid").is_string())
        return fail(e,"Monster Remove UI requires exact family, token and target_action_uuid");
    const auto token=args.at("token").get<std::string>(),birth=args.at("target_action_uuid").get<std::string>();
    if(!token_valid(token)||!token_valid(birth)||token==birth||state.ui_jobs.contains(token)||state.ui_jobs.size()>=64)
        return fail(e,"Monster Remove UI token or birth UUID invalid/used");
    const auto controller=typed_action_runtime::controller_status();
    if(!state.ready||GetCurrentThreadId()!=state.thread||!ui_remove_runtime::enabled()||!net::session_wire::connected()||state.fault||state.pending||
       !remove_ui_token.empty()||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))
        return fail(e,"Monster Remove UI requires ready connected idle Mainthread Controller");
    Memory m;Pin p;d::Sample before;a::MonsterState value;a::Binding binding;ur::LocalBinding local,remove_local;
    if(!info_code(m,e)||!info_target(birth,binding,p,before,value,local,e)||
       !remove_local_binding(local.manager,local.object,remove_local,e)||remove_local.uid!=local.uid)return false;
    local=remove_local;
    if(before.undo.active_graph)return fail(e,"Monster Remove UI requires idle Undo");
    a::ui_diagnostic::Api api;if(!a::ui_diagnostic::resolve_api(m,state.image,exe_sha,api,e))return false;
    api.command=reinterpret_cast<a::ui_diagnostic::UiCommand>(state.image+0x7490c0);api.destroy_label="Destroy";
    a::ui_diagnostic::Context context{state.image,local.manager,local.object,local.uid,binding.native_epoch,binding.native_generation,exe_sha,a::ui_diagnostic::Family::Building};
    a::ui_diagnostic::Receipt receipt;
    auto& job=state.ui_jobs.emplace(token,json{{"operation","remove"},{"family","monster"},{"token",token},{"target_action_uuid",birth},
        {"canonical_record_key",local.canonical_record_key},{"attempted",false},{"returned",false},{"captured",false},{"submitted",false},{"unknown",false}}).first->second;
    const auto unknown=[&](const std::string& cause){e=cause.empty()?"Monster Remove UI failed without cause":cause;job["cause"]=e;job["unknown"]=true;result=job;return false;};
    job["attempted"]=true;json tool;
    if(!info_default_tool(m,tool,e)){job["tool_probe"]=tool;return unknown(e);}job["tool_probe"]=tool;
    auto& calls=a::ui_diagnostic::process_calls();if(!calls.select(api,context,receipt,e))return unknown(e);
    a::ui_diagnostic::Window window;json window_receipt;
    if(!info_window(m,local,window,window_receipt,e)){job["window_probe"]=window_receipt;return unknown(e);}job["window_probe"]=window_receipt;
    d::Sample prepared;a::MonsterState actual;
    if(!effect(m,p,prepared,actual,e,local.uid)||actual.actor!=value.actor||!no_gameplay_change(before,prepared))
        return unknown(e.empty()?"Monster Remove UI selection preparation changed gameplay":e);
    const auto captures=ui_remove_runtime::status().value("captured",uint64_t{});
    remove_ui_token=token;struct Clear{~Clear(){remove_ui_token.clear();}} clear;
    const auto returned=calls.command(api,window,receipt,e);job["returned"]=receipt.ui_command_returned;
    job["record_cleanup_confirmed"]=receipt.record_cleanup_confirmed;
    const auto capture=ui_remove_runtime::status();
    if(!returned||capture.value("captured",uint64_t{})!=captures+1||capture.value("last_origin",uint8_t{})!=uint8_t(ur::Origin::MonsterWindow)||
       !job.value("captured",false)||!job.value("submitted",false))return unknown(e.empty()?"Monster original Destroy command did not confirm exact Host-ordered capture":e);
    if(Config::instance().is_host){
        const auto id=job.value("action_uuid",std::string{});const auto found=removals.find(id);
        const auto ordered=typed_action_runtime::controller_status();
        if(found==removals.end()||!ordered.value("started",false)||ordered.value("locked",true)||ordered.value("action_uuid",std::string{})!=id)
            return unknown("Monster Remove UI lacks exact synchronous Host operation");
        const auto& op=found->second;
        if(op.binding!=binding||op.request.action!=a::Action::RemoveMonster||op.request.action_uuid!=id||
           op.request.snapshot_uuid!=local.snapshot_uuid||op.request.target_action_uuid!=birth||op.request.canonical_record_key!=local.canonical_record_key||
           op.birth.pointer!=local.object||op.birth.uid!=local.uid||!op.attempted||!op.returned||!op.confirmed||op.original_calls!=1||
           op.evidence.monster_before!=value||!no_gameplay_change(prepared,op.before))
            return unknown(op.cause.empty()?"Monster Remove UI synchronous Host binding or original receipt differs":op.cause);
        a::Evidence current;
        if(!remove_observe(binding,op.request,current,e))return unknown(e);
        job["execution_confirmed"]=true;
    }else{
        d::Sample after;a::MonsterState final;
        if(!effect(m,p,after,final,e,local.uid)||final.actor!=value.actor||!no_gameplay_change(prepared,after))
            return unknown(e.empty()?"Monster Remove UI executed gameplay before Host order":e);
        job["execution_confirmed"]=false;
    }
    result=job;return true;
}catch(const std::exception& x){e=std::string("Monster Remove UI boundary: ")+x.what();return false;}
 catch(...){e="Monster Remove UI non-standard exception";return false;}
