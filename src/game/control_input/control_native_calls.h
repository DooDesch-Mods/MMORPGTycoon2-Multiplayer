#pragma once

#include "control_motion.h"
#include <cstring>

#if defined(_WIN32) && defined(_MSC_VER)
#include <Windows.h>

namespace mt2mp::control_input {
// Each call requires the current production Mainthread/census/lease guard.
// An exception means an unknown partial effect. Never retry that mutation.
struct LocalBinding {
    uintptr_t image{};
    uintptr_t actor{};
    uintptr_t map{};
};

inline uint32_t normalise_native(const LocalBinding& b, Vec3& value) noexcept {
    __try {
        reinterpret_cast<void(*)(Vec3*)>(b.image + 0x9ffd80)(&value);
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}

inline bool propose_native(const LocalBinding& b, const Sample& sample, const Motion& motion,
                           const Input& input, float host_scaled_dt, Proposal& out, uint32_t& exception) {
    exception = 0;
    return propose_with_normalizer(sample,motion,input,host_scaled_dt,out,[&](Vec3& value) {
        exception = normalise_native(b,value);
        return exception == 0;
    });
}
inline bool propose_native(const LocalBinding& b,const Sample& sample,const Motion& motion,
                           const BasisInput& input,float host_scaled_dt,Proposal& out,uint32_t& exception) {
    exception=0;
    return propose_basis(sample,motion,input,host_scaled_dt,out,[&](Vec3& value) {
        exception=normalise_native(b,value);return exception==0;
    });
}

inline uint32_t sample_native(const LocalBinding& b, Sample& out) noexcept {
    __try {
        std::memcpy(&out.position, reinterpret_cast<void*>(b.actor + 0x110), sizeof(Vec3));
        out.native_speed = *reinterpret_cast<const float*>(b.actor + 0x364);
        out.action_timer = *reinterpret_cast<const float*>(b.actor + 0x804);
        out.movement_blocked = *reinterpret_cast<const uint8_t*>(b.actor + 0x5e8) != 0;
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}

inline uint32_t constrain_native(const LocalBinding& b, Vec3& position, float& height) noexcept {
    __try {
        const auto region = *reinterpret_cast<const uintptr_t*>(b.actor + 0x178);
        if (region) {
            const auto obstruction = reinterpret_cast<uintptr_t(*)(uintptr_t)>(b.image + 0x350710)(region);
            const auto nav = *reinterpret_cast<const uintptr_t*>(obstruction + 0x1b8);
            if (nav) {
                const auto triangle = reinterpret_cast<uintptr_t(*)(uintptr_t,const Vec3*,int)>(b.image + 0x298a30)(nav,&position,0);
                if (!triangle) {
                    Vec3 constrained{};
                    // MinGW x64 aggregate return: RCX=sret, RDX=this, R8=position.
                    reinterpret_cast<Vec3*(*)(Vec3*,uintptr_t,const Vec3*)>(b.image + 0x298de0)(&constrained,nav,&position);
                    position = constrained;
                }
            }
        }
        height = reinterpret_cast<float(*)(uintptr_t,const Vec3*)>(b.image + 0x32ab80)(b.map,&position);
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}

inline uint32_t jump_native(const LocalBinding& b) noexcept {
    __try {
        reinterpret_cast<void(*)(uintptr_t)>(b.image + 0x39cb50)(b.actor);
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}

inline uint32_t position_native(const LocalBinding& b, const Vec3& position) noexcept {
    __try {
        const auto current = reinterpret_cast<const Vec3*>(b.actor + 0x110);
        if (current->x == position.x && current->y == position.y && current->z == position.z) return 0;
        std::memcpy(reinterpret_cast<void*>(b.actor + 0x110), &position, sizeof(Vec3));
        *reinterpret_cast<uint8_t*>(b.actor + 0x128) = 1;
        const auto render = b.actor + 0x30;
        const auto table = *reinterpret_cast<const uintptr_t*>(render);
        reinterpret_cast<void(*)(uintptr_t)>(*reinterpret_cast<const uintptr_t*>(table + 0x38))(render);
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}

inline uint32_t facing_native(const LocalBinding& b, const Motion& motion) noexcept {
    __try {
        if (moving(motion)) {
            const Vec3 direction{motion.velocity.x,0.0f,motion.velocity.z};
            float orientation[4]{};
            const auto up = reinterpret_cast<const Vec3*>(b.image + 0x1c4e8f0);
            reinterpret_cast<void(*)(float*,const Vec3*,const Vec3*)>(b.image + 0x9fb4d0)(orientation,&direction,up);
            const auto old = reinterpret_cast<const float*>(b.actor + 0x100);
            if (old[0] != orientation[0] || old[1] != orientation[1] || old[2] != orientation[2] || old[3] != orientation[3]) {
                std::memcpy(reinterpret_cast<void*>(b.actor + 0x100),orientation,sizeof(orientation));
                *reinterpret_cast<uint8_t*>(b.actor + 0x128) = 1;
                const auto render = b.actor + 0x30;
                const auto table = *reinterpret_cast<const uintptr_t*>(render);
                reinterpret_cast<void(*)(uintptr_t)>(*reinterpret_cast<const uintptr_t*>(table + 0x38))(render);
            }
        }
        *reinterpret_cast<uint8_t*>(b.actor + 0x7f4) = moving(motion) ? 1 : 0;
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}

inline uint32_t target_native(const LocalBinding& b, uintptr_t local_target, uintptr_t& actual) noexcept {
    __try {
        // Preserve native proxy reference ownership without SetTarget's UI branch.
        reinterpret_cast<uintptr_t(*)(uintptr_t,uintptr_t)>(b.image + 0x3b0530)(b.actor + 0x6d8,local_target);
        actual = reinterpret_cast<uintptr_t(*)(uintptr_t)>(b.image + 0x3b07d0)(b.actor + 0x6d8);
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
inline uint32_t read_target_native(const LocalBinding& b,uintptr_t& actual)noexcept {
    __try {actual=reinterpret_cast<uintptr_t(*)(uintptr_t)>(b.image+0x3b07d0)(b.actor+0x6d8);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}

inline uint32_t controlled_native(const LocalBinding& b, bool value) noexcept {
    __try {
        const auto table = *reinterpret_cast<const uintptr_t*>(b.actor);
        reinterpret_cast<void(*)(uintptr_t,bool)>(*reinterpret_cast<const uintptr_t*>(table + 0x3f0))(b.actor,value);
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
} // namespace mt2mp::control_input
#endif
