#pragma once
#include "root_saved_building.h"
#include "dungeon_saved_building.h"

namespace mt2mp::game::saved_building_path {
inline uint32_t cast(uintptr_t image,uintptr_t object,uintptr_t from,uintptr_t to,uintptr_t& found)noexcept{
    __try{found=uintptr_t(reinterpret_cast<void*(*)(void*,const void*,const void*,ptrdiff_t)>(image+0x12654f0)(
        reinterpret_cast<void*>(object),reinterpret_cast<void*>(image+from),reinterpret_cast<void*>(image+to),0));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline bool read(uintptr_t image,void* map,void* object,int32_t child,std::string& suffix,
                 std::string& error,const void* expected_array=nullptr){
    live_host_state::apply_timing::PageReadScope pages;
    auto at=[](uintptr_t address,size_t offset,auto& value,std::string& e){
        return root_saved_building::read(reinterpret_cast<void*>(address),offset,value,e);
    };
    auto typed=[&](uintptr_t pointer,uintptr_t from,uintptr_t to,std::string& e){
        live_host_state::apply_timing::PageReadScope call(live_host_state::apply_timing::PageBoundary::OriginalCall);
        uintptr_t found=0;const auto exception=cast(image,pointer,from,to,found);
        if(exception||found!=pointer){e="Saved Dungeon Building owner RTTI differs exception="+std::to_string(exception);return false;}
        return true;
    };
    auto root_slot=[&](uintptr_t pointer,int32_t index,int32_t& id,uintptr_t array,std::string& e){
        return root_saved_building::slot(image,map,reinterpret_cast<void*>(pointer),index,id,e,reinterpret_cast<void*>(array));
    };
    return dungeon_saved_building::path(image,uintptr_t(map),uintptr_t(object),child,uintptr_t(expected_array),at,typed,root_slot,suffix,error);
}
}
