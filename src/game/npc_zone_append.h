#pragma once
#include "npc_quest_targets.h"
namespace mt2mp::game::npc_zone_append {
namespace q=npc_quest_targets;
struct Receipt {
    q::World world;
    q::SavedPool before;
    std::string action_uuid;
    bool consumed=false;
};
inline bool same_world(const q::World& a,const q::World& b){
    return a.origin==b.origin&&a.image==b.image&&a.map==b.map&&a.root==b.root&&
        a.region==b.region&&a.selection==b.selection&&a.saved_region==b.saved_region;
}
inline bool same_pool(const q::SavedPool& a,const q::SavedPool& b){
    return a.origin==b.origin&&a.saved_region==b.saved_region&&a.array==b.array&&a.action_keys==b.action_keys;
}
template<class Memory>bool begin(Memory& m,const q::World& world,const q::SavedPool& saved,
    const q::Budget& budget,const std::string& action_uuid,Receipt& out,std::string& e){
    if(!q::valid_origin({action_uuid,world.origin.sha256,world.origin.world_epoch,world.origin.world_generation}))return q::fail(e,"Zone append action UUID invalid");
    for(const auto& entry:saved.action_keys)if(entry.second=="action:"+action_uuid)return q::fail(e,"Zone append action already bound");
    if(!q::validate_saved(m,world,saved,budget,e))return false;
    out={world,saved,action_uuid,false};return true;
}
// Only a completed original action may call this with its observed new object.
// The caller retains Unknown after any failed post-effect check; this receipt is
// consumed even when the original world can no longer be admitted.
template<class Memory>bool finish(Memory& m,const q::World& world,q::SavedPool& saved,
    const q::Budget& budget,Receipt& receipt,uintptr_t created,uint64_t uid,
    const q::Content& required,std::string& key,std::string& e){
    key.clear();if(receipt.consumed)return q::fail(e,"Zone append receipt already consumed; no replay");
    receipt.consumed=true;
    if(!same_world(receipt.world,world)||!same_pool(receipt.before,saved))return q::fail(e,"Zone append bound world or prior pool changed");
    if(!created||!uid||required.type!=1)return q::fail(e,"Zone append requires original Monster Zone identity/type");
    for(const auto& zone:saved.array.zones)if(zone.pointer==created||zone.uid==uid)return q::fail(e,"Zone append new object reuses existing pointer/UID");
    uintptr_t building=0,manager=0;if(!q::owner(m,world,building,manager,e))return false;
    if(saved.array.pointer!=manager+0x48)return q::fail(e,"Zone append native array owner changed");
    auto link=[&](uintptr_t p,int32_t& id,std::string& cause){
        for(const auto& zone:saved.array.zones)if(zone.pointer==p){id=zone.link_id;return true;}
        if(p==created){id=0;return true;}
        return q::fail(cause,"Zone append encountered an unrecorded additional object");
    };
    q::Array fresh;if(!q::observe_array(m,world.image,saved.array.pointer,budget,link,fresh,e))return false;
    if(fresh.zones.size()!=saved.array.zones.size()+1)return q::fail(e,"Zone append requires exactly one new native member");
    for(size_t i=0;i<saved.array.zones.size();++i)if(fresh.zones[i]!=saved.array.zones[i])return q::fail(e,"Zone append changed an existing saved member/order/content");
    const auto& added=fresh.zones.back();uintptr_t root=0,region=0;
    if(added.pointer!=created||added.uid!=uid||added.content!=required)return q::fail(e,"Zone append original identity/content readback differs");
    if(!q::at(m,created,0x170,root,e)||!q::at(m,created,0x178,region,e))return false;
    if(root!=world.root||region!=world.region)return q::fail(e,"Zone append new member reciprocal owner differs");
    auto next=saved;next.array=std::move(fresh);const auto canonical="action:"+receipt.action_uuid;
    if(!next.action_keys.emplace(created,canonical).second)return q::fail(e,"Zone append duplicate action member");
    if(!q::validate_saved(m,world,next,budget,e))return false;
    saved=std::move(next);key=canonical;e.clear();return true;
}
}
