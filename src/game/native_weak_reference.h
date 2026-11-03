#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace mt2mp::game::native_weak_reference {
struct Layout {size_t target_offset=0,proxy_offset=0;};
struct Observation {
    uintptr_t target=0,proxy=0,current=0;
    bool live()const{return current!=0;}
};
inline std::string current_target_delta(const Observation& value){
    return value.current>=value.target?"+"+std::to_string(value.current-value.target):"-"+std::to_string(value.target-value.current);
}
template<class Read>bool observe(Read& at,uintptr_t target,uintptr_t proxy,Observation& out,std::string& error){
    out={target,proxy,0};
    if(!proxy){if(target){error="Weak reference raw target has no proxy";return false;}return true;}
    if(!at(proxy,0,out.current,error))return false;
    if(out.current&&!target){error="Weak live proxy has no raw target";return false;}
    return true;
}
// Call only after the raw target belongs to the caller's current canonical census.
// Native proxies point to the WeakPointerTarget subobject, not the typed receiver.
template<class Read>bool require_owner(Read& at,const Observation& value,Layout layout,std::string& error){
    if(!value.live()||!value.target||!value.proxy){error="Weak current owner requires a live raw target and proxy";return false;}
    if(layout.target_offset>UINTPTR_MAX-value.target||layout.proxy_offset>UINTPTR_MAX-value.target||
       value.target+layout.proxy_offset>UINTPTR_MAX-sizeof(uintptr_t)){
        error="Weak current owner field address overflows";return false;
    }
    if(value.current!=value.target+layout.target_offset){error="Weak current subobject differs current_target_delta="+current_target_delta(value)+" expected_delta=+"+std::to_string(layout.target_offset);return false;}
    uintptr_t reciprocal=0;if(!at(value.target,layout.proxy_offset,reciprocal,error))return false;
    if(reciprocal!=value.proxy){error="Weak canonical owner reciprocal proxy differs";return false;}
    return true;
}
}
