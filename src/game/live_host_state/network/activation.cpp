#include "activation.h"
#include "activation_codes.h"
#include <array>
#include <cstring>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
namespace mt2mp::game::live_host_state::network::activation {
namespace {
bool fail(std::string& e,const char* text){e=text;return false;}
template<class T>bool read(uintptr_t address,T& value){SIZE_T size=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof value,&size)&&size==sizeof value;}
bool owner(const Owner& o,bool activated,std::string& e){
    if(!o.image||!o.node||!o.region||!o.manager||!o.definition||!o.uid||!o.thread||GetCurrentThreadId()!=o.thread)
        return fail(e,"network activation owner/Mainthread absent or differs");
    uintptr_t ap=0,region=0,manager=0,definition=0;uint64_t uid=0;uint8_t flag=0;
    if(!read(o.node,ap)||ap!=o.image+0x16fc700)return fail(e,"network activation Node AddressPoint differs");
    if(!read(o.node+0x18,uid)||uid!=o.uid)return fail(e,"network activation Node UID differs");
    if(!read(o.node+0x178,region)||region!=o.region)return fail(e,"network activation Region differs");
    if(!read(o.region+0x978,manager)||manager!=o.manager||!read(manager,ap)||ap!=o.image+0x17141e0)
        return fail(e,"network activation Manager ownership differs");
    if(!read(o.node+0x258,definition)||definition!=o.definition||!read(definition,ap)||ap!=o.image+0x1738010)
        return fail(e,"network activation Definition ownership differs");
    if(!read(o.node+0x2ec,flag)||flag!=uint8_t(activated))return fail(e,"network activation expected Activated differs");
    return true;
}
bool code(const Owner& o,std::string& e){
    for(const auto& item:codes){
        std::array<unsigned char,512> actual{};SIZE_T size=0;
        if(item.size>actual.size()||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(o.image+item.rva),actual.data(),item.size,&size)||size!=item.size||std::memcmp(actual.data(),item.bytes,item.size))
            return fail(e,"network activation original event/cleanup bytes differ");
    }return true;
}
DWORD mark_raw(const Owner* o)noexcept{
    __try{auto* p=reinterpret_cast<unsigned char*>(o->node);
        if(*reinterpret_cast<uint64_t*>(p+0x18)!=o->uid||p[0x2ec]!=0)return ERROR_INVALID_DATA;
        p[0x2ec]=1;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD construct_raw(uintptr_t image,void* event,uintptr_t node)noexcept{
    __try{reinterpret_cast<void*(*)(void*,void*)>(image+0xd6390)(event,reinterpret_cast<void*>(node));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD send_raw(uintptr_t image,void* event)noexcept{
    __try{reinterpret_cast<void(*)(const void*)>(image+0xd9200)(event);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD cleanup_raw(uintptr_t image,unsigned char* event)noexcept{
    __try{
        *reinterpret_cast<uintptr_t*>(event+0xd0)=image+0x174cca0;
        reinterpret_cast<void(*)(void*)>(image+0x89da30)(event+0xd0);
        reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(event+8);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
class Native final:public detail::Calls {
    union Event {uintptr_t alignment;unsigned char bytes[0x100];};
    Owner owner_;bool expected_,target_;Event event_{};
    bool result(DWORD code,Receipt& r,const char* phase){if(!code)return true;r.seh_code=code;r.cause=std::string("network activation ")+phase+" SEH/compare code="+std::to_string(code);return false;}
public:
    Native(const Owner& o,bool expected,bool target):owner_(o),expected_(expected),target_(target){}
    bool guard(std::string& e)override{return preflight(owner_,expected_,target_,e);}
    bool mark(Receipt& r)override{return result(mark_raw(&owner_),r,"Activated store");}
    bool construct(Receipt& r)override{return result(construct_raw(owner_.image,event_.bytes,owner_.node),r,"AddNodeEvent");}
    bool send(Receipt& r)override{
        int32_t type=0,count=0;uintptr_t region=0,node=0;
        std::memcpy(&type,event_.bytes,4);std::memcpy(&region,event_.bytes+0x28,8);
        std::memcpy(&count,event_.bytes+0x30,4);std::memcpy(&node,event_.bytes+0x50,8);
        if(type!=7||count!=1||region!=owner_.region||node!=owner_.node)return fail(r.cause,"network original AddNodeEvent payload differs");
        if(!read(owner_.image+0x13b9820,r.dispatch_enabled)||r.dispatch_enabled>1)return fail(r.cause,"network original EventManager enabled flag unreadable or invalid");
        return result(send_raw(owner_.image,event_.bytes),r,"SendEvent");
    }
    bool cleanup(Receipt& r)override{return result(cleanup_raw(owner_.image,event_.bytes),r,"event cleanup");}
    bool verify(Receipt& r)override{return owner(owner_,target_,r.cause);}
};
}
bool preflight(const Owner& o,bool expected,bool target,std::string& e){
    e.clear();if(expected&&!target)return fail(e,"network Activated cannot transition 1 to 0");
    return owner(o,expected,e)&&code(o,e);
}
Receipt invoke(const Owner& o,bool expected,bool target){Native native(o,expected,target);return detail::sequence(native,expected,target);}
}
