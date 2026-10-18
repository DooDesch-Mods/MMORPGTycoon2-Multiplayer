#pragma once
#include <cstdint>
#include <string>

namespace mt2mp::game::scenario_vip_authority {
struct Receipt {
    bool owned=false,suppressed=false;
    uintptr_t receiver=0,log=0;
    uint64_t native_epoch=0,native_generation=0;
    std::string snapshot,origin,cause;
};
using HostUpdate=void(*)(uintptr_t,float);
struct Callbacks {
    bool(*client_active)()=nullptr;
    bool(*owner)(uintptr_t,Receipt&,std::string&)=nullptr;
    void(*publish)(const Receipt&)=nullptr;
    void(*host_update)(uintptr_t,float,uintptr_t,HostUpdate)=nullptr;
};
bool initialize(uintptr_t,const Callbacks&,std::string&);
bool verify_hook(std::string&);
}
