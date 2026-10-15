#pragma once
#include "npc_quest_targets.h"
#include "native_polyshape.h"
#include <cmath>
#include <algorithm>
namespace mt2mp::game::monster_zone_read {
namespace q=npc_quest_targets;
struct Monster {
    uintptr_t pointer=0,zone=0;uint64_t uid=0;
    int32_t tag=0,status=0,rank=0,type=0,slot=0,level=0;
    std::array<float,3> position{},scale{};std::array<float,4> rotation{};
    bool operator==(const Monster&)const=default;
};
struct Zone {
    q::Zone value;
    action_session::TerrainShape shape;
    std::vector<uint64_t> monsters;
    bool operator==(const Zone&)const=default;
};
struct State {
    uintptr_t manager=0,monster_manager=0;
    std::vector<Zone> zones;std::vector<Monster> monsters;
    int32_t next_monster_tag=0,created_monsters=0;
    std::array<uint64_t,4> global_random{},region_random{};
    bool operator==(const State&)const=default;
};
// The adapter supplies the native current World and bounded reads. Local UIDs
// are receipt-only identities; they must never become action wire keys.
template<class Memory>
bool read(Memory& m,const q::World& w,State& out,std::string& e) {
    out={};uintptr_t buildings=0;
    if(!q::owner(m,w,buildings,out.manager,e)||
       !q::at(m,w.region,0x988,out.monster_manager,e)||
       !q::typed(m,w.image,out.monster_manager,0x1714100,e))return false;
    uintptr_t owner=0;
    if(!q::at(m,out.monster_manager,0x80,owner,e))return false;
    if(owner!=w.region)return q::fail(e,"monster probe manager reciprocal region differs");
    q::Array raw;const q::Budget budget{128,512,8192,4096};
    const auto no_saved_link=[](uintptr_t,int32_t& id,std::string&){id=0;return true;};
    if(!q::observe_array(m,w.image,out.manager+0x48,budget,no_saved_link,raw,e))return false;
    std::map<uintptr_t,uint64_t> zone_ids;
    for(const auto& value:raw.zones){
        Zone z;z.value=value;uintptr_t root=0,region=0;
        if(!q::at(m,value.pointer,0x170,root,e)||!q::at(m,value.pointer,0x178,region,e))return false;
        if(root!=w.root||region!=w.region)return q::fail(e,"monster probe zone reciprocal owner differs");
        if(!ui_terrain_runtime::poly::read(m,value.pointer+0x2f0,z.shape,e))return false;
        zone_ids.emplace(value.pointer,value.uid);out.zones.push_back(std::move(z));
    }
    q::Bounds monsters;
    if(!q::typed(m,w.image,out.monster_manager+0x38,0x16e8b60,e)||
       !q::bounds(m,out.monster_manager+0x38,4096,monsters,e)||
       !q::at(m,w.image,0x1a30b70,out.next_monster_tag,e)||
       !q::at(m,out.monster_manager,0x90,out.created_monsters,e))return false;
    std::set<uintptr_t> pointers;std::set<uint64_t> uids;std::set<int32_t> tags;
    for(int32_t i=0;i<monsters.count;++i){
        Monster row;uintptr_t root=0,region=0,proxy=0,target=0,owned=0;
        if(!q::at(m,monsters.data,size_t(i)*8,row.pointer,e)||
           !q::typed(m,w.image,row.pointer,0x16c0770,e)||
           !q::at(m,row.pointer,0x18,row.uid,e)||!q::at(m,row.pointer,0x170,root,e)||
           !q::at(m,row.pointer,0x178,region,e)||!q::at(m,row.pointer,0x820,row.tag,e)||
           !q::at(m,row.pointer,0x824,row.status,e)||!q::at(m,row.pointer,0x838,row.rank,e)||
           !q::at(m,row.pointer,0x4a4,row.type,e)||!q::at(m,row.pointer,0x4a8,row.slot,e)||
           !q::at(m,row.pointer,0x6f8,row.level,e)||!q::at(m,row.pointer,0x100,row.rotation,e)||
           !q::at(m,row.pointer,0x110,row.position,e)||!q::at(m,row.pointer,0x11c,row.scale,e)||
           !q::at(m,row.pointer,0x8d0,row.zone,e)||!q::at(m,row.pointer,0x8d8,proxy,e))return false;
        if(root!=w.root||region!=w.region||!row.uid||!pointers.insert(row.pointer).second||
           !uids.insert(row.uid).second||!tags.insert(row.tag).second)
            return q::fail(e,"monster probe row owner/UID/tag differs");
        if(row.zone){
            if(!zone_ids.contains(row.zone)||!proxy||!q::at(m,proxy,0,target,e)||
               !q::at(m,row.zone,0x28,owned,e))return q::fail(e,e.empty()?"monster probe zone membership/weak link absent":e);
            if(target!=row.zone+0x20||proxy!=owned)return q::fail(e,"monster probe zone weak owner differs");
        }else if(proxy)return q::fail(e,"monster probe null zone retains weak proxy");
        for(float v:row.position)if(!std::isfinite(v))return q::fail(e,"monster probe position not finite");
        for(float v:row.rotation)if(!std::isfinite(v))return q::fail(e,"monster probe rotation not finite");
        for(float v:row.scale)if(!std::isfinite(v))return q::fail(e,"monster probe scale not finite");
        out.monsters.push_back(row);
    }
    for(auto& z:out.zones){
        q::Bounds members;if(!q::bounds(m,z.value.pointer+0x380,4096,members,e))return false;
        std::set<uintptr_t> unique;
        for(int32_t i=0;i<members.count;++i){
            uintptr_t pointer=0;if(!q::at(m,members.data,size_t(i)*8,pointer,e))return false;
            const auto found=std::find_if(out.monsters.begin(),out.monsters.end(),[&](const Monster& row){return row.pointer==pointer;});
            if(!unique.insert(pointer).second||found==out.monsters.end()||found->zone!=z.value.pointer)
                return q::fail(e,"monster probe zone/manager reciprocal member differs");
            z.monsters.push_back(found->uid);
        }
        if(!q::unchanged(m,z.value.pointer+0x380,4096,members,e))return false;
    }
    if(!q::unchanged(m,out.monster_manager+0x38,4096,monsters,e))return false;
    uintptr_t random=0;
    if(!q::at(m,w.image,0x164b250,random,e)||!q::at(m,random,0,out.global_random,e)||
       !q::at(m,w.region,0x160,random,e)||!q::at(m,random,0,out.region_random,e))return false;
    e.clear();return true;
}
}
