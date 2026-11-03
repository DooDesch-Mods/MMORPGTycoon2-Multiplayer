namespace character_rename {
namespace {
#include "character_offered_controls.inc"
bool edit_code(Memory& memory,std::string& error){
    constexpr preview::Pin pins[]{
        {0x674780,1002,"fc26de4faf43a67eff514d5f1ac974ec58657afa6f878dd24ee8fe4441920258"},
        {0x6a0a30,19,"b7981a6905e87700da2eecf32d2d7718c21265b5d65c33c17adca7ad51289a87"},
        {0x6a32a0,2777,"3301abb85b53c6157e63744ab106635021b857d0f5add3208708731150d21ea4"},
        {0x6a1780,153,"b788c070ee48b5a25c3b5c7034e2afa585343d63ef447255117ec01598329367"},
        {0x7524f0,25,"4eae098a2651c18f169f48c29e446d832192b33fb437ed351c3dc2beef061ed9"},
        {0x756470,1133,"bdfff6320aef73296e059e2a66eb4a52d955f0859e173cfd38a71ce4d69f7597"},
        {0x564890,269,"5ad60c05465b95aea9cfc6e4e1e8409aa35dc2d7ad44b20f0e2ada09baaa30f1"},
        {0x81ada0,123,"b700973ea00bacf50ea6326378d4a87d21d33b3e3cffd25930840483a77df549"},
        {0x6ab830,862,"feb57404da263f78acbd79de826111431848f84d4cb46f4402e3b13f0375f63e"}};
    for(const auto& pin:pins){std::vector<unsigned char> bytes(pin.bytes);
        if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||
           !hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))
            return fail(error,"Character offered native input code differs RVA="+std::to_string(pin.rva)+" cause="+error);}
    return true;
}
uint32_t edit_set_page(uintptr_t tab,const char* name,size_t size)noexcept{
    __try{const quest_list::QuestTitleView value{name,size};
        reinterpret_cast<void(*)(void*,const quest_list::QuestTitleView&)>(state.image+0x7524f0)(reinterpret_cast<void*>(tab),value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t edit_checkbox(uintptr_t pane)noexcept{
    // These exact native methods do not read their cursor/position arguments.
    __try{using Input=void(*)(void*,void*,const void*);
        reinterpret_cast<Input>(state.image+0x6a0a30)(reinterpret_cast<void*>(pane),nullptr,nullptr);
        reinterpret_cast<Input>(state.image+0x6a32a0)(reinterpret_cast<void*>(pane),nullptr,nullptr);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool edit_target(Memory& memory,const json& args,uintptr_t& view,uintptr_t& outer,uintptr_t& owner,std::string& error){
    uintptr_t editor=0;int32_t kind=-1,id=-1;
    if(!window(memory,0,view,editor,outer,error)||!field(memory,view,0x460,owner,error)||!owner||
       !field(memory,owner,0x2c,kind,error)||!field(memory,owner,0x30,id,error)||args.at("kind")!=kind||args.at("id")!=id)
        return fail(error,error.empty()?"Character offered input selected definition differs":error);
    bool shown=false;
    return offered_visibility(memory,view,view,outer,shown,error)&&(shown||fail(error,"Character offered input requires the visible original Character view"));
}
bool edit_open(UiJob& job,std::string& error){
    Memory memory;OfferedCard card;a::CharacterRenameIntent intent;Sample before,after;DesignMenu menu;
    intent.kind=job.arguments.at("kind").get<int32_t>();intent.id=job.arguments.at("id").get<int32_t>();
    if(!code(memory,error)||!open_code(memory,error)||!sample(memory,job.binding,job.context.snapshot_uuid,intent,before,error)||
       !design_menu(memory,menu,error))return false;
    uintptr_t view=0,editor=0,outer=0,selected=0;uint8_t shown=0;
    if(!window(memory,0,view,editor,outer,error)||!field(memory,view,0x460,selected,error)||!field(memory,outer,0x45f,shown,error))return false;
    if(selected==before.selected&&shown==1){job.result["original_character_already_selected"]=true;return true;}
    if(!menu.collection_shown&&!execute_menu(job,before,intent,error))return false;
    if(!offered_card(memory,job.arguments,card,error))return false;
    a::ui_diagnostic::Api api;if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return false;
    api.command=reinterpret_cast<Command>(state.image+0x681090);api.destroy_label="clicked";
    a::ui_diagnostic::Window control;control.window=card.card;a::ui_diagnostic::Receipt receipt;job.attempted=true;
    if(!a::ui_diagnostic::process_calls().command(api,control,receipt,error)||!receipt.ui_command_returned||!receipt.record_cleanup_confirmed||
       !window(memory,before.selected,view,editor,outer,error)||
       !sample(memory,job.binding,job.context.snapshot_uuid,intent,after,error)||!same_local(before,after)||before.value!=after.value)
        return fail(error,error.empty()?"Character original card open lacks its complete unchanged gameplay/RNG proof":error);
    job.result["original_class_card_open"]=true;job.result["open_complete_sha256"]=after.value.all_sha256;return true;
}
bool edit_page(UiJob& job,uintptr_t root,uintptr_t outer,const std::string& tab_id,const std::string& page_id,bool request,bool& current,std::string& error){
    Memory memory;std::vector<OfferedControl> controls;OfferedControl tab;OfferedPage page;
    if(!offered_controls(memory,root,outer,controls,error)||!offered_tab(controls,tab_id,tab,error)||!offered_page(memory,tab,page_id,page,error))return false;
    current=page.current==page.page&&page.requested==page.page;bool visible=false;
    if(current){uintptr_t parent=0,owner=0;
        if(!field(memory,page.page,0x278,parent,error)||!field(memory,page.page,0x280,owner,error))return false;
        if((parent&&parent!=page.pages)||(owner&&owner!=outer)||bool(parent)!=bool(owner))
            return fail(error,"Character current original page live owner/parent differs parent="+std::to_string(parent)+
                " expected="+std::to_string(page.pages)+" owner="+std::to_string(owner)+" expected="+std::to_string(outer));
        if(!offered_visibility(memory,page.page,root,outer,visible,error))return false;current=visible;
    }
    if(current)return true;
    if(!request&&page.requested!=page.page)return fail(error,"Character original requested page changed while awaiting Layout");
    if(request){
        a::CharacterRenameIntent intent;intent.kind=job.arguments.at("kind").get<int32_t>();intent.id=job.arguments.at("id").get<int32_t>();Sample before,after;
        if(!sample(memory,job.binding,job.context.snapshot_uuid,intent,before,error))return false;
        job.attempted=true;const auto seh=edit_set_page(tab.pane,page_id.c_str(),page_id.size());
        if(seh||!sample(memory,job.binding,job.context.snapshot_uuid,intent,after,error)||!same_local(before,after)||before.value!=after.value)
            return fail(error,error.empty()?"Character original tab selection changed complete gameplay/ArtStore/RNG proof SEH="+std::to_string(seh):error);
        job.result["original_tab_selections"].push_back(page_id);
    }
    job.waiting=true;
    if(++job.waits>8)return fail(error,"Character original page Layout did not become offered after 8 native UI updates page="+page_id);
    return true;
}
bool costume_offered(Memory& memory,const json& args,uintptr_t& outer,uintptr_t& named,std::vector<OfferedControl>& controls,std::string& error){
    uintptr_t view=0,owner=0,live=0,draft=0,actual_owner=0;
    if(!edit_target(memory,args,view,named,owner,error)||!costume_edit::window(memory,outer,live,draft,actual_owner,error)||
       !live||!draft||actual_owner!=owner)return fail(error,error.empty()?"Character offered Costume editor owner/draft differs":error);
    bool visible=false;
    return offered_visibility(memory,outer,outer,named,visible,error)&&(visible||fail(error,"Character original Costume editor is not visible"))&&
        offered_controls(memory,outer,named,controls,error);
}
json costume_controls(const std::vector<OfferedControl>& controls){
    json rows=json::array();const std::string prefix="MessageTo Parent WeaponCategory ";
    for(const auto& value:controls)if(value.checkbox&&value.ready&&value.command.starts_with(prefix))
        rows.push_back({{"category",value.command.substr(prefix.size())},{"enabled",value.checked},{"offered",true}});
    return rows;
}
#include "character_offered_ability.inc"
bool execute_edit_ui(UiJob& job,const std::string& token,std::string& error){
    Memory memory;job.waiting=false;const auto operation=job.arguments.at("operation").get<std::string>();
    if(!code(memory,error)||!edit_code(memory,error))return false;
    if(operation.find("Ability")!=std::string::npos)return execute_ability_ui(job,token,error);
    if(operation=="ShowCostume"){
        if(job.stage==0){if(!edit_open(job,error))return false;job.stage=1;job.waits=0;job.waiting=true;return false;}
        uintptr_t view=0,named=0,owner=0;if(!edit_target(memory,job.arguments,view,named,owner,error))return false;
        bool current=false;
        if(job.stage<=2){
            if(!edit_page(job,view,named,"multi","{design_character_tab_modeleditor}",job.stage==1,current,error))return false;
            job.stage=2;if(!current)return false;job.stage=3;job.waits=0;
        }
        uintptr_t outer=0,live=0,draft=0,actual_owner=0;
        if(!costume_edit::window(memory,outer,live,draft,actual_owner,error)||actual_owner!=owner)return false;
        if(job.stage<=4){
            if(!edit_page(job,outer,named,"","{design_character_editor_tab_settings}",job.stage==3,current,error))return false;
            job.stage=4;if(!current)return false;job.stage=5;
        }
    }
    uintptr_t outer=0,named=0;std::vector<OfferedControl> controls;
    if(!costume_offered(memory,job.arguments,outer,named,controls,error))return false;
    job.result["weapon_categories"]=costume_controls(controls);job.result["kind"]=job.arguments.at("kind");job.result["id"]=job.arguments.at("id");
    if(operation=="ShowCostume"||operation=="CostumeState")return true;
    if(operation!="ToggleWeapon")return fail(error,"Character offered UI operation is not implemented");
    OfferedControl checkbox;const auto category=job.arguments.at("category").get<std::string>();const bool expected=job.arguments.at("expected_enabled").get<bool>();
    if(!offered_unique(controls,"MessageTo Parent WeaponCategory "+category,true,checkbox,error)||checkbox.checked!=expected)
        return fail(error,error.empty()?"Costume original checkbox current state differs from expected_enabled":error);
    a::CostumeEditIntent intent;intent.kind=job.arguments.at("kind").get<int32_t>();intent.id=job.arguments.at("id").get<int32_t>();
    costume_edit::Sample before,after;
    if(!costume_edit::code(memory,error)||!costume_edit::sample(memory,job.binding,job.context.snapshot_uuid,intent,before,error))return false;
    const auto previous=costume_edit::captures;job.attempted=true;active_ui_token=token;
    const auto seh=edit_checkbox(checkbox.pane);active_ui_token.clear();
    uint8_t checked=0,held=1;const auto& saved=costume_edit::pending;
    if(seh||costume_edit::fault||!saved||saved->diagnostic_token!=token||saved->intent.kind!=intent.kind||saved->intent.id!=intent.id||
       !saved->intent.weapon_category||saved->intent.weapon_category->category!=category||saved->intent.weapon_category->enabled==expected||
       costume_edit::captures!=previous+1||!field(memory,checkbox.pane,0x346,checked,error)||checked!=uint8_t(!expected)||
       !field(memory,checkbox.pane,0x344,held,error)||held||
       !costume_edit::sample(memory,job.binding,job.context.snapshot_uuid,intent,after,error)||!costume_edit::same_local(before,after)||before.value!=after.value)
        return fail(error,error.empty()?"Costume original checkbox lacks one exact deferred intent and unchanged gameplay/RNG proof SEH="+std::to_string(seh):error);
    job.result["original_checkbox_press_release"]=true;job.result["captured"]=true;job.result["category"]=category;job.result["enabled"]=!expected;
    return true;
}
}
std::string native_edit_input_token(){
    const auto found=ui_jobs.find(active_ui_token);
    return found!=ui_jobs.end()&&found->second.executor?active_ui_token:std::string{};
}
bool native_edit_submitted(const std::string& token,const std::string& uuid,std::string& error){
    const auto found=ui_jobs.find(token);
    if(found==ui_jobs.end()||!found->second.executor)return fail(error,"Character offered input token does not own this submitted intent");
    return diagnostic_submitted(token,uuid,error);
}
bool native_edit_read(const json& args,json& result,std::string& error)try{
    if(!args.is_object()||args.size()!=1||!args.contains("token")||!args.at("token").is_string()||!Config::instance().local_test||GetCurrentThreadId()!=state.thread)
        return fail(error,"Character offered input read requires one retained token on owned Mainthread");
    const auto found=ui_jobs.find(args.at("token").get<std::string>());
    if(found==ui_jobs.end()||!found->second.executor)return fail(error,"Character offered input token is not retained");
    const auto& job=found->second;result={{"token",found->first},{"input_attempted",job.attempted},{"input_returned",job.returned},
        {"waiting_for_ui",job.waiting},{"action_uuid",job.action_uuid},{"cause",job.cause},{"native",job.result}};return true;
}catch(const std::exception& exception){return fail(error,std::string("Character offered input read boundary: ")+exception.what());}
bool native_edit_ui(const json& args,json& result,std::string& error)try{
    const std::set<std::string> required{"token","operation","kind","id"};
    if(!args.is_object())return fail(error,"Character offered UI requires an object");
    for(const auto& key:required)if(!args.contains(key))return fail(error,"Character offered UI missing field="+key);
    if(!args.at("token").is_string()||!args.at("operation").is_string()||!args.at("kind").is_number_integer()||!args.at("id").is_number_integer())
        return fail(error,"Character offered UI scalar types differ");
    const auto token=args.at("token").get<std::string>(),operation=args.at("operation").get<std::string>();
    const auto kind=args.at("kind").get<int64_t>(),id=args.at("id").get<int64_t>();
    const bool toggle=operation=="ToggleWeapon";
    const std::set<std::string> ability_ops{"ShowAbility","AbilityState","ShowAbilityPage","EditAbilityText","EditAbilityChoice","ToggleAbilityControl","ActivateAbilityControl","ApplyAbility"};
    const bool ability=ability_ops.contains(operation);auto expected=required;
    if(operation!="ShowCostume"&&operation!="CostumeState"&&!toggle&&!ability)return fail(error,"Character offered UI operation is not supported");
    if(toggle){expected.insert("category");expected.insert("expected_enabled");}
    const bool needs_slot=ability&&(operation!="ShowAbility"||args.contains("slot"));
    if(needs_slot)expected.insert("slot");
    if(operation=="ShowAbilityPage")expected.insert("page");
    if(operation=="EditAbilityText"||operation=="EditAbilityChoice"){expected.insert("control");expected.insert("value");}
    if(operation=="EditAbilityText"&&args.contains("commit"))expected.insert("commit");
    if(operation=="ToggleAbilityControl"){expected.insert("control");expected.insert("expected_enabled");}
    if(operation=="ActivateAbilityControl")expected.insert("control");
    if(args.size()!=expected.size()||!token_valid(token)||kind<0||kind>4||id<0||id>=1000000||ui_jobs.contains(token)||ui_jobs.size()>=64)
        return fail(error,"Character offered UI target/token/field count differs or token already attempted; no replay");
    for(const auto& key:expected)if(!args.contains(key))return fail(error,"Character offered UI missing field="+key);
    if(needs_slot&&(!args.at("slot").is_number_integer()||args.at("slot").get<int64_t>()<0||args.at("slot").get<int64_t>()>=10000))
        return fail(error,"Ability offered UI requires an actual canonical slot");
    for(const auto& key:{"control","page","value"})if(expected.contains(key)){
        if(!args.at(key).is_string())return fail(error,std::string("Ability offered UI string type differs field=")+key);
        const auto value=args.at(key).get<std::string>();
        if(value.size()>(std::string_view(key)=="value"?4096u:512u)||value.find('\0')!=std::string::npos||!npc_census::utf8(value)||
           (std::string_view(key)!="value"&&value.empty()))return fail(error,std::string("Ability offered UI text is invalid field=")+key);
    }
    if(expected.contains("expected_enabled")&&!args.at("expected_enabled").is_boolean())return fail(error,"Character offered UI expected_enabled must be Boolean");
    if(expected.contains("commit")&&!args.at("commit").is_boolean())return fail(error,"Ability original text commit flag must be Boolean");
    if(toggle){
        if(!args.contains("category")||!args.contains("expected_enabled")||!args.at("category").is_string()||!args.at("expected_enabled").is_boolean())
            return fail(error,"Costume toggle requires actual category and Boolean expected_enabled");
        const auto category=args.at("category").get<std::string>();
        if(category.empty()||category.size()>128||!npc_census::utf8(category)||category.find_first_of("\0\r\n",0,3)!=std::string::npos)
            return fail(error,"Costume offered category text is invalid");
    }
    const auto controller=typed_action_runtime::controller_status();
    if(!Config::instance().local_test||!ready||fault||GetCurrentThreadId()!=state.thread||!net::session_wire::connected()||!ordered_mode()||
       pending_ui()||costume_edit::pending_ui()||ability_edit::pending_ui()||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))
        return fail(error,"Character offered UI requires idle owned ordered Mainthread");
    UiJob job;job.arguments=args;job.executor=&execute_edit_ui;
    if(!a::selected_session().current(job.context,error)||job.context.snapshot_uuid!=net::current_snapshot_id()||
       Config::instance().is_host!=(job.context.role==a::Role::Host)||!typed_action_runtime::network_binding(job.context.snapshot_uuid,job.binding,error))return false;
    ui_jobs.emplace(token,std::move(job));queued_ui=token;result={{"token",token},{"queued",true},{"input_attempted",false}};return true;
}catch(const std::exception& exception){return fail(error,std::string("Character offered UI boundary: ")+exception.what());}
catch(...){return fail(error,"Character offered UI non-standard boundary exception");}
}
