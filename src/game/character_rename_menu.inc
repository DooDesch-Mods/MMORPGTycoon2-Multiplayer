struct DesignMenu {uintptr_t system=0,design=0,collection=0,button=0,menu=0;bool shown=false,collection_shown=false;
    int32_t item_state=-1;uint8_t enabled=0,clickable=0;};
json design_state(const DesignMenu& menu){return {{"design_shown",menu.shown},{"collection_shown",menu.collection_shown},
    {"item_state",menu.item_state},{"button_enabled",menu.enabled},{"button_clickable",menu.clickable}};}
bool design_menu(Memory& memory,DesignMenu& out,std::string& error){
    out={};uintptr_t data=0;int32_t count=0,capacity=0;
    if(!field(memory,state.image,0x1a3c478,data,error)||!field(memory,state.image,0x1a3c480,count,error)||
       !field(memory,state.image,0x1a3c484,capacity,error)||count<0||count>1024||capacity<count||capacity>65536||(count&&!data))
        return fail(error,error.empty()?"Character Design window registry differs":error);
    for(int32_t i=0;i<count;++i){uintptr_t value=0;std::string name;
        if(!field(memory,data,size_t(i)*8,value,error)||!npc_quest_records::read_string(memory,value,0x1b0,"Character Design window",name,error))return false;
        uintptr_t* target=name=="system_buttons"?&out.system:name=="designmenu"?&out.design:name=="collection_classes"?&out.collection:nullptr;
        if(target){if(*target)return fail(error,"Character Design window name is duplicated");*target=value;}
    }
    uint8_t shown=0;uintptr_t ap=0;
    if(!out.system||!out.design||!out.collection||!field(memory,out.system,0x45f,shown,error)||shown!=1||
       !field(memory,out.design,0,ap,error)||ap!=state.image+0x16e6400||!field(memory,out.design,0x45f,shown,error)||shown>1)
        return fail(error,error.empty()?"Character original Design windows are absent or hidden":error);
    out.shown=shown!=0;if(!field(memory,out.collection,0x45f,shown,error)||shown>1)return false;out.collection_shown=shown!=0;
    std::vector<uintptr_t> panes{out.system};std::set<uintptr_t> seen;
    while(!panes.empty()){const auto pane=panes.back();panes.pop_back();uintptr_t gather=0;
        if(!seen.insert(pane).second||seen.size()>2048||!field(memory,pane,0,ap,error)||!field(memory,ap,0x178,gather,error))
            return fail(error,error.empty()?"Character Design pane cycle or native class differs":error);
        if(ap==state.image+0x16db1c0){std::string name;
            if(!npc_quest_records::read_string(memory,pane,0x318,"Character Design ItemSlot",name,error))return false;
            if(name=="@DesignMenu"){uintptr_t item=0,proxy=0,target=0,owned=0,parent=0,slot=0,slot_item=0;
                if(out.button||!field(memory,pane,0x368,out.button,error)||!out.button||!field(memory,out.button,0,ap,error)||ap!=state.image+0x16e5b90||
                   !field(memory,out.button,0x2d0,item,error)||!field(memory,out.button,0x2d8,proxy,error)||!proxy||
                   !field(memory,pane,0x360,slot_item,error)||slot_item!=item||!field(memory,item,0,ap,error)||ap!=state.image+0x17437f0||
                   !field(memory,proxy,0,target,error)||target!=item+0x20||!field(memory,item,0x28,owned,error)||owned!=proxy||
                   !field(memory,out.button,0x278,parent,error)||parent!=pane||!field(memory,out.button,0x350,slot,error)||slot!=pane||
                   !field(memory,out.button,0x381,out.enabled,error)||!field(memory,out.button,0x174,out.clickable,error)||
                   !field(memory,item,0x38,out.item_state,error))return fail(error,error.empty()?"Character actual Design ItemButton ownership/read differs":error);
                // The window item is ready at 1; its original Activate opens the window and sets it to 0.
                if(out.enabled!=1||out.clickable!=1||out.item_state!=(out.shown?0:1)||!offered_control(memory,out.button,out.system,error))
                    return fail(error,"Character actual Design ItemButton offer differs state="+std::to_string(out.item_state)+
                        " expected="+std::to_string(out.shown?0:1)+" shown="+std::to_string(out.shown)+
                        " enabled="+std::to_string(out.enabled)+" clickable="+std::to_string(out.clickable)+" cause="+error);
            }
        }
        if(gather==state.image+0xc35730)continue;
        if(gather!=state.image+0x8167c0)return fail(error,"Character Design GatherAllContents slot differs");
        for(const auto offset:{size_t(0x2c8),size_t(0x2f8)}){std::vector<uintptr_t> children;
            if(!open_array(memory,pane,offset,children,error))return false;
            for(auto child:children){uintptr_t parent=0,owner=0;if(!field(memory,child,0x278,parent,error)||parent!=pane||
               !field(memory,child,0x280,owner,error)||owner!=out.system)return fail(error,"Character Design child owner differs");panes.push_back(child);}}
    }
    return out.button!=0||fail(error,"Character actual Design ItemButton is absent");
}
bool classes_menu_offered(Memory& memory,DesignMenu& menu,std::string& error){
    uintptr_t callback=0,ap=0;std::vector<uintptr_t> items,rows;
    if(!menu.shown||!field(memory,menu.design,0x540,menu.menu,error)||!field(memory,menu.menu,0,ap,error)||ap!=state.image+0x1743a60||
       !field(memory,menu.menu,0x3b8,callback,error)||callback!=menu.design+0x4e8||!offered_control(memory,menu.menu,menu.design,error)||
       !open_array(memory,menu.design,0x510,items,error)||!open_array(memory,menu.menu,0x368,rows,error))
        return fail(error,error.empty()?"Character original Design Menu callback/rows differ":error);
    unsigned definitions=0,offered=0;
    for(auto item:items){std::string key,command;if(!npc_quest_records::read_string(memory,item,0x18,"Character Design item key",key,error))return false;
        if(key!="pcs")continue;if(!npc_quest_records::read_string(memory,item,0x38,"Character Design item command",command,error))return false;
        if(command!="MessageTo Window HideAndOpenWindow \"collection_classes\"")return fail(error,"Character original classes command differs");++definitions;}
    for(auto row:rows){std::string key;if(!npc_quest_records::read_string(memory,row,0x1b0,"Character Design offered row",key,error))return false;
        if(key=="pcs"){if(!offered_control(memory,row,menu.design,error))return false;++offered;}}
    uintptr_t locked=0;int32_t count=0,capacity=0;
    if(!field(memory,menu.menu,0x3a0,locked,error)||!field(memory,menu.menu,0x3a8,count,error)||!field(memory,menu.menu,0x3ac,capacity,error)||
       count<0||count>256||capacity<count||(count&&!locked))return fail(error,error.empty()?"Character Design locked row bounds differ":error);
    for(int32_t i=0;i<count;++i){std::string key;if(!npc_quest_records::read_string(memory,locked,size_t(i)*32,"Character Design locked key",key,error))return false;
        if(key=="pcs")return fail(error,"Character original classes menu row is locked");}
    return (definitions==1&&offered==1)||fail(error,"Character original classes menu row is absent or ambiguous");
}
uint32_t activate_design(uintptr_t button)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(state.image+0x580870)(reinterpret_cast<void*>(button));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t select_classes(uintptr_t window)noexcept{
    struct Text {const char* data;uint64_t size;unsigned char rest[16];} key{"pcs",3,{}};
    __try{reinterpret_cast<void(*)(void*,const Text*)>(state.image+0x73d8a0)(reinterpret_cast<void*>(window),&key);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool execute_menu(UiJob& job,const Sample& before,const a::CharacterRenameIntent& intent,std::string& error){
    Memory memory;DesignMenu menu;Sample after;
    if(!design_menu(memory,menu,error)||menu.collection_shown)return fail(error,error.empty()?"Character full opening proof requires the original class collection initially hidden":error);
    job.result["design_before_activation"]=design_state(menu);
    if(!menu.shown){job.attempted=true;const auto seh=activate_design(menu.button);
        const bool offered=design_menu(memory,menu,error);job.result["design_after_activation"]=design_state(menu);
        job.result["design_after_activation"]["offer_verified"]=offered;
        job.result["design_after_activation"]["exception"]=seh;
        if(seh||!offered||!menu.shown)return fail(error,"Character original Design ItemButton Activate did not show its offered menu SEH="+std::to_string(seh)+" cause="+error);}
    if(!classes_menu_offered(memory,menu,error))return false;
    job.attempted=true;const auto seh=select_classes(menu.design);
    if(seh||fault||!sample(memory,job.binding,job.context.snapshot_uuid,intent,after,error)||!same_local(before,after)||before.value!=after.value)
        return fail(error,"Character original Design/classes opening changed complete gameplay/ArtStore/RNG proof SEH="+std::to_string(seh)+" cause="+error);
    job.result["original_design_classes_open"]=true;job.result["after_collection"]=diagnostic_sample(after);return true;
}
