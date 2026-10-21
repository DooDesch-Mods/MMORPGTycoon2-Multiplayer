#pragma once
#include <Windows.h>
#include <intrin.h>
#include <array>
#include <cstdint>
#include <cstring>
namespace mt2mp::game::modular_calls {
struct String {const char* data=nullptr;size_t size=0;unsigned char rest[16]{};};
struct Box {uint32_t valid=1;std::array<uint32_t,6> bounds{};};
static_assert(sizeof(Box)==28);
enum class Op {Find,AddBlockPosition,Begin,End,Graph,LayoutInverse,Can,Edit,Release,Purchase,AddBuilding,Scenery,Cost,Cash,Destroy,Commit,Factory,Load,ObjectDestroy,Height,Region,Transform,Theme,ClearPlacement,ConsumePrefab,NewLayout,DiscardGraph};
struct Work {
    Op op=Op::Graph;uintptr_t image=0,object=0,other=0,third=0,fourth=0,result=0;
    int32_t number=0;uint8_t mode=0;bool accepted=false,opened=false,returned=false;
    const void* input=nullptr;const void* extra=nullptr;
    uint32_t cleanup=0;
};
inline uint32_t invoke(Work& w)noexcept{
    alignas(16) unsigned char loc[0x38]{};bool loc_ready=false;uint32_t exception=0;
    __try{switch(w.op){
    case Op::Find:w.result=reinterpret_cast<uintptr_t(*)(void*,void*,void*,int32_t)>(w.image+0x5ba170)(reinterpret_cast<void*>(w.object),reinterpret_cast<void*>(w.other),const_cast<void*>(w.input),w.number);break;
    case Op::AddBlockPosition:reinterpret_cast<void(*)(void*,const void*,const void*)>(w.image+0x405fc0)(const_cast<void*>(w.extra),reinterpret_cast<void*>(w.object),w.input);break;
    case Op::Begin:if(reinterpret_cast<bool(*)()>(w.image+0x264c30)()){reinterpret_cast<void(*)(const void*)>(w.image+0x265300)(w.input);w.opened=true;}break;
    case Op::End:reinterpret_cast<void(*)()>(w.image+0x265650)();break;
    case Op::Graph:w.result=reinterpret_cast<uintptr_t(*)(size_t)>(w.image+0xa02670)(0x110);if(w.result)reinterpret_cast<void(*)(void*)>(w.image+0x5400f0)(reinterpret_cast<void*>(w.result));break;
    case Op::LayoutInverse:w.result=reinterpret_cast<uintptr_t(*)(size_t)>(w.image+0xa02670)(0x2c8);if(w.result){reinterpret_cast<void(*)(void*,void*)>(w.image+0x55d410)(reinterpret_cast<void*>(w.result),reinterpret_cast<void*>(w.other));reinterpret_cast<void(*)(void*,void*)>(w.image+0x540760)(reinterpret_cast<void*>(w.object),reinterpret_cast<void*>(w.result));}break;
    case Op::Can:w.accepted=w.mode==1?reinterpret_cast<bool(*)(const void*,const void*,void*)>(w.image+0x404ec0)(reinterpret_cast<void*>(w.object),w.input,nullptr):reinterpret_cast<bool(*)(const void*,const void*)>(w.image+0x405050)(reinterpret_cast<void*>(w.object),w.input);break;
    case Op::Edit:if(w.mode==1||w.mode==2){reinterpret_cast<void(*)(void*,const void*)>(w.image+(w.mode==1?0x414620:0x414710))(reinterpret_cast<void*>(w.object),w.input);w.accepted=true;}
        else w.accepted=reinterpret_cast<bool(*)(void*,const void*)>(w.image+(w.mode==3?0x413d40:0x413e10))(reinterpret_cast<void*>(w.object),w.input);break;
    // This exact body uses the category/edit values and ignores this/Region.
    case Op::Release:reinterpret_cast<void(*)(void*,int32_t,int32_t,void*)>(w.image+0x58a290)(nullptr,1,1,reinterpret_cast<void*>(w.object));break;
    case Op::Purchase:w.accepted=reinterpret_cast<bool(*)(const void*,const void*)>(w.image+0x40bb10)(w.input,w.extra);break;
    case Op::AddBuilding:w.result=reinterpret_cast<uintptr_t(*)(void*,const void*,const void*,const void*,void*,void*,void*,bool)>(w.image+0x24ed30)(reinterpret_cast<void*>(w.object),w.input,w.extra,reinterpret_cast<void*>(w.other),reinterpret_cast<void*>(w.third),reinterpret_cast<void*>(w.fourth),reinterpret_cast<void*>(w.result),false);break;
    case Op::Scenery:reinterpret_cast<void(*)(void*,const void*,const void*,void*)>(w.image+0x2cd1a0)(reinterpret_cast<void*>(w.object),w.input,w.extra,reinterpret_cast<void*>(w.other));break;
    case Op::Cost:{const auto table=*reinterpret_cast<uintptr_t**>(w.object);w.number=reinterpret_cast<int32_t(*)(const void*)>(table[0x278/8])(reinterpret_cast<void*>(w.object));break;}
    case Op::Cash:w.result=reinterpret_cast<uintptr_t(*)(size_t)>(w.image+0xa02670)(0xc8);if(w.result){reinterpret_cast<void(*)(void*,int32_t,const void*)>(w.image+0x53ccb0)(reinterpret_cast<void*>(w.result),w.number,w.input);reinterpret_cast<void(*)(void*,void*)>(w.image+0x540760)(reinterpret_cast<void*>(w.object),reinterpret_cast<void*>(w.result));}break;
    case Op::Destroy:w.result=reinterpret_cast<uintptr_t(*)(size_t)>(w.image+0xa02670)(0xc0);if(w.result){reinterpret_cast<void(*)(void*,void*)>(w.image+0x53dc30)(reinterpret_cast<void*>(w.result),reinterpret_cast<void*>(w.other));reinterpret_cast<void(*)(void*,void*)>(w.image+0x540760)(reinterpret_cast<void*>(w.object),reinterpret_cast<void*>(w.result));}break;
    case Op::Commit:reinterpret_cast<void(*)(void*,const void*,void*)>(w.image+0x538ae0)(loc,w.input,reinterpret_cast<void*>(w.third));loc_ready=true;reinterpret_cast<void(*)(void*,const void*,void*)>(w.image+0x53b260)(reinterpret_cast<void*>(w.object),loc,reinterpret_cast<void*>(w.other));break;
    case Op::Factory:w.result=reinterpret_cast<uintptr_t(*)(const void*)>(w.image+(w.mode?0xd87ef0:0xd8ad90))(nullptr);break;
    case Op::Load:reinterpret_cast<void(*)(void*,const void*,void*)>(w.image+(w.mode?0xc9ab40:0xcabf50))(reinterpret_cast<void*>(w.object),w.input,reinterpret_cast<void*>(w.other));break;
    case Op::ObjectDestroy:reinterpret_cast<void(*)(void*)>(w.image+(w.mode?0xbd6a20:0x19c030))(reinterpret_cast<void*>(w.object));break;
    case Op::Height:{const auto value=reinterpret_cast<float(*)(const void*,const void*)>(w.image+0x32ace0)(reinterpret_cast<void*>(w.object),w.input);std::memcpy(&w.number,&value,4);break;}
    case Op::Region:w.result=reinterpret_cast<uintptr_t(*)(const void*,const void*,const void*)>(w.image+0x32a990)(reinterpret_cast<void*>(w.object),w.input,nullptr);break;
    case Op::Transform:reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(w.image+0x9ff370)(reinterpret_cast<void*>(w.object),w.input,static_cast<const unsigned char*>(w.input)+16,static_cast<const unsigned char*>(w.input)+28);break;
    case Op::Theme:{const auto manager=reinterpret_cast<void*(*)()>(w.image+0xb319d0)();w.result=reinterpret_cast<uintptr_t(*)(void*,const void*)>(w.image+0x2530d0)(manager,w.input);break;}
    case Op::ClearPlacement:reinterpret_cast<void(*)(void*)>(w.image+0x5f8c20)(reinterpret_cast<void*>(w.object));break;
    case Op::ConsumePrefab:w.accepted=_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(w.object+0x671),0,1)==1;if(w.accepted)reinterpret_cast<void(*)(void*)>(w.image+0x5f8c20)(reinterpret_cast<void*>(w.object+0x3d8));break;
    case Op::NewLayout:reinterpret_cast<void(*)(void*)>(w.image+0x5bbb40)(reinterpret_cast<void*>(w.object));break;
    case Op::DiscardGraph:reinterpret_cast<void(*)(void*)>(w.image+0x540b90)(reinterpret_cast<void*>(w.object));reinterpret_cast<void(*)(void*,size_t)>(w.image+0x1264b60)(reinterpret_cast<void*>(w.object),0x110);break;
    }w.returned=true;}__except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}
    if(loc_ready){__try{reinterpret_cast<void(*)(void*)>(w.image+0x11b2b00)(loc+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){w.cleanup=GetExceptionCode();}__try{reinterpret_cast<void(*)(void*)>(w.image+0x11bd1e0)(loc);}__except(EXCEPTION_EXECUTE_HANDLER){if(!w.cleanup)w.cleanup=GetExceptionCode();}}
    return exception;
}
}
