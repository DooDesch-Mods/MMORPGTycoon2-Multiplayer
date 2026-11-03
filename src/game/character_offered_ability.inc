uint32_t edit_activate(uintptr_t pane,bool icon=false)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(state.image+(icon?0x564890:0x674780))(reinterpret_cast<void*>(pane));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t edit_text(uintptr_t outer,uintptr_t pane,const char* text,size_t size,bool commit)noexcept{
    __try{reinterpret_cast<void(*)(void*,void*)>(state.image+0x81ada0)(reinterpret_cast<void*>(outer),reinterpret_cast<void*>(pane));
        const quest_list::QuestTitleView value{text,size};
        reinterpret_cast<void(*)(void*,const quest_list::QuestTitleView&)>(state.image+0x7f1630)(reinterpret_cast<void*>(pane),value);
        if(commit)reinterpret_cast<void(*)(void*)>(state.image+0x771f50)(reinterpret_cast<void*>(pane));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t edit_choice(uintptr_t pane,const char* text,size_t size)noexcept{
    __try{const quest_list::QuestTitleView value{text,size};
        reinterpret_cast<void(*)(void*,const quest_list::QuestTitleView&,bool)>(state.image+0x6ab830)(reinterpret_cast<void*>(pane),value,true);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool ability_offered(Memory& memory,const json& args,uintptr_t& outer,json& details,std::vector<OfferedControl>& controls,std::string& error){
    uintptr_t live=0,draft=0,owner=0,focus=0;uint8_t shown=0,dirty=0,tooltip_dirty=0;
    if(!ability_edit::native_apply_state(details,error)||details.at("kind")!=args.at("kind")||details.at("id")!=args.at("id")||details.at("slot")!=args.at("slot")||
       !ability_edit::window(memory,outer,live,draft,owner,error)||!field(memory,outer,0x45f,shown,error)||shown!=1)
        return fail(error,error.empty()?"Ability actual offered editor definition/slot differs":error);
    if(!field(memory,outer,0x3c0,focus,error)||!field(memory,outer,0x606,dirty,error)||dirty>1||!field(memory,outer,0x607,tooltip_dirty,error)||tooltip_dirty>1)
        return fail(error,error.empty()?"Ability local editor dirty flags are not Boolean":error);
    details["local_editor"]={{"window",outer},{"live",live},{"draft",draft},{"owner",owner},{"focus",focus},{"dirty",dirty},{"tooltip_dirty",tooltip_dirty}};
    return offered_controls(memory,outer,outer,controls,error);
}
struct OfferedAbility {int32_t slot=-1,level=0;uint64_t uid=0;std::string name;bool positive_effects=false;OfferedControl icon;};
bool ability_icons(Memory& memory,uintptr_t root,uintptr_t named,uintptr_t owner,std::vector<OfferedAbility>& out,std::string& error,json* census=nullptr){
    uintptr_t data=0;int32_t count=0,capacity=0;std::vector<OfferedControl> controls;OfferedControl tab;OfferedPage page;out.clear();
    if(!field(memory,owner,0x130,data,error)||!field(memory,owner,0x138,count,error)||!field(memory,owner,0x13c,capacity,error)||
       count<0||count>10000||capacity<count||(count&&!data)||!offered_controls(memory,root,named,controls,error)||
       !offered_tab(controls,"multi",tab,error)||!offered_page(memory,tab,"{design_character_tab_abilities}",page,error)||
       page.current!=page.page||page.requested!=page.page)
        return fail(error,error.empty()?"Ability catalogue requires its actual current enabled Abilities page and canonical owner array":error);
    uintptr_t list_class=0,slots_view=0,slots_class=0,list_owner=0,slots_owner=0;uint8_t rebuild=0;
    if(!field(memory,page.page,0,list_class,error)||list_class!=state.image+0x1717ed0||
       !field(memory,page.page,0x320,slots_view,error)||!slots_view||!field(memory,page.page,0x328,list_owner,error)||list_owner!=owner||
       !field(memory,slots_view,0,slots_class,error)||slots_class!=state.image+0x171ca70||
       !field(memory,slots_view,0x340,slots_owner,error)||slots_owner!=owner||!field(memory,slots_view,0x378,rebuild,error)||rebuild>1)
        return fail(error,error.empty()?"Ability native list/slots owner or rebuild flag differs":error);
    if(census)*census={{"catalogue_ready",rebuild==0},{"rebuild_pending",rebuild!=0},{"canonical_ability_count",count},
        {"icon_count",0},{"visible_icon_count",0},{"disabled_icon_count",0},{"eligible_icon_count",0}};
    if(rebuild){if(census)return true;return fail(error,"Ability native slot rebuild is pending");}
    bool visible=false;uintptr_t parent=0,window_owner=0;
    if(!field(memory,page.page,0x278,parent,error)||parent!=page.pages||!field(memory,page.page,0x280,window_owner,error)||window_owner!=named||
       !offered_visibility(memory,page.page,page.page,named,visible,error)||!visible||!offered_controls(memory,page.page,named,controls,error))
        return fail(error,error.empty()?"Ability actual offered page owner/ancestry differs":error);
    std::map<uintptr_t,int32_t> slots;
    for(int32_t slot=0;slot<count;++slot){uintptr_t target=0;
        if(!field(memory,data,size_t(slot)*8,target,error)||!target||!slots.emplace(target,slot).second)
            return fail(error,error.empty()?"Ability canonical owner slots are null or duplicate":error);}
    std::set<int32_t> offered;
    for(const auto& control:controls){if(!control.icon)continue;
        if(census){bool shown=false;uint8_t enabled=0;
            if(!offered_visibility(memory,control.pane,page.page,named,shown,error)||!field(memory,control.pane,0x395,enabled,error)||enabled>1)return false;
            (*census)["icon_count"]=census->at("icon_count").get<size_t>()+1;
            if(shown)(*census)["visible_icon_count"]=census->at("visible_icon_count").get<size_t>()+1;
            if(!enabled)(*census)["disabled_icon_count"]=census->at("disabled_icon_count").get<size_t>()+1;
        }
        if(!control.ready)continue;
        uintptr_t actual=0,actual_owner=0,proxy=0,weak_target=0;int32_t refs=0;
        if(!field(memory,control.pane,0x320,actual,error)||!field(memory,control.pane,0x330,actual_owner,error))return false;
        if(actual_owner!=owner)continue;
        const auto slot=slots.find(actual);
        if(slot==slots.end()||!offered.insert(slot->second).second||!field(memory,control.pane,0x328,proxy,error)||!proxy||
           !field(memory,proxy,0,weak_target,error)||weak_target!=actual+0x38||
           !field(memory,proxy,8,refs,error)||refs<=0)return fail(error,error.empty()?"Ability original icon weak ownership is invalid or ambiguous":error);
        OfferedAbility value;value.slot=slot->second;value.icon=control;a::AbilityRecord record;
        if(!field(memory,actual,0x18,value.uid,error)||!value.uid||!ability_edit::read_record(memory,actual,record,error))
            return fail(error,error.empty()?"Ability offered canonical UID/full record is absent":error);
        value.name=record.text[0];value.level=record.level;value.positive_effects=!record.effects.empty();out.push_back(std::move(value));
    }
    uintptr_t again=0;int32_t size=0,cap=0;
    if(!field(memory,owner,0x130,again,error)||again!=data||!field(memory,owner,0x138,size,error)||size!=count||
       !field(memory,owner,0x13c,cap,error)||cap!=capacity)return fail(error,error.empty()?"Ability canonical owner slot array changed during offered census":error);
    if(census)(*census)["eligible_icon_count"]=out.size();
    std::sort(out.begin(),out.end(),[](const OfferedAbility& left,const OfferedAbility& right){return left.slot<right.slot;});return true;
}
bool ability_catalogue(Memory& memory,uintptr_t root,uintptr_t named,uintptr_t owner,json& result,std::string& error,json* census=nullptr){
    std::vector<OfferedAbility> offers;if(!ability_icons(memory,root,named,owner,offers,error,census))return false;result=json::array();
    for(const auto& value:offers)result.push_back({{"slot",value.slot},{"uid",value.uid},{"name",value.name},{"level",value.level},{"positive_effects",value.positive_effects}});
    return true;
}
bool ability_icon(Memory& memory,uintptr_t root,uintptr_t named,uintptr_t owner,int32_t slot,OfferedControl& out,std::string& error){
    out={};std::vector<OfferedAbility> offers;if(!ability_icons(memory,root,named,owner,offers,error))return false;
    for(const auto& value:offers)if(value.slot==slot){out=value.icon;return true;}
    return fail(error,"Ability requested canonical slot has no shown eligible original edit icon");
}
bool ability_controls(Memory& memory,const std::vector<OfferedControl>& controls,json& rows,std::string& error){
    rows=json::array();for(const auto& control:controls){
        if(!control.ready||control.tab||control.icon)continue;
        json row={{"control",offered_key(control)},{"id",control.id},{"command",control.command},
            {"type",control.text?"text":control.combo?"choice":control.checkbox?"checkbox":"button"}};
        if(control.text){std::string text,input,baseline;std::array<uint32_t,8> selection{};uint8_t editing=0;
            if(!npc_quest_records::read_string(memory,control.pane,0x2d0,"Ability actual text",text,error)||
               !npc_quest_records::read_string(memory,control.pane,0x4a8,"Ability original input buffer",input,error)||
               !npc_quest_records::read_string(memory,control.pane,0x4d8,"Ability original text baseline",baseline,error)||
               !field(memory,control.pane,0x544,selection,error)||!field(memory,control.pane,0x584,editing,error)||editing>1)
                return fail(error,error.empty()?"Ability original text editing flag is not Boolean":error);
            row.update({{"value",text},{"input_buffer",input},{"commit_baseline",baseline},{"selection",selection},{"editing",editing}});}
        else if(control.combo){std::vector<std::string> choices;int32_t selected=-1;if(!offered_choices(memory,control,choices,selected,error))return false;
            row["choices"]=choices;row["selected"]=selected;row["value"]=selected>=0?json(choices[size_t(selected)]):json(nullptr);}
        else if(control.checkbox)row["enabled"]=control.checked;
        rows.push_back(std::move(row));
    }
    return true;
}
bool execute_ability_ui(UiJob& job,const std::string& token,std::string& error){
    Memory memory;const auto operation=job.arguments.at("operation").get<std::string>();job.waiting=false;
    if(operation=="ShowAbility"){
        if(job.stage==0){if(!edit_open(job,error))return false;job.stage=1;job.waits=0;job.waiting=true;return false;}
        if(job.stage<=2){uintptr_t view=0,named=0,owner=0;bool current=false;
            if(!edit_target(memory,job.arguments,view,named,owner,error)||
               !edit_page(job,view,named,"multi","{design_character_tab_abilities}",job.stage==1,current,error))return false;
            job.stage=2;if(!current)return false;
            if(!ability_catalogue(memory,view,named,owner,job.result["ability_slots"],error,&job.result["ability_catalogue"]))return false;
            if(!job.result.at("ability_catalogue").at("catalogue_ready").get<bool>()){
                job.waiting=true;
                if(++job.waits>8)return fail(error,"Ability native slot rebuild remained pending after 8 normal UI updates; rebuild_pending=true");
                return false;
            }
            job.result["kind"]=job.arguments.at("kind");job.result["id"]=job.arguments.at("id");
            if(!job.arguments.contains("slot")){job.result["catalogue_only"]=true;job.stage=4;return true;}
            OfferedControl icon;if(!ability_icon(memory,view,named,owner,job.arguments.at("slot").get<int32_t>(),icon,error))return false;
            a::CharacterRenameIntent intent;intent.kind=job.arguments.at("kind").get<int32_t>();intent.id=job.arguments.at("id").get<int32_t>();Sample before,after;
            if(!sample(memory,job.binding,job.context.snapshot_uuid,intent,before,error))return false;
            job.attempted=true;const auto seh=edit_activate(icon.pane,true);
            if(seh||!sample(memory,job.binding,job.context.snapshot_uuid,intent,after,error)||!same_local(before,after)||before.value!=after.value)
                return fail(error,error.empty()?"Ability original icon open changed complete gameplay/ArtStore/RNG proof SEH="+std::to_string(seh):error);
            job.result["original_ability_icon_activate"]=true;job.stage=3;job.waits=0;job.waiting=true;return false;
        }
    }
    uintptr_t outer=0;json details;std::vector<OfferedControl> controls;
    if(!ability_offered(memory,job.arguments,outer,details,controls,error))return false;
    if(operation=="ShowAbilityPage"){
        bool current=false;if(!edit_page(job,outer,outer,"multi",job.arguments.at("page").get<std::string>(),job.stage==0,current,error))return false;
        job.stage=1;if(!current)return false;
        if(!ability_offered(memory,job.arguments,outer,details,controls,error))return false;
    }
    job.result["ability"]=details;
    if(!ability_controls(memory,controls,job.result["controls"],error))return false;
    if(operation=="ShowAbility"||operation=="AbilityState"||operation=="ShowAbilityPage")return true;
    OfferedControl control;
    if(operation=="ApplyAbility"){
        if(!offered_unique(controls,"MessageTo Parent Apply",false,control,error))return false;
    }else if(!offered_named(controls,job.arguments.at("control").get<std::string>(),control,error))return false;
    if((operation=="EditAbilityText"&&!control.text)||(operation=="EditAbilityChoice"&&!control.combo)||
       (operation=="ToggleAbilityControl"&&!control.checkbox)||(operation=="ActivateAbilityControl"&&(control.text||control.combo||control.checkbox||control.tab||control.icon)))
        return fail(error,"Ability requested input does not match the original offered control type");
    if(operation=="ActivateAbilityControl"&&(control.command=="MessageTo Parent Apply"||control.command=="MessageTo Parent SavePrefab"))
        return fail(error,"Ability Apply requires its recorded operation; prefab saving is outside this edit input");
    a::CharacterRenameIntent intent;intent.kind=job.arguments.at("kind").get<int32_t>();intent.id=job.arguments.at("id").get<int32_t>();Sample before,after;
    if(!sample(memory,job.binding,job.context.snapshot_uuid,intent,before,error))return false;
    const auto previous=ability_edit::captures;uint32_t seh=0;
    if(operation=="EditAbilityText"){
        const auto value=job.arguments.at("value").get<std::string>();const bool commit=job.arguments.value("commit",true);
        int32_t maximum=0;
        if(!field(memory,control.pane,0x440,maximum,error)||maximum< -1||maximum>1000000||
           (maximum>0&&std::count_if(value.begin(),value.end(),[](unsigned char c){return (c&0xc0)!=0x80;})>maximum))
            return fail(error,error.empty()?"Ability text exceeds its original offered character limit":error);
        job.attempted=true;seh=edit_text(outer,control.pane,value.c_str(),value.size(),commit);
        std::string actual;uintptr_t focus=0;uint8_t editing=1;
        if(seh||!npc_quest_records::read_string(memory,control.pane,0x2d0,"Ability committed text",actual,error)||actual!=value||
           !field(memory,outer,0x3c0,focus,error)||focus!=(commit?uintptr_t(0):control.pane)||
           !field(memory,control.pane,0x584,editing,error)||editing!=uint8_t(!commit))
            return fail(error,error.empty()?"Ability original text commit/readback differs SEH="+std::to_string(seh):error);
        job.result["text_committed"]=commit;
    }else if(operation=="EditAbilityChoice"){
        std::vector<std::string> choices;int32_t selected=-1;const auto value=job.arguments.at("value").get<std::string>();
        if(!offered_choices(memory,control,choices,selected,error)||std::find(choices.begin(),choices.end(),value)==choices.end())return fail(error,error.empty()?"Ability value is not in its actual offered choice catalogue":error);
        job.attempted=true;seh=edit_choice(control.pane,value.c_str(),value.size());
        if(seh||!offered_choices(memory,control,choices,selected,error)||selected<0||choices[size_t(selected)]!=value)
            return fail(error,error.empty()?"Ability original choice selection/readback differs SEH="+std::to_string(seh):error);
    }else if(operation=="ToggleAbilityControl"){
        const bool expected=job.arguments.at("expected_enabled").get<bool>();uint8_t checked=0,held=1;
        if(control.checked!=expected)return fail(error,"Ability actual checkbox state differs from expected_enabled");
        job.attempted=true;seh=edit_checkbox(control.pane);
        if(seh||!field(memory,control.pane,0x346,checked,error)||checked!=uint8_t(!expected)||!field(memory,control.pane,0x344,held,error)||held)
            return fail(error,error.empty()?"Ability original checkbox/readback differs SEH="+std::to_string(seh):error);
    }else{
        job.attempted=true;if(operation=="ApplyAbility")active_ui_token=token;
        seh=edit_activate(control.pane);active_ui_token.clear();
        if(seh)return fail(error,"Ability original offered button SEH="+std::to_string(seh));
    }
    if(ability_edit::fault||!sample(memory,job.binding,job.context.snapshot_uuid,intent,after,error)||!same_local(before,after)||before.value!=after.value)
        return fail(error,error.empty()?"Ability original input changed complete gameplay/ArtStore/RNG before ordered consumption":error);
    if(operation=="ApplyAbility"&&details.at("positive_effects").get<bool>()){
        const auto& saved=ability_edit::pending;
        if(!saved||saved->diagnostic_token!=token||saved->view!=outer||saved->intent.kind!=intent.kind||saved->intent.id!=intent.id||
           saved->intent.slot!=job.arguments.at("slot")||saved->intent.adjustment||ability_edit::captures!=previous+1)
            return fail(error,"Ability original Apply did not capture one exact ordered complete-draft intent");
        job.result["captured"]=true;
    }else{
        if(ability_edit::pending||ability_edit::captures!=previous)return fail(error,"Ability local draft input unexpectedly captured an ordered action");
        job.result["captured"]=false;if(operation=="ApplyAbility")job.result["original_invalid_draft_validation"]=true;
    }
    job.result["original_offered_input_returned"]=true;
    if(operation!="ApplyAbility"){
        if(!ability_offered(memory,job.arguments,outer,details,controls,error)||!ability_controls(memory,controls,job.result["controls"],error))return false;
        job.result["ability"]=std::move(details);
    }
    return true;
}
