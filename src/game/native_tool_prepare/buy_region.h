#pragma once
#include "tool_cancel.h"
#include <array>
#include <limits>

namespace mt2mp::game::native_tool_prepare::buy_region {
inline bool fail(std::string& e,const char* cause){e=cause;return false;}
inline constexpr std::array<unsigned char,16> deactivate_code{
    0xc3,0x90,0x0f,0x1f,0x00,0x66,0x66,0x2e,0x0f,0x1f,0x84,0,0,0,0,0};
inline constexpr std::array<unsigned char,16> cancel_code{
    0xc3,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90};
template<class M,class T>bool at(M& m,uintptr_t p,size_t offset,T& out,std::string& e){
    if(!p||p>UINTPTR_MAX-offset)return fail(e,"BuyRegion address range invalid");
    return m.copy(p+offset,&out,sizeof out,e);
}
template<class W>bool same_world(const W& a,const W& b){
    return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&
        a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.bytes==b.bytes&&a.uuid==b.uuid&&a.sha256==b.sha256;
}
inline bool flag(const nlohmann::json& value,const char* name,bool expected){
    const auto it=value.find(name);return it!=value.end()&&it->is_boolean()&&it->get<bool>()==expected;
}
inline bool idle(bool local_test,const nlohmann::json& controller,const nlohmann::json& region,
                 float time_scale,int32_t edits,int32_t saves,std::string& e){
    if(!local_test)return fail(e,"BuyRegion requires owned local test runtime");
    if(!flag(controller,"started",true)||!flag(controller,"pending",false)||!flag(controller,"locked",false))
        return fail(e,"BuyRegion Controller not idle");
    if(!flag(region,"initialized",true)||!flag(region,"enabled",true)||region.contains("error")||!flag(region,"pending_ui",false))
        return fail(e,"BuyRegion region UI unavailable or pending");
    if(time_scale!=0||edits||saves)return fail(e,"BuyRegion requires actual paused Clock and no edit/save");
    return true;
}
struct Pin {
    uintptr_t cursor{},old{},desired{},manager{};
    bool operator==(const Pin&)const=default;
};
template<class M>bool inspect(M& m,uintptr_t image,const Pin& expected,bool after,Pin& out,std::string& e){
    out={};uintptr_t ap=0,current=0,activation=0,deactivation=0,cancellation=0;
    uint8_t mode=0,valid=0,select=0,progress=0,queue=0;
    if(!at(m,image,0x13df9d0,out.cursor,e)||out.cursor!=expected.cursor||
       !at(m,out.cursor,0,ap,e)||ap!=image+0x1786dd0||
       !at(m,out.cursor,0x190,current,e)||current!=(after?expected.desired:expected.old)||
       !at(m,out.cursor,0x198,out.desired,e)||out.desired!=expected.desired)
        return fail(e,e.empty()?"BuyRegion Cursor owner changed":e.c_str());
    out.old=expected.old;
    if(!at(m,out.old,0,ap,e)||ap!=image+0x173c170||
       !at(m,ap,0xa8,activation,e)||activation!=image+0x595120||
       !at(m,ap,0xb0,deactivation,e)||deactivation!=image+0x595120||
       !at(m,ap,0x128,cancellation,e)||cancellation!=image+0xbf63f0)
        return fail(e,e.empty()?"BuyRegion actual virtual contract differs":e.c_str());
    if(!at(m,out.old,0x148,mode,e)||!at(m,out.old,0x158,valid,e))return false;
    // Validity can remain set after release; it is not a pending-commit flag.
    if(mode||valid>1)return fail(e,"BuyRegion requires buy mode and boolean validity");
    std::array<unsigned char,16> bytes{};
    if(!m.copy(image+0x595120,bytes.data(),bytes.size(),e))return false;
    if(bytes!=deactivate_code)return fail(e,"BuyRegion full Activate/Deactivate body differs");
    if(!m.copy(image+0xbf63f0,bytes.data(),bytes.size(),e))return false;
    if(bytes!=cancel_code)return fail(e,"BuyRegion full base Cancel body differs");
    if(!at(m,out.desired,0,ap,e)||ap!=image+0x17391a0||
       !at(m,ap,0xa8,activation,e)||activation!=image+0xbf6500||
       !at(m,out.desired,0x120,select,e)||select!=1)
        return fail(e,e.empty()?"BuyRegion Default selection contract differs":e.c_str());
    if(!at(m,image,0x13df630,out.manager,e)||!at(m,out.manager,0,ap,e)||ap!=image+0x17205d0||
       (expected.manager&&out.manager!=expected.manager)||
       !at(m,out.manager,0xd1,progress,e)||!at(m,out.manager,0xd3,queue,e))
        return fail(e,e.empty()?"BuyRegion SelectionManager owner differs":e.c_str());
    if(progress||queue)return fail(e,"BuyRegion SelectionManager not idle");
    return true;
}
inline bool preview_applicable(bool buy,bool prior_tool){return prior_tool&&!buy;}
inline uintptr_t cancel_rva(bool build,bool scenery,bool buy){return buy?0xbf63f0:(scenery?0x5d0690:(build?0x591320:0x5c0900));}
// The same mask sequence is used at the native SEH boundary and by the fixture.
template<class J,class C>void sequence(J& j,C& calls){
    j.attempted|=1;calls.cancel();j.returned|=1;
    j.attempted|=2;calls.switch_default();j.returned|=2;
}
template<class J,class C>bool prepare(J& j,C& calls,std::string& e){
    if(j.attempted||j.confirmed)return fail(e,"BuyRegion retained effect cannot be replayed");
    try{
        if(!calls.pin(false,e))return false;
        calls.invoke();
        if(j.exception||j.returned!=3){
            e="tool_prepare_native_cancel_switch_SEH="+std::to_string(j.exception)+" no replay";
            j.confirmed=false;j.unknown=true;return false;
        }
        if(!calls.pin(true,e)||!calls.observe(e)||!calls.unchanged(e)||!calls.pin(true,e)){
            j.confirmed=false;j.unknown=true;return false;
        }
        j.confirmed=true;return true;
    }catch(...){j.confirmed=false;j.unknown=j.attempted!=0;throw;}
}
}
