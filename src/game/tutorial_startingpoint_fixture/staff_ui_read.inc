struct StaffUI {
    uintptr_t tabs=0,view=0,project_view=0,pips=0,button=0;
    bool operator==(const StaffUI&)const=default;
};
bool staff_baseline(const Pin& p,const tutorial_development::StartingpointSnapshot& s,std::string& e){
    return (p.project&&p.project==s.project&&s.row&&s.creation&&s.row->project_present&&s.row->purchased&&
        !s.row->deployed&&!s.row->custom&&s.row->desired_workers==0)||
        fail(e,"startingpoint Staff requires current purchased Startingpoint project with desired workers0");
}
bool staff_tabs(Pin& p,StaffUI& ui,std::string& e,bool before_show=false){
    ResearchUI common;if(!research_tabs(p,common,e,before_show))return false;
    ui.tabs=common.tabs;ui.view=common.view;return true;
}
bool staff_measure(Pin& p,const tutorial_development::StartingpointSnapshot& s,int32_t available,StaffUI& out,json& values,std::string& e,int32_t expected_value=0){
    values=json::object();out={};p.window=0;if(!development_window(p,e))return false;
    std::vector<std::string> issues;
    const auto issue=[&](const std::string& cause){if(issues.size()<32)issues.push_back(cause);};
    const auto owner_name=[](uintptr_t owner,uintptr_t expected){return owner==expected?"expected":(owner?"foreign":"null");};
    const auto owner=[&](uintptr_t object,const std::string& label){uintptr_t actual=0;if(!at(object,0x280,actual,e))return false;
        values[label+"_owner"]=owner_name(actual,p.window);if(actual!=p.window)issue(label+" owner="+owner_name(actual,p.window));return true;};
    const auto read_type=[&](uintptr_t object,uintptr_t expected,const std::string& label,bool& matches){
        matches=false;values[label+"_present"]=object!=0;if(!object){issue(label+" absent");return true;}
        uintptr_t ap=0;if(!at(object,0,ap,e))return false;
        values[label+"_class_rva"]=ap>=p.image&&ap-p.image<0x1800000?json(ap-p.image):json(nullptr);
        matches=ap==p.image+expected;if(!matches)issue(label+" class differs");return true;};
    const auto page=[&](uintptr_t object,const std::string& prefix,const char* required,uintptr_t& active){
        uintptr_t next=0;int32_t transition=0;std::string requested,current,next_name;
        if(!owner(object,prefix)||!text(object,0x3e8,requested,e)||!at(object,0x410,active,e)||!at(object,0x418,next,e)||!at(object,0x408,transition,e))return false;
        if(active&&(!text(active,0x1b0,current,e)||!owner(active,prefix+"_active")))return false;
        if(next&&!text(next,0x1b0,next_name,e))return false;
        values[prefix+"_requested"]=requested;values[prefix+"_active"]=current;values[prefix+"_next"]=next_name;values[prefix+"_transition"]=transition;
        if(requested!=required)issue(prefix+" requested="+requested+" required="+required);
        if(current!=required)issue(prefix+" active="+current+" required="+required);return true;};
    uint8_t visible=0,layout=0;if(!at(p.window,0x45f,visible,e)||!at(p.window,0x460,layout,e))return false;
    values["window_visible"]=visible;values["window_layout_pending"]=layout;
    if(visible!=1)issue("window visible="+std::to_string(visible)+" required=1");
    values["desired_workers"]=s.row?s.row->desired_workers:-1;values["available_workers"]=available;values["max_workers"]=s.creation?s.creation->max_workers:-1;
    if(available<1||available>50)issue("available workers="+std::to_string(available)+" required=1..50");
    if(!s.creation||s.creation->max_workers<1)issue("native max workers less than1");
    bool done=false;const auto code=staff_done(p.image,p.lounge,done);values["debugging_complete"]=done;values["completion_exception"]=code;
    if(code)return fail(e,"startingpoint Staff completion getter exception="+std::to_string(code));
    if(done)issue("native debugging complete");
    bool tabs_ok=false,view_ok=false,project_ok=false,pips_ok=false,button_ok=false;uintptr_t staff=0,dev=0;
    if(!at(p.window,0x540,out.tabs,e)||!read_type(out.tabs,0x17015b0,"tabs",tabs_ok))return false;
    if(tabs_ok&&(!owner(out.tabs,"tabs")||!at(out.tabs,0x3b8,out.view,e)||!read_type(out.view,0x16dfe90,"view",view_ok)))return false;
    if(view_ok&&!page(out.view,"staff_page","{dev_tab_staff}",staff))return false;
    if(!at(p.window,0x568,out.project_view,e)||!read_type(out.project_view,0x16dfe90,"project_view",project_ok))return false;
    if(project_ok&&!page(out.project_view,"project_page","dev",dev))return false;
    if(!at(p.window,0x588,out.pips,e)||!read_type(out.pips,0x1744290,"pips",pips_ok))return false;
    if(pips_ok){
        uintptr_t data=0,children=0,parent=0;int32_t count=0,capacity=0,child_count=0,child_capacity=0,pip_count=0,value=0;std::string command;
        if(!owner(out.pips,"pips")||!at(out.pips,0x278,parent,e)||!text(out.pips,0x398,command,e)||!at(out.pips,0x3c4,pip_count,e)||
           !at(out.pips,0x3e0,value,e)||!at(out.pips,0x3d0,data,e)||!at(out.pips,0x3d8,count,e)||!at(out.pips,0x3dc,capacity,e)||
           !at(out.pips,0x2d0,children,e)||!at(out.pips,0x2d8,child_count,e)||!at(out.pips,0x2dc,child_capacity,e))return false;
        values["pips_command"]=command;values["pips_value"]=value;values["pip_count"]=pip_count;
        values["buttons_count"]=count;values["buttons_capacity"]=capacity;values["children_count"]=child_count;values["children_capacity"]=child_capacity;
        if(command!="MessageTo Parent SetTechWorkers")issue("pips command="+command+" required=MessageTo Parent SetTechWorkers");
        if(value!=expected_value)issue("pips value="+std::to_string(value)+" required="+std::to_string(expected_value));
        if(!s.creation||pip_count!=std::min(available,s.creation->max_workers))issue("pip count differs from actual available/max workers");
        std::set<uintptr_t> chain;bool has_staff=false,has_dev=false;unsigned depth=0;
        while(parent&&parent!=p.window&&depth<32&&chain.insert(parent).second){
            ++depth;has_staff|=parent==staff;has_dev|=parent==dev;
            if(!owner(parent,"pips_ancestor_"+std::to_string(depth))||!at(parent,0x278,parent,e))return false;
        }
        const bool ancestry=parent==p.window&&has_staff&&has_dev;
        values["pips_parent_chain_depth"]=depth;values["pips_parent_chain_bound"]=ancestry;
        values["pips_parent"]=ancestry?"expected":(parent?"foreign":"null");
        if(!ancestry)issue("pips parent chain does not bind active Staff/dev/Window");
        const auto bounded=[](uintptr_t array,int32_t n,int32_t cap){return array&&n>=1&&n<=50&&cap>=n&&cap<=64;};
        if(!bounded(data,count,capacity))issue("Pips button array bounds invalid");
        if(!bounded(children,child_count,child_capacity))issue("Pips owned children bounds invalid");
        if(bounded(data,count,capacity)&&bounded(children,child_count,child_capacity)){
            std::set<uintptr_t> buttons,owned;
            for(int32_t i=0;i<count;++i){uintptr_t item=0;if(!at(data,size_t(i)*8,item,e))return false;
                if(!item||!buttons.insert(item).second)issue("Pips button array null/duplicate");if(i==0)out.button=item;}
            for(int32_t i=0;i<child_count;++i){uintptr_t item=0;if(!at(children,size_t(i)*8,item,e))return false;
                if(!item||!owned.insert(item).second)issue("Pips owned children null/duplicate");}
            const bool members=buttons==owned&&count==pip_count;values["button_membership_matches"]=members;
            if(!members)issue("Pips button/owned-child membership differs");
        }
        if(!read_type(out.button,0x16e3ec0,"button",button_ok))return false;
    }
    if(button_ok){uintptr_t parent=0;uint8_t disabled=0,toggle=0;std::string command;
        if(!owner(out.button,"button")||!at(out.button,0x278,parent,e)||!text(out.button,0x388,command,e)||!at(out.button,0x46d,disabled,e)||!at(out.button,0x46c,toggle,e))return false;
        values["button_parent"]=owner_name(parent,out.pips);values["button_command"]=command;values["button_disabled"]=disabled;values["button_toggle"]=toggle;
        if(parent!=out.pips)issue("first Button parent differs from Pips");
        if(command!="MessageTo Parent Pressed 0")issue("first Button command="+command+" required=MessageTo Parent Pressed 0");
        if(toggle!=1)issue("first Button toggle="+std::to_string(toggle)+" required=1");
        if(disabled!=0)issue("first Button disabled="+std::to_string(disabled)+" required=0");
    }
    values["ready"]=issues.empty();values["causes"]=issues;values["cause"]=issues.empty()?"":issues.front();return true;
}
bool staff_ready(Pin& p,const tutorial_development::StartingpointSnapshot& s,int32_t available,StaffUI& ui,json& measured,std::string& e,int32_t expected=0){
    if(!staff_measure(p,s,available,ui,measured,e,expected))return false;
    return measured.at("ready").get<bool>()||fail(e,"startingpoint Staff "+measured.at("cause").get<std::string>());
}
bool staff_submission(const json& previous,const json& after,std::string& id,std::string& e){
    if(!after.value("client_staff_submitted",false)||!after.value("client_staff_suppressed",false)||
       !after.value("client_staff_cause",std::string{}).empty()||after.value("client_staff_desired_workers",-1)!=1)
        return fail(e,"startingpoint Client Staff intent not submitted for1 worker: "+after.value("client_staff_cause",std::string{}));
    id=after.value("client_staff_action_uuid",std::string{});
    return (id.size()==36&&id!=previous.value("client_staff_action_uuid",std::string{}))||fail(e,"startingpoint Client Staff requires a fresh submitted action UUID");
}
