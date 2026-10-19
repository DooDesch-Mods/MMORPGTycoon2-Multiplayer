#pragma once
#include "duplicate_native.h"
#include "move_kiss/move_kiss.h"
#include <Windows.h>
namespace mt2mp::game::duplicate_calls {
struct Array {uintptr_t ap=0;const uintptr_t* data=nullptr;int32_t count=0,capacity=0;};
inline uint32_t region(uintptr_t image,uintptr_t root,bool nearest,const std::array<uint32_t,3>& position,uintptr_t& out){
    __try{out=nearest?reinterpret_cast<uintptr_t(*)(void*,const void*)>(image+0x508a40)(reinterpret_cast<void*>(root),position.data()):
        reinterpret_cast<uintptr_t(*)(void*,const void*,void*)>(image+0x5089b0)(reinterpret_cast<void*>(root),position.data(),nullptr);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t cost(uintptr_t object,int32_t& amount,bool& allowed){
    __try{const auto table=*reinterpret_cast<uintptr_t**>(object);
        allowed=reinterpret_cast<bool(*)(void*)>(table[0xd8/8])(reinterpret_cast<void*>(object));
        amount=reinterpret_cast<int32_t(*)(void*)>(table[0x278/8])(reinterpret_cast<void*>(object));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t afford(uintptr_t image,uintptr_t business,int32_t amount,bool& allowed){
    __try{allowed=reinterpret_cast<bool(*)(void*,int32_t)>(image+0x54d00)(reinterpret_cast<void*>(business),amount);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t charge(uintptr_t image,uintptr_t business,int32_t amount,bool& paid){
    __try{paid=amount<=0||reinterpret_cast<bool(*)(void*,int32_t,int32_t)>(image+0x54d80)(reinterpret_cast<void*>(business),amount,6);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t begin(uintptr_t image,bool& opened,uint32_t& cleanup){
    alignas(16) unsigned char name[0x20]{};bool constructed=false;uint32_t code=0;
    __try{
        if(reinterpret_cast<bool(*)()>(image+0x264c30)()){
            reinterpret_cast<void(*)(void*,const char*)>(image+0x5c04b0)(name,"Duplicating collection");constructed=true;
            reinterpret_cast<void(*)(const void*)>(image+0x265300)(name);opened=true;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(constructed)__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(name);}
        __except(EXCEPTION_EXECUTE_HANDLER){cleanup=GetExceptionCode();}
    return code;
}
inline uint32_t end(uintptr_t image){__try{reinterpret_cast<void(*)()>(image+0x265650)();return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
inline uint32_t graph(uintptr_t image,uintptr_t& result){
    __try{result=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x110);
        if(result)reinterpret_cast<void(*)(void*)>(image+0x5400f0)(reinterpret_cast<void*>(result));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t cash(uintptr_t image,uintptr_t graph,int32_t amount,const std::array<uint32_t,3>& position,uintptr_t& inverse){
    __try{inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xc8);
        if(!inverse)return 0xe0010001;
        reinterpret_cast<void(*)(void*,int32_t,const void*)>(image+0x53ccb0)(reinterpret_cast<void*>(inverse),amount,position.data());
        reinterpret_cast<void(*)(void*,void*)>(image+0x540760)(reinterpret_cast<void*>(graph),reinterpret_cast<void*>(inverse));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t duplicate(uintptr_t object,const std::array<unsigned char,0x6c>& transform,uintptr_t& created){
    __try{const auto table=*reinterpret_cast<uintptr_t**>(object);
        created=reinterpret_cast<uintptr_t(*)(void*,const void*)>(table[0xe8/8])(reinterpret_cast<void*>(object),transform.data());return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t destroy_inverse(uintptr_t image,uintptr_t graph,uintptr_t object,uintptr_t& inverse){
    __try{inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xc0);if(!inverse)return 0xe0010001;
        reinterpret_cast<void(*)(void*,void*)>(image+0x53dc30)(reinterpret_cast<void*>(inverse),reinterpret_cast<void*>(object));
        reinterpret_cast<void(*)(void*,void*)>(image+0x540760)(reinterpret_cast<void*>(graph),reinterpret_cast<void*>(inverse));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t select(uintptr_t image,uintptr_t manager,const Array& targets){
    __try{reinterpret_cast<void(*)(void*,const Array*)>(image+0x2d5910)(reinterpret_cast<void*>(manager),&targets);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t release(uintptr_t image,uintptr_t points,int32_t edit){
    __try{reinterpret_cast<void(*)(void*,int32_t,int32_t)>(image+0x4ee5c0)(reinterpret_cast<void*>(points),edit,1);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t commit(uintptr_t image,uintptr_t undo,uintptr_t graph,const Array& targets,uint32_t& cleanup){
    alignas(16) unsigned char name[0x20]{},loc[0x38]{};bool name_ready=false,loc_ready=false;uint32_t code=0;
    __try{reinterpret_cast<void(*)(void*,const char*)>(image+0x5c04b0)(name,"{undo_msg_duplicate}");name_ready=true;
        reinterpret_cast<void(*)(void*,const void*,const Array*)>(image+0x539230)(loc,name,&targets);loc_ready=true;
        reinterpret_cast<void(*)(void*,const void*,void*)>(image+0x53b260)(reinterpret_cast<void*>(undo),loc,reinterpret_cast<void*>(graph));
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(loc_ready){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(loc+0x20);}
        __except(EXCEPTION_EXECUTE_HANDLER){cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(loc);}
        __except(EXCEPTION_EXECUTE_HANDLER){if(!cleanup)cleanup=GetExceptionCode();}
    }
    if(name_ready)__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(name);}
        __except(EXCEPTION_EXECUTE_HANDLER){if(!cleanup)cleanup=GetExceptionCode();}
    return code;
}
}
