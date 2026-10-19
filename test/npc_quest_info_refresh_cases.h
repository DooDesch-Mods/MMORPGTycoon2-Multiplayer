#pragma once
#include "npc_quest_info_refresh.h"
#include <cstring>
#include <functional>
#include <map>
#include <stdexcept>
#include <vector>

namespace quest_info_test {
namespace info=mt2mp::game::npc_quest_info_refresh;
struct Calls {
    static constexpr uintptr_t image=0x140000000,manager=0x100000,handler=0x200000,window=0x300000,
        npc=0x400000,proxy=0x500000,property=0x600000,view=0x700000,argument=0x800000;
    std::map<uintptr_t,std::vector<unsigned char>> values;
    std::map<uintptr_t,std::string> strings;
    std::array<uint32_t,24> camera{};
    uintptr_t keyboard_focus=0x990000,failed_address=0;
    size_t originals=0,verifications=0;
    bool verify_ok=true,original_ok=true,leave_stale=false,fail_after=false;
    std::function<void(Calls&)> after;
    template<class T>void put(uintptr_t at,const T& value){
        auto& bytes=values[at];bytes.resize(sizeof value);std::memcpy(bytes.data(),&value,sizeof value);
    }
    Calls(){
        put(manager+0x30,int32_t(1));put(manager+0x80,npc);put(manager+0xb8,handler);put(manager+0xc0,uintptr_t(0));
        put(handler,image+0x1731820);put(handler+0x10,window);put(window,image+0x1707d20);
        put(window+0x45f,uint8_t(1));put(window+0x550,npc);put(window+0x558,proxy);put(npc+0x28,proxy);
        put(proxy,npc+0x20);put(npc+0x870,int32_t(2));put(window+0x4f8,property);
        put(property,image+0x1701d20);put(property+0x280,window);put(window+0x540,view);
        put(view,image+0x16dfe90);put(view+0x280,window);put(view+0x410,uintptr_t(0x900000));
        put(view+0x418,uintptr_t(0));put(view+0x408,int32_t(0));
        strings[view+0x3c8]=strings[view+0x3e8]="{npcinfo_tab_info}";
        put(window+0x110,std::array<float,2>{120.f,80.f});
        strings[property+0x350]="{value}";put(property+0x370,argument);put(property+0x378,argument+0x68);
        put(property+0x380,argument+0x68);strings[argument]="value";put(argument+0x60,int32_t(1));
        put(argument+0x58,int32_t(3));
    }
    bool copy(uintptr_t at,void* out,size_t size,std::string& e){
        if(at==failed_address||(fail_after&&originals)){e="fixture original native read failed";return false;}
        const auto found=values.find(at);
        if(found==values.end()||found->second.size()!=size){e="fixture missing exact native field";return false;}
        std::memcpy(out,found->second.data(),size);return true;
    }
    bool pin(info::Snapshot& out,bool& eligible,std::string& skip,std::string& e){
        const auto text=[&](uintptr_t at,size_t offset,std::string& value,std::string& error){
            const auto found=strings.find(at+offset);
            if(found==strings.end()){error="fixture original string read failed";return false;}
            value=found->second;return true;};
        if(!info::read(*this,text,image,manager,npc,2,out,eligible,skip,e))return false;
        if(eligible){out.owner.camera=camera;out.owner.keyboard_focus=keyboard_focus;}
        return true;
    }
    bool verify(std::string& e){++verifications;if(verify_ok)return true;e="fixture full original body changed";return false;}
    bool refresh(uintptr_t target,std::string& e){
        ++originals;if(target!=window)throw std::runtime_error("refresh called unrelated Window");
        if(!original_ok){e="fixture native UpdateContents failed";return false;}
        if(!leave_stale)put(argument+0x58,int32_t(2));
        if(after)after(*this);return true;
    }
};
inline size_t run(){
    size_t checks=0;
    const auto expect=[&](bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);};
    std::string e;
    {
        Calls calls;info::Receipt r;
        expect(info::apply(calls,r,e)&&r.checked&&r.attempted&&r.returned&&r.confirmed&&!r.skipped,
               "stale native Info count refreshed");
        expect(r.displayed_before==3&&r.displayed_after==2&&r.count_before==2&&r.count_after==2&&
               r.presentation_unchanged&&calls.originals==1&&calls.verifications==1,"exact original/readback");
        expect(info::apply(calls,r,e)&&calls.originals==1&&calls.verifications==1,"no repeat refresh");
    }
    {
        Calls calls;calls.put(Calls::argument+0x58,int32_t(2));info::Receipt r;
        expect(info::apply(calls,r,e)&&r.skipped&&!r.attempted&&calls.originals==0&&calls.verifications==0,
               "already-current presentation has no extra native call");
    }
    const auto skip=[&](auto change,const char* label){
        Calls calls;change(calls);info::Receipt r;e.clear();
        expect(info::apply(calls,r,e)&&r.skipped&&!r.attempted&&!r.confirmed&&!r.skip_reason.empty()&&calls.originals==0,label);
    };
    skip([](auto& c){c.put(Calls::manager+0x30,int32_t(0));},"no selected NPC is not opened");
    skip([](auto& c){c.put(Calls::manager+0x80,uintptr_t(0xaa0000));},"another NPC selection untouched");
    skip([](auto& c){c.put(Calls::manager+0xb8,uintptr_t(0));},"absent handler untouched");
    skip([](auto& c){c.put(Calls::handler,Calls::image+0x17316a0);},"another native handler untouched");
    skip([](auto& c){c.put(Calls::handler+0x10,uintptr_t(0));},"closed window not reopened");
    skip([](auto& c){c.put(Calls::window+0x45f,uint8_t(0));},"hidden window not shown");
    skip([](auto& c){c.put(Calls::window+0x550,uintptr_t(0xbb0000));},"another Info target untouched");
    const auto reject=[&](auto change,const char* label){
        Calls calls;change(calls);info::Receipt r;e.clear();
        expect(!info::apply(calls,r,e)&&r.checked&&!r.cause.empty()&&!r.confirmed&&calls.originals==0,label);
    };
    reject([](auto& c){c.put(Calls::window,Calls::image+1);},"wrong native window class rejected");
    reject([](auto& c){c.put(Calls::window+0x45f,uint8_t(2));},"invalid visible bool rejected");
    reject([](auto& c){c.put(Calls::window+0x558,uintptr_t(0));},"missing matching proxy rejected");
    reject([](auto& c){c.put(Calls::proxy,uintptr_t(0));},"expired NPC weak target rejected");
    reject([](auto& c){c.put(Calls::npc+0x870,int32_t(3));},"changed authored count rejected");
    reject([](auto& c){c.put(Calls::property+0x280,uintptr_t(0xaa0000));},"foreign count pane owner rejected");
    reject([](auto& c){c.put(Calls::view,Calls::image+1);},"foreign MultiView rejected");
    reject([](auto& c){c.strings[Calls::property+0x350]="another format";},"wrong LocString format rejected");
    reject([](auto& c){c.put(Calls::property+0x378,Calls::argument+0xd0);},"two LocString arguments rejected");
    reject([](auto& c){c.put(Calls::property+0x380,Calls::argument);},"invalid argument capacity rejected");
    reject([](auto& c){c.strings[Calls::argument]="other";},"wrong numeric argument name rejected");
    reject([](auto& c){c.put(Calls::argument+0x60,int32_t(2));},"noninteger argument rejected");
    reject([](auto& c){c.verify_ok=false;},"changed full native body prevents call");
    {
        Calls calls;calls.failed_address=Calls::property+0x280;info::Receipt r;e.clear();
        expect(!info::apply(calls,r,e)&&e=="fixture original native read failed"&&calls.originals==0,"original read cause retained");
    }
    {
        Calls calls;calls.original_ok=false;info::Receipt r;e.clear();
        expect(!info::apply(calls,r,e)&&r.attempted&&!r.returned&&!r.confirmed&&e=="fixture native UpdateContents failed",
               "original failed effect not reported rendered");
        expect(!info::apply(calls,r,e)&&calls.originals==1&&e=="fixture native UpdateContents failed","failed original never replayed");
    }
    {
        Calls calls;calls.fail_after=true;info::Receipt r;e.clear();
        expect(!info::apply(calls,r,e)&&r.returned&&!r.confirmed&&e=="fixture original native read failed",
               "post-call read failure not false rendered success");
    }
    {
        Calls calls;calls.leave_stale=true;info::Receipt r;e.clear();
        expect(!info::apply(calls,r,e)&&r.returned&&!r.confirmed&&r.displayed_after==3,"returned native call with stale count fails");
    }
    const auto changed=[&](auto change,const char* label){
        Calls calls;calls.after=change;info::Receipt r;e.clear();
        expect(!info::apply(calls,r,e)&&r.attempted&&r.returned&&!r.confirmed&&calls.originals==1,label);
    };
    changed([](auto& c){c.strings[Calls::view+0x3c8]="{npcinfo_tab_quests}";},"current tab must remain unchanged");
    changed([](auto& c){c.strings[Calls::view+0x3e8]="{npcinfo_tab_stats}";},"requested tab must remain unchanged");
    changed([](auto& c){c.put(Calls::window+0x110,std::array<float,2>{121.f,80.f});},"position must remain unchanged");
    changed([](auto& c){c.camera[3]=1;},"camera focus must remain unchanged");
    changed([](auto& c){++c.keyboard_focus;},"SDL input focus must remain unchanged");
    changed([](auto& c){c.put(Calls::manager+0xc0,uintptr_t(0x123000));},"other selected handler must remain unchanged");
    changed([](auto& c){c.put(Calls::manager+0x80,uintptr_t(0xaa0000));},"selection change after original is not rendered success");
    return checks;
}
}
