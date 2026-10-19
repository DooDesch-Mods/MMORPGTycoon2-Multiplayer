#pragma once
#include "typed_action/scenery_stroke_action.h"
#include "native_polyshape.h"
#include <Windows.h>
#include <array>
#include <cstring>

namespace mt2mp::game::scenery_stroke_calls {
struct Array {
    uintptr_t ap=0,data=0;int32_t count=0,capacity=0;
};
static_assert(sizeof(Array)==24);
inline uint32_t generate(uintptr_t image,uintptr_t pattern,const void* shape,Array& out,bool& constructed){
    __try{
        out={image+0x1746400,0,0,0};constructed=true;
        auto* table=*reinterpret_cast<uintptr_t**>(pattern);
        reinterpret_cast<void(*)(void*,void*,const void*)>(table[2])(reinterpret_cast<void*>(pattern),&out,shape);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t destroy_generated(uintptr_t image,Array& value){
    __try{reinterpret_cast<void(*)(void*)>(image+0xc3b020)(&value);value={};return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Transfer {
    uintptr_t image=0,data=0,transferred_data=0;
    int32_t count=0;bool attempted=false,prepared=false,transferred=false;
    uint32_t cause=0;
};
inline uint32_t prepare(Transfer& out,const std::vector<action_session::SceneryStrokePose>& dots){
    if(out.attempted||dots.size()>1000000)return 0xE0030001;out.attempted=true;
    __try{
        if(!dots.empty()){
            out.data=reinterpret_cast<uintptr_t(*)(size_t)>(out.image+0xa026c0)(dots.size()*0x6c);
            if(!out.data)return 0xE0030002;
            for(size_t index=0;index<dots.size();++index){const auto& pose=dots[index];
                reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(out.image+0x9ff370)(
                    reinterpret_cast<void*>(out.data+index*0x6c),pose.data(),pose.data()+4,pose.data()+7);
            }
        }
        out.count=int32_t(dots.size());out.prepared=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t release_untransferred(Transfer& value){
    if(value.transferred||!value.data)return 0;
    __try{reinterpret_cast<void(*)(void*)>(value.image+0xa01280)(reinterpret_cast<void*>(value.data));value.data=0;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Pattern {
    const uintptr_t* ap=nullptr;Transfer* value=nullptr;
};
inline void unused_destructor(Pattern* pattern){pattern->value->cause=0xE0030003;}
inline void transfer(Pattern* pattern,Array* output,const void*){
    auto& value=*pattern->value;
    __try{
        if(!value.prepared||value.transferred||output->ap!=value.image+0x1746400||output->count<0||
           output->capacity<output->count||output->capacity>1000000||(!output->data&&output->capacity)){
            value.cause=0xE0030004;return;
        }
        if(output->data)reinterpret_cast<void(*)(void*)>(value.image+0xa01280)(reinterpret_cast<void*>(output->data));
        output->data=value.data;output->count=value.count;output->capacity=value.count;
        value.transferred_data=value.data;value.data=0;value.transferred=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){value.cause=GetExceptionCode();}
}
inline const std::array<uintptr_t,3> pattern_methods{
    reinterpret_cast<uintptr_t>(&unused_destructor),reinterpret_cast<uintptr_t>(&unused_destructor),reinterpret_cast<uintptr_t>(&transfer)};
using Paint=void(*)(void*,const void*);
inline uint32_t paint(Paint original,void* receiver,const void* shape,Transfer* dots,bool& entered,bool& restored){
    uintptr_t previous=0;Pattern pattern{pattern_methods.data(),dots};entered=false;restored=false;
    __try{
        auto* slot=reinterpret_cast<uintptr_t*>(static_cast<unsigned char*>(receiver)+0xbc0);
        if(dots){if(!dots->prepared||dots->transferred)return 0xE0030005;previous=*slot;*slot=reinterpret_cast<uintptr_t>(&pattern);}
        __try{entered=true;original(receiver,shape);}
        __finally{if(dots)*slot=previous;restored=true;}
        return dots?dots->cause:0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct NativeString {const char* data=nullptr;size_t size=0;unsigned char rest[16]{};};
struct Cursor {
    uintptr_t image=0,object=0,options=0;
    bool brush=false,constructor_entered=false,constructed=false,options_constructed=false;
    bool destruction_entered=false,destroyed=false,options_destruction_entered=false,options_destroyed=false;
};
inline uint32_t construct(Cursor& owner,const action_session::SceneryStrokeIntent& intent,uintptr_t region,int32_t category,int32_t variant){
    if(owner.object||owner.constructor_entered)return 0xE0030006;
    NativeString directory{intent.type.data(),intent.type.size(),{}};owner.brush=intent.kind==action_session::SceneryStrokeKind::Paint;
    __try{
        owner.object=reinterpret_cast<uintptr_t(*)(size_t)>(owner.image+0xa02670)(owner.brush?0xc58:0x5c8);
        if(!owner.object)return 0xE0030007;
        owner.constructor_entered=true;
        reinterpret_cast<void(*)(void*,const void*)>(owner.image+(owner.brush?0x5d0770:0x5d63b0))(reinterpret_cast<void*>(owner.object),&directory);
        owner.constructed=true;
        if(owner.brush){
            owner.options=reinterpret_cast<uintptr_t(*)(size_t)>(owner.image+0xa02670)(0xd0);
            if(!owner.options)return 0xE0030008;
            reinterpret_cast<void(*)(void*)>(owner.image+0x5fcb00)(reinterpret_cast<void*>(owner.options));owner.options_constructed=true;
            *reinterpret_cast<uint8_t*>(owner.options+0x60)=intent.replace?1:0;
            *reinterpret_cast<uintptr_t*>(owner.object+0xb68)=owner.options;
            *reinterpret_cast<uintptr_t*>(owner.object+0xb60)=region;
            *reinterpret_cast<int32_t*>(owner.object+0x174)=intent.mode;
            *reinterpret_cast<int32_t*>(owner.object+0x178)=category;
            *reinterpret_cast<int32_t*>(owner.object+0x17c)=variant;
            std::memcpy(reinterpret_cast<void*>(owner.object+0x894),intent.bounds.data(),24);
        }else *reinterpret_cast<uintptr_t*>(owner.object+0x588)=region;
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t destroy(Cursor& owner){
    if(!owner.constructed||owner.destruction_entered)return 0xE0030009;owner.destruction_entered=true;
    __try{reinterpret_cast<void(*)(void*)>(owner.image+(owner.brush?0x5d0fc0:0x5d72d0))(reinterpret_cast<void*>(owner.object));owner.destroyed=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t destroy_options(Cursor& owner){
    if(!owner.options_constructed||owner.options_destruction_entered)return 0xE003000A;owner.options_destruction_entered=true;
    __try{reinterpret_cast<void(*)(void*)>(owner.image+0xbefc50)(reinterpret_cast<void*>(owner.options));owner.options_destroyed=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t begin_edit(Cursor& owner,bool& opened,uint32_t& cleanup){
    alignas(16) unsigned char label[0x20]{};bool label_ready=false;uint32_t code=0;
    if(!owner.brush)return 0;
    __try{
        if(reinterpret_cast<bool(*)()>(owner.image+0x264c30)()){
            const auto scope=reinterpret_cast<uintptr_t(*)(size_t)>(owner.image+0xa02670)(1);
            if(!scope)return 0xE003000D;
            *reinterpret_cast<uint8_t*>(scope)=0;*reinterpret_cast<uintptr_t*>(owner.object+0x360)=scope;
            reinterpret_cast<void(*)(void*,const char*)>(owner.image+0x5c04b0)(label,"Painting scenery");label_ready=true;
            reinterpret_cast<void(*)(const void*)>(owner.image+0x265300)(label);
            *reinterpret_cast<uint8_t*>(scope)=1;opened=true;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(label_ready)__try{reinterpret_cast<void(*)(void*)>(owner.image+0x11bd1e0)(label);}
        __except(EXCEPTION_EXECUTE_HANDLER){cleanup=GetExceptionCode();}
    return code;
}
inline uint32_t shape_make(uintptr_t image,ui_terrain_runtime::poly::Storage& out,const ui_terrain_runtime::poly::VectorView& paths,bool& constructed){
    __try{
        reinterpret_cast<void(*)(void*)>(image+0xf1810)(&out);constructed=true;
        auto* data=*reinterpret_cast<void**>(out.bytes.data()+0x18);
        reinterpret_cast<void(*)(void*,const void*)>(image+0xf7ed0)(data,&paths);
        reinterpret_cast<void(*)(void*)>(image+0xf3b60)(&out);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t shape_destroy(uintptr_t image,ui_terrain_runtime::poly::Storage& value){
    __try{reinterpret_cast<void(*)(void*)>(image+0xf15a0)(&value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
using Finish=void(*)(void*);
struct Context {
    uintptr_t manager=0,cursor=0,root=0;
    std::array<uint32_t,3> position{};
    bool entered=false,installed=false,restored=false;
};
inline uint32_t enter(Context& owner,uintptr_t root,const std::array<uint32_t,3>& position){
    if(owner.entered)return 0xE003000B;owner.entered=true;
    __try{
        owner.root=*reinterpret_cast<uintptr_t*>(owner.manager+0x60);
        std::memcpy(owner.position.data(),reinterpret_cast<const void*>(owner.cursor+0x1e4),12);
        owner.installed=true;
        *reinterpret_cast<uintptr_t*>(owner.manager+0x60)=root;
        std::memcpy(reinterpret_cast<void*>(owner.cursor+0x1e4),position.data(),12);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t restore(Context& owner){
    if(!owner.installed||owner.restored)return 0xE003000C;
    __try{
        *reinterpret_cast<uintptr_t*>(owner.manager+0x60)=owner.root;
        std::memcpy(reinterpret_cast<void*>(owner.cursor+0x1e4),owner.position.data(),12);owner.restored=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t finish(Finish original,uintptr_t receiver,bool& entered,bool& returned){
    __try{entered=true;original(reinterpret_cast<void*>(receiver));returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t seed(uintptr_t global,uintptr_t region,const action_session::SceneryStrokeState& state){
    __try{
        std::memcpy(reinterpret_cast<void*>(global),state.random.data(),32);
        std::memcpy(reinterpret_cast<void*>(region),state.region_random.data(),32);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
