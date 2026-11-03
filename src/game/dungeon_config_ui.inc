namespace {
json ui_state(const a::DungeonConfigState& value){json instances=json::array();for(const auto& instance:value.instances)instances.push_back({instance.index,instance.started});
    return {{"desired",value.desired},{"price",value.price},{"sandbox",value.sandbox},{"instances",instances},{"global_random",value.global_random},{"undo_count",value.undo_count}};}
json ui_money(const a::Ledger& value){return {{"cash",value.cash},{"flat_day",value.flat_day},{"record_present",value.record_present},{"record_day",value.record_day},
    {"record_cash",value.record_cash},{"record_income",value.record_income},{"record_expense",value.record_expense},{"line_counts",value.line_counts},{"line_income",value.line_income}};}
bool ui_codes(Memory& memory,std::string& error){
    const dungeon_config_codes::Code pins[]{
        {0x81ec20,49,"741174f0f71b5b51a67006e724613e4a79d92ed3088aa68001973f591c064467"},
        {0x66d990,3042,"03e58cce712898efef65baa4d798734dc1a8cd24fdb1707a845482f77393654f"},
        {0x7568e0,157,"a1483366c1b99428768c0f27a851f5466c6181d55c2bc83ac5901bd95bb47808"},
        {0x755cc0,647,"653ea957dc8eff74cdf62ef43456256989112d9012823bc15530f006c98aaeb4"},
        {0x752d70,130,"1047a7db0ece8b4fb40aaae3c1682bb8d44c61f9cc66f514903fde62a507c308"},
        {0x7c3560,731,"9a676e5edc0c89142f56f43fcd89230687982ef411e5e4aa952f20101120b56f"},
        {0x7c0240,2018,"cef1e12c887a67410e68aa552f823111a9193a075889f5b2dfb217e14da2faef"}};
    for(const auto& pin:pins){std::string bytes(pin.size,'\0');if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||
        !mt2mp::hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))return false;}
    return code(memory,error);
}
uint32_t tab_enabled(uintptr_t view,uintptr_t page,bool& enabled)noexcept{__try{
    enabled=reinterpret_cast<bool(*)(void*,const void*)>(state.image+0x755cc0)(reinterpret_cast<void*>(view),reinterpret_cast<void*>(page+0x1b0));return 0;
}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool tab_record_equals(const void* record,const char* expected){
    return reinterpret_cast<a::ui_diagnostic::RecordEquals>(state.image+0x1113d80)(record,expected);
}
uint32_t find_control(uintptr_t outer,const char* name,size_t size,uintptr_t& found)noexcept{__try{
    const quest_list::QuestTitleView label{name,size};found=reinterpret_cast<uintptr_t(*)(void*,const quest_list::QuestTitleView&)>(state.image+0x81d860)(reinterpret_cast<void*>(outer),label);return 0;
}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool controls(Memory& memory,uintptr_t outer,uintptr_t dungeon,uintptr_t& spinner,dungeon_config_ui::Controls& value,std::string& error){
    uintptr_t accept=0;auto seh=find_control(outer,"instances_count",15,spinner);if(!seh)seh=find_control(outer,"accept",6,accept);
    if(seh)return fail(error,"Dungeon config original named control lookup SEH="+std::to_string(seh));
    const auto read=[&](uintptr_t object,size_t offset,auto& out,std::string& e){return field(memory,object,offset,out,e);};
    const auto text=[&](uintptr_t address,std::string& out,std::string& e){return action_runtime::property_ui_text(memory,address,out,e);};
    return dungeon_config_ui::validate(read,text,state.image,outer,dungeon,spinner,accept,value,error);
}
}
bool read_ui(const json& args,json& result,std::string& error){
    result=json::object();if(!args.is_object()||args.size()!=1||!args.contains("token")||!args.at("token").is_string())return fail(error,"Dungeon config UI read requires exact token");
    if(GetCurrentThreadId()!=state.thread)return fail(error,"Dungeon config UI read requires bound Mainthread");
    const auto found=ui_jobs.find(args.at("token").get<std::string>());if(found==ui_jobs.end())return fail(error,"Dungeon config UI token is unknown");result=found->second;result["capture_fault"]=fault;result["capture_cause"]=cause;
    const auto id=result.value("action_uuid",std::string{});const auto operation=operations.find(id);
    if(operation!=operations.end()){const auto& op=operation->second;result["execution"]={{"ordered",op.ordered},{"attempted",op.attempted},{"returned",op.returned},
        {"original_calls",op.calls},{"confirmed",op.confirmed},{"retired",op.retired},{"seed_attempted",op.seed_attempted},{"cause",op.cause}};
        if(op.attempted){result["canonical_before"]=ui_state(op.evidence.dungeon_config_before);result["ledger_before"]=ui_money(op.evidence.before);}
        if(op.confirmed){result["canonical_after"]=ui_state(op.evidence.dungeon_config_after);result["ledger_after"]=ui_money(op.evidence.after);}}
    error.clear();return true;
}
bool native_ui(const json& args,json& result,std::string& error)try{
    result=json::object();if(!args.is_object()||args.size()!=4||!args.contains("token")||!args.at("token").is_string()||
       !args.contains("key")||!args.at("key").is_string()||!args.contains("desired")||!args.at("desired").is_number_integer()||
       !args.contains("read_only")||!args.at("read_only").is_boolean())return fail(error,"Dungeon config UI requires exact token/key/desired/read_only");
    const auto token=args.at("token").get<std::string>(),key=args.at("key").get<std::string>();const auto count=args.at("desired").get<int64_t>();
    const bool continuing=ui_resuming&&ui_preparation&&ui_preparation->args==args;
    if(!token_valid(token)||count<1||count>10||!dungeon_scene_key::building_key(key)||(!continuing&&(ui_jobs.contains(token)||ui_jobs.size()>=64)))
        return fail(error,"Dungeon config UI token/key/domain invalid or already attempted; no replay");
    const auto controller=typed_action_runtime::controller_status();
    if(!Config::instance().local_test||!ready||fault||pending||replaying||(!continuing&&ui_preparation)||GetCurrentThreadId()!=state.thread||!net::session_wire::connected()||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))return fail(error,"Dungeon config UI requires idle connected local-test Mainthread");
    const auto snapshot=net::current_snapshot_id();a::Binding binding;Memory memory;a::DungeonConfigIntent intent{key,0,int32_t(count)};Sample before;
    if(!action_runtime::capture_binding(snapshot,binding,error)||!ui_codes(memory,error)||!sample(memory,binding,snapshot,intent,before,error))return false;
    std::optional<a::Context> context;if(!capture_context(snapshot,binding,context,error))return false;
    if(continuing&&(GetTickCount64()>ui_preparation->deadline||ui_jobs.at(token).at("snapshot_uuid")!=snapshot||ui_preparation->binding!=binding||ui_preparation->context!=context||
       !same_local(ui_preparation->before,before)||ui_preparation->before.value!=before.value||ui_preparation->before.money!=before.money))return fail(error,"Dungeon config original tab wait timed out or captured Session/world changed");
    const int64_t cost=std::max<int64_t>(count-before.value.desired,0)*before.value.price;
    if(cost>INT32_MAX||(!before.value.sandbox&&before.money.cash<cost))return fail(error,"Dungeon config UI actual original purchase is unaffordable");
    auto& job=continuing?ui_jobs.at(token):ui_jobs.emplace(token,json{{"token",token},{"key",key},{"desired",count},{"snapshot_uuid",snapshot},{"attempted",false},{"captured",false},
        {"submitted",false},{"unknown",false},{"read_only",args.at("read_only")},{"canonical_before",ui_state(before.value)},{"ledger_before",ui_money(before.money)}}).first->second;
    const auto unknown=[&](const std::string& why){ui_token.clear();ui_preparation.reset();error=why.empty()?"Dungeon config original UI failed without cause":why;job["cause"]=error;job["unknown"]=job.value("attempted",false);
        if(pending&&pending->token==token){fault=true;reject(error);}result=job;return false;};
    a::ui_diagnostic::Api api;if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return unknown(error);
    uintptr_t manager=0;if(!field(memory,state.image,0x13df630,manager,error)||!manager)return unknown(error.empty()?"Dungeon config actual selection Manager absent":error);
    a::ui_diagnostic::Context selected{state.image,manager,before.local.exterior,before.local.uid,binding.native_epoch,binding.native_generation,exe_sha,a::ui_diagnostic::Family::Building};
    a::ui_diagnostic::Receipt selection;job["attempted"]=true;
    if(!continuing&&!a::ui_diagnostic::process_calls().select(api,selected,selection,error))return unknown(error);
    a::ui_diagnostic::Window selected_window;if(!a::ui_diagnostic::read_live_window(memory,selected,selected_window,error))return unknown(error);
    job["selection_confirmed"]=true;
    const auto offer_window=selected_window.window;uintptr_t tabs=0,page=0;auto seh=find_control(offer_window,"multitabview",12,tabs);
    if(!seh)seh=find_control(offer_window,"{dungeoninfo_tab_instances}",27,page);
    if(seh)return unknown("Dungeon config original offered control lookup SEH="+std::to_string(seh));
    const auto read=[&](uintptr_t object,size_t offset,auto& out,std::string& e){return field(memory,object,offset,out,e);};
    const auto text=[&](uintptr_t address,std::string& out,std::string& e){return action_runtime::property_ui_text(memory,address,out,e);};
    dungeon_config_ui::Tab tab;if(!dungeon_config_ui::prepare(read,text,state.image,offer_window,before.local.exterior,tabs,page,before.local.dungeon,tab,error))return unknown(error);
    bool enabled=false;seh=tab_enabled(tab.view,tab.page,enabled);if(seh||!enabled)return unknown("Dungeon config original Instances tab is disabled or failed SEH="+std::to_string(seh));
    if(!continuing){
        const auto page_record="SetPage \""+tab.label+"\"";const auto original_label=api.label_equals;
        api.command=reinterpret_cast<Command>(state.image+0x7568e0);api.destroy_label=page_record.c_str();api.label_equals=&tab_record_equals;
        a::ui_diagnostic::Window pages;pages.window=tab.view;a::ui_diagnostic::Receipt changed;
        if(!a::ui_diagnostic::process_calls().command(api,pages,changed,error)||!changed.ui_command_returned||!changed.record_cleanup_confirmed)return unknown(error.empty()?"Dungeon config original Instances tab lacks return/cleanup":error);
        api.label_equals=original_label;
        ui_preparation=UiPreparation{args,binding,context,before,offer_window,0,tab.page,tab.view,tab.label,GetTickCount64()+5000};
    }else if(ui_preparation->window!=offer_window||ui_preparation->page!=tab.page||
             ui_preparation->view!=tab.view||ui_preparation->label!=tab.label)return unknown("Dungeon config actual Instances tab owner changed while waiting");
    uintptr_t active_page=0;uint8_t visible=0;
    if(!field(memory,tab.view,0x410,active_page,error)||!field(memory,tab.page,0x69,visible,error))return unknown(error);
    if(active_page!=tab.page||visible!=1){job["waiting_for_tab"]=true;result=job;error.clear();return true;}
    uintptr_t button=0;seh=find_control(offer_window,"instances_config",16,button);if(seh)return unknown("Dungeon config original visible Configure lookup SEH="+std::to_string(seh));
    dungeon_config_ui::Tab shown;if(!dungeon_config_ui::offer(read,text,state.image,offer_window,before.local.exterior,button,before.local.dungeon,shown,error)||
       shown.page!=tab.page||shown.view!=tab.view||shown.label!=tab.label)return unknown(error.empty()?"Dungeon config actual Instances tab or Configure visibility differs":error);
    ui_preparation.reset();job["waiting_for_tab"]=false;
    job["tab_confirmed"]=true;job["offer_confirmed"]=true;api.command=reinterpret_cast<Command>(state.image+0x66d990);api.destroy_label="OpenInstancesConfig";
    a::ui_diagnostic::Window offered;offered.window=offer_window;a::ui_diagnostic::Receipt opened;
    if(!a::ui_diagnostic::process_calls().command(api,offered,opened,error)||!opened.ui_command_returned||!opened.record_cleanup_confirmed)return unknown(error.empty()?"Dungeon config original OpenInstancesConfig lacks return/cleanup":error);
    uintptr_t outer=0,owner=0;int32_t desired=0,price=0;if(!window(memory,outer,owner,desired,price,error)||owner!=before.local.dungeon)return unknown(error.empty()?"Dungeon config original OpenInstancesConfig selected another owner":error);
    uintptr_t spinner=0;dungeon_config_ui::Controls actual;
    if(!controls(memory,outer,before.local.dungeon,spinner,actual,error)||actual.value!=before.value.desired)return unknown(error.empty()?"Dungeon config original prepared Spinner differs":error);
    const auto unchanged=[&](){Sample now;if(!sample(memory,binding,snapshot,intent,now,error))return false;
        return (same_local(before,now)&&before.value==now.value&&before.money==now.money)||fail(error,"Dungeon config original UI preparation/capture changed gameplay");};
    if(!unchanged())return unknown(error);job["controls_confirmed"]=true;
    if(args.at("read_only").get<bool>()){result=job;error.clear();return true;}
    unsigned steps=0;while(actual.value!=count){if(++steps>9)return unknown("Dungeon config Spinner exceeded original 1-10 step bound");
        const auto expected=actual.value+(actual.value<count?1:-1);api.command=reinterpret_cast<Command>(state.image+0x7c3560);
        api.destroy_label=actual.value<count?"SpinnerIncrement":"SpinnerDecrement";a::ui_diagnostic::Window control;control.window=spinner;a::ui_diagnostic::Receipt receipt;
        if(!a::ui_diagnostic::process_calls().command(api,control,receipt,error)||!receipt.ui_command_returned||!receipt.record_cleanup_confirmed)return unknown(error.empty()?"Dungeon config original Spinner callback lacks return/cleanup":error);
        uintptr_t current_spinner=0;if(!controls(memory,outer,before.local.dungeon,current_spinner,actual,error)||current_spinner!=spinner||actual.value!=expected||!unchanged())return unknown(error.empty()?"Dungeon config actual Spinner step or owner differs":error);
    }
    if(!window(memory,outer,owner,desired,price,error)||owner!=before.local.dungeon||desired!=count||price!=cost)return unknown(error.empty()?"Dungeon config original Spinner did not send exact SetInstanceCount/cost":error);
    const auto previous=captures;api.command=reinterpret_cast<Command>(state.image+ui_code.rva);api.destroy_label="Accept";a::ui_diagnostic::Window target;target.window=outer;a::ui_diagnostic::Receipt receipt;
    ui_token=token;const bool returned=a::ui_diagnostic::process_calls().command(api,target,receipt,error);ui_token.clear();job["returned"]=receipt.ui_command_returned;job["record_cleanup_confirmed"]=receipt.record_cleanup_confirmed;job["spinner_steps"]=steps;
    if(!returned||!receipt.ui_command_returned||!receipt.record_cleanup_confirmed||captures!=previous+1||!pending||pending->token!=token||
       pending->intent!=a::DungeonConfigIntent{key,before.value.desired,int32_t(count)}||!unchanged())return unknown(error.empty()?"Dungeon config original Accept lacks exact unchanged capture":error);
    result=job;error.clear();return true;
}catch(const std::exception& exception){ui_token.clear();ui_preparation.reset();error=std::string("Dungeon config UI exception: ")+exception.what();
    if(args.is_object()&&args.contains("token")&&args.at("token").is_string()){const auto token=args.at("token").get<std::string>();const auto found=ui_jobs.find(token);if(found!=ui_jobs.end()){found->second["cause"]=error;found->second["unknown"]=found->second.value("attempted",false);result=found->second;}
        if(pending&&pending->token==token){fault=true;reject(error);}}
    return false;}
void tick_ui(){
    if(!ui_preparation)return;
    const auto args=ui_preparation->args;const auto token=args.at("token").get<std::string>();json result;std::string error;
    ui_resuming=true;const bool okay=native_ui(args,result,error);ui_resuming=false;
    if(!okay){ui_preparation.reset();if(error.empty())error="Dungeon config original tab continuation failed without cause";auto& job=ui_jobs.at(token);job["cause"]=error;job["unknown"]=true;job["waiting_for_tab"]=false;fault=true;reject(error);}
}
