namespace {
bool property_ui_text(Memory& memory,uintptr_t address,std::string& out,std::string& error){
    PropertyString text;
    if(!memory.copy(address,&text,sizeof text,error)||text.size>4096||(text.size&&(!text.data||uintptr_t(text.data)>UINTPTR_MAX-text.size)))
        return fail(error,error.empty()?"NPC property UI native string invalid":error);
    out.resize(text.size);return !text.size||memory.copy(uintptr_t(text.data),out.data(),text.size,error);
}
bool property_ui_codes(Memory& memory,std::string& error){
    struct Code {uintptr_t rva;size_t size;const char* sha;};
    const Code codes[]{
        {0x628c90,1104,"6c66c4ce3d4f807570a7eff011986d0873cd34f8aca8e586e64fd59126a2dbd4"},
        {0x7f1630,928,"e6b8e0151bcd7c78e528b2789f6f020c128507222f15ac54273ace73ab470dae"},
        {0x7d88b0,208,"9f2b8af5eddd1ca31d7dfe2ecd93f574daedaab31628dd8af240c47ea58a0540"},
        {0x8167c0,592,"bcad314728ab3392b6013ad666b3f69be773c4fcc646b7b9381611f141df981e"},
        {0xc35730,256,"37b604ccc54fb36f6beb2cdde2acc9fc27fcf6ed087c6b7376363af6e7ddb45f"}};
    for(const auto& code:codes){std::vector<uint8_t> bytes(code.size);
        if(!memory.copy(state.image+code.rva,bytes.data(),bytes.size(),error)||
           !mt2mp::hooks::framework::verify_original_hash(state.image+code.rva,bytes.data(),bytes.size(),code.sha,error))
            return fail(error,"NPC property original UI body differs RVA="+std::to_string(code.rva)+" original="+error);
    }
    return monster_action_runtime::quest_list::verify_npc_command(state.image+0x759ad0,error);
}
uint32_t property_ui_set_text(uintptr_t edit,const std::string& text)noexcept{
    const PropertyString view{text.data(),text.size(),{}};
    __try{reinterpret_cast<void(*)(void*,const PropertyString*)>(state.image+0x7f1630)(reinterpret_cast<void*>(edit),&view);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool native_property_ui(const json& args,json& result,std::string& error)try{
    result=json::object();
    if(!args.is_object()||args.size()!=6||!args.contains("token")||!args.at("token").is_string()||
       !args.contains("key")||!args.at("key").is_string()||!args.contains("kind")||!args.at("kind").is_number_integer()||
       !args.contains("name")||!args.at("name").is_string()||!args.contains("online")||!args.at("online").is_number_integer()||
       !args.contains("read_only")||!args.at("read_only").is_boolean())return fail(error,"NPC property UI requires exact token/key/kind/name/online/read_only");
    const auto token=args.at("token").get<std::string>(),key=args.at("key").get<std::string>();
    const auto kind=args.at("kind").get<int64_t>(),online=args.at("online").get<int64_t>();
    if(kind<1||kind>4||online<0||online>2)return fail(error,"NPC property UI kind/state outside domain");
    action::NpcPropertyIntent intent;intent.kind=action::NpcPropertyKind(kind);intent.name=args.at("name").get<std::string>();intent.online=int32_t(online);
    if(!token_valid(token)||!npc_target_key::valid(key)||!action::valid_npc_property(intent)||state.ui_jobs.contains(token)||state.ui_jobs.size()>=64)
        return fail(error,"NPC property UI token/key/intent invalid or already attempted; no replay");
    const auto controller=typed_action_runtime::controller_status();
    if(!Config::instance().local_test||!state.ready||!property_installed||GetCurrentThreadId()!=state.thread||
       !net::session_wire::connected()||state.pending||property_pending||state.capture_failed||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))
        return fail(error,"NPC property UI requires ready connected idle local-test Mainthread");
    const auto snapshot=net::current_snapshot_id();action::Binding binding;Memory memory;
    if(!capture_binding(snapshot,binding,error)||!property_ui_codes(memory,error))return false;
    ui::LocalBinding local;uintptr_t region=0;npc_census::State census;
    action::NpcPropertyState before;action::Ledger money;undo::UndoWitness witness;json tasks;
    if(!target_binding(npc_target_key::argument(key),binding,snapshot,local,region,census,error)||
       !property_read(memory,region,local.uid,local.object,before,money,witness,census,tasks,error)||witness.active_graph||
       action::npc_property_unchanged(before,intent))return fail(error,error.empty()?"NPC property UI needs an actual changed value and idle Undo":error);
    action::ui_diagnostic::Api api;if(!action::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return false;
    auto& job=state.ui_jobs.emplace(token,json{{"operation","property"},{"token",token},{"canonical_record_key",key},
        {"attempted",false},{"returned",false},{"captured",false},{"submitted",false},{"terminal",false},{"confirmed",false},
        {"intent",{{"kind",kind},{"name",intent.name},{"online",online}}}}).first->second;
    const auto unknown=[&](const std::string& cause){error=cause.empty()?"NPC property UI failed without cause":cause;
        state.diagnostic_token.clear();job["cause"]=error;job["unknown"]=job.value("attempted",false);job["terminal"]=true;result=job;return false;};
    json tool;if(!prepare_info_selection(memory,state.image,tool,error)){job["tool"]=tool;return unknown(error);}job["tool"]=tool;
    action::ui_diagnostic::Context context{state.image,local.manager,local.object,local.uid,binding.native_epoch,binding.native_generation,exe_sha,action::ui_diagnostic::Family::Building};
    action::ui_diagnostic::Receipt receipt;
    job["attempted"]=true;
    if(!action::ui_diagnostic::process_calls().select(api,context,receipt,error))return unknown(error);
    action::ui_diagnostic::Window window;json probe;
    if(!live_npc_window(memory,state.image,local,window,probe,error)){job["window"]=probe;return unknown(error);}job["window"]=probe;
    npc_property_ui_controls::Offered offered;
    const auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return field(memory,base,offset,value,cause);};
    const auto text=[&](uintptr_t address,std::string& value,std::string& cause){return property_ui_text(memory,address,value,cause);};
    if(!npc_property_ui_controls::find(read,text,state.image,window.window,local.object,intent,offered,error))return unknown(error);
    job["control_confirmed"]=true;job["command"]=offered.command;
    const auto callback_rva=intent.kind==action::NpcPropertyKind::Name?0x628c90:
        intent.kind==action::NpcPropertyKind::OnlineState?0x7d88b0:0x759ad0;
    uintptr_t receiver_class=0,callback=0;
    if(!field(memory,offered.receiver,0,receiver_class,error)||!field(memory,receiver_class,0x168,callback,error)||
       callback!=state.image+callback_rva)return unknown(error.empty()?"NPC property actual original UI callback slot differs":error);
    const auto unchanged=[&](){action::NpcPropertyState current;action::Ledger current_money;undo::UndoWitness current_undo;npc_census::State current_census;json current_tasks;
        if(!property_read(memory,region,local.uid,local.object,current,current_money,current_undo,current_census,current_tasks,error))return false;
        return (current==before&&current_money==money&&current_undo==witness&&current_census.manager==census.manager&&
            current_census.rows==census.rows&&current_census.next_local_tag==census.next_local_tag&&
            current_census.global_random==census.global_random&&current_tasks==tasks)||
            fail(error,"NPC original property UI preparation/capture changed gameplay state");};
    if(!unchanged())return unknown(error);
    if(args.at("read_only").get<bool>()){job["read_only"]=true;job["terminal"]=true;result=job;error.clear();return true;}
    const auto captures=state.captures,rejections=state.rejections;
    window.window=offered.receiver;
    api.command=reinterpret_cast<action::ui_diagnostic::UiCommand>(callback);
    if(intent.kind==action::NpcPropertyKind::Name){
        api.destroy_label="Edit";if(!action::ui_diagnostic::process_calls().command(api,window,receipt,error))return unknown(error);
        uint8_t editing=0;if(!field(memory,offered.receiver,0x348,editing,error)||editing!=1)return unknown(error.empty()?"NPC original Header did not enable editing":error);
        const auto seh=property_ui_set_text(offered.text_edit,intent.name);if(seh)return unknown("NPC original text control SetText SEH="+std::to_string(seh));
        std::string prepared;if(!property_ui_text(memory,offered.text_edit+0x2d0,prepared,error)||prepared!=intent.name||!unchanged())return unknown(error.empty()?"NPC original prepared name differs":error);
    }
    receipt={};
    api.destroy_label=offered.command.c_str();state.diagnostic_token=token;
    const bool returned=action::ui_diagnostic::process_calls().command(api,window,receipt,error);state.diagnostic_token.clear();
    job["returned"]=receipt.ui_command_returned;job["record_cleanup_confirmed"]=receipt.record_cleanup_confirmed;
    if(!returned||state.captures!=captures+1||state.rejections!=rejections||!property_pending||!job.value("captured",false)||
       property_pending->intent!=intent||property_pending->key!=key||property_pending->token!=token||!unchanged())
        return unknown(error.empty()?"NPC original property callback lacks exact unchanged capture":error);
    result=job;error.clear();return true;
}catch(const std::exception& exception){state.diagnostic_token.clear();error=std::string("NPC property UI exception: ")+exception.what();
    if(args.is_object()&&args.contains("token")&&args.at("token").is_string()){
        const auto found=state.ui_jobs.find(args.at("token").get<std::string>());
        if(found!=state.ui_jobs.end()){found->second["cause"]=error;found->second["unknown"]=found->second.value("attempted",false);found->second["terminal"]=true;result=found->second;}}
    return false;}
