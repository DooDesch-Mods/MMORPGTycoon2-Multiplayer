struct ResearchUI {
    uintptr_t tabs=0,view=0,grid=0,button=0;
    int32_t row=-1,catalogue=-1;
    bool operator==(const ResearchUI&)const=default;
};
bool research_tabs(Pin& p,ResearchUI& out,std::string& e,bool before_show=false){
    p.window=0;if(!development_window(p,e))return false;
    const auto owner=[&](uintptr_t child,const char* cause){
        uintptr_t window=0;if(!at(child,0x280,window,e))return false;
        if(window==p.window)return true;
        if(window||!before_show)return fail(e,cause);
        // Pane constructors leave this backlink null until original Window::Show
        // performs Layout. No child method is called before the strict repin.
        uint8_t visible=0,layout_pending=0;
        if(!at(p.window,0x45f,visible,e)||!at(p.window,0x460,layout_pending,e))return false;
        return (visible==0&&layout_pending==1)||fail(e,"startingpoint Research unbound child requires hidden Window with native Layout pending");
    };
    if(!at(p.window,0x540,out.tabs,e)||!type(out.tabs,p.image+0x17015b0,e)||
       !owner(out.tabs,"startingpoint Research tabs have a different window owner"))return false;
    if(!at(out.tabs,0x3b8,out.view,e)||!type(out.view,p.image+0x16dfe90,e)||
       !owner(out.view,"startingpoint Research page view has a different window owner"))return false;
    return true;
}
bool research_measure(Pin& p,const tutorial_development::StartingpointSnapshot& s,ResearchUI& out,json& values,std::string& e){
    values=json::object();out={};p.window=0;if(!development_window(p,e))return false;
    std::vector<std::string> issues;
    const auto issue=[&](const std::string& cause){if(issues.size()<16)issues.push_back(cause);};
    const auto owner_name=[&](uintptr_t owner){return owner==p.window?"expected":(owner?"foreign":"null");};
    const auto read_type=[&](uintptr_t object,uintptr_t expected,const char* label,bool& matches){
        matches=false;values[std::string(label)+"_present"]=object!=0;
        if(!object){issue(std::string(label)+" absent");return true;}
        uintptr_t ap=0;if(!at(object,0,ap,e))return false;
        values[std::string(label)+"_class_rva"]=ap>=p.image&&ap-p.image<0x1800000?json(ap-p.image):json(nullptr);
        matches=ap==p.image+expected;if(!matches)issue(std::string(label)+" class differs");return true;
    };
    const auto read_owner=[&](uintptr_t object,const char* label){
        uintptr_t owner=0;if(!at(object,0x280,owner,e))return false;
        values[std::string(label)+"_owner"]=owner_name(owner);
        if(owner!=p.window)issue(std::string(label)+" owner="+owner_name(owner));return true;
    };
    uint8_t visible=0,layout=0;
    if(!at(p.window,0x45f,visible,e)||!at(p.window,0x460,layout,e))return false;
    values["window_visible"]=visible;values["window_layout_pending"]=layout;
    if(visible!=1)issue("window visible="+std::to_string(visible)+" required=1");
    bool tabs_type=false,view_type=false,grid_type=false,button_type=false;
    if(!at(p.window,0x540,out.tabs,e)||!read_type(out.tabs,0x17015b0,"tabs",tabs_type))return false;
    if(tabs_type){
        if(!read_owner(out.tabs,"tabs")||!at(out.tabs,0x3b8,out.view,e)||!read_type(out.view,0x16dfe90,"view",view_type))return false;
    }
    if(view_type){
        uintptr_t active=0,next=0;int32_t transition=0;std::string requested,current,next_name;
        if(!read_owner(out.view,"view")||!text(out.view,0x3e8,requested,e)||!at(out.view,0x410,active,e)||
           !at(out.view,0x418,next,e)||!at(out.view,0x408,transition,e))return false;
        if(active&&(!text(active,0x1b0,current,e)||!read_owner(active,"active_page")))return false;
        if(next&&!text(next,0x1b0,next_name,e))return false;
        values["requested_page"]=requested;values["active_page"]=current;values["next_page"]=next_name;values["page_transition"]=transition;
        if(requested!="{dev_tab_tech}")issue("requested page="+requested+" required={dev_tab_tech}");
        if(current!="{dev_tab_tech}")issue("active page="+current+" required={dev_tab_tech}");
    }
    if(!at(p.window,0x5d8,out.grid,e)||!read_type(out.grid,0x16dade0,"grid",grid_type)||
       !at(p.window,0x600,out.button,e)||!read_type(out.button,0x16e3ec0,"button",button_type))return false;
    bool selected=false;
    if(!s.creation||!s.creation->source||s.project||!s.row||s.row->purchased||!s.record)
        issue("owned unpurchased catalogue/no-project binding differs");
    else for(size_t i=0;i<s.creation->catalogue.size();++i)if(s.creation->catalogue[i].address==s.creation->source){
        if(out.catalogue!=-1)issue("catalogue source duplicated");out.catalogue=int32_t(i);}
    if(out.catalogue<0)issue("Startingpoint source absent from owned catalogue");
    if(grid_type){
        uintptr_t data=0,source=0;int32_t count=0,capacity=0,index=-1;uint8_t active=0;std::string command;
        if(!read_owner(out.grid,"grid")||!text(out.grid,0x360,command,e)||!at(out.grid,0x3c0,data,e)||
           !at(out.grid,0x3c8,count,e)||!at(out.grid,0x3cc,capacity,e)||!at(out.grid,0x3e8,index,e)||
           !at(out.grid,0x3ec,active,e)||!at(p.window,0x610,source,e))return false;
        values["grid_command"]=command;values["grid_count"]=count;values["grid_capacity"]=capacity;
        values["grid_selected_index"]=index;values["grid_selection_active"]=active;
        values["selected_catalogue_matches"]=s.creation&&source==s.creation->source;
        if(command!="MessageTo Parent SelectProject")issue("grid command="+command+" required=MessageTo Parent SelectProject");
        if(count<1||count>1024||capacity<count||capacity>1024||!data)issue("grid bounds count="+std::to_string(count)+" capacity="+std::to_string(capacity));
        else{
            std::set<int32_t> ids;
            for(int32_t i=0;i<count;++i){int32_t id=-1;if(!at(data,size_t(i)*12,id,e))return false;
                if(!s.creation||id<0||size_t(id)>=s.creation->catalogue.size()||!ids.insert(id).second)issue("grid identifier invalid/duplicate at row="+std::to_string(i)+" id="+std::to_string(id));
                if(id==out.catalogue)out.row=i;
            }
            if(out.row<0)issue("Startingpoint row absent from current Grid");
            else{uint8_t row_selected=0;if(!at(data,size_t(out.row)*12+9,row_selected,e))return false;
                values["target_row_selected"]=row_selected;
                selected=index==out.row&&active==1&&row_selected==1&&s.creation&&source==s.creation->source;}
        }
    }
    if(button_type){
        uint8_t disabled=0,toggle=0;std::string command;
        if(!read_owner(out.button,"button")||!text(out.button,0x388,command,e)||!at(out.button,0x46d,disabled,e)||!at(out.button,0x46c,toggle,e))return false;
        values["button_command"]=command;values["button_disabled"]=disabled;values["button_toggle"]=toggle;
        if(command!="MessageTo Window StartTech")issue("button command="+command+" required=MessageTo Window StartTech");
        if(toggle)issue("button toggle="+std::to_string(toggle)+" required=0");
        if(disabled>1)issue("button disabled byte invalid="+std::to_string(disabled));
    }
    values["grid_row"]=out.row;values["catalogue_index"]=out.catalogue;values["selection_confirmed"]=selected;
    values["ready"]=issues.empty();values["causes"]=issues;values["cause"]=issues.empty()?"":issues.front();
    return true;
}
bool research_selection(Pin& p,const tutorial_development::StartingpointSnapshot& s,ResearchUI& out,
                        std::string& e,bool selected,bool enabled,bool page_ready=false){
    (void)page_ready;json measured;if(!research_measure(p,s,out,measured,e))return false;
    if(!measured.at("ready").get<bool>())return fail(e,"startingpoint Research "+measured.at("cause").get<std::string>());
    if(selected&&!measured.at("selection_confirmed").get<bool>())return fail(e,"startingpoint Research actual selected Grid/source differs index="+std::to_string(measured.at("grid_selected_index").get<int32_t>())+" required="+std::to_string(out.row));
    if(enabled&&measured.at("button_disabled")!=0)return fail(e,"startingpoint Research original button disabled="+std::to_string(measured.at("button_disabled").get<int>()));
    if(selected){tutorial_replication::start_intent::Window bound;
        if(!tutorial_replication::start_intent::window(memory,s,s.creation->source,bound,e)||bound.address!=p.window||bound.tabs!=out.tabs)
            return fail(e,e.empty()?"startingpoint Research capture window differs":e);}
    return true;
}
bool research_submission(const json& previous,const json& after,std::string& id,std::string& e){
    if(!after.value("client_start_submitted",false)||!after.value("client_start_suppressed",false)||
       !after.value("client_start_cause",std::string{}).empty())
        return fail(e,"startingpoint Client Research intent not submitted: "+after.value("client_start_cause",std::string{}));
    id=after.value("client_start_action_uuid",std::string{});
    return (id.size()==36&&id!=previous.value("client_start_action_uuid",std::string{}))||
        fail(e,"startingpoint Client Research requires a fresh submitted action UUID");
}
