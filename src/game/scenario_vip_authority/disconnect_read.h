#pragma once
#include "alert_probe_reader.h"
#include "../live_host_state/vip_transitions.h"

namespace mt2mp::game::scenario_vip_authority::disconnect_read {
namespace vip=live_host_state::vip;
template<class T>bool at(placement_ui_probe::Memory& memory,uintptr_t p,size_t offset,T& value,std::string& e){
    if(!p||p>UINTPTR_MAX-offset||p+offset>UINTPTR_MAX-sizeof value)return vip::fail(e,"VIP message read extent invalid");
    return memory.copy(p+offset,&value,sizeof value,e);
}
inline bool text(placement_ui_probe::Memory& memory,uintptr_t alert,std::string& out,std::string& e){
    uintptr_t data=0;uint64_t size=0;
    if(!at(memory,alert,0xb0,data,e)||!at(memory,alert,0xb8,size,e))return false;
    if(!size||size>live_host_state::max_logical_frame_payload||!data||data>UINTPTR_MAX-size_t(size))
        return vip::fail(e,"VIP message native text extent differs from Frame bound");
    out.resize(size_t(size));if(!memory.copy(data,out.data(),out.size(),e))return false;
    return (out.find('\0')==std::string::npos&&npc_census::utf8(out))||vip::fail(e,"VIP message native text is not safe UTF-8");
}
inline bool same_owner(const alert_probe::Graph& a,const alert_probe::Graph& b){
    return a.present&&b.present&&a.hud==b.hud&&a.manager==b.manager&&a.window==b.window&&
        a.window_ap==b.window_ap&&a.window_name==b.window_name&&a.list==b.list&&a.log==b.log&&a.ancestry==b.ancestry;
}
inline unsigned count(const alert_probe::Array& a,uintptr_t p){return unsigned(std::count(a.items.begin(),a.items.end(),p));}
inline bool inspect(placement_ui_probe::Memory& memory,uintptr_t image,const alert_probe::Graph& g,
                    uintptr_t alert,int expected_shown,const std::string* expected,uintptr_t& selector,std::string& message,std::string& e,int32_t expected_type=9){
    selector=0;message.clear();uintptr_t ap=0,d0=0;int32_t type=0;uint8_t shown=0;
    if(!g.present)return vip::fail(e,"VIP message current UI owner absent");
    if(!at(memory,alert,0,ap,e)||!at(memory,ap,8,d0,e)||!at(memory,alert,0x18,type,e)||
       !at(memory,alert,0x130,selector,e)||!at(memory,alert,0x138,shown,e))return false;
    if(ap!=image+0x1749770||d0!=image+0x234c70||(expected_type!=0&&expected_type!=9)||type!=expected_type||shown>1||shown!=expected_shown)
        return vip::fail(e,"VIP message Alert class/type/shown differs");
    if(shown){
        uintptr_t selected=0,list=0,parent=0,window=0,selector_ap=0;
        if(count(g.owned,alert)!=1||count(g.active,alert)!=1||!selector||count(g.selectors,selector)!=1||count(g.panes,selector)!=1)
            return vip::fail(e,"VIP message handoff lacks unique owned/active/selector membership");
        if(!at(memory,selector,0,selector_ap,e)||!at(memory,selector,0x330,selected,e)||!at(memory,selector,0x318,list,e)||
           !at(memory,selector,0x278,parent,e)||!at(memory,selector,0x280,window,e))return false;
        if(selector_ap!=image+0x1704fa0||selected!=alert||list!=g.list||parent!=g.list||window!=g.window)
            return vip::fail(e,"VIP message selector reciprocal ownership differs");
    }else if(selector||count(g.owned,alert)||count(g.active,alert))return vip::fail(e,"VIP unhanded message already belongs to a manager/UI owner");
    std::string second;if(!text(memory,alert,message,e)||!text(memory,alert,second,e))return false;
    if(message!=second||(expected&&message!=*expected))return vip::fail(e,"VIP message final bytes changed or LocString expanded differently");
    e.clear();return true;
}
// Original EndEvent creates its type9 message in the same Scenario Update that
// changes completed. Ownership difference alone is insufficient evidence.
inline bool ended(placement_ui_probe::Memory& memory,uintptr_t image,uintptr_t scenario,uint8_t before,
                  const alert_probe::Graph& old,const alert_probe::Graph& now,std::optional<std::string>& out,std::string& e){
    out.reset();uint8_t completed=0;uintptr_t ap=0;
    if(!at(memory,scenario,0,ap,e)||ap!=image+0x1730e90||!at(memory,scenario,0x38,completed,e))
        return vip::fail(e,e.empty()?"VIP EndEvent exact scenario owner differs":e.c_str());
    if(before>1||completed>1)return vip::fail(e,"VIP EndEvent completed flag is not boolean");
    if(before||!completed){e.clear();return true;}
    if(!same_owner(old,now))return vip::fail(e,"VIP EndEvent current UI owner changed across original Update");
    unsigned matches=0;
    for(auto alert:now.owned.items){
        if(count(old.owned,alert))continue;
        int32_t type=0;if(!at(memory,alert,0x18,type,e))return false;if(type!=9)continue;
        uintptr_t selector=0;std::string message;
        if(!inspect(memory,image,now,alert,1,nullptr,selector,message,e))return false;
        out=std::move(message);++matches;
    }
    if(matches!=1){out.reset();return vip::fail(e,"VIP EndEvent requires exactly one new original owned type9 message");}
    e.clear();return true;
}
}
