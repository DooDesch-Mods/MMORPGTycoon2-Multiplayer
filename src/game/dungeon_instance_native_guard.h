#pragma once
#include "dungeon_instance_lifecycle.h"
#include <array>

namespace mt2mp::game::dungeon_instance_native::guard {
namespace life=dungeon_instance_lifecycle;
struct Pool {
    uintptr_t dungeon=0,exterior=0,root=0;
    uint64_t dungeon_uid=0,exterior_uid=0;
    int32_t counter=-1;
    std::vector<life::Member> members;
};
inline bool same_owner(const Pool& a,const Pool& b){return a.dungeon==b.dungeon&&a.dungeon_uid==b.dungeon_uid&&
    a.exterior==b.exterior&&a.exterior_uid==b.exterior_uid&&a.root==b.root;}
inline bool transition(const Pool& before,const Pool& after,life::Effect effect,uintptr_t target,uintptr_t result,
                       life::Event& out,std::string& error){
    if(!same_owner(before,after))return life::fail(error,"observed native owner changed during original call");
    out.effect=effect;out.counter_before=before.counter;out.counter_after=after.counter;
    out.before.clear();out.after.clear();
    for(const auto& row:before.members)out.before.push_back(row.index);
    for(const auto& row:after.members)out.after.push_back(row.index);
    auto expected=before.members;
    if(effect==life::Effect::Make){
        if(after.members.size()!=before.members.size()+1||!result||after.members.back().instance!=result)
            return life::fail(error,"observed Make return differs from appended actual Instance");
        out.index=after.members.back().index;expected.push_back(after.members.back());
    }else{
        const auto found=std::find_if(before.members.begin(),before.members.end(),[&](const auto& row){return row.instance==target;});
        if(found==before.members.end())return life::fail(error,"observed target absent from original owner array");
        out.index=found->index;
        if(effect==life::Effect::Destroy)std::erase_if(expected,[&](const auto& row){return row.instance==target;});
    }
    if(expected!=after.members)return life::fail(error,"observed original replaced or reordered surviving native Instances");
    return life::valid(out,error);
}
struct Update {
    uintptr_t instance=0,dungeon=0,shard=0,region=0;
    int32_t index=-1;
    std::array<uint32_t,3> floats{};
    std::array<uint8_t,4> flags{};
};
inline constexpr std::array<size_t,3> float_offsets{0x1c,0x24,0x70};
inline constexpr std::array<size_t,4> flag_offsets{0x20,0x28,0x74,0x75};
template<class Read>bool update(Read& at,uintptr_t image,uintptr_t instance,Update& out,std::string& error){
    out={};out.instance=instance;uintptr_t ap=0;
    if(!at(instance,0,ap,error)||ap!=image+0x1719450||!at(instance,0x18,out.index,error)||
       !at(instance,0x30,out.dungeon,error)||!at(instance,0x38,out.shard,error)||!at(instance,0x40,out.region,error))
        return life::fail(error,error.empty()?"Update actual Instance owner unreadable":error);
    if(out.index<0||!out.dungeon||!out.shard||!out.region)return life::fail(error,"Update actual Instance owner invalid");
    for(size_t i=0;i<float_offsets.size();++i)if(!at(instance,float_offsets[i],out.floats[i],error))return false;
    for(size_t i=0;i<flag_offsets.size();++i){if(!at(instance,flag_offsets[i],out.flags[i],error))return false;
        if(out.flags[i]>1)return life::fail(error,"Update actual saved bool invalid");}
    return true;
}
template<class Read,class Store>bool restore_update(Read& at,Store& store,uintptr_t image,const Update& before,std::string& error){
    Update after;if(!update(at,image,before.instance,after,error))return false;
    if(before.index!=after.index||before.dungeon!=after.dungeon||before.shard!=after.shard||before.region!=after.region)
        return life::fail(error,"Update changed actual Instance lifetime/owner before scalar guard");
    for(size_t i=0;i<float_offsets.size();++i)if(before.floats[i]!=after.floats[i]&&
        !store(before.instance,float_offsets[i],&after.floats[i],&before.floats[i],sizeof(uint32_t),error))return false;
    for(size_t i=0;i<flag_offsets.size();++i)if(before.flags[i]!=after.flags[i]&&
        !store(before.instance,flag_offsets[i],&after.flags[i],&before.flags[i],sizeof(uint8_t),error))return false;
    // +68 is an original weak Party link. Never restore a potentially retired Party.
    Update final;if(!update(at,image,before.instance,final,error))return false;
    return (final.index==before.index&&final.dungeon==before.dungeon&&final.shard==before.shard&&final.region==before.region&&
        final.floats==before.floats&&final.flags==before.flags)||life::fail(error,"Update scalar guard readback differs");
}
inline bool suppress_force(bool client,bool action,bool replay,uintptr_t caller){return client&&!action&&!replay&&caller==0xb78b5;}
inline bool suppress_dip(bool replay_make,uintptr_t caller,uintptr_t manager,uintptr_t expected_manager){
    return replay_make&&caller==0xa795f&&manager==expected_manager;
}
inline bool camera_restore(uintptr_t before,uintptr_t after,uintptr_t born_shard,bool suppressed,bool& restore,std::string& error){
    restore=false;if(after==before)return true;
    if(!born_shard||!suppressed||after!=born_shard)return life::fail(error,"replay Make changed a foreign Dungeon displayed Shard");
    restore=true;return true;
}
}
