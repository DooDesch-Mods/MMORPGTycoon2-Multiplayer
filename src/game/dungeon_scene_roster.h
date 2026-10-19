#pragma once
#include "dungeon_scene_key.h"
#include <array>
#include <utility>

namespace mt2mp::game::dungeon_scene_roster {
struct Scene {dungeon_scene_key::Scene owner;int32_t exterior_region=-1,exterior_slot=-1;uint64_t dungeon_uid=0;};
struct Array {uintptr_t data=0;int32_t count=0,capacity=0;std::vector<uintptr_t> members;};
template<class Read>bool array(Read& at,uintptr_t object,Array& out,std::string& e){
    if(!at(object,8,out.data,e)||!at(object,0x10,out.count,e)||!at(object,0x14,out.capacity,e))return false;
    if(out.count<0||out.capacity<out.count||(out.count&&(!out.data||out.data>UINTPTR_MAX-size_t(out.count)*8))){e="Dungeon scene saved owner-array bounds/span differ";return false;}
    std::set<uintptr_t> unique;
    for(int32_t i=0;i<out.count;++i){uintptr_t member=0;if(!at(out.data,size_t(i)*8,member,e))return false;
        if(!member||!unique.insert(member).second){e="Dungeon scene saved owner-array member absent or duplicated";return false;}out.members.push_back(member);}return true;
}
template<class Read>bool unchanged(Read& at,uintptr_t object,const Array& before,std::string& e){
    Array after;if(!array(at,object,after,e))return false;
    if(after.data!=before.data||after.count!=before.count||after.capacity!=before.capacity||after.members!=before.members){e="Dungeon scene saved owner-array changed during read";return false;}return true;
}
// Traverse actual saved root Building children. The native Dungeon and Instance
// reciprocal readers determine scene membership independently of the camera.
template<class Read,class Typed,class Find,class Identify>
bool read(Read& at,Typed& typed,Find& find,Identify& identify,uintptr_t image,uintptr_t map,uintptr_t root,std::vector<Scene>& out,std::string& e){
    out.clear();std::vector<Scene> result;uintptr_t ap=0;
    if(!at(map,0,ap,e))return false;if(ap!=image+0x1742860||root!=map+0x1c0){e="Dungeon scene enumeration actual NativeOpen map/root differs";return false;}
    if(!at(map,0x230,ap,e))return false;if(ap!=image+0x16f04e0){e="Dungeon scene saved root Region array class differs";return false;}
    Array regions;if(!array(at,map+0x230,regions,e))return false;
    std::set<int32_t> ids;std::set<uintptr_t> dungeons,shards,scene_regions;std::set<uint64_t> exterior_uids,dungeon_uids;std::set<std::string> keys;
    for(const auto region:regions.members){uintptr_t shard=0,manager=0,reverse=0;int32_t id=-1;
        if(!at(region,0,ap,e)||!at(region,0x70,shard,e)||!at(region,0x834,id,e)||!at(region,0x950,manager,e))return false;
        if(ap!=image+0x1787930||shard!=root||id<0||!ids.insert(id).second){e="Dungeon scene saved root Region class/Shard/ID differs";return false;}
        if(!at(manager,0,ap,e)||!at(manager,0x450,shard,e)||!at(manager,0x458,reverse,e))return false;
        if(ap!=image+0x17183c0||shard!=root||reverse!=region){e="Dungeon scene root Building manager reciprocal owner differs";return false;}
        if(!at(manager,0x408,ap,e))return false;if(ap!=image+0x172d7c0){e="Dungeon scene root Building main array class differs";return false;}
        Array buildings;if(!array(at,manager+0x408,buildings,e))return false;
        for(size_t slot=0;slot<buildings.members.size();++slot){const auto exterior=buildings.members[slot];uintptr_t dungeon=0;
            if(!at(exterior,0,ap,e)||!at(exterior,0xa38,dungeon,e))return false;
            if(!dungeon)continue;
            if(ap!=image+0x16d93c0||!dungeons.insert(dungeon).second){e="Dungeon scene exterior class or unique Dungeon owner differs";return false;}
            Scene base;base.exterior_region=id;base.exterior_slot=int32_t(slot);uintptr_t template_shard=0,found=0;
            if(!at(dungeon,0x18,base.dungeon_uid,e)||!at(dungeon,0x48,template_shard,e)||!find(base.dungeon_uid,found,e))return false;
            if(!base.dungeon_uid||found!=dungeon||!dungeon_uids.insert(base.dungeon_uid).second){e="Dungeon scene full Dungeon UID/registry differs";return false;}
            const auto add=[&](uintptr_t scene_shard)->bool {Scene entry=base;
                if(!dungeon_scene_key::read(image,root,scene_shard,at,typed,identify,entry.owner,e))return false;
                if(entry.owner.dungeon!=dungeon||entry.owner.exterior!=exterior||!shards.insert(scene_shard).second||
                   !scene_regions.insert(entry.owner.region).second||!keys.insert(entry.owner.key).second){e="Dungeon scene canonical owner/Shard/Region duplicated or foreign";return false;}
                if(!find(entry.owner.exterior_uid,found,e))return false;
                if(found!=exterior){e="Dungeon scene exterior full UID original registry differs";return false;}
                result.push_back(std::move(entry));return true;};
            if(!add(template_shard))return false;
            if(!exterior_uids.insert(result.back().owner.exterior_uid).second){e="Dungeon scene exterior full UID duplicated";return false;}
            // Dungeon.instances is a vsArray header at +4b8; its slots retain the
            // saved native Instance.index, which need not equal the array slot.
            Array instances;if(!array(at,dungeon+0x4b8,instances,e))return false;
            for(const auto instance:instances.members){uintptr_t instance_shard=0;if(!at(instance,0x38,instance_shard,e)||!add(instance_shard))return false;}
            if(!unchanged(at,dungeon+0x4b8,instances,e))return false;
        }
        if(!unchanged(at,manager+0x408,buildings,e))return false;
    }
    if(!unchanged(at,map+0x230,regions,e))return false;out=std::move(result);e.clear();return true;
}
}
