#pragma once

namespace npc_remove_selection {
template<class Read,class ActiveTool,class Switch>
bool prepare_info_selection_body(uintptr_t image,nlohmann::json& probe,std::string& error,
                                 Read&& read,ActiveTool&& active_tool,Switch&& switch_default) {
    probe=nlohmann::json::object();uintptr_t cursor=0,ap=0,current=0,desired=0,slot=0;
    uint8_t keeps_selection=0;
    if(!read(image,0x13df9d0,cursor,error)||!cursor||
       !read(cursor,0,ap,error)||ap!=image+0x1786dd0||
       !read(cursor,0x190,current,error)||!current||
       !read(cursor,0x198,desired,error)||!desired||
       !read(desired,0,ap,error)||ap!=image+0x17391a0||
       !read(desired,0x120,keeps_selection,error)||keeps_selection!=1) {
        if(error.empty())error="NPC Remove original cursor/default selection contract differs";
        return false;
    }
    probe["default_already_active"]=current==desired;
    probe["tool_switch_attempted"]=false;
    if(current!=desired){
        const auto old=current;
        if(!active_tool(old,error))return false;
        if(!read(old,0,ap,error)||ap!=image+0x172b450||
           !read(ap,0xb0,slot,error)||slot!=image+0x5c3240) {
            if(error.empty())error="NPC Remove original tool Deactivate slot differs";
            return false;
        }
        probe["tool_switch_attempted"]=true;
        const auto seh=switch_default(image,cursor);
        probe["tool_switch_returned"]=seh==0;
        if(seh){error="NPC original Cursor SetBehaviour SEH="+std::to_string(seh);return false;}
        uintptr_t preview=0,window=0;uint8_t visible=1;
        if(!read(cursor,0x190,current,error)||
           !read(old,0x358,preview,error)||
           !read(old,0x168,window,error))return false;
        if(!window){error="NPC original Deactivate ThemeWindow absent";return false;}
        if(!read(window,0x45f,visible,error))return false;
        probe["preview_absent"]=preview==0;probe["theme_visible"]=visible;
        if(preview||visible){error="NPC original Deactivate left preview or ThemeWindow visible";return false;}
    }
    probe["default_confirmed"]=current==desired;
    if(current!=desired){error="NPC original Cursor did not select Default behavior";return false;}
    error.clear();return true;
}
}
