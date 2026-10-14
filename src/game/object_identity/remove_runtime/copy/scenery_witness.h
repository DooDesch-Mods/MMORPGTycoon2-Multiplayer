#pragma once
#include "selection_witness.h"
#include <array>
#include <cstring>
#include <string_view>

namespace mt2mp::build_actions::private_native::place {
struct Vec3 {float x=0,y=0,z=0;bool operator==(const Vec3&)const=default;};
struct Box {uint8_t valid=0;std::array<uint8_t,3> padding{};Vec3 min,max;bool operator==(const Box&)const=default;};
struct Transform {
    std::array<float,4> rotation{};Vec3 position,scale;
    uint8_t dirty=0;std::array<uint8_t,3> padding{};std::array<float,16> matrix{};
    bool operator==(const Transform& other)const{return std::memcmp(this,&other,sizeof(*this))==0;}
};
static_assert(sizeof(Box)==0x1c&&sizeof(Transform)==0x6c);
static_assert(offsetof(Transform,position)==0x10&&offsetof(Transform,dirty)==0x28&&offsetof(Transform,matrix)==0x2c);
struct SceneryContext {
    uint64_t epoch=0;uintptr_t region=0,shard=0,manager=0;
    bool exact_build=false,main_thread=false,owned_world_valid=false;
    bool operator==(const SceneryContext&)const=default;
};
struct SceneryMember {
    uintptr_t object=0;uint64_t uid=0;int32_t category=-1;Box bounds;Transform transform;
    bool operator==(const SceneryMember&)const=default;
};
struct SceneryWitness {
    SceneryContext context;std::array<int32_t,47> counts{};std::vector<SceneryMember> members;
    bool operator==(const SceneryWitness&)const=default;
};
struct SceneryHost {
    virtual ~SceneryHost()=default;
    // Current global-root/dungeon membership, not the camera's view shard.
    virtual bool current(SceneryContext&,std::string&)=0;
    virtual bool registry_object(uint64_t uid,uintptr_t& actual,std::string&)=0;
};
bool read_scenery(Memory&,SceneryHost&,uintptr_t image,SceneryWitness&,std::string&);
bool valid_geometry(const Box&,const Transform&,std::string&);
using OrientedBoxCtor=void(*)(void* storage,const Box*,const Transform*);
using OrientedBoxIntersects=bool(*)(const void* first,const void* second);
struct GeometryApi {OrientedBoxCtor construct=nullptr;OrientedBoxIntersects intersects=nullptr;};
bool resolve_geometry(Memory&,uintptr_t image,std::string_view exact_exe_sha256,GeometryApi&,std::string&);
struct OverlapResult {bool complete=false;size_t tested=0;uint32_t seh_code=0;std::vector<size_t> member_indices;std::string error;};
// Native constructors operate on private scalar copies and private stack objects only.
OverlapResult overlaps(const GeometryApi&,const SceneryWitness&,const Box&,const Transform&);
}
