#pragma once
#include "district_saved_identity.h"
#include "typed_action/district_action.h"
namespace mt2mp::game::district_title_reader {
namespace q=district_saved_identity;
template<class Memory>
bool capture(Memory& m,uintptr_t image,uintptr_t object,uintptr_t point,uintptr_t& behaviour,
             action_session::DistrictTitleIntent& intent,std::string& e){
    uintptr_t cursor=0,target=0,proxy=0,weak=0;uint8_t pressed=0;
    if(!q::at(m,image,0x13df9d0,cursor,e)||!q::typed(m,image,cursor,0x1786dd0,e)||
       !q::at(m,cursor,0x190,behaviour,e)||!q::typed(m,image,behaviour,0x1741aa0,e)||
       !q::at(m,behaviour,0x150,target,e)||!q::at(m,behaviour,0x158,proxy,e)||
       !q::at(m,proxy,0,weak,e)||!q::at(m,behaviour,0x16c,pressed,e))return false;
    if(target!=object||weak!=object+0x20||pressed!=1||point!=behaviour+0x160)
        return q::fail(e,"District title original Release receiver/weak target/pressed point differs");
    return q::at(m,behaviour,0x160,intent.position,e);
}
template<class Memory>
bool cleaned(Memory& m,uintptr_t image,uintptr_t behaviour,std::string& e){
    uintptr_t cursor=0,current=0,target=0,proxy=0;
    if(!q::typed(m,image,behaviour,0x1741aa0,e)||!q::at(m,image,0x13df9d0,cursor,e)||
       !q::typed(m,image,cursor,0x1786dd0,e)||!q::at(m,cursor,0x190,current,e)||
       !q::at(m,behaviour,0x150,target,e)||!q::at(m,behaviour,0x158,proxy,e))return false;
    return (current!=behaviour&&!target&&!proxy)||q::fail(e,"District title original Release/Deactivate cleanup incomplete");
}
}
