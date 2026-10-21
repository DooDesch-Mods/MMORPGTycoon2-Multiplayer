#pragma once
#include "tutorial_network_tab_reader.h"
#include "tutorial_network_tab_code.h"
namespace mt2mp::game::network_tab {
using tutorial_network_tab::Pin;
using tutorial_network_tab::inspect;
using tutorial_network_tab::at;
using tutorial_network_tab::array;
using tutorial_network_tab::fail;
inline bool same_owner(const Pin& a,const Pin& b){
    return a.bar==b.bar&&a.tab==b.tab&&a.content==b.content&&a.cursor==b.cursor&&
        a.manager==b.manager&&a.network==b.network;
}
template<class M>bool verify(M& m,uintptr_t image,std::string& e){
    std::array<unsigned char,tutorial_network_tab::press_code.size()> actual{};
    return m.copy(image+0x57c950,actual.data(),actual.size(),e)&&
        (actual==tutorial_network_tab::press_code||fail(e,"Network original Tab::Press body differs"));
}
struct Status {bool ready=false;std::string cause;};
template<class M>bool status(M& m,const Pin& p,Status& out,std::string& e){
    out={};uintptr_t content=0,parent=0,owner=0;uint8_t visible=0;
    if(!at(m,p.bar,0x598,content,e)||!at(m,p.content,0x278,parent,e)||
       !at(m,p.content,0x280,owner,e)||!at(m,p.content,0x69,visible,e))return false;
    if((parent&&parent!=p.bar)||(owner&&owner!=p.bar))return fail(e,"Network content foreign parent/window owner");
    if(visible>1)return fail(e,"Network content visibility outside bool");
    if(p.request){
        int32_t requested=-1;if(!at(m,p.request,0,requested,e))return false;
        if(requested!=p.network)return fail(e,"Network original view request mode differs");
        std::vector<uintptr_t> requests;if(!array(m,p.manager,size_t(p.network)*24,256,requests,e))return false;
        size_t count=0;for(auto q:requests)if(q==p.request)++count;
        if(count!=1)return fail(e,"Network request not uniquely owned by WorldViewManager");
    }
    if(!p.active)out.cause="Network tab not active";
    else if(content!=p.content)out.cause="Network content not current";
    else if(!parent||!owner)out.cause="Network content layout owner pending";
    else if(!visible)out.cause="Network content not visible";
    else if(!p.request)out.cause="Network original view request pending";
    else if(p.view!=p.network)out.cause="Network view pending actual="+std::to_string(p.view)+" required="+std::to_string(p.network);
    else out.ready=true;
    return true;
}
struct Receipt {bool attempted=false,returned=false,confirmed=false,unknown=false;Status state;};
template<class Calls>bool prepare(Calls& calls,Receipt& out,std::string& e){
    out={};Pin before,again;
    if(!calls.pin(before,e)||!calls.pin(again,e))return false;
    if(before!=again)return fail(e,"Network tab binding changed before Press");
    if(before.active)return fail(e,"Network tab already active; no Press replay");
    if(!calls.reserve(before,e))return false;
    out.attempted=true;
    try{
        if(!calls.press(before,e)){out.unknown=true;return false;}out.returned=true;
        Pin after;if(!calls.pin(after,e)){out.unknown=true;return false;}
        if(!same_owner(before,after)){out.unknown=true;return fail(e,"Network tab owner changed after Press");}
        if(!calls.status(after,out.state,e)||!calls.unchanged(e)){out.unknown=true;return false;}
        Pin final;if(!calls.pin(final,e)){out.unknown=true;return false;}
        if(!same_owner(after,final)){out.unknown=true;return fail(e,"Network tab owner changed during full readback");}
        out.confirmed=true;return true;
    }catch(...){out.unknown=true;throw;}
}
}
