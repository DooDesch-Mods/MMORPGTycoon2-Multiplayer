#include "scenery_witness.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#ifdef _WIN32
#ifndef NOMINMAX
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include <Windows.h>
#endif

namespace mt2mp::build_actions::private_native::place {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
template<class T>bool at(Memory& m,uintptr_t p,size_t off,T& value,std::string& e){
    if(p>std::numeric_limits<uintptr_t>::max()-off)return fail(e,"scenery_address_overflow");
    if(m.copy(p+off,&value,sizeof(value),e))return true;
    if(e.empty())e="scenery_memory_read_failed";return false;
}
bool vec(const Vec3& v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
struct alignas(16) OrientedBox {std::array<uint8_t,0xf0> bytes;};
uint32_t native_overlap(const GeometryApi& api,const Box& a,const Transform& at,const Box& b,const Transform& bt,bool& yes){
    OrientedBox first{},second{};
#ifdef _MSC_VER
    __try {
#endif
        api.construct(&first,&a,&at);api.construct(&second,&b,&bt);yes=api.intersects(&first,&second);return 0;
#ifdef _MSC_VER
    }__except(GetExceptionCode()==0xe06d7363UL?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
#endif
}
}
bool valid_geometry(const Box& box,const Transform& t,std::string& e){
    if(box.valid!=1)return fail(e,"scenery_box_not_initialized");
    if(!vec(box.min)||!vec(box.max)||box.min.x>box.max.x||box.min.y>box.max.y||box.min.z>box.max.z)return fail(e,"scenery_bounds_invalid");
    if(!vec(t.position)||!vec(t.scale)||!std::all_of(t.rotation.begin(),t.rotation.end(),[](float f){return std::isfinite(f);}))return fail(e,"scenery_transform_nonfinite");
    if(t.dirty>1)return fail(e,"scenery_transform_dirty_not_bool");
    if(!t.dirty&&!std::all_of(t.matrix.begin(),t.matrix.end(),[](float f){return std::isfinite(f);}))return fail(e,"scenery_cached_matrix_nonfinite");
    return true;
}
bool resolve_geometry(Memory& m,uintptr_t image,std::string_view hash,GeometryApi& out,std::string& e){
    out={};e.clear();
    if(hash!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"scenery_geometry_exe_hash_mismatch");
    if(!image||image>std::numeric_limits<uintptr_t>::max()-0x9f1b60)return fail(e,"scenery_geometry_image_range");
    constexpr std::array<uint8_t,12> ctor{0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x48};
    constexpr std::array<uint8_t,10> intersection{0x57,0x56,0x53,0x48,0x81,0xec,0x40,0x01,0x00,0x00};
    try {
        std::array<uint8_t,12> actual_ctor{};std::array<uint8_t,10> actual_intersection{};
        if(!m.copy(image+0x9f17e0,actual_ctor.data(),actual_ctor.size(),e)||!m.copy(image+0x9f1b40,actual_intersection.data(),actual_intersection.size(),e)){if(e.empty())e="scenery_geometry_code_unreadable";return false;}
        if(actual_ctor!=ctor)return fail(e,"scenery_geometry_constructor_prologue");
        if(actual_intersection!=intersection)return fail(e,"scenery_geometry_intersection_prologue");
        out={reinterpret_cast<OrientedBoxCtor>(image+0x9f17e0),reinterpret_cast<OrientedBoxIntersects>(image+0x9f1b40)};return true;
    }catch(const std::exception& x){e=std::string("scenery_geometry_resolver_exception: ")+x.what();return false;}
    catch(...){return fail(e,"scenery_geometry_resolver_unknown_exception");}
}
bool read_scenery(Memory& m,SceneryHost& host,uintptr_t image,SceneryWitness& out,std::string& e){
    out={};e.clear();
    try {
        if(!image||image>std::numeric_limits<uintptr_t>::max()-0x172d6a0)return fail(e,"scenery_image_range");
        auto once=[&](SceneryWitness& w){
            if(!host.current(w.context,e))return false;
            const auto& c=w.context;
            if(!c.main_thread)return fail(e,"scenery_wrong_thread");
            if(!c.exact_build)return fail(e,"scenery_build_guard");
            if(!c.owned_world_valid||!c.epoch||!c.region||!c.shard||!c.manager)return fail(e,"scenery_world_guard");
            uintptr_t own_manager=0,ap=0,region=0,shard=0;
            if(!at(m,c.region,0x960,own_manager,e)||!at(m,c.manager,0,ap,e)||!at(m,c.manager,0x900,region,e)||!at(m,c.manager,0x908,shard,e))return false;
            if(own_manager!=c.manager)return fail(e,"scenery_region_manager_mismatch");
            if(ap!=image+0x17152d0)return fail(e,"scenery_manager_addresspoint");
            if(region!=c.region||shard!=c.shard)return fail(e,"scenery_reverse_membership");
            std::set<uintptr_t> objects;std::set<uint64_t> uids;
            for(size_t category=0;category<w.counts.size();++category){
                const size_t off=0x18+category*0x30;uintptr_t data=0,array_ap=0;int32_t capacity=0;
                if(!at(m,c.manager,off,array_ap,e)||!at(m,c.manager,off+8,data,e)||!at(m,c.manager,off+0x10,w.counts[category],e)||!at(m,c.manager,off+0x14,capacity,e))return false;
                if(array_ap!=image+0x172d6a0)return fail(e,"scenery_category_addresspoint");
                const auto count=w.counts[category];
                if(count<0||capacity<count)return fail(e,"scenery_count_capacity");
                if(count&&(!data||data>std::numeric_limits<uintptr_t>::max()-static_cast<size_t>(count)*sizeof(uintptr_t)))return fail(e,"scenery_array_range");
                for(int32_t i=0;i<count;++i){
                    SceneryMember member;uintptr_t actual=0;uint64_t second_uid=0;
                    if(!at(m,data,static_cast<size_t>(i)*8,member.object,e))return false;
                    if(!member.object||!objects.insert(member.object).second)return fail(e,"scenery_duplicate_or_null_member");
                    if(!at(m,member.object,0,ap,e)||!at(m,member.object,0x18,member.uid,e)||!at(m,member.object,0x170,shard,e)||!at(m,member.object,0x178,region,e)||!at(m,member.object,0x268,member.category,e))return false;
                    if(ap!=image+0x16c1390)return fail(e,"scenery_member_addresspoint");
                    if(shard!=c.shard||region!=c.region||member.category!=static_cast<int32_t>(category))return fail(e,"scenery_member_ownership");
                    if(!member.uid||!uids.insert(member.uid).second)return fail(e,"scenery_duplicate_or_null_uid");
                    if(!host.registry_object(member.uid,actual,e))return false;
                    if(actual!=member.object)return fail(e,"scenery_registry_identity_mismatch");
                    if(!at(m,member.object,0x90,member.bounds,e)||!at(m,member.object,0x100,member.transform,e)||!at(m,member.object,0x18,second_uid,e))return false;
                    if(second_uid!=member.uid)return fail(e,"scenery_member_lifetime_changed");
                    if(!valid_geometry(member.bounds,member.transform,e))return false;
                    w.members.push_back(member);
                }
            }
            SceneryContext after;if(!host.current(after,e))return false;
            if(after!=c)return fail(e,"scenery_world_changed_during_read");
            return true;
        };
        SceneryWitness a,b;if(!once(a)||!once(b))return false;
        if(a!=b)return fail(e,"scenery_changed_during_read");
        out=std::move(b);return true;
    }catch(const std::exception& x){e=std::string("scenery_read_exception: ")+x.what();return false;}
    catch(...){return fail(e,"scenery_read_unknown_exception");}
}
OverlapResult overlaps(const GeometryApi& api,const SceneryWitness& source,const Box& box,const Transform& transform){
    OverlapResult result;
    try {
        if(!api.construct||!api.intersects){result.error="scenery_geometry_api_missing";return result;}
        if(!source.context.main_thread||!source.context.exact_build||!source.context.owned_world_valid||!source.context.epoch){result.error="scenery_geometry_context_guard";return result;}
        if(!valid_geometry(box,transform,result.error))return result;
        result.member_indices.reserve(source.members.size());
        for(size_t i=0;i<source.members.size();++i){
            const auto& member=source.members[i];
            if(!valid_geometry(member.bounds,member.transform,result.error))return result;
            bool hit=false;result.seh_code=native_overlap(api,box,transform,member.bounds,member.transform,hit);
            if(result.seh_code){result.error="scenery_native_geometry_seh_"+std::to_string(result.seh_code);return result;}
            ++result.tested;if(hit)result.member_indices.push_back(i);
        }
        result.complete=true;return result;
    }catch(const std::exception& x){result.error=std::string("scenery_native_geometry_exception: ")+x.what();return result;}
    catch(...){result.error="scenery_native_geometry_unknown_exception";return result;}
}
}
