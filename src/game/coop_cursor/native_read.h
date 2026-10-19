#pragma once
#include "core.h"
#include <functional>

namespace mt2mp::game::coop_cursor {
using Copy=std::function<bool(uintptr_t,void*,size_t,std::string&)>;
inline bool read_native_sample(uintptr_t image,const Copy& copy,Sample& out,std::string& error){
    out={};
    if(!image||image>UINTPTR_MAX-0x1786dd0||!copy){error="cursor native image/copy unavailable";return false;}
    const auto read=[&]<class T>(uintptr_t object,size_t offset,T& value){
        if(!object||object>UINTPTR_MAX-offset||object+offset>UINTPTR_MAX-(sizeof(T)-1)){
            error="cursor native field address overflow";return false;
        }
        return copy(object+offset,&value,sizeof(T),error);
    };
    uintptr_t cursor=0,table=0,capture=0;
    uint8_t enabled=0;
    std::array<float,3> point{};
    if(!read(image,0x13df9d0,cursor)||!read(cursor,0,table))return false;
    if(table!=image+0x1786dd0){error="cursor native class differs";return false;}
    if(!read(cursor,0x257,enabled)||!read(cursor,0x180,capture))return false;
    if(enabled>1){error="cursor native enabled flag outside0..1";return false;}
    if(!enabled||capture){error.clear();return true;}
    if(!read(cursor,0x1e4,point))return false;
    Sample sampled{point};
    if(!valid(sampled)){error="cursor native world position outside finite bound";return false;}
    uintptr_t current=0,current_table=0;
    if(!read(image,0x13df9d0,current)||!read(cursor,0,current_table))return false;
    if(current!=cursor||current_table!=table){error="cursor native lifetime changed during read";return false;}
    out=sampled;error.clear();return true;
}
}
