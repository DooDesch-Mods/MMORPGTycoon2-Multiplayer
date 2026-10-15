#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace mt2mp::game::tutorial_network_tab {
struct Pin {
    uintptr_t bar=0,tab=0,content=0,cursor=0,manager=0,request=0;
    int32_t network=-1,view=-1;
    uint8_t active=0;
    bool operator==(const Pin&)const=default;
};
inline bool fail(std::string& e,const std::string& s){e=s;return false;}
template<class M,class T>bool at(M& m,uintptr_t p,size_t offset,T& out,std::string& e){
    if(!p||p>UINTPTR_MAX-offset)return fail(e,"Network tab address overflow");
    return m.copy(p+offset,&out,sizeof(out),e);
}
template<class M>bool text(M& m,uintptr_t p,std::string& out,std::string& e){
    uintptr_t data=0;uint64_t size=0;if(!at(m,p,0,data,e)||!at(m,p,8,size,e))return false;
    if(size>128||!data)return fail(e,"Network tab string span invalid");
    std::array<char,129> value{};if(!m.copy(data,value.data(),size_t(size)+1,e))return false;
    if(value[size]||std::memchr(value.data(),0,size_t(size)))return fail(e,"Network tab string terminator differs");
    out.assign(value.data(),size_t(size));return true;
}
template<class M>bool typed(M& m,uintptr_t p,uintptr_t ap,std::string& e){uintptr_t value=0;return at(m,p,0,value,e)&&(value==ap||fail(e,"Network tab native class differs"));}
template<class M>bool array(M& m,uintptr_t p,size_t offset,size_t bound,std::vector<uintptr_t>& out,std::string& e){
    uintptr_t data=0;int32_t count=0,capacity=0;
    if(!at(m,p,offset+8,data,e)||!at(m,p,offset+16,count,e)||!at(m,p,offset+20,capacity,e))return false;
    if(count<0||size_t(count)>bound||capacity<count||capacity>4096||(count&&!data)||data>UINTPTR_MAX-size_t(count)*8)return fail(e,"Network tab array bound differs");
    out.resize(size_t(count));return out.empty()||m.copy(data,out.data(),out.size()*8,e);
}
template<class M>bool inspect(M& m,uintptr_t image,uintptr_t world,Pin& p,std::string& e){
    p={};uintptr_t global=0;std::vector<uintptr_t> registry,tabs,children,contents;
    // The manager is an inline World member, not an independently trusted singleton.
    if(world>UINTPTR_MAX-0x180)return fail(e,"Network tab World owner overflow");p.manager=world+0x180;
    if(!at(m,image,0x13df650,global,e)||global!=p.manager)return fail(e,e.empty()?"Network tab WorldView owner differs":e);
    for(int32_t i=0;i<5;++i){std::string name;if(!text(m,image+0x1a36520+uintptr_t(i)*32,name,e))return false;
        if(name=="Network"){if(p.network!=-1)return fail(e,"Network view name duplicated");p.network=i;}}
    if(p.network<0)return fail(e,"Network view absent from native enum names");
    if(!at(m,p.manager,0x78,p.view,e)||p.view<0||p.view>4)return fail(e,e.empty()?"Network current view outside native enum":e);
    if(!at(m,image,0x1a3aec8,p.bar,e)||!typed(m,p.bar,image+0x16dcec0,e))return false;
    // The global ActionBar registry uses a vsArray pointer header at +0x1a3af10.
    if(!array(m,image,0x1a3af10,64,registry,e))return false;
    size_t matches=0;for(auto q:registry)if(q==p.bar)++matches;
    if(matches!=1)return fail(e,"Network current ActionBar registry membership differs");
    uint8_t shown=0,target=0,locked=0;uintptr_t logic=0;std::string bar_name;
    if(!at(m,p.bar,0x69,shown,e)||!at(m,p.bar,0x45f,target,e)||!at(m,p.bar,0x377,locked,e)||!at(m,p.bar,0x318,logic,e)||!text(m,p.bar+0x1b0,bar_name,e))return false;
    if(shown!=1||target!=1||locked||logic)return fail(e,"Network ActionBar not visible/unlocked/original");
    if(bar_name!="Edit")return fail(e,"Network fixed tutorial requires Edit ActionBar");
    if(!array(m,p.bar,0x518,64,tabs,e)||!array(m,p.bar,0x2f8,256,children,e)||!array(m,p.bar,0x4e8,64,contents,e))return false;
    for(auto tab:tabs){if(!typed(m,tab,image+0x16fe380,e))return false;std::string name;if(!text(m,tab+0x2c8,name,e))return false;
        if(name=="Network"){if(p.tab)return fail(e,"Network tab duplicated");p.tab=tab;}}
    if(!p.tab)return fail(e,"Network tab absent from actual ActionBar");
    matches=0;for(auto q:children)if(q==p.tab)++matches;
    if(matches!=1)return fail(e,"Network tab not a unique native ActionBar child");
    uintptr_t parent=0,owner=0;uint8_t visible=0,clickable=0;std::string command;
    if(!at(m,p.tab,0x278,parent,e)||!at(m,p.tab,0x280,owner,e)||!at(m,p.tab,0x69,visible,e)||!at(m,p.tab,0x174,clickable,e)||!at(m,p.tab,0x360,p.active,e)||!text(m,p.tab+0x308,command,e))return false;
    if(parent!=p.bar||owner!=p.bar)return fail(e,"Network tab parent/window owner differs");
    // These are the exact visibility and input gates in original Pane::FocusForClick.
    if(visible!=1||clickable!=1||p.active>1)return fail(e,"Network tab native visible/clickable/active flag differs");
    if(command!="ActionBar Edit SelectContent \"Network\"")return fail(e,"Network tab authored command differs");
    for(auto content:contents){if(!typed(m,content,image+0x171ccf0,e))return false;std::string name;if(!text(m,content+0x1b0,name,e))return false;
        if(name=="Network"){if(p.content)return fail(e,"Network content duplicated");p.content=content;}}
    if(!p.content)return fail(e,"Network content absent from actual ActionBar");
    int32_t mode=-1;if(!at(m,p.content,0x348,mode,e)||mode!=p.network)return fail(e,e.empty()?"Network content native view mode differs":e);
    if(!at(m,image,0x13df9d0,p.cursor,e)||!typed(m,p.cursor,image+0x1786dd0,e))return false;
    if(!at(m,p.content,0x368,p.request,e))return false;
    return true;
}
template<class M>bool selected(M& m,const Pin& p,std::string& e){
    uintptr_t content=0,parent=0,owner=0;uint8_t visible=0;
    if(!p.active||p.view!=p.network||!p.request)return fail(e,"Network tab active/view/request readback differs");
    if(!at(m,p.bar,0x598,content,e)||!at(m,p.content,0x278,parent,e)||!at(m,p.content,0x280,owner,e)||!at(m,p.content,0x69,visible,e))return false;
    if(content!=p.content||parent!=p.bar||owner!=p.bar||visible!=1)return fail(e,"Network selected content owner/visible differs");
    int32_t requested=-1;if(!at(m,p.request,0,requested,e)||requested!=p.network)return fail(e,e.empty()?"Network original view request differs":e);
    std::vector<uintptr_t> requests;if(!array(m,p.manager,size_t(p.network)*24,256,requests,e))return false;
    size_t count=0;for(auto q:requests)if(q==p.request)++count;
    return count==1||fail(e,"Network request not uniquely owned by WorldViewManager");
}
struct Receipt{bool attempted=false,returned=false,confirmed=false,unknown=false;};
template<class Calls>bool activate(Calls& calls,Receipt& r,std::string& e){
    r={};Pin before,again;
    if(!calls.pin(before,e)||!calls.pin(again,e))return false;
    if(before!=again)return fail(e,"Network tab binding changed before Press");
    if(before.active||before.view==before.network)return fail(e,"Network tab already selected; no Press replay");
    if(!calls.reserve(e))return false;
    r.attempted=true;
    try{
        if(!calls.press(before,e)){r.unknown=true;return false;}r.returned=true;
        Pin after;if(!calls.pin(after,e)){r.unknown=true;return false;}
        if(before.bar!=after.bar||before.tab!=after.tab||before.content!=after.content||before.cursor!=after.cursor||before.manager!=after.manager||before.network!=after.network){r.unknown=true;return fail(e,"Network tab owner changed after Press");}
        if(!calls.selected(after,e)||!calls.unchanged(e)){r.unknown=true;return false;}
        r.confirmed=true;return true;
    }catch(...){r.unknown=true;throw;}
}
}
