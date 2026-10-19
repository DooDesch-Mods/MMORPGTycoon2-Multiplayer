#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

namespace mt2mp::game::npc_quest_info_refresh {
struct Owner {
    uintptr_t manager=0,handler=0,secondary=0,window=0,npc=0,proxy=0,property=0,view=0,active=0,next=0,keyboard_focus=0;
    int32_t transition=0;
    std::array<float,2> position{};
    std::array<uint32_t,24> camera{};
    std::string current_page,requested_page;
    bool operator==(const Owner&)const=default;
};
struct Snapshot {Owner owner;int32_t count=-1,displayed=-1;};
struct Receipt {
    bool checked=false,attempted=false,returned=false,confirmed=false,skipped=false,presentation_unchanged=false;
    int32_t count_before=-1,count_after=-1,displayed_before=-1,displayed_after=-1;
    std::string skip_reason,cause;
};
inline bool fail(std::string& e,const char* why){e=why;return false;}
template<class Memory,class T>
bool at(Memory& m,uintptr_t p,size_t offset,T& out,std::string& e){
    if(!p||p>UINTPTR_MAX-offset||p+offset>UINTPTR_MAX-(sizeof out-1))
        return fail(e,"Quest Info refresh address is null or overflows");
    return m.copy(p+offset,&out,sizeof out,e);
}

// A closed, hidden or unrelated local window is not opened or selected here.
template<class Memory,class String>
bool read(Memory& m,String&& text,uintptr_t image,uintptr_t manager,uintptr_t npc,int32_t expected,
          Snapshot& out,bool& eligible,std::string& skip,std::string& e){
    out={};eligible=false;skip.clear();
    if(!image||!manager||!npc||expected<0||expected>5)return fail(e,"Quest Info refresh owner/count invalid");
    int32_t selected=0;uintptr_t focused=0,ap=0;
    if(!at(m,manager,0x30,selected,e)||!at(m,manager,0x80,focused,e))return false;
    if(selected!=1||focused!=npc){skip="modified NPC is not the local single selection";return true;}
    auto& owner=out.owner;owner.manager=manager;owner.npc=npc;
    if(!at(m,manager,0xb8,owner.handler,e)||!at(m,manager,0xc0,owner.secondary,e))return false;
    if(!owner.handler){skip="local NPC Info handler is absent";return true;}
    if(!at(m,owner.handler,0,ap,e))return false;
    if(ap!=image+0x1731820){skip="local selection has another Info handler";return true;}
    if(!at(m,owner.handler,0x10,owner.window,e))return false;
    if(!owner.window){skip="local NPC Info window is closed";return true;}
    if(!at(m,owner.window,0,ap,e))return false;
    if(ap!=image+0x1707d20)return fail(e,"Quest Info refresh window class differs");
    uint8_t visible=0;uintptr_t target=0;
    if(!at(m,owner.window,0x45f,visible,e)||!at(m,owner.window,0x550,target,e))return false;
    if(visible>1)return fail(e,"Quest Info refresh visibility is not a native bool");
    if(!visible){skip="local NPC Info window is hidden";return true;}
    if(target!=npc){skip="local NPC Info window shows another NPC";return true;}
    uintptr_t proxy=0,weak=0;
    if(!at(m,npc,0x28,proxy,e)||!at(m,owner.window,0x558,owner.proxy,e))return false;
    if(!proxy||proxy!=owner.proxy)return fail(e,"Quest Info refresh NPC weak binding differs");
    if(!at(m,proxy,0,weak,e))return false;
    if(weak!=npc+0x20)return fail(e,"Quest Info refresh NPC weak target expired or differs");
    if(!at(m,npc,0x870,out.count,e))return false;
    if(out.count!=expected)return fail(e,"Quest Info refresh native count differs from confirmed action");
    if(!at(m,owner.window,0x4f8,owner.property,e))return false;
    if(!owner.property)return fail(e,"Quest Info refresh count property is absent");
    uintptr_t property_owner=0;
    if(!at(m,owner.property,0,ap,e)||!at(m,owner.property,0x280,property_owner,e))return false;
    if(ap!=image+0x1701d20||property_owner!=owner.window)return fail(e,"Quest Info refresh count property class/Window owner differs");
    if(!at(m,owner.window,0x540,owner.view,e)||!at(m,owner.view,0,ap,e))return false;
    uintptr_t view_owner=0;
    if(!at(m,owner.view,0x280,view_owner,e))return false;
    if(ap!=image+0x16dfe90||view_owner!=owner.window)return fail(e,"Quest Info refresh MultiView class/Window owner differs");
    if(!at(m,owner.view,0x410,owner.active,e)||!at(m,owner.view,0x418,owner.next,e)||
       !at(m,owner.view,0x408,owner.transition,e)||!text(owner.view,0x3c8,owner.current_page,e)||
       !text(owner.view,0x3e8,owner.requested_page,e)||!at(m,owner.window,0x110,owner.position,e))return false;
    for(float value:owner.position)if(!std::isfinite(value))return fail(e,"Quest Info refresh window position is not finite");
    std::string format,name;uintptr_t data=0,end=0,capacity=0;int32_t kind=0;
    if(!text(owner.property,0x350,format,e)||!at(m,owner.property,0x370,data,e)||
       !at(m,owner.property,0x378,end,e)||!at(m,owner.property,0x380,capacity,e))return false;
    if(format!="{value}"||!data||data>UINTPTR_MAX-0x68||end!=data+0x68||capacity<end||
       capacity-data>16*0x68)return fail(e,"Quest Info refresh count LocString argument span differs");
    if(!text(data,0,name,e)||!at(m,data,0x60,kind,e)||!at(m,data,0x58,out.displayed,e))return false;
    if(name!="value"||kind!=1||out.displayed<0||out.displayed>5)
        return fail(e,"Quest Info refresh count LocString is not one bounded integer value");
    eligible=true;return true;
}

template<class Calls>
bool apply(Calls& calls,Receipt& receipt,std::string& e){
    if(receipt.checked){e=receipt.cause;return e.empty();}
    receipt.checked=true;
    const auto failed=[&](){receipt.cause=e.empty()?"Quest Info refresh failed without cause":e;e=receipt.cause;return false;};
    Snapshot before;bool eligible=false;
    if(!calls.pin(before,eligible,receipt.skip_reason,e))return failed();
    if(!eligible){receipt.skipped=true;return true;}
    receipt.count_before=before.count;receipt.displayed_before=before.displayed;
    if(before.displayed==before.count){receipt.skipped=true;receipt.skip_reason="native Info count is already current";
        receipt.count_after=before.count;receipt.displayed_after=before.displayed;receipt.presentation_unchanged=true;return true;}
    if(!calls.verify(e))return failed();
    receipt.attempted=true;
    if(!calls.refresh(before.owner.window,e))return failed();
    receipt.returned=true;
    Snapshot after;std::string skip;
    if(!calls.pin(after,eligible,skip,e))return failed();
    receipt.count_after=after.count;receipt.displayed_after=after.displayed;
    if(!eligible||after.owner!=before.owner||after.count!=before.count){
        e="Quest Info refresh changed local Window/selection/page/position/camera or native count";return failed();}
    receipt.presentation_unchanged=true;
    if(after.displayed!=after.count){e="Quest Info original UpdateContents returned with stale count";return failed();}
    receipt.confirmed=true;return true;
}
}
