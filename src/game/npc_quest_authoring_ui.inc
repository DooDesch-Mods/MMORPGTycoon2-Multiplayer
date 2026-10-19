namespace {
uint32_t selector_press(uintptr_t selector,uintptr_t cursor,const std::array<float,2>& point)noexcept{
    __try{reinterpret_cast<void(*)(void*,void*,const float*)>(state.image+0x78be40)(reinterpret_cast<void*>(selector),reinterpret_cast<void*>(cursor),point.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t cursor_press(uintptr_t cursor,const std::array<float,2>& screen,const std::array<float,3>& world)noexcept{
    __try{reinterpret_cast<void(*)(void*,const float*,const float*)>(state.image+0x584d10)(reinterpret_cast<void*>(cursor),screen.data(),world.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t collision_target(uintptr_t result,uintptr_t& target)noexcept{
    __try{
        target=reinterpret_cast<uintptr_t(*)(void*)>(state.image+0x5233f0)(reinterpret_cast<void*>(result));
        if(!target)target=reinterpret_cast<uintptr_t(*)(void*)>(state.image+0x523950)(reinterpret_cast<void*>(result));
        if(!target)target=reinterpret_cast<uintptr_t(*)(void*)>(state.image+0x523790)(reinterpret_cast<void*>(result));
        if(!target)target=reinterpret_cast<uintptr_t(*)(void*)>(state.image+0x523560)(reinterpret_cast<void*>(result));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool native_ui(const json& args,json& result,std::string& e)try{
    result=json::object();
    if(!args.is_object()||args.size()!=7||!args.contains("token")||!args.at("token").is_string()||
       !args.contains("owner_key")||!args.at("owner_key").is_string()||!args.contains("sequence_id")||!args.at("sequence_id").is_number_integer()||
       !args.contains("region_id")||!args.at("region_id").is_number_integer()||!args.contains("destination_region")||!args.at("destination_region").is_number_integer()||
       !args.contains("destination_key")||!args.at("destination_key").is_string()||!args.contains("family")||!args.at("family").is_string())
        return fail(e,"Quest target UI requires exact token, owner_key, sequence_id, region_id, destination_region, destination_key and family");
    const auto token=args.at("token").get<std::string>(),key=args.at("owner_key").get<std::string>(),target_key=args.at("destination_key").get<std::string>();
    const auto region=args.at("region_id").get<int64_t>(),target_region=args.at("destination_region").get<int64_t>(),sequence=args.at("sequence_id").get<int64_t>();
    const auto family_text=args.at("family").get<std::string>();
    if(!token_valid(token)||!npc_target_key::valid(key)||jobs.contains(token)||jobs.size()>=64||region<0||region>INT32_MAX||target_region<0||target_region>INT32_MAX||sequence<0||sequence>4095||
       (family_text!="building"&&family_text!="zone"&&family_text!="monster"&&family_text!="npc"))return fail(e,"Quest target UI token, canonical key, family or integer outside domain");
    const auto family=family_text=="npc"?a::QuestDestinationFamily::NPC:family_text=="monster"?a::QuestDestinationFamily::DirectMonster:family_text=="zone"?a::QuestDestinationFamily::Zone:a::QuestDestinationFamily::Building;
    a::Binding binding;Memory m;Pin p;action_runtime::ui::LocalBinding local;const auto snapshot=net::current_snapshot_id();
    if(!ready||fault||pending_ui()||GetCurrentThreadId()!=state.thread||!codes(m,true,e)||!typed_action_runtime::network_binding(snapshot,binding,e)||
       !owner(m,binding,snapshot,key,int32_t(region),local,p,e))return fail(e,e.empty()?"Quest target UI requires idle bound Mainthread":e);
    uintptr_t target=0,quest=0;uint64_t uid=0;qr::Witness before;d::Sample sample;a::NpcQuestState value;
    if(!destination(m,binding,snapshot,int32_t(target_region),family,target_key,target,uid,e)||
       !state_read(m,binding,snapshot,p,local.object,int32_t(sequence),before,sample,value,quest,e)||sample.undo.active_graph)
        return fail(e,e.empty()?"Quest target UI requires existing slot and idle Undo":e);
    a::NpcQuestIntent intent;intent.region=int32_t(region);intent.before=value.record;
    if(!target_intent(m,binding,snapshot,target,intent,e))return false;
    if(intent.before.destination_key==intent.destination_key&&intent.before.action==intent.action&&intent.before.monsters_to_kill==intent.kills)
        return fail(e,"Quest target UI destination is already selected");
    std::array<float,3> point{};if(!field(m,target,0x110,point,e))return false;
    for(float v:point)if(!std::isfinite(v)||std::abs(v)>1000000.f)return fail(e,"Quest destination actual pose is outside domain");
    a::ui_diagnostic::Api api;if(!a::ui_diagnostic::resolve_api(m,state.image,exe_sha,api,e))return false;
    auto& job=jobs.emplace(token,json{{"token",token},{"owner_key",key},{"sequence_id",sequence},{"destination_key",target_key},{"family",family_text},
        {"attempted",false},{"returned",false},{"captured",false},{"submitted",false},{"unknown",false},{"terminal",false}}).first->second;
    const auto unknown=[&](const std::string& why){e=why.empty()?"Quest target UI failed without cause":why;job["cause"]=e;job["unknown"]=true;job["terminal"]=true;fault=true;ui_token.clear();result=job;return false;};
    job["attempted"]=true;json tool;if(!info_default_tool(m,tool,e)){job["tool"]=tool;return unknown(e);}job["tool"]=tool;
    a::ui_diagnostic::Context context{state.image,local.manager,local.object,local.uid,binding.native_epoch,binding.native_generation,exe_sha,a::ui_diagnostic::Family::Building};
    a::ui_diagnostic::Receipt receipt;auto& calls=a::ui_diagnostic::process_calls();
    if(!calls.select(api,context,receipt,e))return unknown(e);
    a::ui_diagnostic::Window window;json window_receipt;
    if(!action_runtime::live_npc_window(m,state.image,local,window,window_receipt,e)){job["window"]=window_receipt;return unknown(e);}job["window"]=window_receipt;
    uintptr_t list=0,ap=0,data=0,selector=0,cursor=0;int32_t count=0,capacity=0;
    if(!field(m,window.window,0x530,list,e)||!field(m,list,0,ap,e)||ap!=state.image+0x16e02b0||
       !field(m,list,0x340,data,e)||!field(m,list,0x348,count,e)||!field(m,list,0x34c,capacity,e)||count<1||count>4096||capacity<count||capacity>100000||!data)
        return unknown(e.empty()?"Quest original InfoWindow list or bounds differs":e);
    for(int32_t i=0;i<count;++i){uintptr_t row=0,selected=0,proxy=0,weak=0;
        if(!field(m,data,size_t(i)*8,row,e)||!field(m,row,0,ap,e)||ap!=state.image+0x17088a0||!field(m,row,0x3e0,selected,e))return unknown(e.empty()?"Quest original selector class differs":e);
        if(selected!=quest)continue;
        if(selector||!field(m,row,0x3e8,proxy,e)||!field(m,proxy,0,weak,e)||weak!=quest+0x18)return unknown(e.empty()?"Quest selector duplicate or stale weak slot":e);selector=row;}
    if(!selector||!field(m,state.image,0x13df9d0,cursor,e)||!field(m,cursor,0,ap,e)||ap!=state.image+0x1786dd0)return unknown(e.empty()?"Quest selector or current Cursor absent":e);
    float sx=0,sy=0;SDL_GetMouseState(&sx,&sy);const std::array<float,2> screen{sx,sy};
    auto seh=selector_press(selector,cursor,screen);if(seh)return unknown("Quest original selector Press SEH="+std::to_string(seh));
    api.command=reinterpret_cast<a::ui_diagnostic::UiCommand>(state.image+0x78ed30);api.destroy_label="SetTarget";window.window=selector;
    if(!calls.command(api,window,receipt,e))return unknown(e);job["set_target_returned"]=receipt.ui_command_returned;
    uintptr_t display=0,actual_selector=0;if(!field(m,state.image,0x13df400,display,e)||!ui_binding(m,display,quest,local,actual_selector,e)||actual_selector!=selector)return unknown(e.empty()?"Quest original SetTarget did not bind expected selector":e);
    bool warped=false;seh=action_runtime::warp_cursor(state.image,cursor,point,warped);if(seh||!warped)return unknown("Quest original cursor Warp failed SEH="+std::to_string(seh));
    const float* hit_pointer=nullptr;seh=action_runtime::calculate_cursor_hit(state.image,cursor,hit_pointer);if(seh)return unknown("Quest original hit calculation SEH="+std::to_string(seh));
    std::array<float,3> hit{};uintptr_t collision=0,resolved=0;uint8_t hovered=0;
    if(uintptr_t(hit_pointer)!=cursor+0x1e4||!m.copy(uintptr_t(hit_pointer),hit.data(),12,e)||!field(m,cursor,0x230,collision,e)||!collision||!field(m,cursor,0x189,hovered,e)||!hovered)
        return unknown(e.empty()?"Quest original ray hit absent":e);
    seh=collision_target(collision,resolved);if(seh||resolved!=target)return unknown("Quest original collision priority does not select the exact bound destination SEH="+std::to_string(seh));
    job["actual_hit"]=hit;job["collision_matches"]=true;
    qr::Witness prepared;d::Sample prepared_sample;a::NpcQuestState prepared_value;uintptr_t prepared_quest=0;
    if(!state_read(m,binding,snapshot,p,local.object,int32_t(sequence),prepared,prepared_sample,prepared_value,prepared_quest,e)||prepared_value!=value||prepared_quest!=quest||
       !qr::unchanged(before,prepared,e)||!no_gameplay_change(sample,prepared_sample))return unknown(e.empty()?"Quest UI preparation changed gameplay":e);
    const auto prior_captures=captures;ui_token=token;seh=cursor_press(cursor,screen,hit);ui_token.clear();
    if(seh)return unknown("Quest original Cursor Press SEH="+std::to_string(seh));job["returned"]=true;
    if(captures!=prior_captures+1||!pending||pending->token!=token||pending->owner!=key||pending->intent!=intent)return unknown("Quest original Cursor Press did not capture exact selected intent: "+cause);
    qr::Witness after;d::Sample after_sample;a::NpcQuestState after_value;uintptr_t after_quest=0;
    if(!state_read(m,binding,snapshot,p,local.object,int32_t(sequence),after,after_sample,after_value,after_quest,e)||after_value!=value||after_quest!=quest||
       !qr::unchanged(before,after,e)||!no_gameplay_change(sample,after_sample))return unknown(e.empty()?"Quest captured Press changed gameplay before Host ordering":e);
    result=job;return true;
}catch(const std::exception& x){fault=true;e=std::string("Quest UI boundary: ")+x.what();reject(e);return false;}
 catch(...){fault=true;e="Quest UI non-standard exception";reject(e);return false;}
