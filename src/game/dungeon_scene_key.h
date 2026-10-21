#pragma once
#include "snapshot_building_key.h"
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::dungeon_scene_key {
struct Scene {
    std::string key,exterior_key;
    int32_t type=-1,instance_index=-1;
    uintptr_t shard=0,region=0,dungeon=0,instance=0,exterior=0;
    uint64_t exterior_uid=0;
};
inline bool fail(std::string& e,const char* cause){e=cause;return false;}
inline bool building_key(const std::string& key){
    if(snapshot_building_key::valid(key))return true;
    if(key.size()!=43||!key.starts_with("action:"))return false;
    for(size_t i=7;i<43;++i){const auto at=i-7;const char c=key[i];
        if(at==8||at==13||at==18||at==23){if(c!='-')return false;}
        else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;}
    return true;
}
// The caller supplies its current NativeOpen root and ShardManager active Shard.
// typed uses the existing native GCC RTTI ABI. identify must resolve the exterior
// through the existing full-UID root Building registry and current owner census.
// Instance indices identify current scene membership within that world lifetime;
// they do not manufacture a historical action/birth UUID after a bare load.
template<class Read,class Typed>
bool read_owner(uintptr_t image,uintptr_t root,uintptr_t current,Read&& at,Typed&& typed,Scene& out,std::string& e){
    out={};e.clear();
    if(!image||!root||!current||image>UINTPTR_MAX-0x174ad20)return fail(e,"Dungeon scene requires the current image/root/active Shard");
    const auto addresspoint=[&](uintptr_t object,uintptr_t rva){uintptr_t ap=0;
        return object&&at(object,0,ap,e)&&(ap==image+rva||fail(e,"Dungeon scene native addresspoint differs"));};
    int32_t root_type=-1,type=-1;
    if(!addresspoint(root,0x174ad20)||!at(root,0x48,root_type,e)||root_type!=0||
       !addresspoint(current,0x174ad20)||!at(current,0x48,type,e))return fail(e,e.empty()?"Dungeon scene current root type differs":e.c_str());
    Scene result;result.type=type;result.shard=current;
    if(current==root){if(type!=0)return fail(e,"Dungeon scene root Shard type differs");result.key="world";out=std::move(result);return true;}
    if(type==1){if(!at(current,0x18,result.dungeon,e))return false;}
    else if(type==2){
        if(!at(current,0x20,result.instance,e)||!addresspoint(result.instance,0x1719450)||
           !typed(result.instance,0x1657270,0x16650e0,e))return false;
        uintptr_t reverse=0;
        if(!at(result.instance,0x38,reverse,e)||reverse!=current||!at(result.instance,0x30,result.dungeon,e)||
           !at(result.instance,0x40,result.region,e)||!result.region||!at(result.instance,0x18,result.instance_index,e)||result.instance_index<0)
            return fail(e,e.empty()?"Dungeon scene Instance reciprocal Shard/Region/index differs":e.c_str());
    }else return fail(e,"Dungeon scene active non-root Shard has unsupported type");
    if(!addresspoint(result.dungeon,0x16c0560)||!typed(result.dungeon,0x1673900,0x164e3d0,e))return false;
    uintptr_t template_shard=0,template_region=0,reverse=0;int32_t template_type=-1;
    if(!at(result.dungeon,0x48,template_shard,e)||!at(result.dungeon,0x50,template_region,e)||!template_region||
       !addresspoint(template_shard,0x174ad20)||!at(template_shard,0x48,template_type,e)||template_type!=1||
       !at(template_shard,0x18,reverse,e)||reverse!=result.dungeon)
        return fail(e,e.empty()?"Dungeon scene template reciprocal owner differs":e.c_str());
    if(type==1){if(template_shard!=current)return fail(e,"Dungeon scene template Shard differs");result.region=template_region;}
    else {
        uintptr_t data=0;int32_t count=-1,capacity=-1;
        if(!at(result.dungeon,0x4c0,data,e)||!at(result.dungeon,0x4c8,count,e)||!at(result.dungeon,0x4cc,capacity,e)||
           !data||count<1||count>1000000||capacity<count||size_t(count)>(UINTPTR_MAX-data)/8)
            return fail(e,e.empty()?"Dungeon scene Instance owner-array bounds differ":e.c_str());
        std::set<uintptr_t> pointers;std::set<int32_t> indices;unsigned matches=0;
        std::vector<uintptr_t> members;
        for(int32_t i=0;i<count;++i){uintptr_t member=0,owner=0;int32_t index=-1;
            if(!at(data,size_t(i)*8,member,e)||!addresspoint(member,0x1719450)||!at(member,0x30,owner,e)||owner!=result.dungeon||
               !at(member,0x18,index,e)||index<0||!pointers.insert(member).second||!indices.insert(index).second)
                return fail(e,e.empty()?"Dungeon scene Instance member owner/pointer/index is foreign or duplicated":e.c_str());
            if(member==result.instance)++matches;members.push_back(member);}
        if(matches!=1)return fail(e,"Dungeon scene active Instance absent from unique owner membership");
        uintptr_t final_data=0;int32_t final_count=0,final_capacity=0;
        if(!at(result.dungeon,0x4c0,final_data,e)||final_data!=data||!at(result.dungeon,0x4c8,final_count,e)||final_count!=count||
           !at(result.dungeon,0x4cc,final_capacity,e)||final_capacity!=capacity)return fail(e,e.empty()?"Dungeon scene Instance owner array changed":e.c_str());
        for(size_t i=0;i<members.size();++i){uintptr_t member=0;if(!at(data,i*8,member,e)||member!=members[i])
                return fail(e,e.empty()?"Dungeon scene Instance owner membership changed":e.c_str());}
    }
    if(!addresspoint(result.region,0x1787930)||!at(result.region,0x70,reverse,e)||reverse!=current)
        return fail(e,e.empty()?"Dungeon scene Region reciprocal Shard differs":e.c_str());
    if(!at(result.dungeon,0x6f8,result.exterior,e)||!addresspoint(result.exterior,0x16d93c0)||
       !at(result.exterior,0x18,result.exterior_uid,e)||!result.exterior_uid||!typed(result.exterior,0x1673900,0x1655090,e)||
       !at(result.exterior,0xa38,reverse,e)||reverse!=result.dungeon||!at(result.exterior,0x170,reverse,e)||reverse!=root)
        return fail(e,e.empty()?"Dungeon scene exterior full UID/RTTI/root/reverse owner differs":e.c_str());
    uintptr_t final_owner=0;uint64_t final_uid=0;
    if(!at(result.exterior,0x18,final_uid,e)||final_uid!=result.exterior_uid||!at(result.exterior,0xa38,final_owner,e)||final_owner!=result.dungeon||
       !at(current,type==1?0x18:0x20,final_owner,e)||final_owner!=(type==1?result.dungeon:result.instance))
        return fail(e,e.empty()?"Dungeon scene identity changed during canonical lookup":e.c_str());
    out=std::move(result);return true;
}
template<class Read,class Typed,class Identify>
bool read(uintptr_t image,uintptr_t root,uintptr_t current,Read&& at,Typed&& typed,Identify&& identify,Scene& out,std::string& e){
    out={};Scene result;
    if(!read_owner(image,root,current,at,typed,result,e))return false;
    if(current==root){out=std::move(result);return true;}
    if(!identify(result.exterior,result.exterior_uid,result.exterior_key,e)||!building_key(result.exterior_key))
        return fail(e,e.empty()?"Dungeon scene exterior has no current canonical Building key":e.c_str());
    uintptr_t final_owner=0;uint64_t final_uid=0;
    if(!at(result.exterior,0x18,final_uid,e)||final_uid!=result.exterior_uid||!at(result.exterior,0xa38,final_owner,e)||final_owner!=result.dungeon||
       !at(current,result.type==1?0x18:0x20,final_owner,e)||final_owner!=(result.type==1?result.dungeon:result.instance))
        return fail(e,e.empty()?"Dungeon scene identity changed during canonical lookup":e.c_str());
    result.key=result.exterior_key+(result.type==1?"/template":"/instance/"+std::to_string(result.instance_index));
    out=std::move(result);return true;
}
}
