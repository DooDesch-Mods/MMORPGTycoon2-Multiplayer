#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <Windows.h>

namespace mt2mp::game::zone_edit_calls {
struct Choices {uintptr_t character=0,density_text=0,level_text=0;int32_t density=-1;};
inline uint32_t choices(uintptr_t image,uintptr_t window,Choices& out)noexcept{
    __try{
        out={};
        const auto type=*reinterpret_cast<const uintptr_t*>(window+0x568);
        const auto density=*reinterpret_cast<const uintptr_t*>(window+0x578);
        const auto level=*reinterpret_cast<const uintptr_t*>(window+0x570);
        if(!type||!density||!level)return 0xe0010003;
        const auto selected=reinterpret_cast<const void*(*)(const void*)>(image+0x6ab190)(reinterpret_cast<const void*>(type));
        const auto manager=*reinterpret_cast<const uintptr_t*>(image+0x13df7a0);
        out.character=reinterpret_cast<uintptr_t(*)(void*,int32_t,const void*)>(image+0x9e640)(reinterpret_cast<void*>(manager),1,selected);
        out.density_text=reinterpret_cast<uintptr_t(*)(const void*)>(image+0x6ab190)(reinterpret_cast<const void*>(density));
        out.density=reinterpret_cast<int32_t(*)(const void*)>(image+0x451d80)(reinterpret_cast<const void*>(out.density_text));
        out.level_text=reinterpret_cast<uintptr_t(*)(const void*)>(image+0x6ab190)(reinterpret_cast<const void*>(level));
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct ExistingReceipt {
    uintptr_t graph=0,node=0;
    bool label_constructed=false,label_destroyed=false,open_attempted=false,open_returned=false;
    bool edit_created=false,edit_added=false,effect_attempted=false,effect_returned=false,close_attempted=false,close_returned=false;
};
// This is the original ZoneApply/ZoneReset command sequence, with values read
// from the initiating peer's real controls instead of the Host's local window.
inline uint32_t existing(uintptr_t image,uintptr_t zone,bool apply,int32_t type,int32_t density,int32_t level,
    ExistingReceipt& receipt)noexcept{
    alignas(16) unsigned char label[0x40]{};
    __try{
        reinterpret_cast<void(*)(void*,const char*)>(image+0xa16f10)(label,apply?"{undo_msg_zoneedit}":"{undo_msg_zonereset}");receipt.label_constructed=true;
        receipt.open_attempted=true;reinterpret_cast<void(*)(const void*)>(image+0x539990)(label);receipt.open_returned=true;
        reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(label+0x20);
        reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);receipt.label_destroyed=true;
        receipt.graph=reinterpret_cast<uintptr_t(*)()>(image+0x537890)();if(!receipt.graph)return 0xe0010003;
        if(apply){
            receipt.node=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x150);
            if(!receipt.node)return 0xe0010003;
            reinterpret_cast<void(*)(void*,const void*)>(image+0x55c2f0)(reinterpret_cast<void*>(receipt.node),reinterpret_cast<const void*>(zone));receipt.edit_created=true;
            reinterpret_cast<void(*)(void*,void*)>(image+0x540760)(reinterpret_cast<void*>(receipt.graph),reinterpret_cast<void*>(receipt.node));receipt.edit_added=true;
            receipt.effect_attempted=true;
            reinterpret_cast<void(*)(void*,int32_t)>(image+0x455380)(reinterpret_cast<void*>(zone),type);
            reinterpret_cast<void(*)(void*,int32_t)>(image+0x451b80)(reinterpret_cast<void*>(zone),density);
            reinterpret_cast<void(*)(void*,int32_t)>(image+0x456390)(reinterpret_cast<void*>(zone),level);
        }
        receipt.effect_attempted=true;reinterpret_cast<void(*)(void*)>(image+0x456ed0)(reinterpret_cast<void*>(zone));receipt.effect_returned=true;
        receipt.close_attempted=true;reinterpret_cast<void(*)()>(image+0x5379c0)();receipt.close_returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t respawn(uintptr_t image,uintptr_t zone,bool& attempted,bool& returned)noexcept{
    __try{attempted=true;reinterpret_cast<void(*)(void*)>(image+0x4538d0)(reinterpret_cast<void*>(zone));returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
using Press=void(*)(void*,void*,const float*);
using Finish=void(*)(void*,const void*);
inline uint32_t discard_capture(uintptr_t image,uintptr_t brush,uintptr_t graph,Finish finish,
                               const void* shape,bool& destroyed,bool& returned)noexcept{
    __try{
        reinterpret_cast<void(*)(void*)>(image+0x540b90)(reinterpret_cast<void*>(graph));
        reinterpret_cast<void(*)(void*,size_t)>(image+0x1264b60)(reinterpret_cast<void*>(graph),0x110);destroyed=true;
        *reinterpret_cast<uintptr_t*>(brush+0x740)=0;
        finish(reinterpret_cast<void*>(brush),shape);returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct alignas(16) Brush {
    std::array<unsigned char,0x750> bytes{};
    uintptr_t graph=0;
    bool constructed=false,destructor_entered=false,finish_entered=false,finish_returned=false;
    uint32_t stamps_entered=0,stamps_returned=0;
};
// Populate the private brush with the same native-owned fields that HandlePress
// establishes. Its collision receiver belongs to the initiating peer, so the
// admitted canonical selected Zone supplies that field on both executors.
inline uint32_t construct(uintptr_t image,Brush& brush,uintptr_t region,uintptr_t window,uintptr_t selected,
                          int32_t zone_type,int32_t level,bool add,const uint32_t* press)noexcept{
    __try{
        auto* value=brush.bytes.data();
        reinterpret_cast<void(*)(void*)>(image+0x5f1a30)(value);brush.constructed=true;
        *reinterpret_cast<uintptr_t*>(value+0x170)=region;
        *reinterpret_cast<int32_t*>(value+0x178)=zone_type;*reinterpret_cast<int32_t*>(value+0x17c)=level;
        *reinterpret_cast<uintptr_t*>(value+0x5d8)=window;
        for(size_t index=0;index<3;++index){
            *reinterpret_cast<uint32_t*>(value+0x5c0+index*4)=press[index];
            *reinterpret_cast<uint32_t*>(value+0x5cc+index*4)=index==1?0:press[index];
        }
        *reinterpret_cast<int32_t*>(value+0x6fc)=add?1:2;value[0x700]=add?1:0;
        *reinterpret_cast<uint64_t*>(value+0x704)=0;
        *reinterpret_cast<uintptr_t*>(value+0x710)=selected;value[0x718]=1;value[0x719]=0;
        *reinterpret_cast<int32_t*>(value+0x730)=0;*reinterpret_cast<int32_t*>(value+0x738)=0;
        *reinterpret_cast<int32_t*>(value+0x748)=0;
        brush.graph=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x110);
        if(!brush.graph)return 0xe0010003;
        reinterpret_cast<void(*)(void*)>(image+0x5400f0)(reinterpret_cast<void*>(brush.graph));
        *reinterpret_cast<uintptr_t*>(value+0x740)=brush.graph;
        reinterpret_cast<void(*)(void*,const void*)>(image+0xfa330)(value+0x1d8,*reinterpret_cast<void**>(region+0x28));
        if(selected){
            auto*& members=*reinterpret_cast<uintptr_t**>(value+0x728);
            auto& capacity=*reinterpret_cast<int32_t*>(value+0x734);
            if(capacity<1){
                auto* allocated=reinterpret_cast<uintptr_t*(*)(size_t)>(image+0xa026c0)(4*sizeof(uintptr_t));
                if(!allocated)return 0xe0010003;
                if(members)reinterpret_cast<void(*)(void*)>(image+0xa01280)(members);
                members=allocated;capacity=4;
            }
            if(!members)return 0xe0010003;
            members[0]=selected;*reinterpret_cast<int32_t*>(value+0x730)=1;
            auto* node=reinterpret_cast<void*(*)(size_t)>(image+0xa02670)(0x150);
            if(!node)return 0xe0010003;
            reinterpret_cast<void(*)(void*,const void*)>(image+0x55c2f0)(node,reinterpret_cast<const void*>(selected));
            reinterpret_cast<void(*)(void*,void*)>(image+0x540760)(reinterpret_cast<void*>(brush.graph),node);
            reinterpret_cast<void(*)(void*,const void*)>(image+0xfa330)(value+0x680,reinterpret_cast<const void*>(selected+0x2f0));
            reinterpret_cast<void(*)(void*,const void*)>(image+0xf97f0)(value+0x680,reinterpret_cast<const void*>(selected+0x110));
        }
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t paint(Brush& brush,void(*original)(void*,const void*),const float* position,const void* shape)noexcept{
    __try{
        for(size_t index=0;index<3;++index)*reinterpret_cast<float*>(brush.bytes.data()+0x5cc+index*4)=position[index];
        ++brush.stamps_entered;original(brush.bytes.data(),shape);++brush.stamps_returned;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t finish(Brush& brush,Finish original)noexcept{
    __try{brush.finish_entered=true;original(brush.bytes.data(),nullptr);brush.finish_returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t destroy(uintptr_t image,Brush& brush)noexcept{
    __try{brush.destructor_entered=true;reinterpret_cast<void(*)(void*)>(image+0x5f1c20)(brush.bytes.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t remove(uintptr_t image,uintptr_t manager,uintptr_t object,bool& entered,bool& returned)noexcept{
    __try{entered=true;reinterpret_cast<void(*)(void*,void*)>(image+0x2d5520)(reinterpret_cast<void*>(manager),reinterpret_cast<void*>(object));returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
