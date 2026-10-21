#pragma once
#include "dungeon_cells_calls.h"
#include "duplicate_calls.h"
#include "native_polyshape.h"
#include <array>
#include <Windows.h>
namespace mt2mp::game::dungeon_input_calls {
using Text=dungeon_cells_calls::Text;
struct alignas(16) Transform {
    std::array<unsigned char,0x6c> bytes{};
    std::array<unsigned char,4> alignment_padding{};
};
static_assert(sizeof(Transform)==0x70&&alignof(Transform)==16);
inline uint32_t transform(uintptr_t image,const std::array<float,4>& rotation,const std::array<float,3>& position,
                          const std::array<float,3>& scale,Transform& out)noexcept{
    __try{reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(image+0x9ff370)(out.bytes.data(),rotation.data(),position.data(),scale.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t purchase(uintptr_t image,const Text& type,const std::array<float,3>& position,bool& accepted)noexcept{
    __try{accepted=reinterpret_cast<bool(*)(const Text&,const void*)>(image+0x147a20)(type,position.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t cost(uintptr_t image,const Text& type,int32_t& amount)noexcept{
    __try{amount=reinterpret_cast<int32_t(*)(const Text&)>(image+0x146af0)(type);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t gizmo(uintptr_t image,uintptr_t manager,const Text& type,const Text& variant,const Transform& transform,uintptr_t& created)noexcept{
    __try{created=reinterpret_cast<uintptr_t(*)(void*,const Text&,const Text&,const void*)>(image+0x153920)(reinterpret_cast<void*>(manager),type,variant,transform.bytes.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Edge {int32_t first=-1,second=-1;};
inline uint32_t nearest_edge(uintptr_t image,uintptr_t dungeon,const std::array<float,3>& position,Edge& out)noexcept{
    // The original Win64 call site supplies an explicit result buffer in RCX,
    // Dungeon in RDX and position in R8. It is not a packed integer return.
    __try{reinterpret_cast<void(*)(void*,const void*,const void*)>(image+0xa4f10)(&out,reinterpret_cast<const void*>(dungeon),position.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t edge(uintptr_t image,uintptr_t gizmo,const Edge& value)noexcept{
    __try{reinterpret_cast<void(*)(void*,int32_t,int32_t)>(image+0x145950)(reinterpret_cast<void*>(gizmo),value.first,value.second);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t monster(uintptr_t image,uintptr_t manager,const Transform& transform,int32_t rank,int32_t character_id,uintptr_t& created)noexcept{
    __try{created=reinterpret_cast<uintptr_t(*)(void*,const void*,int32_t,void*,int32_t)>(image+0x293520)
        (reinterpret_cast<void*>(manager),transform.bytes.data(),rank,nullptr,character_id);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Paint {
    ui_terrain_runtime::poly::Storage accumulated;
    uintptr_t stroke=0,pending_stroke=0;bool constructed=false,finish_attempted=false,finished=false;
    float painted=0,erased=0;
};
inline uint32_t shape(uintptr_t image,ui_terrain_runtime::poly::Storage& out,
                      const ui_terrain_runtime::poly::VectorView& paths,bool& constructed)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(image+0xf1810)(&out);constructed=true;
        auto* data=*reinterpret_cast<void**>(out.bytes.data()+0x18);
        reinterpret_cast<void(*)(void*,const void*)>(image+0xf7ed0)(data,&paths);
        reinterpret_cast<void(*)(void*)>(image+0xf3b60)(&out);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t shape_destroy(uintptr_t image,ui_terrain_runtime::poly::Storage& value)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(image+0xf15a0)(&value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t paint_begin(uintptr_t image,Paint& value)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(image+0xf1810)(&value.accumulated);value.constructed=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t paint_stamp(uintptr_t image,uintptr_t canvas,uintptr_t fill,bool erase,
                            const ui_terrain_runtime::poly::Storage& shape,Paint& value)noexcept{
    __try{
        reinterpret_cast<void(*)(void*,const void*)>(image+0xf8c30)(&value.accumulated,&shape);
        if(erase){value.erased+=reinterpret_cast<float(*)(void*,const void*,void*)>(image+0x157720)
            (reinterpret_cast<void*>(canvas),&shape,nullptr);return 0;}
        if(!value.stroke)value.stroke=reinterpret_cast<uintptr_t(*)(void*,const void*,const void*)>(image+0x157680)
            (reinterpret_cast<void*>(canvas),&shape,reinterpret_cast<const void*>(fill));
        if(value.stroke){value.painted+=reinterpret_cast<float(*)(void*,const void*)>(image+0x15d610)
            (reinterpret_cast<void*>(value.stroke),&shape);return 0;}
        const auto area=reinterpret_cast<float(*)(const void*)>(image+0xf1230)(&value.accumulated);
        if(area<=50.0f)return 0;
        value.pending_stroke=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x140);
        if(!value.pending_stroke)return 0xe0010001;
        reinterpret_cast<void(*)(void*)>(image+0x15d440)(reinterpret_cast<void*>(value.pending_stroke));
        reinterpret_cast<void(*)(void*,const void*)>(image+0x15d140)(reinterpret_cast<void*>(value.pending_stroke),&shape);
        reinterpret_cast<void(*)(void*,const void*)>(image+0x15d250)(reinterpret_cast<void*>(value.pending_stroke),reinterpret_cast<const void*>(fill));
        reinterpret_cast<void(*)(void*,void*)>(image+0x1563c0)(reinterpret_cast<void*>(canvas),reinterpret_cast<void*>(value.pending_stroke));
        value.stroke=value.pending_stroke;value.pending_stroke=0;
        reinterpret_cast<void(*)(void*)>(image+0x15d360)(reinterpret_cast<void*>(value.stroke));
        value.painted+=area;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t paint_finish(uintptr_t image,uintptr_t canvas,Paint& value)noexcept{
    if(value.finish_attempted)return 0xe0010002;
    value.finish_attempted=true;
    __try{
        if(value.stroke){reinterpret_cast<void(*)(void*)>(image+0x15d1c0)(reinterpret_cast<void*>(value.stroke));
            value.erased+=reinterpret_cast<float(*)(void*,const void*,void*)>(image+0x157720)
                (reinterpret_cast<void*>(canvas),reinterpret_cast<const void*>(value.stroke+0x18),reinterpret_cast<void*>(value.stroke));
            reinterpret_cast<void(*)(void*)>(image+0x15d3d0)(reinterpret_cast<void*>(value.stroke));value.stroke=0;}
        value.finished=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t paint_inverse(uintptr_t image,uintptr_t dungeon,uintptr_t graph,uintptr_t& inverse)noexcept{
    __try{inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xd8);if(!inverse)return 0xe0010001;
        reinterpret_cast<void(*)(void*,void*)>(image+0x53e6e0)(reinterpret_cast<void*>(inverse),reinterpret_cast<void*>(dungeon));
        reinterpret_cast<void(*)(void*,void*)>(image+0x540760)(reinterpret_cast<void*>(graph),reinterpret_cast<void*>(inverse));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
