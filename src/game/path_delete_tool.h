#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace mt2mp::game::path_delete_tool {
struct Pin {
    uintptr_t cursor=0,current=0,current_ap=0,desired=0,manager=0;
    uintptr_t block=0,preview=0,split=0;
    std::array<uint8_t,5> flags{};
    bool road=false;
};
inline bool fail(std::string& e,const std::string& cause){e="path delete tool "+cause;return false;}
template<class Read>bool inspect(uintptr_t image,int32_t road,int32_t node,uintptr_t manager,Read&& at,Pin& out,std::string& e){
    out={};out.manager=manager;uintptr_t ap=0,activate=0,deactivate=0,cancel=0;uint8_t selection=0,progress=0,queued=0;
    if(!at(image,0x13df9d0,out.cursor,e)||!at(out.cursor,0,ap,e)||ap!=image+0x1786dd0||
       !at(out.cursor,0x190,out.current,e)||!out.current||!at(out.current,0,out.current_ap,e)||
       !at(out.cursor,0x198,out.desired,e)||!at(out.desired,0,ap,e)||ap!=image+0x17391a0||
       !at(ap,0xa8,activate,e)||activate!=image+0xbf6500||!at(out.desired,0x120,selection,e)||selection!=1)
        return fail(e,"Cursor/Default identity or selection contract differs current_ap="+std::to_string(out.current_ap)+" original="+e);
    uintptr_t current_manager=0;
    if(!at(image,0x13df630,current_manager,e)||current_manager!=manager||!at(manager,0,ap,e)||ap!=image+0x17205d0||!at(manager,0xd1,progress,e)||!at(manager,0xd3,queued,e)||progress||queued)
        return fail(e,"SelectionManager identity or idle flags differ original="+e);
    if(out.current==out.desired)return true;
    out.road=true;int32_t actual_road=-1,actual_node=-1;uintptr_t start_split=0;
    if(out.current_ap!=image+0x172f570||!at(out.current_ap,0xa8,activate,e)||activate!=image+0x5ca570||
       !at(out.current_ap,0xb0,deactivate,e)||deactivate!=image+0x5ca660||
       !at(out.current_ap,0x128,cancel,e)||cancel!=image+0xbf63f0||
       !at(out.current,0x120,selection,e)||selection||!at(out.current,0x188,actual_road,e)||actual_road!=road||
       !at(out.current,0x18c,actual_node,e)||actual_node!=node)
        return fail(e,"current tool is not canonical Road or Default current_ap="+std::to_string(out.current_ap)+" original="+e);
    if(!at(out.current,0x1a0,out.block,e)||!at(out.current,0x1a8,out.preview,e)||
       !at(out.current,0x1b0,start_split,e)||!at(out.current,0x1f0,out.split,e)||!at(out.current,0x230,out.flags,e))return false;
    if(out.flags[0]||start_split||out.split)return fail(e,"Road has active placement or borrowed split target; no cleanup attempted");
    if(out.preview){uint8_t ghost=0;
        if(!at(out.preview,0,ap,e)||ap!=image+0x1743f10||!at(out.preview,0x46e,ghost,e)||ghost!=1)
            return fail(e,"Road preview is not an owned native ghost original="+e);
    }
    return true;
}
template<class Read>bool cleaned(uintptr_t image,const Pin& before,Read&& at,Pin& after,std::string& e){
    if(!inspect(image,0,0,before.manager,at,after,e))return false;
    if(after.cursor!=before.cursor||after.desired!=before.desired||after.current!=before.desired)
        return fail(e,"original Default transition not observed current_ap="+std::to_string(after.current_ap)+"; no replay");
    if(before.road){uintptr_t ap=0,block=0,preview=0,start=0;int32_t splines=-1;std::array<uint8_t,5> flags{};
        if(!at(before.current,0,ap,e)||ap!=image+0x172f570||!at(before.current,0x1a0,block,e)||!at(before.current,0x1a8,preview,e)||!at(before.current,0x1b0,start,e)||
           !at(before.current,0x228,splines,e)||!at(before.current,0x230,flags,e))return false;
        if(block||preview||start||splines||flags[0]||flags[1]||flags[3])return fail(e,"original Road Deactivate retained preview/placement state");
    }
    return true;
}
struct Calls {bool cancel_attempted=false,cancel_returned=false,switch_attempted=false,switch_returned=false;};
template<class Native>void sequence(Calls& receipt,Native& native){
    receipt.cancel_attempted=true;native.cancel();receipt.cancel_returned=true;
    receipt.switch_attempted=true;native.switch_default();receipt.switch_returned=true;
}
}
