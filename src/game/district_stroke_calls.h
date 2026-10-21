#pragma once
#include "typed_action/district_stroke_action.h"
#include "scenery_stroke_calls.h"

namespace mt2mp::game::district_stroke_calls {
using Paint=void(*)(void*,const void*);
using Finish=void(*)(void*,const void*);
struct Cursor {
    uintptr_t image=0,object=0;
    bool constructor_entered=false,constructed=false,press_entered=false,press_returned=false;
    bool finish_entered=false,finish_returned=false,destructor_entered=false,destroyed=false;
    size_t stamps_entered=0,stamps_returned=0;
};
inline uint32_t begin_edit(Cursor& owner,bool& opened){
    scenery_stroke_calls::NativeString label{"Paint District",14,{}};
    __try{
        if(*reinterpret_cast<uintptr_t*>(owner.object+0x310))return 0xE0040008;
        const auto scope=reinterpret_cast<uintptr_t(*)(size_t)>(owner.image+0xa02670)(1);
        if(!scope)return 0xE0040009;
        *reinterpret_cast<uint8_t*>(scope)=0;*reinterpret_cast<uintptr_t*>(owner.object+0x310)=scope;
        if(reinterpret_cast<bool(*)()>(owner.image+0x264c30)()){
            reinterpret_cast<void(*)(const void*)>(owner.image+0x265300)(&label);
            *reinterpret_cast<uint8_t*>(scope)=1;opened=true;
        }
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t construct(Cursor& owner,const action_session::DistrictStrokeIntent& intent,uintptr_t region,uintptr_t selected,const void* clip){
    if(owner.object||owner.constructor_entered)return 0xE0040001;
    __try{
        owner.object=reinterpret_cast<uintptr_t(*)(size_t)>(owner.image+0xa02670)(0x7a0);
        if(!owner.object)return 0xE0040002;
        owner.constructor_entered=true;
        reinterpret_cast<void(*)(void*)>(owner.image+0x5af420)(reinterpret_cast<void*>(owner.object));owner.constructed=true;
        *reinterpret_cast<uintptr_t*>(owner.object+0x1d8)=region;
        *reinterpret_cast<uintptr_t*>(owner.object+0x760)=selected;
        *reinterpret_cast<int32_t*>(owner.object+0x160)=intent.geo;
        *reinterpret_cast<int32_t*>(owner.object+0x1d0)=intent.kind==action_session::DistrictStrokeKind::Erase?2:0;
        std::memcpy(reinterpret_cast<void*>(owner.object+0x188),intent.color.data(),16);
        std::memcpy(reinterpret_cast<void*>(owner.object+0x198),intent.water_color.data(),16);
        *reinterpret_cast<uint8_t*>(owner.object+0x1c9)=uint8_t(intent.change_color);
        *reinterpret_cast<uint8_t*>(owner.object+0x1ca)=uint8_t(intent.change_water_color);
        reinterpret_cast<void(*)(void*,const void*)>(owner.image+0xfa330)(reinterpret_cast<void*>(owner.object+0x368),clip);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t press(Cursor& owner,uintptr_t cursor,const std::array<uint32_t,3>& point){
    if(!owner.constructed||owner.press_entered)return 0xE0040003;
    __try{
        owner.press_entered=true;
        reinterpret_cast<void(*)(void*,void*,const void*)>(owner.image+0x5ad320)(reinterpret_cast<void*>(owner.object),reinterpret_cast<void*>(cursor),point.data());
        owner.press_returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t paint(Cursor& owner,Paint original,const void* shape){
    __try{++owner.stamps_entered;original(reinterpret_cast<void*>(owner.object),shape);++owner.stamps_returned;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t finish(Cursor& owner,Finish original,const std::array<uint32_t,3>& point){
    if(owner.finish_entered)return 0xE0040004;
    __try{owner.finish_entered=true;original(reinterpret_cast<void*>(owner.object),point.data());owner.finish_returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t discard_capture(uintptr_t image,uintptr_t receiver,uintptr_t graph,Finish finish_original,const void* point,bool& destroyed,bool& returned){
    __try{
        if(*reinterpret_cast<uintptr_t*>(receiver+0x790)!=graph)return 0xE0040005;
        reinterpret_cast<void(*)(void*)>(image+0x540b90)(reinterpret_cast<void*>(graph));
        reinterpret_cast<void(*)(void*,size_t)>(image+0x1264b60)(reinterpret_cast<void*>(graph),0x110);
        destroyed=true;*reinterpret_cast<uintptr_t*>(receiver+0x790)=0;
        finish_original(reinterpret_cast<void*>(receiver),point);returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t destroy(Cursor& owner){
    if(!owner.constructed||owner.destructor_entered)return 0xE0040006;
    __try{
        if(*reinterpret_cast<uintptr_t*>(owner.object+0x790)||*reinterpret_cast<uintptr_t*>(owner.object+0x310))return 0xE0040007;
        owner.destructor_entered=true;
        reinterpret_cast<void(*)(void*)>(owner.image+0x5af810)(reinterpret_cast<void*>(owner.object));owner.destroyed=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t remove(uintptr_t image,uintptr_t manager,uintptr_t object,bool& entered,bool& returned){
    __try{entered=true;reinterpret_cast<void(*)(void*,void*)>(image+0x2d5520)(reinterpret_cast<void*>(manager),reinterpret_cast<void*>(object));returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
