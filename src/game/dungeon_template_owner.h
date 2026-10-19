#pragma once
#include "dungeon_scene_key.h"
#include <array>
#include <cstddef>
#include <set>
#include <utility>

namespace mt2mp::game::dungeon_template_owner {
enum class Manager:size_t {Scenery,Zone,Monster,Gizmo};
struct Owner {
    std::string key;uintptr_t root=0,dungeon=0,exterior=0,shard=0,region=0;uint64_t dungeon_uid=0,exterior_uid=0;
    std::array<uintptr_t,4> managers{};
    bool operator==(const Owner&)const=default;
};
inline constexpr std::array<size_t,4> region_offsets{0x960,0x968,0x988,0x9a8};
inline constexpr std::array<uintptr_t,4> addresspoints{0x17152d0,0x16fe170,0x1714100,0x1700260};
inline constexpr std::array<size_t,4> reverse_region{0x900,0xf0,0x80,0x70};

// The caller supplies the bound NativeOpen root, actual Dungeon and original UID/RTTI lookup. No current camera Shard participates.
template<class Read,class Typed,class Find,class Identify>
bool read(Read& at,Typed& typed,Find& find,Identify& identify,uintptr_t image,uintptr_t root,uintptr_t dungeon,Owner& out,std::string& error){
    out={};uintptr_t template_shard=0,found=0;Owner owner;owner.root=root;owner.dungeon=dungeon;
    if(!at(dungeon,0x48,template_shard,error)||!at(dungeon,0x18,owner.dungeon_uid,error))return false;
    if(!owner.dungeon_uid){error="Dungeon template owner has no full native UID";return false;}
    dungeon_scene_key::Scene scene;
    if(!dungeon_scene_key::read(image,root,template_shard,at,typed,identify,scene,error))return false;
    if(scene.type!=1||scene.dungeon!=dungeon){error="Dungeon template owner reciprocal native scene differs";return false;}
    if(!find(owner.dungeon_uid,found,error))return false;
    if(found!=dungeon){error="Dungeon template full UID registry resolves another owner";return false;}
    owner.key=scene.key;owner.shard=scene.shard;owner.region=scene.region;owner.exterior=scene.exterior;owner.exterior_uid=scene.exterior_uid;
    if(!find(owner.exterior_uid,found,error))return false;
    if(found!=owner.exterior){error="Dungeon template exterior full UID registry differs";return false;}
    std::set<uintptr_t> unique;
    for(size_t i=0;i<owner.managers.size();++i){uintptr_t ap=0,region=0,shard=0;
        if(!at(owner.region,region_offsets[i],owner.managers[i],error))return false;
        if(!owner.managers[i]||!unique.insert(owner.managers[i]).second){error="Dungeon template manager is absent or aliases another family";return false;}
        if(!at(owner.managers[i],0,ap,error)||!at(owner.managers[i],reverse_region[i],region,error))return false;
        if(ap!=image+addresspoints[i]||region!=owner.region){error="Dungeon template manager class/reciprocal Region differs";return false;}
        if(i==size_t(Manager::Scenery)||i==size_t(Manager::Gizmo)){
            if(!at(owner.managers[i],i==size_t(Manager::Scenery)?0x908:0x68,shard,error))return false;
            if(shard!=owner.shard){error="Dungeon template manager reciprocal Shard differs";return false;}}
    }
    uintptr_t shard=0,region=0,exterior=0;uint64_t uid=0;
    if(!at(dungeon,0x18,uid,error)||!at(dungeon,0x48,shard,error)||!at(dungeon,0x50,region,error)||!at(dungeon,0x6f8,exterior,error))return false;
    if(uid!=owner.dungeon_uid||shard!=owner.shard||region!=owner.region||exterior!=owner.exterior){error="Dungeon template native owner changed during read";return false;}
    for(size_t i=0;i<owner.managers.size();++i){uintptr_t manager=0,reverse=0;
        if(!at(owner.region,region_offsets[i],manager,error)||!at(owner.managers[i],reverse_region[i],reverse,error))return false;
        if(manager!=owner.managers[i]||reverse!=owner.region){error="Dungeon template manager ownership changed during read";return false;}}
    out=std::move(owner);return true;
}
}
