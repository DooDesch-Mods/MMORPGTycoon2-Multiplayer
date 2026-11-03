struct OrderedView {
    uintptr_t view=0,editor=0,outer=0,selected=0,button=0,pages=0;
    uint8_t disabled=0,clickable=0,editing=0,selectable=0;uintptr_t focus=0;
    std::array<uint32_t,2> numeric_cache{};std::array<uint32_t,8> selection{};
    std::string text,input_cache,commit_baseline;bool origin=false,armed=false,restore_attempted=false,restored=false;
    json evidence=json::object();
};
struct RenameTail {
    bool armed=false;uintptr_t view=0,target=0;uint32_t calls=0;
    const a::Binding* binding=nullptr;const a::Request* request=nullptr;
};
RenameTail rename_tail;
bool preparing_view=false;uintptr_t preparing_receiver=0,preparing_editor=0;uint32_t preparation_notifications=0;
uint32_t write_view_target(uintptr_t view,uintptr_t selected)noexcept{
    __try{std::memcpy(reinterpret_cast<void*>(view+0x460),&selected,sizeof selected);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t write_view_numeric(uintptr_t editor,const std::array<uint32_t,2>& value)noexcept{
    __try{std::memcpy(reinterpret_cast<void*>(editor+0x4d0),value.data(),sizeof value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t restore_view_selection(uintptr_t editor,const std::array<uint32_t,8>& value)noexcept{
    __try{std::memcpy(reinterpret_cast<void*>(editor+0x544),value.data(),sizeof value);
        reinterpret_cast<void(*)(void*)>(state.image+0x7f0970)(reinterpret_cast<void*>(editor));
        reinterpret_cast<void(*)(void*)>(state.image+0x7ee710)(reinterpret_cast<void*>(editor));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t drop_view_focus(uintptr_t editor)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(state.image+0x771f50)(reinterpret_cast<void*>(editor));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t set_view_text(uintptr_t editor,const char* text,size_t length)noexcept{
    __try{const quest_list::QuestTitleView value{text,length};
        reinterpret_cast<void(*)(void*,const quest_list::QuestTitleView&)>(state.image+0x7f1630)(reinterpret_cast<void*>(editor),value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t set_view_disabled(uintptr_t button,bool disabled)noexcept{
    __try{reinterpret_cast<void(*)(void*,bool)>(state.image+0x670e00)(reinterpret_cast<void*>(button),disabled);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t dirty_view_title(uintptr_t view)noexcept{
    __try{*reinterpret_cast<uint8_t*>(view+0x484)=1;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool view_owner(Memory& memory,uintptr_t pane,uintptr_t view,uintptr_t outer,std::string& error){
    std::set<uintptr_t> seen;
    while(pane!=view){uintptr_t owner=0,parent=0;
        if(!pane||!seen.insert(pane).second||seen.size()>64||!field(memory,pane,0x280,owner,error)||owner!=outer||
           !field(memory,pane,0x278,parent,error))return fail(error,error.empty()?"Character rename current control ancestry differs":error);
        pane=parent;
    }
    return true;
}
bool editor_page(Memory& memory,uintptr_t view,OrderedView& out,std::string& error){
    uintptr_t ap=0,data=0,parent=0,owner=0;int32_t count=0,capacity=0;std::string name;
    if(!field(memory,view,0x358,out.pages,error)||!out.pages||!field(memory,out.pages,0,ap,error)||ap!=state.image+0x16dfe90||
       !npc_quest_records::read_string(memory,out.pages,0x1b0,"Character name MultiView",name,error)||name!="general_name_multiview"||
       !field(memory,out.pages,0x388,data,error)||!field(memory,out.pages,0x390,count,error)||!field(memory,out.pages,0x394,capacity,error)||
       count<1||count>32||capacity<count||capacity>256||!data)return fail(error,error.empty()?"Character cached name MultiView catalogue differs":error);
    if(!field(memory,out.pages,0x278,parent,error)||!field(memory,out.pages,0x280,owner,error))return false;
    if((parent||owner)&&(!parent||owner!=out.outer||!view_owner(memory,out.pages,view,out.outer,error)))
        return fail(error,error.empty()?"Character cached name MultiView has foreign live ancestry":error);
    unsigned matches=0;for(int32_t i=0;i<count;++i){uintptr_t page=0;
        if(!field(memory,data,size_t(i)*8,page,error)||!page)return fail(error,error.empty()?"Character cached name page is absent":error);
        if(page==out.editor)++matches;
    }
    if(matches!=1||!npc_quest_records::read_string(memory,out.editor,0x1b0,"Character cached name editor",name,error)||name!="general_edit_name"||
       !field(memory,out.editor,0x278,parent,error)||!field(memory,out.editor,0x280,owner,error))
        return fail(error,error.empty()?"Character editor is not its unique cached name page":error);
    // Inactive pages are owned by the MultiView catalogue before native Layout attaches them.
    if((parent&&parent!=out.pages)||(owner&&owner!=out.outer)||bool(parent)!=bool(owner))
        return fail(error,"Character cached editor has foreign live ancestry parent="+std::to_string(parent==out.pages)+
            " owner="+std::to_string(owner==out.outer)+" detached="+std::to_string(!parent&&!owner));
    if(parent&&!view_owner(memory,out.pages,view,out.outer,error))return false;
    return true;
}
bool read_ordered_view(Memory& memory,uintptr_t view,OrderedView& out,std::string& error){
    out={};uintptr_t actual=0,ap=0,bound=0;uint8_t held=1,pressed=1;std::string command;
    if(!window(memory,0,actual,out.editor,out.outer,error)||actual!=view||!ability_edit::presentation_route(error)||
       !field(memory,view,0x460,out.selected,error)||!field(memory,view,0x370,out.button,error))
        return fail(error,error.empty()?"Character Rename named current Window/View differs":error);
    if(!field(memory,out.editor,0,ap,error)||ap!=state.image+0x1703290)
        return fail(error,error.empty()?"Character Rename cached TextEditPane address point differs RVA="+std::to_string(ap-state.image):error);
    if(!npc_quest_records::read_string(memory,out.editor,0x3e0,"Character rename committed editor command",command,error)||command!="MessageTo Parent DoRename")
        return fail(error,error.empty()?"Character Rename cached editor command differs: "+command:error);
    if(!npc_quest_records::read_string(memory,out.editor,0x400,"Character rename preview command",command,error)||!command.empty())
        return fail(error,error.empty()?"Character Rename editor preview command must be empty: "+command:error);
    if(!editor_page(memory,view,out,error)||
       !npc_quest_records::read_string(memory,out.editor,0x2d0,"Character rename saved draft",out.text,error)||
       !npc_quest_records::read_string(memory,out.editor,0x4a8,"Character rename input cache",out.input_cache,error)||
       !npc_quest_records::read_string(memory,out.editor,0x4d8,"Character rename commit baseline",out.commit_baseline,error)||
       !field(memory,out.editor,0x4a0,bound,error)||bound||!field(memory,out.editor,0x584,out.editing,error)||out.editing>1||
       !field(memory,out.editor,0x445,out.selectable,error)||out.selectable>1||!field(memory,out.outer,0x3c0,out.focus,error)||
       bool(out.editing)!=(out.focus==out.editor)||!field(memory,out.editor,0x4d0,out.numeric_cache,error)||
       !field(memory,out.editor,0x544,out.selection,error))return fail(error,error.empty()?"Character Rename editor binding/cache/focus state differs":error);
    if(out.button&&(!field(memory,out.button,0,ap,error)||ap!=state.image+0x16e3ec0||
       !view_owner(memory,out.button,view,out.outer,error)||!field(memory,out.button,0x578,bound,error)||bound||
       !field(memory,out.button,0x584,held,error)||held||!field(memory,out.button,0x585,pressed,error)||pressed||
       !field(memory,out.button,0x46d,out.disabled,error)||out.disabled>1||!field(memory,out.button,0x174,out.clickable,error)||out.clickable!=1))
        return fail(error,error.empty()?"Character rename button has a live value/edit binding or different ownership":error);
    out.view=view;return true;
}
bool prepare_view_text(const OrderedView& view,const std::string& text,std::string& error){
    if(preparing_view)return fail(error,"Character rename UI preparation scope is already active");
    const auto previous_rejections=rejected;
    preparing_view=true;preparing_receiver=view.view;preparing_editor=view.editor;
    const auto seh=set_view_text(view.editor,text.c_str(),text.size());preparing_view=false;preparing_receiver=preparing_editor=0;
    if(seh)return fail(error,"Character rename original text preparation SEH="+std::to_string(seh));
    return previous_rejections==rejected||fail(error,"Character rename text preparation notification failed: "+cause);
}
bool same_editor_input(const OrderedView& lhs,const OrderedView& rhs){return lhs.input_cache==rhs.input_cache&&
    lhs.commit_baseline==rhs.commit_baseline&&lhs.editing==rhs.editing&&lhs.focus==rhs.focus&&lhs.selectable==rhs.selectable;}
json ordered_view_state(const OrderedView& view){return {{"view",view.view},{"editor",view.editor},{"window",view.outer},
    {"selected",view.selected},{"button",view.button},{"pages",view.pages},{"text",view.text},{"input_cache",view.input_cache},
    {"commit_baseline",view.commit_baseline},{"editing",view.editing},{"focus",view.focus},{"selectable",view.selectable},
    {"numeric_cache",view.numeric_cache},{"selection",view.selection},{"disabled",view.disabled},{"clickable",view.clickable}};}
bool prepare_ordered_view(Memory& memory,uintptr_t view,uintptr_t target,const a::Request& request,OrderedView& saved,std::string& error){
    if(!read_ordered_view(memory,view,saved,error))return false;
    a::Context context;if(!a::selected_session().current(context,error))return false;saved.origin=request.peer==context.local_peer;
    saved.evidence={{"origin",saved.origin},{"target",target},{"kind",request.character_rename.kind},{"id",request.character_rename.id},
        {"recorded_before",request.character_rename.before},{"recorded_name",request.character_rename.name},
        {"current_target_matches",saved.selected==target},{"current_draft_matches",saved.text==request.character_rename.name},
        {"before",ordered_view_state(saved)}};
    // Unfocused native UpdateContents restores model text while the captured command awaits consumption.
    // The consumed request owns the target and name; each peer's current local UI is saved independently.
    saved.armed=true;const auto seh=write_view_target(view,target);
    if(seh)return fail(error,"Character rename scoped UI target preparation SEH="+std::to_string(seh));
    if(!prepare_view_text(saved,request.character_rename.name,error))return false;
    OrderedView current;const bool readback=read_ordered_view(memory,view,current,error);
    saved.evidence["prepared_readback_valid"]=readback;saved.evidence["prepared"]=ordered_view_state(current);
    return (readback&&same_editor_input(saved,current))||
        fail(error,error.empty()?"Character Rename preparation changed input cache/baseline/focus":error);
}
bool restore_ordered_view(Memory& memory,OrderedView& saved,std::string& error){
    if(!saved.armed)return true;
    if(saved.restore_attempted)return fail(error,"Character rename earlier UI restoration failed; no replay");
    saved.restore_attempted=true;uintptr_t actual=0,editor=0,outer=0;
    if(!window(memory,0,actual,editor,outer,error)||actual!=saved.view||editor!=saved.editor||outer!=saved.outer)
        return fail(error,error.empty()?"Character rename UI owner changed before restoration":error);
    auto seh=write_view_target(saved.view,saved.selected);if(seh)return fail(error,"Character rename scoped UI target restoration SEH="+std::to_string(seh));
    if(!prepare_view_text(saved,saved.text,error))return false;
    seh=restore_view_selection(saved.editor,saved.selection);if(seh)return fail(error,"Character rename native UI selection restoration SEH="+std::to_string(seh));
    seh=write_view_numeric(saved.editor,saved.numeric_cache);if(seh)return fail(error,"Character rename UI numeric cache restoration SEH="+std::to_string(seh));
    if(saved.button){seh=set_view_disabled(saved.button,saved.disabled!=0);
        if(seh)return fail(error,"Character rename original button restoration SEH="+std::to_string(seh));}
    OrderedView current;const bool readback=read_ordered_view(memory,saved.view,current,error);
    saved.evidence["restored_readback_valid"]=readback;saved.evidence["restored"]=ordered_view_state(current);
    if(!readback)return false;
    if(current.selected!=saved.selected||current.editor!=saved.editor||current.button!=saved.button||current.text!=saved.text||
       current.numeric_cache!=saved.numeric_cache||current.selection!=saved.selection||!same_editor_input(saved,current)||
       current.disabled!=saved.disabled||current.clickable!=saved.clickable)
        return fail(error,"Character rename restored UI target/draft/cursor/button differs");
    saved.armed=false;saved.restored=true;return true;
}
bool prepared_notification(void* receiver,std::string& error){
    if(GetCurrentThreadId()!=state.thread||uintptr_t(receiver)!=preparing_receiver)return fail(error,"Character rename text notification receiver/thread differs");
    Memory memory;uintptr_t editor=0;if(!field(memory,uintptr_t(receiver),0x360,editor,error)||editor!=preparing_editor)
        return fail(error,error.empty()?"Character rename text notification editor differs":error);
    ++preparation_notifications;return true;
}
struct OrderedViewGuard {
    Memory& memory;OrderedView& saved;
    ~OrderedViewGuard(){
        if(!saved.armed||saved.restore_attempted)return;
        try{std::string error;if(!restore_ordered_view(memory,saved,error)){fault=true;reject("Character rename exception UI restoration failed: "+error);}}
        catch(const std::exception& exception){fault=true;reject(std::string("Character rename UI restoration exception: ")+exception.what());}
        catch(...){fault=true;reject("Character rename UI restoration non-standard exception");}
    }
};
