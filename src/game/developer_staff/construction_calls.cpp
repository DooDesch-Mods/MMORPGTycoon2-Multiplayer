#include "construction_calls.h"
#include "initial_release/release_entry.h"
#include <Windows.h>
namespace mt2mp::game::developer_staff::construction {
uint32_t reorder(uintptr_t image,uintptr_t manager,uintptr_t actor,Call* out){
    __try{out->attempted=true;
        reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x264420)(manager,actor);
        reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x264180)(manager,actor);
        out->returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}return out->exception;
}
uint32_t unary(uintptr_t image,Unary command,uintptr_t object,Call* out){
    __try{out->attempted=true;
        if(command==Unary::OnMap||command==Unary::OffMap)
            reinterpret_cast<void(*)(void*,bool)>(image+0x3b6710)(reinterpret_cast<void*>(object),command==Unary::OnMap);
        else reinterpret_cast<void(*)(void*)>(image+(command==Unary::UpdateQuadtree?0x3b6660:
            command==Unary::UpdateBuildingColors?0x40f750:0xc3b0c0))(reinterpret_cast<void*>(object));
        out->returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}
    return out->exception;
}
uint32_t stop_loaded_movement(uintptr_t image,uintptr_t actor,Call* out){
    __try{out->attempted=true;
        reinterpret_cast<void(*)(void*)>(image+0x3b8250)(reinterpret_cast<void*>(actor));
        reinterpret_cast<void(*)(void*)>(image+0xc4cf60)(reinterpret_cast<void*>(actor+0x318));
        out->returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}return out->exception;
}
uint32_t membership(uintptr_t image,uintptr_t lounge,uintptr_t actor,bool outside,Call* out){
    __try{out->attempted=true;reinterpret_cast<void(*)(void*,void*)>(image+(outside?0x261c30:0x261c00))(
        reinterpret_cast<void*>(lounge),reinterpret_cast<void*>(actor));out->returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}return out->exception;
}
uint32_t pose(uintptr_t image,uintptr_t actor,const ActorPose* target,const std::array<float,3>* position,Call* out){
    alignas(4) unsigned char transform[0x6c]{};
    __try{reinterpret_cast<void(*)(void*,const float*,const float*,const float*)>(image+0xa09f30)(
        transform,target->orientation.data(),position->data(),target->scale.data());
        out->attempted=true;reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0xc733c0)(actor+0x30,transform);out->returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}return out->exception;
}
uint32_t shift(uintptr_t image,uintptr_t destination,uintptr_t source,size_t bytes,Call* out){
    __try{out->attempted=true;reinterpret_cast<void*(*)(void*,const void*,size_t)>(image+0xa9c7b0)(
        reinterpret_cast<void*>(destination),reinterpret_cast<const void*>(source),bytes);out->returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}return out->exception;
}
uint32_t active_property(uintptr_t image,uintptr_t building,Call* out){
    initial_release::Record record{};bool constructed=false;
    __try{reinterpret_cast<initial_release::RecordCtor>(image+0x970590)(&record,"active true");constructed=true;
        out->record_confirmed=reinterpret_cast<initial_release::RecordEquals>(image+0x111f720)(&record,"active true");
        if(out->record_confirmed){out->attempted=true;out->accepted=reinterpret_cast<bool(*)(const void*,void*,void*,void*)>(image+0xd2c730)(
            reinterpret_cast<void*>(image+0x13d7f80),reinterpret_cast<void*>(building),&record,nullptr);out->returned=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}
    if(constructed){__try{reinterpret_cast<initial_release::RecordDtor>(image+0x9708c0)(&record);out->cleanup_confirmed=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){out->cleanup_exception=GetExceptionCode();}}
    return out->exception;
}
uint32_t link_property(uintptr_t image,uintptr_t property,uintptr_t receiver,bool developer_property,
                       bool attach,void* context,Call* out){
    const char* text=developer_property?(attach?"job 1":"job 0"):(attach?"dev 2":"dev 0");
    initial_release::Record record{};bool constructed=false;
    __try{reinterpret_cast<initial_release::RecordCtor>(image+0x970590)(&record,text);constructed=true;
        out->record_confirmed=reinterpret_cast<initial_release::RecordEquals>(image+0x111f720)(&record,text);
        if(out->record_confirmed){out->attempted=true;
            out->accepted=reinterpret_cast<bool(*)(const void*,void*,void*,void*)>(image+(developer_property?0xe0eae0:0xe0e460))(
                reinterpret_cast<void*>(property),reinterpret_cast<void*>(receiver),&record,context);
            if(out->accepted)reinterpret_cast<void(*)(const void*,void*,void*)>(image+(developer_property?0xe0eca0:0xe0e620))(
                reinterpret_cast<void*>(property),reinterpret_cast<void*>(receiver),context);
            out->returned=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}
    if(constructed){__try{reinterpret_cast<initial_release::RecordDtor>(image+0x9708c0)(&record);out->cleanup_confirmed=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){out->cleanup_exception=GetExceptionCode();}}
    return out->exception;
}
}
