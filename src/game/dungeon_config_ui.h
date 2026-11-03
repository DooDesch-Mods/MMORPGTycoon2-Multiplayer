#pragma once
#include "dungeon_config_input.h"
#include <set>
namespace mt2mp::game::dungeon_config_ui {
struct Controls {int32_t value=0;bool operator==(const Controls&)const=default;};
template<class Read,class Text>bool validate(Read&& read,Text&& text,uintptr_t image,uintptr_t window,
    uintptr_t dungeon,uintptr_t spinner,uintptr_t accept,Controls& out,std::string& error){
    out={};const auto fail=[&](const char* why){if(error.empty())error=why;return false;};
    uintptr_t ap=0,owner=0,method=0;uint8_t shown=0;
    if(!window||!dungeon||!read(window,0,ap,error)||ap!=image+0x173bee0||
       !read(window,0x510,owner,error)||owner!=dungeon||!read(window,0x45f,shown,error)||shown!=1||
       !read(ap,0x168,method,error)||method!=image+0x6aea60||
       !read(ap,0x1d0,method,error)||method!=image+0x81ec20)return fail("Dungeon config original Window owner/visibility/callback differs");
    const auto visible=[&](uintptr_t pane){std::set<uintptr_t> seen;
        while(pane!=window){uintptr_t parent=0,actual_owner=0;uint8_t visible=0;
            if(!pane||!seen.insert(pane).second||seen.size()>64||!read(pane,0x69,visible,error)||visible!=1||
               !read(pane,0x280,actual_owner,error)||actual_owner!=window||!read(pane,0x278,parent,error))
                return fail("Dungeon config control ancestry is hidden, cyclic or foreign");
            pane=parent;
        }return true;
    };
    int32_t low=0,high=0,step=0;float scale=0;uint8_t integer=0;std::string route;
    if(!dungeon_config_input::spinner(read,image,spinner,error)||!visible(spinner)||
       !read(spinner,0x338,integer,error)||integer!=1||!read(spinner,0x438,scale,error)||scale!=1||
       !read(spinner,0x43c,low,error)||low!=1||!read(spinner,0x440,high,error)||high!=10||
       !read(spinner,0x444,step,error)||step!=1||!read(spinner,0x44c,out.value,error)||out.value<1||out.value>10||
       !text(spinner+0x378,route,error)||route!="MessageTo Window SetInstanceCount")
        return fail("Dungeon config named Spinner domain or original route differs");
    uint8_t disabled=1,clickable=0;
    if(!accept||!read(accept,0,ap,error)||ap!=image+0x16e3ec0||!visible(accept)||
       !read(accept,0x46d,disabled,error)||disabled||!read(accept,0x174,clickable,error)||clickable!=1||
       !text(accept+0x388,route,error)||route!="MessageTo Window Accept")
        return fail("Dungeon config actual Accept is disabled, hidden or foreign");
    return true;
}
struct Tab {uintptr_t page=0,view=0;std::string label;};
template<class Read,class Text>bool prepare(Read&& read,Text&& text,uintptr_t image,uintptr_t window,
    uintptr_t exterior,uintptr_t tabs,uintptr_t page,uintptr_t dungeon,Tab& out,std::string& error){
    out={};uintptr_t ap=0,owner=0,method=0;uint8_t shown=0;
    const auto fail=[&](){if(error.empty())error="Dungeon config original exterior Instances tab/owner differs";return false;};
    if(!window||!exterior||!dungeon||!tabs||!page||!read(window,0,ap,error)||ap!=image+0x172af10||
       !read(window,0x45f,shown,error)||shown!=1||!read(window,0x4f8,owner,error)||owner!=exterior||
       !read(exterior,0xa38,owner,error)||owner!=dungeon||!read(dungeon,0x6f8,owner,error)||owner!=exterior||
       !read(tabs,0,ap,error)||ap!=image+0x17015b0||!read(ap,0xf0,method,error)||method!=image+0x752d70||
       !read(tabs,0x69,shown,error)||shown!=1||!read(tabs,0x280,owner,error)||owner!=window||
       !read(tabs,0x3b8,out.view,error)||!read(out.view,0,ap,error)||ap!=image+0x16dfe90||
       !read(ap,0x168,method,error)||method!=image+0x7568e0||!read(out.view,0x278,owner,error)||owner!=tabs||
       !read(out.view,0x280,owner,error)||owner!=window||!text(page+0x1b0,out.label,error)||out.label!="{dungeoninfo_tab_instances}")return fail();
    out.page=page;return true;
}
template<class Read,class Text>bool offer(Read&& read,Text&& text,uintptr_t image,uintptr_t window,
    uintptr_t exterior,uintptr_t button,uintptr_t dungeon,Tab& tab,std::string& error){
    tab={};
    const auto fail=[&](const char* why){if(error.empty())error=why;return false;};
    uintptr_t ap=0,owner=0,parent=0,callback=0;uint8_t shown=0,disabled=1,clickable=0;std::string route;
    if(!window||!dungeon||!exterior||!button||!read(window,0,ap,error)||ap!=image+0x172af10||
       !read(ap,0x168,callback,error)||callback!=image+0x66d990||!read(window,0x45f,shown,error)||shown!=1||
       !read(window,0x4f8,owner,error)||owner!=exterior||!read(exterior,0xa38,owner,error)||owner!=dungeon||
       !read(dungeon,0x6f8,owner,error)||owner!=exterior||
       !read(button,0,ap,error)||ap!=image+0x16e3ec0||!read(button,0x278,tab.page,error)||
       !read(tab.page,0x278,tab.view,error)||!read(tab.view,0,ap,error)||ap!=image+0x16dfe90||
       !read(ap,0x168,callback,error)||callback!=image+0x7568e0||
       !text(tab.page+0x1b0,tab.label,error)||tab.label.empty()||tab.label.size()>128||
       !read(button,0x280,owner,error)||owner!=window||!read(button,0x46d,disabled,error)||disabled||
       !read(button,0x174,clickable,error)||clickable!=1||!text(button+0x388,route,error)||route!="MessageTo Window OpenInstancesConfig")
        return fail("Dungeon config original offered Config button or Dungeon owner differs");
    for(unsigned char character:tab.label)if(character<32||character=='"'||character=='\\')return fail("Dungeon config original tab label cannot form an exact native record");
    std::string requested;if(!read(tab.view,0x410,parent,error)||parent!=tab.page||!text(tab.view+0x3e8,requested,error)||requested!=tab.label)
        return fail("Dungeon config original Instances page has not become current");
    std::set<uintptr_t> seen;for(auto pane=button;pane!=window;pane=parent){
        if(!pane||!seen.insert(pane).second||seen.size()>64||!read(pane,0x69,shown,error)||shown!=1||
           !read(pane,0x280,owner,error)||owner!=window||!read(pane,0x278,parent,error))return fail("Dungeon config original offer is hidden, cyclic or foreign");
    }
    return true;
}
}
