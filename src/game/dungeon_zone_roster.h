#pragma once
#include "dungeon_scene_roster.h"

namespace mt2mp::game::dungeon_zone_roster {
struct Member {
    uintptr_t object=0;uint64_t uid=0;int32_t type=-1;
    bool operator==(const Member&)const=default;
};
struct Census {
    uintptr_t manager=0,shard=0,region=0;
    std::array<dungeon_scene_roster::Array,4> arrays;
    std::vector<Member> members;
};
inline bool fail(std::string& error,const char* cause){error=cause;return false;}

// CloseCell calls the original CullPolyShapeFromZonesExcept, which visits all
// four native arrays. The Monster-only Quest pool cannot witness that cascade.
// This reader retains local identities only; it does not invent saved keys.
template<class Read,class Find>
bool read(Read& at,Find& find,uintptr_t image,uintptr_t manager,uintptr_t shard,uintptr_t region,Census& out,std::string& error){
    out={};Census next;next.manager=manager;next.shard=shard;next.region=region;
    uintptr_t ap=0,owner=0,reverse=0;
    if(!manager||!shard||!region)return fail(error,"Dungeon Zone census requires explicit current scene owners");
    if(!at(manager,0,ap,error)||!at(manager,0xf0,owner,error)||!at(region,0x968,reverse,error))return false;
    if(ap!=image+0x16fe170||owner!=region||reverse!=manager)return fail(error,"Dungeon Zone manager reciprocal Region/class differs");
    if(!at(region,0x70,reverse,error))return false;
    if(reverse!=shard)return fail(error,"Dungeon Zone actual Region/Shard differs");
    std::set<uintptr_t> pointers;std::set<uint64_t> uids;
    for(size_t type=0;type<next.arrays.size();++type){
        const auto array=manager+0x18+type*0x30;
        if(!at(array,0,ap,error))return false;
        if(ap!=image+0x172e1e0)return fail(error,"Dungeon Zone actual four-array class differs");
        if(!dungeon_scene_roster::array(at,array,next.arrays[type],error))return false;
        for(const auto object:next.arrays[type].members){Member row;row.object=object;uintptr_t actor_shard=0,actor_region=0,registered=0;
            if(!at(object,0,ap,error)||!at(object,0x18,row.uid,error)||!at(object,0x170,actor_shard,error)||
               !at(object,0x178,actor_region,error)||!at(object,0x24c,row.type,error))return false;
            if(ap!=image+0x1745ce0||!row.uid||row.type!=int32_t(type)||actor_shard!=shard||actor_region!=region||
               !pointers.insert(object).second||!uids.insert(row.uid).second)return fail(error,"Dungeon Zone native type/full UID/scene membership duplicated or foreign");
            if(!find(row.uid,registered,error))return false;
            if(registered!=object)return fail(error,"Dungeon Zone original full UID registry differs");
            next.members.push_back(row);
        }
    }
    for(size_t type=0;type<next.arrays.size();++type)
        if(!dungeon_scene_roster::unchanged(at,manager+0x18+type*0x30,next.arrays[type],error))return false;
    for(const auto& before:next.members){Member after;after.object=before.object;uintptr_t actor_shard=0,actor_region=0,registered=0;
        if(!at(after.object,0,ap,error)||!at(after.object,0x18,after.uid,error)||!at(after.object,0x170,actor_shard,error)||
           !at(after.object,0x178,actor_region,error)||!at(after.object,0x24c,after.type,error)||!find(after.uid,registered,error))return false;
        if(ap!=image+0x1745ce0||after!=before||actor_shard!=shard||actor_region!=region||registered!=after.object)
            return fail(error,"Dungeon Zone full UID/type/scene changed during census");
    }
    if(!at(manager,0xf0,owner,error)||!at(region,0x968,reverse,error))return false;
    if(owner!=region||reverse!=manager)return fail(error,"Dungeon Zone reciprocal manager changed during census");
    if(!at(region,0x70,reverse,error))return false;
    if(reverse!=shard)return fail(error,"Dungeon Zone reciprocal Shard changed during census");
    out=std::move(next);error.clear();return true;
}
}
