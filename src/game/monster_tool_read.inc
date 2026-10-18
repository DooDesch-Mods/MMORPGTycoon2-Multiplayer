struct ToolPin {
    uintptr_t item=0,type=0,behaviour=0,cursor=0,window=0,character=0;
    std::string definition_name;
    int32_t rank=-1,level=-1;
    uint8_t loose=2;
    std::array<int32_t,2> character_id{-1,-1};
    bool operator==(const ToolPin&)const=default;
};
bool tools(Memory& m,uintptr_t image,ToolPin& pin,json& inventory,std::string& e) {
    inventory=json::array();pin={};uintptr_t player=0,inv=0,ap=0;
    npc_census::InventoryHeader before,after;
    if(!field(m,image,0x13df9f0,player,e)||!field(m,player,0,ap,e)||ap!=image+0x17873b0||
       !field(m,player,0x1d0,inv,e)||!field(m,inv,0,ap,e)||ap!=image+0x16df0f0||
       !field(m,image,0x13df9d0,pin.cursor,e)||!field(m,pin.cursor,0,ap,e)||ap!=image+0x1786dd0||
       !npc_census::read_inventory_header(m,inv,before,e))return fail(e,e.empty()?"Elite Player/inventory/cursor class differs":e);
    std::set<uintptr_t> unique;std::vector<uintptr_t> members;
    for(int32_t i=0;i<before.count;++i) {
        uintptr_t item=0,type=0,proxy=0,target=0,owned=0,behaviour=0;
        if(!field(m,before.items,size_t(i)*8,item,e))return false;members.push_back(item);
        if(!item)continue;
        if(!unique.insert(item).second||!field(m,item,0,ap,e)||ap!=image+0x17437f0||
           !field(m,item,0x68,type,e)||!field(m,item,0x70,proxy,e)||!field(m,proxy,0,target,e)||
           !field(m,type,0x40,owned,e)||target!=type+0x38||owned!=proxy||!field(m,type,0,ap,e))
            return fail(e,e.empty()?"Elite inventory duplicate item/weak type owner differs":e);
        if(ap!=image+0x173b7b0)continue;
        if(!field(m,type,0x160,behaviour,e)||!field(m,behaviour,0,ap,e))return false;
        if(ap!=image+0x1739500)continue;
        std::string key;int32_t rank=-1,level=-1;uint8_t loose=2;
        if(!name(m,behaviour+0x50,key,e)||!field(m,behaviour,0x364,rank,e)||
           !field(m,behaviour,0x358,level,e)||!field(m,behaviour,0x368,loose,e))return false;
        inventory.push_back({{"key",key},{"rank",rank},{"level",level},{"loose",loose},
            {"local_behaviour",behaviour},{"local_item",item}});
        if(key!="item_elitemonster")continue;
        uintptr_t getter=0;std::string definition;
        if(!field(m,image+0x173b7b0,0xa8,getter,e)||getter!=image+0xd31590||
           !name(m,type+0x48,definition,e)||definition.empty())return fail(e,e.empty()?"Elite ItemType::GetName definition unavailable":e);
        inventory.back()["definition_name"]=definition;
        pin.definition_name=std::move(definition);
        if(pin.behaviour)return fail(e,"Elite inventory tool is duplicated");
        pin.item=item;pin.type=type;pin.behaviour=behaviour;pin.rank=rank;pin.level=level;pin.loose=loose;
    }
    uintptr_t current_player=0,current_inv=0,current_cursor=0;
    if(!npc_census::read_inventory_header(m,inv,after,e)||before!=after||
       !field(m,image,0x13df9f0,current_player,e)||current_player!=player||
       !field(m,player,0x1d0,current_inv,e)||current_inv!=inv||
       !field(m,image,0x13df9d0,current_cursor,e)||current_cursor!=pin.cursor)
        return fail(e,e.empty()?"Elite inventory ownership changed during read":e);
    for(int32_t i=0;i<before.count;++i) {
        uintptr_t item=0;if(!field(m,before.items,size_t(i)*8,item,e))return false;
        if(item!=members[size_t(i)])return fail(e,"Elite inventory membership changed during read");
    }
    return true;
}
bool window_member(Memory& m,uintptr_t header,size_t data_off,size_t count_off,size_t cap_off,
                   uintptr_t wanted,int32_t limit,unsigned expected,std::string& e) {
    uintptr_t data=0;int32_t count=0,cap=0;
    if(!field(m,header,data_off,data,e)||!field(m,header,count_off,count,e)||!field(m,header,cap_off,cap,e))return false;
    if(count<0||count>limit||cap<count||(count&&!data)||data>UINTPTR_MAX-size_t(count)*8)
        return fail(e,"Elite window owner array bounds differ");
    std::vector<uintptr_t> values;std::set<uintptr_t> seen;unsigned matches=0;
    for(int32_t i=0;i<count;++i) {
        uintptr_t value=0;if(!field(m,data,size_t(i)*8,value,e)||!value||!seen.insert(value).second)
            return fail(e,e.empty()?"Elite window owner member absent/duplicated":e);
        values.push_back(value);if(value==wanted)++matches;
    }
    uintptr_t again=0;int32_t next_count=0,next_cap=0;
    if(!field(m,header,data_off,again,e)||!field(m,header,count_off,next_count,e)||!field(m,header,cap_off,next_cap,e)||
       again!=data||next_count!=count||next_cap!=cap)return fail(e,e.empty()?"Elite window owner array changed":e);
    for(size_t i=0;i<values.size();++i) {
        uintptr_t value=0;if(!field(m,data,i*8,value,e)||value!=values[i])return fail(e,e.empty()?"Elite window owner entry changed":e);
    }
    return matches==expected||fail(e,"Elite ThemeWindow owner membership actual="+std::to_string(matches)+" required="+std::to_string(expected));
}
bool selection(Memory& m,uintptr_t image,uintptr_t mode,ToolPin& pin,json& output,std::string& e,bool allow_unselected=false) {
    uintptr_t ap=0,list=0,data=0,selected_entry=0;int32_t count=0;
    if(!field(m,pin.behaviour,0x148,pin.window,e)||!field(m,pin.window,0,ap,e)||ap!=image+0x17321a0||
       !field(m,pin.window,0x4e8,list,e)||!field(m,list,0,ap,e)||ap!=image+0x172b1d0||
       !field(m,list,0x348,data,e)||!field(m,list,0x350,count,e)||!field(m,list,0x360,selected_entry,e))
        return fail(e,e.empty()?"Elite active ThemeWindow/List unavailable or wrong class":e);
    if(count<0||count>256||(count&&!data)||data>UINTPTR_MAX-size_t(count)*8)
        return fail(e,"Elite ThemeList bounds differ");
    std::string key;uint8_t visible=0;
    if(!name(m,pin.window+0x1b0,key,e)||key!="character_theme"||
       !field(m,pin.window,0x45f,visible,e)||visible!=1||
       !window_member(m,mode,0xa0,0xa8,0xac,pin.window,256,0,e)||
       !window_member(m,image,0x1a3c478,0x1a3c480,0x1a3c484,pin.window,1024,1,e))
        return fail(e,e.empty()?"Elite visible ThemeWindow identity/ownership differs":e);
    output=json::array();std::vector<uintptr_t> entries;std::set<uintptr_t> seen;std::set<std::array<int32_t,2>> ids;
    for(int32_t i=0;i<count;++i) {
        uintptr_t entry=0,character=0;
        if(!field(m,data,size_t(i)*8,entry,e)||!entry||!seen.insert(entry).second||
           !field(m,entry,0,ap,e)||ap!=image+0x1738b10||!field(m,entry,0x338,character,e))
            return fail(e,e.empty()?"Elite ThemeSelector class/member differs":e);
        entries.push_back(entry);if(!character)continue;
        std::array<int32_t,2> id{-1,-1};
        if(!field(m,character,0,ap,e)||ap!=image+0x1705990||!field(m,character,0x2c,id,e)||
           id[0]<0||id[1]<0||!ids.insert(id).second)return fail(e,e.empty()?"Elite CharacterType ID/class differs":e);
        output.push_back(id);
        if(entry==selected_entry){pin.character=character;pin.character_id=id;}
    }
    uintptr_t next_data=0,next_entry=0;int32_t next_count=0;
    if(!field(m,list,0x348,next_data,e)||!field(m,list,0x350,next_count,e)||!field(m,list,0x360,next_entry,e)||
       next_data!=data||next_count!=count||next_entry!=selected_entry)return fail(e,e.empty()?"Elite catalogue header/selection changed":e);
    for(size_t i=0;i<entries.size();++i) {
        uintptr_t entry=0;if(!field(m,data,i*8,entry,e)||entry!=entries[i])return fail(e,e.empty()?"Elite catalogue membership changed":e);
    }
    if(!pin.character)return allow_unselected||fail(e,"Elite catalogue has no unique selected CharacterType");
    d::ReadMemory bounded{m};monster_zone_selection::Catalogue catalogue;
    if(!monster_zone_selection::read(bounded,image,pin.character_id[1],catalogue,e)||catalogue.character!=pin.character)
        return fail(e,e.empty()?"Elite selected CharacterType is not the actual Monster catalogue member":e);
    return true;
}
struct LevelState {
    uintptr_t window=0,view=0,widget=0,value=0,rows=0;
    int32_t count=0,capacity=0,minimum=-1,maximum=-1;
    std::array<int32_t,2> selected{-1,-1};uint8_t valid=2;
    json output;
};
bool tutorial_clear(Memory& m,uintptr_t image,const host::WorldState& world,json& out,bool& clear,std::string& e) {
    json observed;if(!tutorial_observer::read(observed,e))return false;
    out={{"observation",observed},{"message_windows",json::array()},{"blocked",false}};clear=true;
    uintptr_t log=0,data=0;int32_t count=0,cap=0;
    if(!field(m,uintptr_t(world.origin.state),0x1e8,log,e)||!field(m,log,0x20,data,e)||
       !field(m,log,0x28,count,e)||!field(m,log,0x2c,cap,e)||count<0||count>64||cap<count||cap>64)
        return fail(e,e.empty()?"Elite tutorial owner bounds differ":e);
    for(const auto& row:observed.at("tutorials")) {
        if(row.at("complete")==true||row.at("current_step_type")!="mmoStepMessage"||row.at("current_step_complete")==true)continue;
        uintptr_t instance=0;
        for(int32_t i=0;i<count;++i){uintptr_t candidate=0;std::string id;
            if(!field(m,data,size_t(i)*8,candidate,e)||!name(m,candidate+0x18,id,e))return false;
            if(id==row.at("scenario_id").get<std::string>()){if(instance)return fail(e,"Elite tutorial owner duplicated");instance=candidate;}}
        uintptr_t sequence=0,step=0,window=0,ap=0,show=0;uint8_t visible=2,target=2;
        if(!instance||!field(m,instance,0x58,sequence,e)||!field(m,sequence,0x48,step,e)||
           !field(m,step,0,ap,e)||ap!=image+0x16fda00||!field(m,step,0x1a8,window,e))
            return fail(e,e.empty()?"Elite tutorial message current owner differs":e);
        if(!window){clear=false;out["message_windows"].push_back({{"scenario_id",row.at("scenario_id")},
            {"current_step_index",row.at("current_step_index")},{"local_window",nullptr},{"pending",true}});continue;}
        if(!field(m,window,0,ap,e)||!field(m,ap,0x1d0,show,e)||show!=image+0x81ec20||
           !field(m,window,0x69,visible,e)||!field(m,window,0x45f,target,e)||visible>1||target>1)
            return fail(e,e.empty()?"Elite tutorial message Window/visibility differs":e);
        out["message_windows"].push_back({{"scenario_id",row.at("scenario_id")},{"current_step_index",row.at("current_step_index")},
            {"local_window",window},{"visible",visible==1},{"target_visible",target==1},{"pending",false}});
        if(visible||target)clear=false;
    }
    out["blocked"]=!clear;return true;
}
bool region_state(Memory& m,uintptr_t image,const host::WorldState& world,const q::World& region,
                  const ToolPin& tool,json& out,std::string& e) {
    uintptr_t manager=0,edit=0,current=0;int32_t view=-1;std::array<int32_t,2> range{-1,-1};
    if(!field(m,image,0x13df650,manager,e)||manager!=uintptr_t(world.origin.world)+0x180||
       !field(m,manager,0x78,view,e)||!field(m,manager,0x88,edit,e)||
       !field(m,tool.behaviour,0x350,current,e)||!field(m,region.region,0x838,range,e))
        return fail(e,e.empty()?"Elite WorldViewManager current world owner differs":e);
    if(view<0||view>4||range[0]<1||range[1]<range[0])return fail(e,"Elite actual view/Region level range invalid");
    out={{"ready",view==2&&edit==region.region&&current==region.region},{"expected_saved_region_id",region.saved_region},
        {"view_mode",view},{"view_region_matches",edit==region.region},{"tool_region_matches",current==region.region},
        {"local_manager",manager},{"local_edit_region",edit},{"local_tool_region",current},
        {"region_range",{{"minimum",range[0]},{"maximum",range[1]}}}};
    return true;
}
bool level_state_read(Memory& m,uintptr_t image,const host::WorldState& world,const q::World& region,
                 const ToolPin& tool,LevelState& s,std::string& e) {
    s={};uintptr_t ap=0,parent=0,owner=0;uint8_t shown=2,target=2,multi=2;
    s.output={{"available",false},{"ready",false},{"rows",json::array()},
        {"cause","Elite level reader has not completed"},{"binding_fields",json::object()}};
    std::string key;std::array<int32_t,2> selected{-1,-1},bounds{-1,-1};uint8_t valid=2;
    if(!field(m,tool.behaviour,0x150,s.window,e)||!field(m,s.window,0,ap,e)||ap!=image+0x17883d0||
       !name(m,s.window+0x1b0,key,e)||key!="zone_edit"||
       !field(m,s.window,0x69,shown,e)||shown!=1||!field(m,s.window,0x45f,target,e)||target!=1||
       !window_member(m,image,0x1a3c478,0x1a3c480,0x1a3c484,s.window,1024,1,e))
        return fail(e,e.empty()?"Elite original zone_edit Window ownership/visibility differs":e);
    // Serialized content uses the first View child array; FindPane searches both.
    // zone_edit.win places level_view directly under the Window.
    s.output["view_search"]={{"child_lists",json::array()},{"matches",json::array()}};
    for(const size_t offset:{size_t(0x2d0),size_t(0x300)}) {
        uintptr_t data=0;int32_t count=0,cap=0;
        if(!field(m,s.window,offset,data,e)||!field(m,s.window,offset+8,count,e)||!field(m,s.window,offset+12,cap,e))
            return fail(e,"Elite level Window child list "+std::to_string(offset)+" read failed: "+e);
        s.output["view_search"]["child_lists"].push_back({{"data_offset",offset},{"local_data",data},{"count",count},{"capacity",cap}});
        if(count<0||count>256||cap<count||cap>256||(count&&!data))
            return fail(e,"Elite level Window child list "+std::to_string(offset)+" bounds differ");
        for(int32_t i=0;i<count;++i) {
            uintptr_t child=0;
            if(!field(m,data,size_t(i)*8,child,e)||!child)return fail(e,e.empty()?"Elite level child absent":e);
            if(!name(m,child+0x1b0,key,e))return false;
            if(key=="level_view") {
                s.output["view_search"]["matches"].push_back({{"data_offset",offset},{"index",i},{"local_view",child}});
                if(s.view)return fail(e,"Elite level_view duplicated in Window child lists");
                s.view=child;
            }
        }
    }
    if(!s.view)return fail(e,"Elite level_view absent from both Window child lists");
    // Keep all independent binding observations when any original guard differs.
    auto& fields=s.output["binding_fields"];
    std::vector<std::string> mismatches;
    const auto observe=[&](const char* label,uintptr_t object,size_t offset,auto& actual,const auto& expected,bool nonzero=false){
        std::string cause;const bool readable=object&&field(m,object,offset,actual,cause);
        const bool matched=readable&&(nonzero?actual!=0:actual==expected);
        fields[label]={{"readable",readable},{"local_object",object},{"offset",offset},
            {"value",readable?json(actual):json(nullptr)},{"expected",nonzero?json("nonzero"):json(expected)},
            {"matched",matched}};
        if(!readable)fields[label]["cause"]=object?cause:"Required owner is absent";
        if(!matched)mismatches.push_back(label);
        return matched;
    };
    const auto observe_name=[&](const char* label,uintptr_t object,const char* expected){
        std::string actual,cause;const bool readable=object&&name(m,object+0x1b0,actual,cause);
        const bool matched=readable&&actual==expected;
        fields[label]={{"readable",readable},{"local_object",object},{"offset",0x1b0},
            {"value",readable?json(actual):json(nullptr)},{"expected",expected},{"matched",matched}};
        if(!readable)fields[label]["cause"]=object?cause:"Required owner is absent";
        if(!matched)mismatches.push_back(label);
    };
    observe("view_address_point",s.view,0,ap,image+0x171a800);
    observe("view_parent",s.view,0x278,parent,s.window);
    observe("view_window",s.view,0x280,owner,s.window);
    observe("widget_pointer",s.view,0x320,s.widget,uintptr_t(0),true);
    observe("value_pointer",s.view,0x348,s.value,uintptr_t(0),true);
    observe("widget_address_point",s.widget,0,ap,image+0x17241f0);
    observe("widget_parent",s.widget,0x278,parent,s.view);
    observe("widget_window",s.widget,0x280,owner,s.window);
    observe_name("widget_name",s.widget,"range");
    observe("widget_multiselect",s.widget,0x311,multi,uint8_t(0));
    observe("value_address_point",s.value,0,ap,image+0x1749570);
    observe("value_bound_range",s.value,0x38,parent,tool.behaviour+0x358);
    if(!mismatches.empty())return fail(e,"Elite level binding fields differ: "+json(mismatches).dump()+"; inspect levels.binding_fields");
    if(!field(m,s.widget,0x320,s.rows,e)||!field(m,s.widget,0x328,s.count,e)||!field(m,s.widget,0x32c,s.capacity,e)||
       !field(m,s.widget,0x348,s.selected,e)||!field(m,s.widget,0x350,s.valid,e)||
       !field(m,s.widget,0x354,bounds,e)||!field(m,tool.behaviour,0x358,selected,e)||
       !field(m,tool.behaviour,0x360,valid,e)||!field(m,region.region,0x838,bounds,e))return false;
    s.minimum=bounds[0];s.maximum=bounds[1];
    if(s.count<1||s.count>256||s.capacity<s.count||s.capacity>256||!s.rows||s.valid>1||valid>1)
        return fail(e,"Elite level rows/range bounds differ");
    uintptr_t value_ap=0;std::array<int32_t,2> widget_bounds{};uint8_t bounds_valid=2;
    if(!field(m,s.value,0,value_ap,e)||value_ap!=image+0x1749570||
       !field(m,s.widget,0x354,widget_bounds,e)||!field(m,s.widget,0x35c,bounds_valid,e)||
       bounds_valid!=1||widget_bounds!=bounds)return fail(e,e.empty()?"Elite bound range value or widget bounds differ from Region":e);
    json rows=json::array();std::set<uintptr_t> unique;
    for(int32_t i=0;i<s.count;++i){uintptr_t button=0;uint8_t disabled=2,visible=2,clickable=2;
        if(!field(m,s.rows,size_t(i)*8,button,e)||!button||!unique.insert(button).second||!field(m,button,0,ap,e)||ap!=image+0x16e3ec0||
           !field(m,button,0x278,parent,e)||parent!=s.widget||!field(m,button,0x280,owner,e)||owner!=s.window||
           !field(m,button,0x69,visible,e)||!field(m,button,0x46d,disabled,e)||!field(m,button,0x174,clickable,e)||
           visible>1||disabled>1||clickable>1||!name(m,button+0x1b0,key,e))return fail(e,e.empty()?"Elite actual level row class/owner/flags differ":e);
        rows.push_back({{"level",i+1},{"enabled",visible==1&&!disabled&&clickable==1&&i+1>=s.minimum&&i+1<=s.maximum},
            {"visible",visible==1},{"disabled",disabled==1},{"clickable",clickable==1},{"id",key},{"local_button",button}});
    }
    const bool ready=valid==1&&s.valid==1&&selected==s.selected&&selected[0]>=s.minimum&&
        selected[0]<=s.maximum&&selected[0]==selected[1];
    s.output.update({{"available",true},{"ready",ready},{"selection",{{"minimum",selected[0]},{"maximum",selected[1]},{"valid",valid==1}}},
        {"widget_selection",{{"minimum",s.selected[0]},{"maximum",s.selected[1]},{"valid",s.valid==1}}},
        {"region_range",{{"minimum",s.minimum},{"maximum",s.maximum}}},{"rows",rows},
        {"local_window",s.window},{"local_view",s.view},{"local_widget",s.widget},{"local_value",s.value}});
    s.output.erase("cause");
    return true;
}
bool level_state(Memory& m,uintptr_t image,const host::WorldState& world,const q::World& region,
                 const ToolPin& tool,LevelState& s,std::string& e) {
    const bool okay=level_state_read(m,image,world,region,tool,s,e);
    if(!okay){s.output["available"]=false;s.output["ready"]=false;s.output["cause"]=e;}
    return okay;
}
uint32_t prepare_region_call(uintptr_t image,uintptr_t manager,uintptr_t region)noexcept {
    __try{reinterpret_cast<void(*)(void*,void*)>(image+0x482cb0)(reinterpret_cast<void*>(manager),reinterpret_cast<void*>(region));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t update_call(uintptr_t image,uintptr_t behaviour)noexcept {
    __try{reinterpret_cast<void(*)(void*,float)>(image+0x5bfa60)(reinterpret_cast<void*>(behaviour),0.0f);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t level_command(uintptr_t image,uintptr_t widget,const char* command)noexcept {
    struct String{uintptr_t data=0;size_t size=0;unsigned char storage[16]{};};
    String value;bool built=false;uint32_t code=0;
    __try{reinterpret_cast<void(*)(void*,const char*)>(image+0x49e090)(&value,command);built=true;
        reinterpret_cast<void(*)(void*,const void*)>(image+0x770300)(reinterpret_cast<void*>(widget),&value);}
    __except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(built){__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(&value);}__except(EXCEPTION_EXECUTE_HANDLER){if(!code)code=GetExceptionCode();}}
    return code;
}
