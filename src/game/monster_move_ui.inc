namespace {
std::string move_ui_token;
}
bool move_ui_captured(const move_kiss::Intent& intent,const std::string& id,std::string& e){
    if(move_ui_token.empty())return true;
    auto job=state.ui_jobs.find(move_ui_token);
    if(job==state.ui_jobs.end()||job->second.value("operation",std::string{})!="move"||
       job->second.value("canonical_record_key",std::string{})!=intent.object_key||job->second.value("captured",false)||
       intent.family!=move_kiss::Family::DirectMonster||intent.object_key!="action:"+job->second.at("target_action_uuid").get<std::string>())
        return fail(e,"Monster Move UI exact birth capture differs");
    job->second["captured"]=true;job->second["awaiting_original_update"]=false;job->second["action_uuid"]=id;
    job->second["intent"]={{"position",{intent.after.x,intent.after.y,intent.after.z}},
        {"rotation",intent.after.rotation},{"scale",intent.after.scale}};
    return true;
}
void move_ui_submitted(const std::string& id){
    if(move_ui_token.empty())return;
    auto job=state.ui_jobs.find(move_ui_token);
    if(job==state.ui_jobs.end()||job->second.value("action_uuid",std::string{})!=id){move_ui_failed("Monster Move UI action differs after Controller submit");return;}
    job->second["submitted"]=true;move_ui_token.clear();
}
void move_ui_failed(const std::string& e){
    if(move_ui_token.empty())return;
    auto job=state.ui_jobs.find(move_ui_token);
    if(job!=state.ui_jobs.end()){job->second["cause"]=e;job->second["unknown"]=true;}
}

bool native_move_ui(const json& args,json& result,std::string& e)try{
    result=json::object();
    if(!args.is_object()||args.size()!=4||args.value("family",std::string{})!="monster"||
       !args.contains("token")||!args.at("token").is_string()||!args.contains("target_action_uuid")||!args.at("target_action_uuid").is_string()||
       !args.contains("position")||!args.at("position").is_array()||args.at("position").size()!=3)
        return fail(e,"Monster Move UI requires exact family, token, target_action_uuid and position");
    const auto token=args.at("token").get<std::string>(),birth=args.at("target_action_uuid").get<std::string>();
    if(!token_valid(token)||!token_valid(birth)||token==birth||state.ui_jobs.contains(token)||state.ui_jobs.size()>=64||!move_ui_token.empty())
        return fail(e,"Monster Move UI token or birth UUID invalid/used");
    std::array<float,3> point{};
    for(size_t i=0;i<3;++i){const auto& scalar=args.at("position")[i];
        if(!scalar.is_number())return fail(e,"Monster Move UI position requires numeric coordinates");
        const auto v=scalar.get<double>();if(!std::isfinite(v)||std::abs(v)>1000000)return fail(e,"Monster Move UI position outside finite domain");point[i]=float(v);}
    const auto controller=typed_action_runtime::controller_status();
    if(!state.ready||GetCurrentThreadId()!=state.thread||!ui_move_runtime::enabled()||!net::session_wire::connected()||state.fault||state.pending||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))
        return fail(e,"Monster Move UI requires ready connected idle Mainthread Controller");
    Memory m;a::Binding binding;Pin p;d::Sample before;a::MonsterState value;a::ui_capture::LocalBinding local;
    if(!info_code(m,e)||!info_target(birth,binding,p,before,value,local,e))return false;
    if(before.undo.active_graph)return fail(e,"Monster Move UI requires idle Undo");
    uintptr_t destination=0;const auto region_seh=shard_region(state.image,p.world.root,point,destination);
    if(region_seh||destination!=p.region.region)return fail(e,"Monster Move UI original same-Region destination differs SEH="+std::to_string(region_seh));
    a::ui_diagnostic::Api api;if(!a::ui_diagnostic::resolve_api(m,state.image,exe_sha,api,e))return false;
    api.command=reinterpret_cast<a::ui_diagnostic::UiCommand>(state.image+0x7490c0);api.destroy_label="Move";
    a::ui_diagnostic::Context context{state.image,local.manager,local.object,local.uid,binding.native_epoch,binding.native_generation,exe_sha,a::ui_diagnostic::Family::Building};
    a::ui_diagnostic::Receipt receipt;
    auto& job=state.ui_jobs.emplace(token,json{{"operation","move"},{"family","monster"},{"token",token},{"target_action_uuid",birth},
        {"canonical_record_key",local.canonical_record_key},{"attempted",false},{"returned",false},{"captured",false},{"submitted",false},{"unknown",false}}).first->second;
    const auto unknown=[&](const std::string& cause){e=cause.empty()?"Monster Move UI failed without cause":cause;job["cause"]=e;job["unknown"]=true;result=job;return false;};
    job["attempted"]=true;move_ui_token=token;json tool;
    if(!info_default_tool(m,tool,e)){job["tool_probe"]=tool;return unknown(e);}job["tool_probe"]=tool;
    auto& calls=a::ui_diagnostic::process_calls();if(!calls.select(api,context,receipt,e))return unknown(e);
    a::ui_diagnostic::Window window;json window_receipt;
    if(!info_window(m,local,window,window_receipt,e)){job["window_probe"]=window_receipt;return unknown(e);}job["window_probe"]=window_receipt;
    d::Sample selected;a::MonsterState selected_value;
    if(!effect(m,p,selected,selected_value,e,local.uid)||selected_value.actor!=value.actor||!no_gameplay_change(before,selected))
        return unknown(e.empty()?"Monster Move UI Select changed actor/roster/Undo/ledger":e);
    if(!calls.command(api,window,receipt,e))return unknown(e);
    job["command_returned"]=receipt.ui_command_returned;job["record_cleanup_confirmed"]=receipt.record_cleanup_confirmed;
    uintptr_t cursor=0,behaviour=0,ap=0,records=0,ghost=0,target=0;int32_t count=0,capacity=0;unsigned char flags[2]{};
    if(!field(m,state.image,0x13df9d0,cursor,e)||!field(m,cursor,0x190,behaviour,e)||!field(m,behaviour,0,ap,e)||ap!=state.image+0x172f1f0||
       !field(m,behaviour,0x460,records,e)||!field(m,behaviour,0x468,count,e)||!field(m,behaviour,0x46c,capacity,e)||count!=1||capacity<1||!records||
       !field(m,records,8,ghost,e)||!field(m,ghost,0,ap,e)||ap!=state.image+0x1705230||!field(m,records,0x10,target,e)||target!=local.object||
       !m.copy(behaviour+0x400,flags,2,e)||flags[0]!=1||flags[1]!=0)
        return unknown(e.empty()?"Monster original Move cursor/ghost/record or non-duplicate flags differ":e);
    const std::array<uintptr_t,3> steps{0x5c0200,0x5c05c0,0x5c01e0};
    for(size_t i=0;i<steps.size();++i){job["input_step"]=i==0?"MouseOver":i==1?"Press":"Release";
        job["input_attempted_mask"]=job.value("input_attempted_mask",0u)|(1u<<i);
        const auto seh=action_runtime::move_input(state.image,behaviour,cursor,point,steps[i]);
        if(seh)return unknown("Monster original Move cursor input SEH="+std::to_string(seh));
        job["input_returned_mask"]=job.value("input_returned_mask",0u)|(1u<<i);}
    if(!m.copy(behaviour+0x400,flags,2,e)||flags[0]!=0||flags[1]!=1)return unknown(e.empty()?"Monster original Move Release did not arm one pending Purchase":e);
    d::Sample after;a::MonsterState final;
    if(!effect(m,p,after,final,e,local.uid)||final.actor!=value.actor||!no_gameplay_change(selected,after))
        return unknown(e.empty()?"Monster Move UI changed gameplay before normal Update/Purchase capture":e);
    job["returned"]=true;job["awaiting_original_update"]=true;result=job;return true;
}catch(const std::exception& x){e=std::string("Monster Move UI boundary: ")+x.what();move_ui_failed(e);return false;}
 catch(...){e="Monster Move UI non-standard exception";move_ui_failed(e);return false;}
