#pragma once
#include "tutorial_network_tab_reader.h"
#include "tutorial_network_tab_code.h"
namespace mt2mp::game::buildings_tab {
using tutorial_network_tab::at;
using tutorial_network_tab::array;
using tutorial_network_tab::typed;
using tutorial_network_tab::text;
using tutorial_network_tab::fail;
struct Pin {
    uintptr_t bar=0,tab=0,content=0,cursor=0,manager=0,request=0;
    int32_t edit=-1,view=-1,edit_kind=-1;
    uint8_t active=0;
    bool operator==(const Pin&)const=default;
};
inline bool same_owner(const Pin& a,const Pin& b){
    return a.bar==b.bar&&a.tab==b.tab&&a.content==b.content&&a.cursor==b.cursor&&
        a.manager==b.manager&&a.edit==b.edit&&a.edit_kind==b.edit_kind;
}
template<class M>bool verify(M& m,uintptr_t image,std::string& e){
    std::array<unsigned char,tutorial_network_tab::press_code.size()> actual{};
    return m.copy(image+0x57c950,actual.data(),actual.size(),e)&&
        (actual==tutorial_network_tab::press_code||fail(e,"Buildings original Tab::Press body differs"));
}
template<class M>bool inspect(M& m,uintptr_t image,uintptr_t world,Pin& p,std::string& e){
    p={};uintptr_t global=0;std::vector<uintptr_t> registry,tabs,children,contents;
    // The manager is an inline World member, not an independently trusted singleton.
    if(world>UINTPTR_MAX-0x180)return fail(e,"Buildings tab World owner overflow");p.manager=world+0x180;
    if(!at(m,image,0x13df650,global,e)||global!=p.manager)return fail(e,e.empty()?"Buildings tab WorldView owner differs":e);
    for(int32_t i=0;i<5;++i){std::string name;if(!text(m,image+0x1a36520+uintptr_t(i)*32,name,e))return false;
        if(name=="Edit"){if(p.edit!=-1)return fail(e,"Buildings view name duplicated");p.edit=i;}}
    if(p.edit<0)return fail(e,"Buildings view absent from native enum names");
    if(!at(m,p.manager,0x78,p.view,e)||p.view<0||p.view>4)return fail(e,e.empty()?"Buildings current view outside native enum":e);
    if(!at(m,image,0x1a3aec8,p.bar,e)||!typed(m,p.bar,image+0x16dcec0,e))return false;
    // The global ActionBar registry uses a vsArray pointer header at +0x1a3af10.
    if(!array(m,image,0x1a3af10,64,registry,e))return false;
    size_t matches=0;for(auto q:registry)if(q==p.bar)++matches;
    if(matches!=1)return fail(e,"Buildings current ActionBar registry membership differs");
    uint8_t shown=0,target=0,locked=0;uintptr_t logic=0;std::string bar_name;
    if(!at(m,p.bar,0x69,shown,e)||!at(m,p.bar,0x45f,target,e)||!at(m,p.bar,0x377,locked,e)||!at(m,p.bar,0x318,logic,e)||!text(m,p.bar+0x1b0,bar_name,e))return false;
    if(shown!=1||target!=1||locked||logic)return fail(e,"Buildings ActionBar not visible/unlocked/original");
    if(bar_name!="Edit")return fail(e,"Buildings preparation requires Edit ActionBar");
    if(!array(m,p.bar,0x518,64,tabs,e)||!array(m,p.bar,0x2f8,256,children,e)||!array(m,p.bar,0x4e8,64,contents,e))return false;
    for(auto tab:tabs){if(!typed(m,tab,image+0x16fe380,e))return false;std::string name;if(!text(m,tab+0x2c8,name,e))return false;
        if(name=="Buildings"){if(p.tab)return fail(e,"Buildings tab duplicated");p.tab=tab;}}
    if(!p.tab)return fail(e,"Buildings tab absent from actual ActionBar");
    matches=0;for(auto q:children)if(q==p.tab)++matches;
    if(matches!=1)return fail(e,"Buildings tab not a unique native ActionBar child");
    uintptr_t parent=0,owner=0;uint8_t visible=0,clickable=0;std::string command;
    if(!at(m,p.tab,0x278,parent,e)||!at(m,p.tab,0x280,owner,e)||!at(m,p.tab,0x69,visible,e)||!at(m,p.tab,0x174,clickable,e)||!at(m,p.tab,0x360,p.active,e)||!text(m,p.tab+0x308,command,e))return false;
    if(parent!=p.bar||owner!=p.bar)return fail(e,"Buildings tab parent/window owner differs");
    // These are the exact visibility and input gates in original Pane::FocusForClick.
    if(visible!=1||clickable!=1||p.active>1)return fail(e,"Buildings tab native visible/clickable/active flag differs");
    if(command!="ActionBar Edit SelectContent \"Buildings\"")return fail(e,"Buildings tab authored command differs");
    for(auto content:contents){if(!typed(m,content,image+0x171ccf0,e))return false;std::string name;if(!text(m,content+0x1b0,name,e))return false;
        if(name=="Buildings"){if(p.content)return fail(e,"Buildings content duplicated");p.content=content;}}
    if(!p.content)return fail(e,"Buildings content absent from actual ActionBar");
    int32_t mode=-1;if(!at(m,p.content,0x348,mode,e)||mode!=p.edit)return fail(e,e.empty()?"Buildings content native view mode differs":e);
    if(!at(m,image,0x13df9d0,p.cursor,e)||!typed(m,p.cursor,image+0x1786dd0,e))return false;
    if(!at(m,p.content,0x368,p.request,e))return false;
    if(!at(m,p.content,0x34c,p.edit_kind,e)||p.edit_kind<0)return fail(e,"Buildings content edit subtype differs");
    return true;
}
struct Status {bool ready=false;std::string cause;};
template<class M>bool status(M& m,const Pin& p,Status& out,std::string& e){
    out={};uintptr_t content=0,parent=0,owner=0;uint8_t visible=0;int32_t edit_kind=-1;
    if(!at(m,p.bar,0x598,content,e)||!at(m,p.content,0x278,parent,e)||
       !at(m,p.content,0x280,owner,e)||!at(m,p.content,0x69,visible,e)||
       !at(m,p.manager,0x7c,edit_kind,e))return false;
    if((parent&&parent!=p.bar)||(owner&&owner!=p.bar))return fail(e,"Buildings content foreign parent/window owner");
    if(visible>1)return fail(e,"Buildings content visibility outside bool");
    if(p.request){
        int32_t requested=-1;if(!at(m,p.request,0,requested,e))return false;
        if(requested!=p.edit)return fail(e,"Buildings original view request mode differs");
        std::vector<uintptr_t> requests;if(!array(m,p.manager,size_t(p.edit)*24,256,requests,e))return false;
        size_t count=0;for(auto q:requests)if(q==p.request)++count;
        if(count!=1)return fail(e,"Buildings request not uniquely owned by WorldViewManager");
    }
    if(!p.active)out.cause="Buildings tab not active";
    else if(content!=p.content)out.cause="Buildings content not current";
    else if(!parent||!owner)out.cause="Buildings content layout owner pending";
    else if(!visible)out.cause="Buildings content not visible";
    else if(!p.request)out.cause="Buildings original view request pending";
    else if(p.view!=p.edit)out.cause="Buildings Edit view pending actual="+std::to_string(p.view)+" required="+std::to_string(p.edit);
    else if(edit_kind!=p.edit_kind)out.cause="Buildings edit subtype pending actual="+std::to_string(edit_kind)+" required="+std::to_string(p.edit_kind);
    else out.ready=true;
    return true;
}
// Observation failure after original input stays Unknown. A valid but unsettled
// native view is a read-only wait, not permission to execute another Press.
struct Receipt {bool attempted=false,returned=false,confirmed=false,unknown=false;Status state;};
template<class Calls>bool prepare(Calls& calls,Receipt& out,std::string& e){
    out={};Pin before,again;
    if(!calls.pin(before,e)||!calls.pin(again,e))return false;
    if(before!=again)return fail(e,"Buildings tab binding changed before Press");
    if(before.active)return fail(e,"Buildings tab already active; no Press replay");
    if(!calls.reserve(before,e))return false;
    out.attempted=true;
    try{
        if(!calls.press(before,e)){out.unknown=true;return false;}out.returned=true;
        Pin after;if(!calls.pin(after,e)){out.unknown=true;return false;}
        if(!same_owner(before,after)){out.unknown=true;return fail(e,"Buildings tab owner changed after Press");}
        if(!calls.status(after,out.state,e)||!calls.unchanged(e)){out.unknown=true;return false;}
        Pin final;if(!calls.pin(final,e)){out.unknown=true;return false;}
        if(!same_owner(after,final)){out.unknown=true;return fail(e,"Buildings tab owner changed during full readback");}
        out.confirmed=true;return true;
    }catch(...){out.unknown=true;throw;}
}
}
