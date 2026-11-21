#pragma once
#include "duplicate_native.h"
#include "move_kiss/move_kiss.h"
#include <Windows.h>
#include <cstring>
namespace mt2mp::game::duplicate_calls {
inline uint32_t restore_random(uintptr_t owner,const std::array<uint64_t,4>& saved){
    __try{std::memcpy(reinterpret_cast<void*>(owner),saved.data(),sizeof saved);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Array {uintptr_t ap=0;const uintptr_t* data=nullptr;int32_t count=0,capacity=0;};
inline uint32_t region(uintptr_t image,uintptr_t root,bool nearest,const std::array<uint32_t,3>& position,uintptr_t& out){
    __try{out=nearest?reinterpret_cast<uintptr_t(*)(void*,const void*)>(image+0x50eb90)(reinterpret_cast<void*>(root),position.data()):
        reinterpret_cast<uintptr_t(*)(void*,const void*,void*)>(image+0x50eb00)(reinterpret_cast<void*>(root),position.data(),nullptr);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t cost(uintptr_t object,int32_t& amount,bool& allowed){
    __try{const auto table=*reinterpret_cast<uintptr_t**>(object);
        allowed=reinterpret_cast<bool(*)(void*)>(table[0xd8/8])(reinterpret_cast<void*>(object));
        amount=reinterpret_cast<int32_t(*)(void*)>(table[0x278/8])(reinterpret_cast<void*>(object));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t afford(uintptr_t image,uintptr_t business,int32_t amount,bool& allowed){
    __try{allowed=reinterpret_cast<bool(*)(void*,int32_t)>(image+0x55200)(reinterpret_cast<void*>(business),amount);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t charge(uintptr_t image,uintptr_t business,int32_t amount,bool& paid){
    __try{paid=amount<=0||reinterpret_cast<bool(*)(void*,int32_t,int32_t)>(image+0x55280)(reinterpret_cast<void*>(business),amount,6);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t begin(uintptr_t image,bool& opened,uint32_t& cleanup){
    alignas(16) unsigned char name[0x20]{};bool constructed=false;uint32_t code=0;
    __try{
        if(reinterpret_cast<bool(*)()>(image+0x267d30)()){
            reinterpret_cast<void(*)(void*,const char*)>(image+0x5c6c50)(name,"Duplicating collection");constructed=true;
            reinterpret_cast<void(*)(const void*)>(image+0x268400)(name);opened=true;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(constructed)__try{reinterpret_cast<void(*)(void*)>(image+0x11c8b80)(name);}
        __except(EXCEPTION_EXECUTE_HANDLER){cleanup=GetExceptionCode();}
    return code;
}
inline uint32_t end(uintptr_t image){__try{reinterpret_cast<void(*)()>(image+0x268750)();return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
inline uint32_t graph(uintptr_t image,uintptr_t& result){
    __try{result=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0x110);
        if(result)reinterpret_cast<void(*)(void*)>(image+0x5466b0)(reinterpret_cast<void*>(result));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t cash(uintptr_t image,uintptr_t graph,int32_t amount,const std::array<uint32_t,3>& position,uintptr_t& inverse){
    __try{inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0xc8);
        if(!inverse)return 0xe0010001;
        reinterpret_cast<void(*)(void*,int32_t,const void*)>(image+0x543230)(reinterpret_cast<void*>(inverse),amount,position.data());
        reinterpret_cast<void(*)(void*,void*)>(image+0x546d20)(reinterpret_cast<void*>(graph),reinterpret_cast<void*>(inverse));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t duplicate(uintptr_t object,const std::array<unsigned char,0x6c>& transform,uintptr_t& created){
    __try{const auto table=*reinterpret_cast<uintptr_t**>(object);
        created=reinterpret_cast<uintptr_t(*)(void*,const void*)>(table[0xe8/8])(reinterpret_cast<void*>(object),transform.data());return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t destroy_inverse(uintptr_t image,uintptr_t graph,uintptr_t object,uintptr_t& inverse){
    __try{inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0xc0);if(!inverse)return 0xe0010001;
        reinterpret_cast<void(*)(void*,void*)>(image+0x5441f0)(reinterpret_cast<void*>(inverse),reinterpret_cast<void*>(object));
        reinterpret_cast<void(*)(void*,void*)>(image+0x546d20)(reinterpret_cast<void*>(graph),reinterpret_cast<void*>(inverse));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t select(uintptr_t image,uintptr_t manager,const Array& targets){
    __try{reinterpret_cast<void(*)(void*,const Array*)>(image+0x2d9000)(reinterpret_cast<void*>(manager),&targets);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t release(uintptr_t image,uintptr_t points,int32_t edit){
    __try{reinterpret_cast<void(*)(void*,int32_t,int32_t)>(image+0x4f4480)(reinterpret_cast<void*>(points),edit,1);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t commit(uintptr_t image,uintptr_t undo,uintptr_t graph,const Array& targets,uint32_t& cleanup){
    alignas(16) unsigned char name[0x20]{},loc[0x38]{};bool name_ready=false,loc_ready=false;uint32_t code=0;
    __try{reinterpret_cast<void(*)(void*,const char*)>(image+0x5c6c50)(name,"{undo_msg_duplicate}");name_ready=true;
        reinterpret_cast<void(*)(void*,const void*,const Array*)>(image+0x53f740)(loc,name,&targets);loc_ready=true;
        reinterpret_cast<void(*)(void*,const void*,void*)>(image+0x541790)(reinterpret_cast<void*>(undo),loc,reinterpret_cast<void*>(graph));
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(loc_ready){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11be4a0)(loc+0x20);}
        __except(EXCEPTION_EXECUTE_HANDLER){cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11c8b80)(loc);}
        __except(EXCEPTION_EXECUTE_HANDLER){if(!cleanup)cleanup=GetExceptionCode();}
    }
    if(name_ready)__try{reinterpret_cast<void(*)(void*)>(image+0x11c8b80)(name);}
        __except(EXCEPTION_EXECUTE_HANDLER){if(!cleanup)cleanup=GetExceptionCode();}
    return code;
}
}
