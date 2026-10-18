#pragma once
#include "host_transitions.h"
#include "../placement_ui_probe/ui_probe.h"
#include "../npc_census.h"
#include <set>

namespace mt2mp::game::scenario_vip_authority::host_read {
template<class T>bool at(placement_ui_probe::Memory& memory,uintptr_t p,size_t offset,T& value,std::string& e){
    if(!p||p>UINTPTR_MAX-offset||p+offset>UINTPTR_MAX-sizeof value)return wire::fail(e,"VIP Host read extent invalid");
    return memory.copy(p+offset,&value,sizeof value,e);
}
inline bool type(placement_ui_probe::Memory& memory,uintptr_t image,uintptr_t p,uintptr_t rva,std::string& e){
    uintptr_t ap=0;return at(memory,p,0,ap,e)&&(ap==image+rva||wire::fail(e,"VIP Host native class differs"));
}
inline bool scenarios(placement_ui_probe::Memory& memory,uintptr_t image,uintptr_t state,std::vector<uintptr_t>& out,std::string& e){
    out.clear();uintptr_t log=0,singleton=0,data=0;int32_t count=0,capacity=0;
    if(!at(memory,state,0x1e8,log,e)||!at(memory,image,0x13df330,singleton,e))return false;
    if(log!=singleton)return wire::fail(e,"VIP Host current ScenarioLog differs");
    if(!type(memory,image,log,0x16fd7b0,e))return false;
    if(!at(memory,log,0x20,data,e)||!at(memory,log,0x28,count,e)||!at(memory,log,0x2c,capacity,e))return false;
    if(count<0||count>64||capacity<count||capacity>64||(count&&!data)||data>UINTPTR_MAX-size_t(count)*8)
        return wire::fail(e,"VIP Host ScenarioLog bounds differ required=0..64");
    std::set<uintptr_t> unique;
    for(int32_t i=0;i<count;++i){uintptr_t instance=0,ap=0;
        if(!at(memory,data,size_t(i)*8,instance,e))return false;
        if(!instance||!unique.insert(instance).second)return wire::fail(e,"VIP Host ScenarioLog contains null/duplicate owner");
        if(!at(memory,instance,0,ap,e))return false;
        if(ap==image+0x1730e90)out.push_back(instance);
    }
    return true;
}
inline bool sample(placement_ui_probe::Memory& memory,uintptr_t image,uintptr_t receiver,HostSample& out,std::string& e){
    out={};out.scenario=receiver;uint8_t arrived=0,shown=0;uintptr_t method=0;
    if(!type(memory,image,receiver,0x1730e90,e)||!at(memory,image+0x1730e90,0xa0,method,e))return false;
    if(method!=image+0x4f85b0)return wire::fail(e,"VIP Host virtual Update entry differs");
    if(!at(memory,receiver,0x98,arrived,e)||!at(memory,receiver,0xa0,out.alert,e))return false;
    if(arrived>1)return wire::fail(e,"VIP Host arrival flag is not boolean");
    if(!arrived||!out.alert)return true;
    int32_t kind=0,extra=0;uintptr_t selector=0;
    if(!type(memory,image,out.alert,0x1749770,e)||!at(memory,out.alert,0x18,kind,e))return false;
    if(kind!=8)return true;
    if(!at(memory,out.alert,0x138,shown,e)||!at(memory,out.alert,0x130,selector,e))return false;
    if(shown>1)return wire::fail(e,"VIP Host shown flag is not boolean");
    if(!shown||!selector)return true;
    if(!at(memory,out.alert,0x68,out.toon,e)||!at(memory,out.alert,0x70,out.proxy,e))return false;
    if(!out.toon||!out.proxy)return true;
    uintptr_t target=0,owned_proxy=0,subscriber=0,backlink=0,actual_toon=0;int32_t refs=0;
    if(!at(memory,out.proxy,0,target,e))return false;
    if(!target)return true;
    if(out.toon>UINTPTR_MAX-0x28||target!=out.toon+0x20)return wire::fail(e,"VIP Host alert Toon Weak target differs");
    if(!at(memory,out.proxy,8,refs,e)||!type(memory,image,out.toon,0x1744e90,e)||!at(memory,out.toon,0x28,owned_proxy,e))return false;
    if(refs<=0||owned_proxy!=out.proxy)return wire::fail(e,"VIP Host alert Toon Proxy ownership differs");
    if(!at(memory,out.alert,0xf0,extra,e)||!at(memory,receiver,0x68,subscriber,e)||
       !type(memory,image,subscriber,0x16e75f0,e)||!at(memory,subscriber,0x20,out.subscriber,e)||
       !at(memory,subscriber,0x80,actual_toon,e)||!at(memory,out.toon,0x940,backlink,e))return false;
    if(extra||out.subscriber<0||actual_toon!=out.toon||backlink!=subscriber)
        return wire::fail(e,"VIP Host arrival immutable Subscriber/Toon binding differs");
    uintptr_t name=0;uint64_t size=0;
    if(!at(memory,out.alert,0x88,name,e)||!at(memory,out.alert,0x90,size,e))return false;
    if(size>256||(size&&!name)||name>UINTPTR_MAX-size_t(size))return wire::fail(e,"VIP Host copied alert name bound differs");
    out.name.resize(size_t(size));if(size&&!memory.copy(name,out.name.data(),size_t(size),e))return false;
    if(out.name.find('\0')!=std::string::npos||!npc_census::utf8(out.name))return wire::fail(e,"VIP Host alert name is not safe UTF-8");
    out.active=true;e.clear();return true;
}
}
