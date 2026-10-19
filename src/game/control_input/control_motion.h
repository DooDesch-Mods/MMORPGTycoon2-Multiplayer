#pragma once

#include <cmath>
#include <cstdint>

// This state belongs to one Control lease. It is never passed to mmoPlayer.
namespace mt2mp::control_input {
struct Vec3 { float x{}, y{}, z{}; };
inline bool operator==(const Vec3& a,const Vec3& b) {return a.x==b.x&&a.y==b.y&&a.z==b.z;}
struct Motion { Vec3 velocity{}; bool falling{}; };
struct Input { float world_x{}, world_z{}; bool jump_pressed{}; };
struct BasisInput {
    Vec3 right{1.0f,0.0f,0.0f},forward{0.0f,0.0f,1.0f};
    float axis1{},negative_axis0{};
    bool jump_pressed{};
    bool operator==(const BasisInput&)const=default;
};
struct Sample {
    Vec3 position{};
    float native_speed{};
    float action_timer{};
    bool movement_blocked{};
};
struct Proposal {
    Motion next{};
    Vec3 position{};
    bool jump{};
    bool advance{};
};

inline bool finite(Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
inline bool valid(const BasisInput& input) {
    if(!finite(input.right)||!finite(input.forward)||!std::isfinite(input.axis1)||
       !std::isfinite(input.negative_axis0)||std::abs(input.axis1)>1.0f||std::abs(input.negative_axis0)>1.0f)return false;
    const auto length=[](Vec3 v){return v.x*v.x+v.z*v.z;};
    return input.right.y==0.0f&&input.forward.y==0.0f&&
        std::abs(length(input.right)-1.0f)<0.0001f&&std::abs(length(input.forward)-1.0f)<0.0001f&&
        std::abs(input.right.x*input.forward.x+input.right.z*input.forward.z)<0.0001f&&
        input.right.x*input.forward.z-input.right.z*input.forward.x>0.9999f;
}

template<class Normalise>
inline bool propose_velocity(const Sample& sample, const Motion& motion,
                    const Input& input, float host_scaled_dt, Proposal& out, Normalise&& normalise) {
    // Native input axes can combine diagonally. Do not normalize their sum.
    const float input_squared = input.world_x * input.world_x + input.world_z * input.world_z;
    if (!finite(sample.position) || !finite(motion.velocity) ||
        !std::isfinite(sample.native_speed) || sample.native_speed < 0.0f ||
        !std::isfinite(sample.action_timer) ||
        !std::isfinite(host_scaled_dt) || host_scaled_dt < 0.0f ||
        !std::isfinite(input_squared)) return false;
    out = {motion, sample.position, false, host_scaled_dt > 0.0f};
    if (!out.advance) return true;
    if (sample.action_timer > 0.0f || sample.movement_blocked) {
        out.next.velocity.x = 0.0f;
        out.next.velocity.z = 0.0f;
    } else {
        out.jump = !motion.falling && input.jump_pressed;
        float dx = (0.0f - motion.velocity.x) + input.world_x;
        float dz = (0.0f - motion.velocity.z) + input.world_z;
        const float limit = 30.0f * host_scaled_dt;
        const float squared = dx * dx + dz * dz;
        if (squared > limit * limit) {
            Vec3 delta{dx,0.0f,dz};
            if (!normalise(delta) || !finite(delta)) return false;
            dx = delta.x * limit;
            dz = delta.z * limit;
        }
        out.next.velocity.x = motion.velocity.x + dx;
        out.next.velocity.z = motion.velocity.z + dz;
    }
    if (motion.falling) out.next.velocity.y += -19.6f * host_scaled_dt;
    out.position.x += out.next.velocity.x * host_scaled_dt;
    out.position.y += out.next.velocity.y * host_scaled_dt;
    out.position.z += out.next.velocity.z * host_scaled_dt;
    return finite(out.position) && finite(out.next.velocity);
}

template<class Normalise>
inline bool propose_with_normalizer(const Sample& sample, const Motion& motion,
                    const Input& input, float host_scaled_dt, Proposal& out, Normalise&& normalise) {
    const float squared=input.world_x*input.world_x+input.world_z*input.world_z;
    if(!std::isfinite(squared)||squared>2.00001f)return false;
    return propose_velocity(sample,motion,{input.world_x*sample.native_speed,input.world_z*sample.native_speed,input.jump_pressed},host_scaled_dt,out,normalise);
}

template<class Normalise>
inline bool propose_basis(const Sample& sample, const Motion& motion,
                    const BasisInput& input,float host_scaled_dt,Proposal& out,Normalise&& normalise) {
    if(!valid(input))return false;
    // Player::UpdateOnFoot multiplies each rotated basis by speed, then its
    // corresponding axis, before adding. Factoring speed changes rounding.
    const float right_x=(input.right.x*sample.native_speed)*input.axis1;
    const float right_z=(input.right.z*sample.native_speed)*input.axis1;
    const float forward_x=(input.forward.x*sample.native_speed)*input.negative_axis0;
    const float forward_z=(input.forward.z*sample.native_speed)*input.negative_axis0;
    return propose_velocity(sample,motion,{forward_x+right_x,forward_z+right_z,input.jump_pressed},host_scaled_dt,out,normalise);
}

// Portable fixture arithmetic. The native adapter uses the engine normalizer.
inline bool propose(const Sample& sample, const Motion& motion,
                    const Input& input, float host_scaled_dt, Proposal& out) {
    return propose_with_normalizer(sample,motion,input,host_scaled_dt,out,[](Vec3& value) {
        const float inverse_length = 1.0f / std::sqrt(value.x*value.x+value.z*value.z);
        value.x *= inverse_length; value.z *= inverse_length;
        return true;
    });
}

// Call after the original NavMesh methods have constrained proposal.position.
inline bool ground(Proposal& proposal, float original_map_height) {
    if (!std::isfinite(original_map_height) || !finite(proposal.position)) return false;
    if (!proposal.advance) return true;
    if (proposal.next.falling) {
        if (original_map_height > proposal.position.y) {
            proposal.next.velocity.y = 0.0f;
            proposal.next.falling = false;
            proposal.position.y = original_map_height;
        }
    } else if (original_map_height + 0.05f >= proposal.position.y) {
        proposal.next.velocity.y = 0.0f;
        proposal.position.y = original_map_height;
    } else {
        proposal.next.falling = true;
    }
    return true;
}

inline bool moving(const Motion& motion) {
    return motion.velocity.x * motion.velocity.x + motion.velocity.z * motion.velocity.z > 1.0e-7f;
}
} // namespace mt2mp::control_input
