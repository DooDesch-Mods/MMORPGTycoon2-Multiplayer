#pragma once
#include "control_native_calls.h"

#if defined(_WIN32) && defined(_MSC_VER)
namespace mt2mp::control_input {
struct PresentationBinding {
    uintptr_t image{},player{},actor{},input{},camera{},settings{},selection{};
};
struct LocalInput {
    BasisInput motion;
    uintptr_t selected{};
};
struct Quaternion {float values[4]{};};
struct Vec2 {float x{},y{};};

// The owning Mainthread runtime validates all singleton addresses and native
// route fingerprints before entering these leaves. No network pointer is used.
inline uint32_t capture_local_native(const PresentationBinding& b,LocalInput& out)noexcept {
    __try {
        const auto axis=[&](int n)->float {
            const auto p=reinterpret_cast<uintptr_t(*)(uintptr_t,int)>(b.image+0xc38440)(b.input+0x48,n);
            return *reinterpret_cast<const float*>(p+0x5c);
        };
        out.motion.axis1=axis(1);
        out.motion.negative_axis0=-axis(0);
        out.motion.jump_pressed=reinterpret_cast<bool(*)(uintptr_t,int)>(b.image+0x9e6e00)(b.input,0x10);
        Vec3 camera_forward{};
        const auto forward=reinterpret_cast<const Vec3*>(b.image+0x1c4e8e0);
        const auto right=reinterpret_cast<const Vec3*>(b.image+0x1c4e900);
        const auto up=reinterpret_cast<const Vec3*>(b.image+0x1c4e8f0);
        using Apply=Vec3*(*)(Vec3*,const Quaternion*,const Vec3*);
        const auto apply=reinterpret_cast<Apply>(b.image+0x9fbcf0);
        apply(&camera_forward,reinterpret_cast<const Quaternion*>(b.camera+8),forward);
        const auto yaw=reinterpret_cast<float(*)(float,float)>(b.image+0x9f42c0)(camera_forward.x,camera_forward.z);
        Quaternion yaw_rotation;
        reinterpret_cast<void(*)(Quaternion*,const Vec3*,float)>(b.image+0x9fb080)(&yaw_rotation,up,yaw);
        apply(&out.motion.right,&yaw_rotation,right);
        apply(&out.motion.forward,&yaw_rotation,forward);
        out.selected=reinterpret_cast<uintptr_t(*)(uintptr_t)>(b.image+0x2d2370)(b.selection);
        return valid(out.motion)?0:ERROR_INVALID_DATA;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}

// Reproduce only native local Player presentation. The replicated actor is
// read to anchor the camera; no actor field, Clock or selection is changed.
inline uint32_t present_local_native(const PresentationBinding& b)noexcept {
    __try {
        const auto position=reinterpret_cast<const Vec3*>(b.actor+0x110);
        const auto previous=reinterpret_cast<const Vec3*>(b.player+0xe0);
        if(!finite(*position))return ERROR_INVALID_DATA;
        if(*previous!=*position){
            std::memcpy(reinterpret_cast<void*>(b.player+0xe0),position,sizeof(Vec3));
            *reinterpret_cast<uint8_t*>(b.player+0xf8)=1;
            const auto table=*reinterpret_cast<const uintptr_t*>(b.player);
            reinterpret_cast<void(*)(uintptr_t)>(*reinterpret_cast<const uintptr_t*>(table+0x38))(b.player);
        }
        const auto zoom_axis=reinterpret_cast<uintptr_t(*)(uintptr_t,int)>(b.image+0xc38440)(b.input+0x48,0x21);
        const auto zoom=0.25f* *reinterpret_cast<const float*>(zoom_axis+0x5c)+ *reinterpret_cast<const float*>(b.player+0x1b8);
        if(!std::isfinite(zoom))return ERROR_INVALID_DATA;
        *reinterpret_cast<float*>(b.player+0x1b8)=zoom;
        const auto mouse_look=reinterpret_cast<bool(*)(uintptr_t,int)>(b.image+0x9e6dc0)(b.input,0x1d);
        reinterpret_cast<void(*)(uintptr_t,bool)>(b.image+0x9e5530)(b.input,mouse_look);
        if(!mouse_look)return 0;
        const auto packed=reinterpret_cast<uint64_t(*)(uintptr_t,int)>(b.image+0x9e5470)(b.input,0xc);
        Vec2 mouse;std::memcpy(&mouse,&packed,sizeof mouse);
        const float sensitivity=float(*reinterpret_cast<const int32_t*>(b.settings+0x120))*0.1f;
        const Vec2 desired{(mouse.x*0.0031415929552167654f)*sensitivity,(mouse.y*0.0031415929552167654f)*sensitivity};
        const auto interpolated=reinterpret_cast<uint64_t(*)(float,const Vec2*,const Vec2*)>(b.image+0xa006b0)(0.5f,reinterpret_cast<const Vec2*>(b.player+0x1a8),&desired);
        Vec2 smooth;std::memcpy(&smooth,&interpolated,sizeof smooth);
        if(!std::isfinite(smooth.x)||!std::isfinite(smooth.y))return ERROR_INVALID_DATA;
        // vsAngle contains cached values; its constructor/Set own those values.
        alignas(16) unsigned char angle[16]{};
        const auto construct=reinterpret_cast<void(*)(void*,float)>(b.image+0x9eec90);
        const auto set=reinterpret_cast<void(*)(uintptr_t,float)>(b.image+0x9eedc0);
        construct(angle,smooth.y);
        set(b.player+0x150,*reinterpret_cast<const float*>(b.player+0x150)+ *reinterpret_cast<const float*>(angle));
        construct(angle,smooth.x);
        set(b.player+0x160,*reinterpret_cast<const float*>(b.player+0x160)+ *reinterpret_cast<const float*>(angle));
        std::memcpy(reinterpret_cast<void*>(b.player+0x1a8),&smooth,sizeof smooth);
        Quaternion rotation,result;
        reinterpret_cast<void(*)(Quaternion*,uintptr_t)>(b.image+0x9fb670)(&rotation,b.player+0x150);
        reinterpret_cast<Quaternion*(*)(Quaternion*,const Quaternion*,const Quaternion*)>(b.image+0x9fb8e0)(&result,&rotation,reinterpret_cast<const Quaternion*>(b.player+0x180));
        for(const auto v:result.values)if(!std::isfinite(v))return ERROR_INVALID_DATA;
        std::memcpy(reinterpret_cast<void*>(b.player+0xd0),&result,sizeof result);
        *reinterpret_cast<uint8_t*>(b.player+0xf8)=1;
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
#endif
