#pragma once
#include <cstdint>
#include <string>

namespace mt2mp::game::dungeon_build_local_ui {
struct Receipt {
    bool attempted=false,switched=false,selected=false,superseded=false;
};
template<class Read,class Call,class Selected>
bool complete(uintptr_t image,uintptr_t captured,uintptr_t building,Receipt& receipt,
              Read&& read,Call&& call,Selected&& selected,std::string& error){
    if(receipt.attempted){error="Dungeon Building local UI completion already attempted; no replay";return false;}
    uintptr_t cursor=0,ap=0,current=0,desired=0,selection=0;
    if(!image||!captured||!building||!read(image,0x13df9d0,cursor,error)||!cursor||
       !read(cursor,0,ap,error)||ap!=image+0x1786dd0||
       !read(cursor,0x190,current,error)){
        if(error.empty())error="Dungeon Building local UI current Cursor owner differs";
        return false;
    }
    if(current!=captured){receipt.attempted=true;receipt.superseded=true;return true;}
    if(!read(current,0,ap,error)||ap!=image+0x17327d0||
       !read(cursor,0x198,desired,error)||!desired||
       !read(desired,0,ap,error)||ap!=image+0x17391a0||
       !read(cursor,0x148,selection,error)||!selection){
        if(error.empty())error="Dungeon Building local UI original Build/Default owner differs";
        return false;
    }
    receipt.attempted=true;
    if(!call(false,cursor,uintptr_t{0},error))return false;
    receipt.switched=true;
    if(!read(cursor,0x190,current,error)||current!=desired){
        if(error.empty())error="Dungeon Building local UI original Default selection failed";
        return false;
    }
    if(!call(true,cursor,building,error))return false;
    if(!selected(selection,building,error))return false;
    receipt.selected=true;error.clear();return true;
}
}
