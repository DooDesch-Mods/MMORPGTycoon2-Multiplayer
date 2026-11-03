struct OfferedControl {
    uintptr_t pane=0;
    std::string id,command;
    bool checkbox=false,checked=false,ready=false,tab=false,text=false,combo=false,icon=false;
};
bool offered_visibility(Memory& memory,uintptr_t pane,uintptr_t root,uintptr_t outer,bool& ready,std::string& error){
    ready=true;bool reached=root==outer;std::set<uintptr_t> seen;
    while(pane!=outer){
        uintptr_t parent=0,owner=0;uint8_t shown=0;
        if(!pane||!seen.insert(pane).second||seen.size()>64||!field(memory,pane,0x278,parent,error)||
           !field(memory,pane,0x280,owner,error)||!field(memory,pane,0x69,shown,error)||shown>1)
            return fail(error,error.empty()?"Character offered control owner/ancestry differs":error);
        if(!shown||owner!=outer||!parent){ready=false;return true;}
        reached|=pane==root;pane=parent;
    }
    uint8_t shown=0;
    if(!field(memory,outer,0x45f,shown,error)||shown>1)return fail(error,error.empty()?"Character offered window visibility differs":error);
    ready=ready&&reached&&shown==1;return true;
}
bool offered_controls(Memory& memory,uintptr_t root,uintptr_t outer,std::vector<OfferedControl>& result,std::string& error){
    result.clear();std::vector<uintptr_t> panes{root};std::set<uintptr_t> seen;
    while(!panes.empty()){
        const auto pane=panes.back();panes.pop_back();uintptr_t ap=0,gather=0;
        if(!pane||!seen.insert(pane).second||seen.size()>4096||!field(memory,pane,0,ap,error)||!field(memory,ap,0x178,gather,error))
            return fail(error,error.empty()?"Character offered content is unreadable, cyclic or oversized":error);
        if(ap==state.image+0x16e3ec0||ap==state.image+0x16d98c0||ap==state.image+0x17015b0||
           ap==state.image+0x1703290||ap==state.image+0x16d9b30||ap==state.image+0x1717790){
            OfferedControl value;value.pane=pane;value.checkbox=ap==state.image+0x16d98c0;value.tab=ap==state.image+0x17015b0;
            value.text=ap==state.image+0x1703290;value.combo=ap==state.image+0x16d9b30;value.icon=ap==state.image+0x1717790;
            if(!npc_quest_records::read_string(memory,pane,0x1b0,"Character offered control id",value.id,error)||
               (!value.tab&&!value.icon&&!npc_quest_records::read_string(memory,pane,value.checkbox?0x348:value.text?0x3e0:value.combo?0x3b8:0x388,"Character offered control command",value.command,error))||
               !offered_visibility(memory,pane,root,outer,value.ready,error))return false;
            if(value.tab){result.push_back(std::move(value));}
            else{
            uint8_t clickable=0,held=0,checked=0;
            if(!field(memory,pane,0x174,clickable,error)||clickable>1)return fail(error,error.empty()?"Character offered control clickable flag differs":error);
            if(value.text){uint8_t editable=0;if(!field(memory,pane,0x3d8,editable,error)||editable>1)return fail(error,error.empty()?"Character original editable flag is not Boolean":error);value.ready=value.ready&&editable==1;}
            else if(value.combo){}
            else if(value.icon){uint8_t enabled=0,dragged=0;
                if(!field(memory,pane,0x395,enabled,error)||enabled>1||!field(memory,pane,0x396,held,error)||held>1||
                   !field(memory,pane,0x39a,dragged,error)||dragged>1)return fail(error,error.empty()?"Character original Ability icon state is not Boolean":error);
                value.ready=value.ready&&enabled==1&&!dragged;
            }else if(value.checkbox){
                uintptr_t check_visual=0,press_visual=0;uint8_t visible_check=0,visible_press=0;
                if(!field(memory,pane,0x344,held,error)||held>1||!field(memory,pane,0x346,checked,error)||checked>1)
                    return fail(error,error.empty()?"Character offered checkbox state differs":error);
                if(!field(memory,pane,0x320,check_visual,error)||!check_visual||!field(memory,pane,0x328,press_visual,error)||!press_visual||
                   !field(memory,check_visual,0x51,visible_check,error)||visible_check!=checked||!field(memory,press_visual,0x51,visible_press,error)||visible_press>1)
                    return fail(error,error.empty()?"Character offered checkbox native visual bindings differ":error);
                value.checked=checked!=0;
            }else{
                uint8_t pressed=0,disabled=0;uintptr_t bound=0;
                if(!field(memory,pane,0x584,held,error)||held>1||!field(memory,pane,0x585,pressed,error)||pressed>1||
                   !field(memory,pane,0x46d,disabled,error)||disabled>1||!field(memory,pane,0x578,bound,error))return fail(error,error.empty()?"Character original ButtonPane state is not Boolean":error);
                value.ready=value.ready&&!pressed&&!disabled&&!bound;
            }
            value.ready=value.ready&&clickable==1&&!held;result.push_back(std::move(value));
            }
        }
        if(gather==state.image+0xc35730)continue;
        if(gather!=state.image+0x8167c0)return fail(error,"Character offered GatherAllContents implementation differs");
        for(const auto offset:{size_t(0x2c8),size_t(0x2f8)}){
            std::vector<uintptr_t> children;if(!open_array(memory,pane,offset,children,error))return false;
            panes.insert(panes.end(),children.begin(),children.end());
        }
    }
    return true;
}
bool offered_unique(const std::vector<OfferedControl>& controls,const std::string& command,bool checkbox,OfferedControl& result,std::string& error){
    result={};
    for(const auto& value:controls){
        if(value.tab||value.text||value.combo||value.icon||value.checkbox!=checkbox||value.command!=command||!value.ready)continue;
        if(result.pane)return fail(error,"Character requested original offered command is ambiguous");result=value;
    }
    return result.pane!=0||fail(error,"Character requested original control is not currently offered");
}
std::string offered_key(const OfferedControl& control){return control.id.empty()?control.command:control.id;}
bool offered_named(const std::vector<OfferedControl>& controls,const std::string& key,OfferedControl& result,std::string& error){
    result={};for(const auto& value:controls){if(!value.ready||offered_key(value)!=key)continue;
        if(result.pane)return fail(error,"Character offered control key is ambiguous");result=value;}
    return result.pane!=0||fail(error,"Character control key is not currently offered");
}
bool offered_choices(Memory& memory,const OfferedControl& control,std::vector<std::string>& values,int32_t& selected,std::string& error){
    values.clear();uintptr_t data=0;int32_t count=0,capacity=0;
    if(!control.combo||!control.ready||!field(memory,control.pane,0x348,data,error)||!field(memory,control.pane,0x350,count,error)||
       !field(memory,control.pane,0x354,capacity,error)||count<1||count>4096||capacity<count||capacity>65536||!data||
       !field(memory,control.pane,0x4a0,selected,error)||selected< -1||selected>=count)
        return fail(error,error.empty()?"Character original ComboBox choice catalogue differs":error);
    std::set<std::string> unique;
    for(int32_t i=0;i<count;++i){std::string value;
        if(!npc_quest_records::read_string(memory,data,size_t(i)*32,"Character offered choice",value,error)||!unique.insert(value).second)
            return fail(error,error.empty()?"Character original ComboBox choice is unreadable or duplicate":error);
        values.push_back(std::move(value));}
    return true;
}
bool offered_tab(const std::vector<OfferedControl>& controls,const std::string& id,OfferedControl& result,std::string& error){
    result={};for(const auto& value:controls){if(!value.tab||value.id!=id||!value.ready)continue;
        if(result.pane)return fail(error,"Character requested original tab is ambiguous");result=value;}
    return result.pane!=0||fail(error,"Character requested original tab is not offered");
}
struct OfferedPage {uintptr_t pages=0,page=0,current=0,requested=0;};
bool offered_page(Memory& memory,const OfferedControl& tab,const std::string& id,OfferedPage& out,std::string& error){
    out={};uintptr_t ap=0,data=0,enabled=0;int32_t count=0,capacity=0,enable_count=0,enable_capacity=0;
    if(!tab.tab||!tab.ready||!field(memory,tab.pane,0x3b8,out.pages,error)||!out.pages||
       !field(memory,out.pages,0,ap,error)||ap!=state.image+0x16dfe90||
       !field(memory,out.pages,0x388,data,error)||!field(memory,out.pages,0x390,count,error)||
       !field(memory,out.pages,0x394,capacity,error)||count<1||count>128||capacity<count||capacity>4096||!data||
       !field(memory,out.pages,0x3a0,enabled,error)||!field(memory,out.pages,0x3a8,enable_count,error)||
       !field(memory,out.pages,0x3ac,enable_capacity,error)||enable_count!=count||enable_capacity<count||enable_capacity>4096||!enabled||
       !field(memory,out.pages,0x410,out.current,error)||!field(memory,out.pages,0x418,out.requested,error))
        return fail(error,error.empty()?"Character original tab page catalogue/enabled bounds differ":error);
    for(int32_t i=0;i<count;++i){uintptr_t page=0;std::string name;uint8_t allowed=0;
        if(!field(memory,data,size_t(i)*8,page,error)||!page||!field(memory,enabled,size_t(i),allowed,error)||allowed>1||
           !npc_quest_records::read_string(memory,page,0x1b0,"Character original page id",name,error))return fail(error,error.empty()?"Character original page entry is absent or enable flag is not Boolean":error);
        if(name!=id)continue;
        if(out.page||!allowed)return fail(error,"Character requested original page is locked or ambiguous");out.page=page;
    }
    return out.page!=0||fail(error,"Character requested original page is absent from its catalogue");
}
