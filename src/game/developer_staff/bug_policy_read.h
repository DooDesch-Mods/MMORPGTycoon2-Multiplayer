#pragma once
#include "bug_policy.h"
#include "developer_staff/window.h"
#include <cstring>
#include <set>

namespace mt2mp::game::developer_staff::bug_policy {
namespace detail {
inline bool fail(std::string& e,const std::string& why){e=why;return false;}
template<class Copy,class T>bool at(Copy&& copy,uintptr_t owner,size_t offset,T& out,std::string& e){
    if(!owner||owner>UINTPTR_MAX-offset||owner+offset>UINTPTR_MAX-sizeof(T))return fail(e,"Bug-C policy native field address overflow");
    return copy(owner+offset,&out,sizeof(T),e);
}
template<class Copy>bool text(Copy&& copy,uintptr_t owner,size_t offset,std::string& out,std::string& e){
    uintptr_t data=0;uint64_t count=0;
    if(!at(copy,owner,offset,data,e)||!at(copy,owner,offset+8,count,e))return false;
    if(count>200||!data||data>UINTPTR_MAX-size_t(count)-1)return fail(e,"Bug-C policy native text bounds invalid");
    std::array<char,201> bytes{};
    if(!copy(data,bytes.data(),size_t(count)+1,e))return false;
    if(bytes[size_t(count)]||std::memchr(bytes.data(),0,size_t(count)))return fail(e,"Bug-C policy native text terminator differs");
    out.assign(bytes.data(),size_t(count));return true;
}
template<class Copy>bool type(Copy&& copy,uintptr_t owner,uintptr_t expected,std::string& e){
    uintptr_t actual=0;if(!at(copy,owner,0,actual,e))return false;
    return actual==expected||fail(e,"Bug-C policy actual native class differs");
}
}
template<class Copy>bool read_policy(Copy&& copy,const Capture& c,Snapshot& out,std::string& e){
    out={};out.capture=c;uintptr_t lounge=0,server=0;
    if(!detail::at(copy,c.image,0x13df290,lounge,e)||!detail::at(copy,c.image,0x13df430,server,e))return false;
    if(lounge!=c.local.lounge||!detail::type(copy,lounge,c.image+0x16de6c0,e)||!detail::type(copy,lounge+0x38,c.image+0x16de7a0,e)||
       !detail::type(copy,server,c.image+0x1702a40,e))return detail::fail(e,e.empty()?"Bug-C policy current Lounge/server owner differs":e);
    for(size_t i=0;i<6;++i){if(!detail::at(copy,lounge,0x50+i,out.policies[i],e))return false;
        if(out.policies[i]>1)return detail::fail(e,"Bug-C policy native bool outside0..1");}
    if(!detail::at(copy,server,0x18,out.server_current,e)||!detail::at(copy,server,0x1c,out.server_target,e))return false;
    return true;
}
template<class Copy>bool read_tabs(Copy&& copy,const Snapshot& s,Control& out,std::string& e){
    out={};const auto& c=s.capture;
    if(!read_window(copy,c,out.window,e)||!detail::at(copy,out.window,0x540,out.tabs,e)||
       !detail::type(copy,out.tabs,c.image+0x17015b0,e)||!detail::at(copy,out.tabs,0x3b8,out.view,e)||
       !detail::type(copy,out.view,c.image+0x16dfe90,e))return false;
    for(const auto pane:{out.tabs,out.view}){uintptr_t owner=0;
        if(!detail::at(copy,pane,0x280,owner,e))return false;
        if(owner==out.window)continue;
        uint8_t visible=0,pending=0;
        if(owner)return detail::fail(e,"Bug-C policy tabs have a foreign window owner");
        if(!detail::at(copy,out.window,0x45f,visible,e)||!detail::at(copy,out.window,0x460,pending,e))return false;
        if(visible!=0||pending!=1)return detail::fail(e,"Bug-C policy unbound tabs require hidden native Layout-pending window");
    }
    return true;
}
struct PageWitness {
    bool sampled=false;
    uint8_t visible=0;
    bool requested_policies=false,current_page_present=false,next_matches_current=false;
    int32_t transition=0;
};
inline nlohmann::json page_witness(const PageWitness& w){
    if(!w.sampled)return {{"sampled",false}};
    return {{"sampled",true},{"window_visible",w.visible},{"requested_policies",w.requested_policies},
        {"current_page_present",w.current_page_present},{"next_matches_current",w.next_matches_current},{"transition",w.transition}};
}
template<class Copy>bool read_control(Copy&& copy,const Snapshot& s,Scope scope,Control& out,bool pressed,std::string& e,PageWitness* witness=nullptr){
    if(witness)*witness={};
    if(!read_tabs(copy,s,out,e))return false;
    const auto& c=s.capture;uint8_t visible=0;uintptr_t next=0;int32_t transition=0;
    std::string requested,current;
    if(!detail::at(copy,out.window,0x45f,visible,e)||!detail::text(copy,out.view,0x3e8,requested,e)||
       !detail::at(copy,out.view,0x410,out.page,e)||!detail::at(copy,out.view,0x418,next,e)||!detail::at(copy,out.view,0x408,transition,e))return false;
    const PageWitness page{true,visible,requested=="{dev_tab_policies}",out.page!=0,next==out.page,transition};
    if(witness)*witness=page;
    // Original SwapToNextPane retains the target at +0x418 after copying it to +0x410.
    if(visible!=1||!page.requested_policies||!out.page||next!=out.page||transition)
        return detail::fail(e,"Bug-C policy original Policies page not settled/visible; visible="+std::to_string(visible)+
            " requested_policies="+std::to_string(page.requested_policies)+" current_page_present="+std::to_string(page.current_page_present)+
            " next_matches_current="+std::to_string(page.next_matches_current)+" transition="+std::to_string(transition));
    if(!detail::type(copy,out.page,c.image+0x1705ee0,e)||!detail::text(copy,out.page,0x1b0,current,e))return false;
    if(current!="{dev_tab_policies}")return detail::fail(e,"Bug-C policy active native page ID differs");
    std::vector<std::pair<uintptr_t,uintptr_t>> pending{{out.page,out.view}};std::set<uintptr_t> seen;
    while(!pending.empty()){
        const auto [pane,parent]=pending.back();pending.pop_back();
        if(!pane||!seen.insert(pane).second||seen.size()>512)return detail::fail(e,"Bug-C policy view child cycle/duplicate/bound exceeded");
        uintptr_t actual_parent=0,owner=0,ap=0;
        if(!detail::at(copy,pane,0x278,actual_parent,e)||!detail::at(copy,pane,0x280,owner,e)||!detail::at(copy,pane,0,ap,e))return false;
        if(actual_parent!=parent||owner!=out.window)return detail::fail(e,"Bug-C policy pane ancestry/window owner differs");
        if(ap==c.image+0x16d98c0){
            uintptr_t value=0;
            if(!detail::at(copy,pane,0x3c8,value,e))return false;
            if(!value)return detail::fail(e,"Bug-C policy checkbox lacks original property value binding");
            std::string name;if(!detail::text(copy,value,0x18,name,e))return false;
            if(name!=property(scope))continue;
            if(out.checkbox)return detail::fail(e,"Bug-C policy checkbox property binding is ambiguous");
            out.checkbox=pane;out.value=value;
        }else if(ap==c.image+0x1745a80||pane==out.page){
            uintptr_t data=0;int32_t count=0,capacity=0;
            if(!detail::at(copy,pane,0x2d0,data,e)||!detail::at(copy,pane,0x2d8,count,e)||!detail::at(copy,pane,0x2dc,capacity,e))return false;
            if(count<0||capacity<count||capacity>512||(count&&(!data||data>UINTPTR_MAX-size_t(count)*8)))return detail::fail(e,"Bug-C policy native view child bounds invalid");
            for(int32_t i=0;i<count;++i){uintptr_t child=0;if(!detail::at(copy,data,size_t(i)*8,child,e))return false;pending.emplace_back(child,pane);}
        }
        // Non-View panes, including checkbox labels, are opaque leaves.
    }
    if(!out.checkbox)return detail::fail(e,"Bug-C policy exact checkbox property is absent");
    uintptr_t bound_owner=0,direct=0,listener=0,down_sprite=0,toggled_sprite=0;
    uint8_t enabled=0,clickable=0,down=0,value=0,cached=0,editing=0,field=0;uint64_t command_size=0;int32_t option=0;
    if(!detail::at(copy,out.value,8,bound_owner,e)||!detail::at(copy,out.value,0x38,out.field,e)||
       !detail::at(copy,out.value,0x40,cached,e)||!detail::at(copy,out.value,0x10,editing,e)||
       !detail::at(copy,out.checkbox,0x3c0,direct,e)||!detail::at(copy,out.checkbox,0x2c8,listener,e)||
       !detail::at(copy,out.checkbox,0x328,down_sprite,e)||!detail::at(copy,out.checkbox,0x320,toggled_sprite,e)||
       !detail::at(copy,out.checkbox,0x174,clickable,e)||!detail::at(copy,out.checkbox,0x345,enabled,e)||
       !detail::at(copy,out.checkbox,0x344,down,e)||!detail::at(copy,out.checkbox,0x346,value,e)||
       !detail::at(copy,out.checkbox,0x350,command_size,e)||!detail::at(copy,out.checkbox,0x3b8,option,e))return false;
    if(bound_owner!=c.local.lounge+0x38||out.field!=c.local.lounge+0x50+index(scope)||direct||listener||!down_sprite||!toggled_sprite||
       option!=13||command_size||editing||enabled!=1||clickable!=1||down!=uint8_t(pressed)||value!=s.policies[index(scope)]||cached!=value)
        return detail::fail(e,"Bug-C policy checkbox owner/field/enabled/pressed/value/command differs");
    if(!copy(out.field,&field,1,e))return false;
    if(field!=value)return detail::fail(e,"Bug-C policy checkbox actual bound bool differs");
    return true;
}
}
