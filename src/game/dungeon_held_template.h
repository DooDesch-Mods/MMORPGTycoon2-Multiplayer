#pragma once
#include "dungeon_scene_roster.h"
#include <algorithm>

namespace mt2mp::game::dungeon_held_template {
// MakeDo transfers the live Dungeon to MakeFunctionalBuilding+1e0. Its
// destructor owns that Dungeon until Do transfers it to the restored exterior.
template<class Read,class Find>
bool read(Read& at,Find& find,uintptr_t image,uintptr_t root,uintptr_t node,
          const dungeon_scene_roster::Scene& before,const std::vector<dungeon_scene_roster::Scene>& scenes,std::string& error){
    const auto& owner=before.owner;
    const auto fail=[&](const char* cause){if(error.empty())error=cause;return false;};
    if(!image||!node||!root||owner.type!=1||owner.instance||owner.instance_index!=-1||
       owner.key!=owner.exterior_key+"/template"||!dungeon_scene_key::building_key(owner.exterior_key)||
       !owner.exterior_uid||!before.dungeon_uid)
        return fail("Held Dungeon template requires its exact original exterior and template");
    const auto sample=[&]()->bool{
        uintptr_t ap=0,method=0,dungeon=0,actual_root=0,found=0,shard=0,region=0,exterior=0,reverse=0;
        uint64_t exterior_uid=0,dungeon_uid=0;int32_t type=-1;
        if(!at(node,0,ap,error)||ap!=image+0x173c5b0||
           !at(ap,0x10,method,error)||method!=image+0x548c70||
           !at(node,0xb8,exterior_uid,error)||exterior_uid!=owner.exterior_uid||
           !at(node,0x168,actual_root,error)||actual_root!=root||
           !at(node,0x1e0,dungeon,error)||dungeon!=owner.dungeon||
           !find(exterior_uid,found,error)||found)
            return fail("Held Dungeon template original inverse class/method/exterior absence differs");
        if(!at(dungeon,0,ap,error)||ap!=image+0x16c0560||
           !at(dungeon,0x18,dungeon_uid,error)||dungeon_uid!=before.dungeon_uid||
           !find(dungeon_uid,found,error)||found!=dungeon||
           !at(dungeon,0x48,shard,error)||shard!=owner.shard||
           !at(dungeon,0x50,region,error)||region!=owner.region||
           !at(dungeon,0x6f8,exterior,error)||exterior)
            return fail("Held Dungeon template full UID or detached original ownership differs");
        if(!at(shard,0,ap,error)||ap!=image+0x174ad20||
           !at(shard,0x48,type,error)||type!=1||
           !at(shard,0x18,reverse,error)||reverse!=dungeon||
           !at(region,0,ap,error)||ap!=image+0x1787930||
           !at(region,0x70,reverse,error)||reverse!=shard)
            return fail("Held Dungeon template reciprocal Shard/Region differs");
        dungeon_scene_roster::Array instances;
        if(!dungeon_scene_roster::array(at,dungeon+0x4b8,instances,error))return false;
        if(scenes.size()!=instances.members.size()+1)
            return fail("Held Dungeon complete template/Instance roster count differs");
        std::set<uintptr_t> expected;
        for(const auto& scene:scenes){
            const auto& member=scene.owner;
            if(member.dungeon!=dungeon||member.exterior_key!=owner.exterior_key||
               scene.dungeon_uid!=before.dungeon_uid)
                return fail("Held Dungeon retained scene belongs to another original owner");
            if(member.type==1){if(member.shard!=owner.shard||member.region!=owner.region)return fail("Held Dungeon template owner differs");continue;}
            int32_t index=-1;uintptr_t instance_owner=0;
            if(member.type!=2||!member.instance||!expected.insert(member.instance).second||
               std::find(instances.members.begin(),instances.members.end(),member.instance)==instances.members.end()||
               !at(member.instance,0,ap,error)||ap!=image+0x1719450||
               !at(member.instance,0x18,index,error)||index!=member.instance_index||
               !at(member.instance,0x30,instance_owner,error)||instance_owner!=dungeon||
               !at(member.instance,0x38,shard,error)||shard!=member.shard||
               !at(member.instance,0x40,region,error)||region!=member.region||
               !at(shard,0,ap,error)||ap!=image+0x174ad20||
               !at(shard,0x48,type,error)||type!=2||
               !at(shard,0x20,reverse,error)||reverse!=member.instance||
               !at(region,0,ap,error)||ap!=image+0x1787930||
               !at(region,0x70,reverse,error)||reverse!=shard)
                return fail("Held Dungeon original Instance reciprocal membership differs");
        }
        if(expected.size()!=instances.members.size()||!dungeon_scene_roster::unchanged(at,dungeon+0x4b8,instances,error))
            return fail("Held Dungeon original Instance roster changed");
        return true;
    };
    if(!sample()||!sample())return false;
    error.clear();return true;
}
}
