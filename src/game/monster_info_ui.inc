namespace {
bool info_window(Memory& m,const a::ui_capture::LocalBinding& local,a::ui_diagnostic::Window& window,json& receipt,std::string& e){
    window={};receipt=json::object();uintptr_t ap=0,focused=0,command=0,proxy=0,target=0;int32_t count=0;uint8_t visible=0;
    if(!field(m,local.manager,0x30,count,e)||!field(m,local.manager,0x80,focused,e))return false;
    receipt["selected_count"]=count;receipt["focused_matches"]=focused==local.object;
    if(count!=1||focused!=local.object)return fail(e,"Monster original single selection or focus differs");
    if(!field(m,local.manager,0xb8,window.handler,e)||!field(m,window.handler,0,ap,e))return false;
    receipt["handler_matches"]=ap==state.image+0x1731660;
    if(ap!=state.image+0x1731660)return fail(e,"Monster original InfoWindow handler class differs");
    if(!field(m,window.handler,0x10,window.window,e)||!field(m,window.window,0,ap,e)||
       !field(m,window.window,0x45f,visible,e))return false;
    receipt["window_matches"]=ap==state.image+0x1724f40;receipt["visible"]=visible;
    if(ap!=state.image+0x1724f40||visible!=1)return fail(e,"Monster original visible InfoWindow class differs");
    if(!field(m,ap,0x168,command,e)||!field(m,window.window,0x570,window.target,e)||
       !field(m,window.window,0x578,window.proxy,e)||!field(m,local.object,0x28,proxy,e)||
       !field(m,window.proxy,0,target,e))return false;
    receipt["command_matches"]=command==state.image+0x7490c0;receipt["target_matches"]=window.target==local.object;
    receipt["weak_matches"]=proxy&&proxy==window.proxy&&target==local.object+0x20;
    return (command==state.image+0x7490c0&&window.target==local.object&&proxy&&proxy==window.proxy&&target==local.object+0x20)||
        fail(e,"Monster original InfoWindow Command or full weak target differs");
}
bool info_default_tool(Memory& m,json& receipt,std::string& e){
    receipt=json::object();uintptr_t cursor=0,ap=0,current=0,desired=0,slot=0;uint8_t keeps=0;
    if(!field(m,state.image,0x13df9d0,cursor,e)||!field(m,cursor,0,ap,e)||ap!=state.image+0x1786dd0||
       !field(m,cursor,0x190,current,e)||!field(m,cursor,0x198,desired,e)||!field(m,desired,0,ap,e)||ap!=state.image+0x17391a0||
       !field(m,desired,0x120,keeps,e)||keeps!=1)return fail(e,e.empty()?"Monster InfoWindow original default Cursor differs":e);
    receipt["switch_attempted"]=false;receipt["default_already_active"]=current==desired;
    if(current!=desired){
        if(!field(m,current,0,ap,e))return false;
        if(ap==state.image+0x172fa80){
            receipt["source_tool"]="monster_zone";
            const auto idle=[&](){const auto controller=typed_action_runtime::controller_status(),zone=zone_action_runtime::status();
                return state.ready&&GetCurrentThreadId()==state.thread&&!state.pending&&!state.fault&&
                    controller.value("started",false)&&!controller.value("locked",true)&&!controller.value("pending",true)&&
                    zone.value("enabled",false)&&!zone_action_runtime::pending_ui()&&!zone.value("capture_active",true)&&!zone.value("capture_fault",true);};
            if(!idle())return fail(e,"MonsterZone Default switch requires idle Controller and no pending/faulted native capture");
            uintptr_t brush=0,owned_cursor=0,region=0;int32_t saved_region=-1;
            if(!d::tool(m,state.image,brush,owned_cursor,e)||brush!=current||owned_cursor!=cursor||
               !field(m,ap,0xb0,slot,e)||slot!=state.image+0x5effd0||
               !field(m,current,0x170,region,e)||!region||!field(m,region,0x834,saved_region,e))
                return fail(e,e.empty()?"MonsterZone Default switch exact inventory/Deactivate/Region differs":e);
            const auto cleanup=[&](json& out)->bool{
                int32_t painting=0,mode=0,alternate=0;uint16_t flags=0;uintptr_t graph=0,shape_a=0,shape_b=0;
                int32_t sections_a=0,sections_b=0;uint8_t paint_active=0,paint_secondary=0,draw_a=0,draw_b=0,cursor_flag=0;
                if(!field(m,current,0x6fc,painting,e)||!field(m,current,0x740,graph,e)||!field(m,current,0x718,flags,e)||
                   !field(m,current,0x730,mode,e)||!field(m,current,0x738,alternate,e)||
                   !field(m,current,0x5b8,paint_active,e)||!field(m,current,0x5b9,paint_secondary,e)||
                   !field(m,current,0x330,sections_a,e)||!field(m,current,0x3a0,sections_b,e)||
                   !field(m,current,0x4a0,shape_a,e)||!field(m,current,0x4a8,shape_b,e)||!shape_a||!shape_b||
                   !field(m,shape_a,0x51,draw_a,e)||!field(m,shape_b,0x51,draw_b,e)||!field(m,cursor,0x175,cursor_flag,e))return false;
                out={{"painting",painting},{"graph_absent",!graph},{"flags",flags},{"mode",mode},{"alternate",alternate},
                    {"paint_active",paint_active},{"paint_secondary",paint_secondary},{"sections",{sections_a,sections_b}},
                    {"draw_visible",{draw_a,draw_b}},{"cursor_flag",cursor_flag},{"windows",json::array()}};
                for(size_t offset:{size_t(0x5d8),size_t(0x5e0)}){uintptr_t window=0;uint8_t visible=0;
                    if(!field(m,current,offset,window,e)||(window&&!field(m,window,0x45f,visible,e)))return false;
                    out["windows"].push_back({{"offset",offset},{"pointer",window},{"visible",visible}});}
                return true;
            };
            json before_cleanup,after_cleanup;
            if(!cleanup(before_cleanup))return false;receipt["zone_before"]=before_cleanup;
            if(before_cleanup.at("painting")!=0||!before_cleanup.at("graph_absent").get<bool>())
                return fail(e,"MonsterZone Default switch requires painting=0 and null private Graph");
            const auto snapshot=net::current_snapshot_id();a::Binding binding;Pin before_pin;d::Sample before;a::MonsterState before_value;
            if(!typed_action_runtime::network_binding(snapshot,binding,e)||!pin(m,binding,snapshot,saved_region,before_pin,e)||
               before_pin.region.region!=region||!effect(m,before_pin,before,before_value,e))
                return fail(e,e.empty()?"MonsterZone Default switch current bound Region differs":e);
            receipt["before"]=d::sample_json(before);receipt["content_before"]=before_value.roster.content_sha256;
            if(before.undo.active_graph||(!before.money.clock.paused&&before.money.clock.time_scale!=0))
                return fail(e,"MonsterZone Default switch requires idle Undo and paused actual Clock");
            receipt["switch_attempted"]=true;const auto seh=action_runtime::select_default_tool(state.image,cursor);
            receipt["switch_returned"]=seh==0;
            if(seh)return fail(e,"MonsterZone original Cursor SetBehaviour SEH="+std::to_string(seh));
            if(!cleanup(after_cleanup))return false;receipt["zone_after"]=after_cleanup;
            bool clean=after_cleanup.at("painting")==0&&after_cleanup.at("graph_absent").get<bool>()&&
                after_cleanup.at("flags")==0&&after_cleanup.at("mode")==0&&after_cleanup.at("alternate")==0&&
                after_cleanup.at("paint_active")==0&&after_cleanup.at("paint_secondary")==0&&
                after_cleanup.at("sections")==json::array({0,0})&&after_cleanup.at("draw_visible")==json::array({0,0})&&after_cleanup.at("cursor_flag")==0;
            for(size_t i=0;i<2;++i)clean=clean&&after_cleanup["windows"][i]["pointer"]==before_cleanup["windows"][i]["pointer"]&&
                after_cleanup["windows"][i]["visible"]==0;
            receipt["cleanup_confirmed"]=clean;
            Pin after_pin;d::Sample after;a::MonsterState after_value;
            uintptr_t after_brush=0,after_cursor=0,after_region=0;
            if(!idle()||!d::tool(m,state.image,after_brush,after_cursor,e)||after_brush!=brush||after_cursor!=cursor||
               !field(m,brush,0x170,after_region,e)||after_region!=region||
               !pin(m,binding,snapshot,saved_region,after_pin,e)||!same_world(before_pin.world,after_pin.world)||
               after_pin.region.region!=region||!effect(m,after_pin,after,after_value,e))
                return fail(e,e.empty()?"MonsterZone Default switch current World/Region or idle capture changed":e);
            receipt["after"]=d::sample_json(after);receipt["content_after"]=after_value.roster.content_sha256;
            receipt["global_random_unchanged"]=before.state.global_random==after.state.global_random;
            receipt["region_random_unchanged"]=before.state.region_random==after.state.region_random;
            const bool unchanged=no_gameplay_change(before,after)&&before_value.roster.content_sha256==after_value.roster.content_sha256;
            receipt["gameplay_unchanged"]=unchanged;
            if(!clean||!unchanged)return fail(e,!clean?"MonsterZone original Deactivate cleanup differs":"MonsterZone Default switch changed roster/name/pose/Region RNG/Undo/Release/ledger");
        }else{
        ToolPin tool;json inventory;
        if(!tools(m,state.image,tool,inventory,e)||tool.behaviour!=current||!field(m,current,0,ap,e)||ap!=state.image+0x1739500||
           !field(m,ap,0xb0,slot,e)||slot!=state.image+0x5be5c0)return fail(e,e.empty()?"Monster InfoWindow active tool is not the exact original Elite inventory tool":e);
        receipt["switch_attempted"]=true;const auto seh=action_runtime::select_default_tool(state.image,cursor);
        receipt["switch_returned"]=seh==0;
        if(seh)return fail(e,"Monster InfoWindow original Cursor SetBehaviour SEH="+std::to_string(seh));
        uintptr_t preview=0,character=0;
        if(!field(m,current,0x338,preview,e)||!field(m,current,0x348,character,e))return false;
        receipt["preview_absent"]=!preview;receipt["character_absent"]=!character;
        if(preview||character)return fail(e,"Monster original Deactivate retained preview or Character");
        for(size_t offset:{size_t(0x148),size_t(0x150)}){uintptr_t window=0;uint8_t visible=0;
            if(!field(m,current,offset,window,e))return false;
            if(window&&(!field(m,window,0x45f,visible,e)||visible))return fail(e,e.empty()?"Monster original Deactivate retained a visible tool window":e);}
        }
    }
    if(!field(m,cursor,0x190,current,e))return false;receipt["default_confirmed"]=current==desired;
    return current==desired||fail(e,"Monster original Default tool readback differs");
}

bool info_code(Memory& m,std::string& e){
    for(const auto& c:monster_info_codes::values){std::string bytes(c.size,'\0'),sha;
        if(!m.copy(state.image+c.rva,bytes.data(),bytes.size(),e)||!quest_bridge::sha256(bytes,sha,e))return false;
        if(sha!=c.sha)return fail(e,"Monster InfoWindow native body differs RVA="+std::to_string(c.rva));}
    for(const auto& guard:move_ui_probe::code::guards){std::vector<unsigned char> bytes(guard.size);
        if(!m.copy(state.image+guard.rva,bytes.data(),bytes.size(),e))return false;
        if(std::memcmp(bytes.data(),guard.bytes,guard.size))return fail(e,"Monster original Move input body differs RVA="+std::to_string(guard.rva));}
    return true;
}
bool info_target(const std::string& birth,a::Binding& binding,Pin& p,d::Sample& sample,a::MonsterState& value,
                 a::ui_capture::LocalBinding& local,std::string& e){
    local={};const auto snapshot=net::current_snapshot_id();
    if(!state.ready||GetCurrentThreadId()!=state.thread||!monster_target_key::input(birth)||
       !typed_action_runtime::network_binding(snapshot,binding,e))return fail(e,e.empty()?"Monster InfoWindow current canonical target unavailable":e);
    Memory m;monster_zone_read::Monster row;int32_t saved=-1;
    if(!target_region(m,binding,snapshot,birth,saved,e)||!move_target(m,binding,snapshot,saved,birth,p,sample,row,e)||
       !effect(m,p,sample,value,e,row.uid))return false;
    local={p.world.selection,row.pointer,row.uid,binding.native_epoch,binding.native_generation,snapshot,monster_target_key::canonical(birth),false,false,false};
    return true;
}
}
