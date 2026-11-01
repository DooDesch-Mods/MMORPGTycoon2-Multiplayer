#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace mt2mp::game::native_lobby {
inline bool joinable_lobby_state(std::string_view state){return state=="open"||state=="playing";}

// Cache only an acknowledged publication for this actual Steam lobby.
struct OrderedPublication {
    uint64_t lobby=0;
    std::string state;
    bool joinable=false;
    template<class Steam> bool publish(Steam& steam,uint64_t actual,const std::string& next,bool allow,std::string& error){
        if(!actual){error="Ordered lobby publication requires an actual Steam lobby ID";return false;}
        if(lobby==actual&&state==next&&joinable==allow){error.clear();return true;}
        // Close admission before publishing a frozen/failed state. Open only after
        // Steam accepted the ready metadata, never on a partially published state.
        if(!steam.set_lobby_joinable(false,error)||!steam.publish_lobby_data("lobby_state",next,error)||
           (allow&&!steam.set_lobby_joinable(true,error)))return false;
        lobby=actual;state=next;joinable=allow;error.clear();return true;
    }
};
}
