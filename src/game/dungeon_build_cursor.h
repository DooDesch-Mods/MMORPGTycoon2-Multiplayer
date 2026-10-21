#pragma once
#include <cstdint>
#include <string>

namespace mt2mp::game::dungeon_build_cursor {
enum class Route {Forward,Suppressed,Rejected};
struct Scope {
    bool exterior=false;
    uintptr_t cursor=0,building=0;
    unsigned behaviour=0,selection=0;
    std::string cause;
};
inline Route route(Scope& scope,bool selected,uintptr_t caller_rva,uintptr_t receiver,uintptr_t value){
    const auto expected=selected?uintptr_t(0x5944f5):uintptr_t(0x5944e5);
    if(!scope.exterior||caller_rva!=expected)return Route::Forward;
    auto& calls=selected?scope.selection:scope.behaviour;
    if(!scope.cursor||receiver!=scope.cursor||!scope.building||
       (selected?value!=scope.building:value!=0)||calls||
       (selected?scope.behaviour!=1:scope.selection!=0)){
        if(scope.cause.empty())scope.cause="Private Dungeon Build original Cursor owner/order/argument differs";
        return Route::Rejected;
    }
    ++calls;
    return Route::Suppressed;
}
inline bool complete(const Scope& scope,std::string& error){
    if(!scope.cause.empty()){error=scope.cause;return false;}
    if(scope.exterior&&(scope.behaviour!=1||scope.selection!=1)){
        error="Private Dungeon Build original Cursor completion is incomplete";return false;
    }
    return true;
}
}
