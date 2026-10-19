#pragma once
#include "direct_monster_identity.h"
#include "dungeon_scene_roster.h"
#include <algorithm>

namespace mt2mp::game::dungeon_monster_reference {
struct Member {
    uintptr_t object=0,zone=0,proxy=0;uint64_t uid=0;
    bool operator==(const Member&)const=default;
};
struct Witness {
    direct_monster_identity::Scope scope;std::string scene,key;
    dungeon_scene_roster::Array array;std::vector<Member> members;
    uintptr_t object=0,shard=0;uint64_t uid=0;
};
inline bool fail(std::string& error,const char* cause){error=cause;return false;}

// Scene ownership comes from the fresh exterior/template/Instance roster. The
// resolver accepts only an already confirmed original full-UID binding.
template<class Read,class Find,class Resolve>
bool read(Read& at,Find& find,Resolve& resolve,uintptr_t image,
          const direct_monster_identity::Scope& scope,const dungeon_scene_roster::Scene& scene,
          uintptr_t manager,uintptr_t selected,Witness& out,std::string& error){
    out={};Witness next;next.scope=scope;next.scene=scene.owner.key;next.object=selected;next.shard=scene.owner.shard;
    uintptr_t ap=0,owner=0,shard=0,actual_manager=0;
    if(scope.snapshot.empty()||!scope.epoch||!scope.generation||!scope.root||scope.region!=scene.owner.region||scope.manager!=manager||
       !selected||!manager||scene.owner.type<1||scene.owner.type>2||scene.owner.shard==scope.root||next.scene.empty())
        return fail(error,"Dungeon direct Monster requires an explicit current template/Instance scope");
    if(!at(manager,0,ap,error)||!at(manager,0x80,owner,error)||!at(scene.owner.region,0x70,shard,error)||
       !at(scene.owner.region,0x988,actual_manager,error))return false;
    if(ap!=image+0x1714100||owner!=scene.owner.region||shard!=scene.owner.shard||actual_manager!=manager)
        return fail(error,"Dungeon direct Monster manager/Region/Shard ownership differs");
    if(!at(manager,0x38,ap,error))return false;
    if(ap!=image+0x16e8b60)return fail(error,"Dungeon direct Monster current main array class differs");
    if(!dungeon_scene_roster::array(at,manager+0x38,next.array,error))return false;
    std::set<uint64_t> uids;bool found=false;
    for(const auto object:next.array.members){Member member;member.object=object;uintptr_t region=0,member_shard=0,registered=0;
        if(!at(object,0,ap,error)||!at(object,0x18,member.uid,error)||!at(object,0x170,member_shard,error)||!at(object,0x178,region,error)||
           !at(object,0x8d0,member.zone,error)||!at(object,0x8d8,member.proxy,error))return false;
        if(ap!=image+0x16c0770||!member.uid||!uids.insert(member.uid).second||member_shard!=scene.owner.shard||region!=scene.owner.region)
            return fail(error,"Dungeon Monster current member class/full UID/scene differs");
        if(!find(member.uid,registered,error))return false;
        if(registered!=object)return fail(error,"Dungeon Monster original full UID registry differs");
        if(object==selected){
            if(found||member.zone||member.proxy)return fail(error,"Dungeon Quest direct Monster is duplicated or owned by a Zone");
            next.uid=member.uid;found=true;
        }
        next.members.push_back(member);
    }
    if(!found)return fail(error,"Dungeon Quest direct Monster is absent from its current manager");
    if(!resolve(scope,selected,next.uid,next.key,error))return false;
    if(!monster_target_key::valid(next.key))return fail(error,"Dungeon Quest direct Monster canonical key is invalid");
    if(!dungeon_scene_roster::unchanged(at,manager+0x38,next.array,error))return false;
    for(const auto& before:next.members){Member after;after.object=before.object;uintptr_t region=0,member_shard=0;
        if(!at(after.object,0x18,after.uid,error)||!at(after.object,0x170,member_shard,error)||!at(after.object,0x178,region,error)||
           !at(after.object,0x8d0,after.zone,error)||!at(after.object,0x8d8,after.proxy,error))return false;
        if(after!=before||member_shard!=scene.owner.shard||region!=scene.owner.region)
            return fail(error,"Dungeon Monster current full UID/scene/Zone reference changed during read");
    }
    if(!at(manager,0x80,owner,error)||!at(scene.owner.region,0x70,shard,error)||!at(scene.owner.region,0x988,actual_manager,error))return false;
    if(owner!=scene.owner.region||shard!=scene.owner.shard||actual_manager!=manager)
        return fail(error,"Dungeon Monster reciprocal scene changed during reference read");
    out=std::move(next);error.clear();return true;
}
}
