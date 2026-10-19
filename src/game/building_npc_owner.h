#pragma once
#include "building_npc_lifecycle.h"
#include "building_npc_key.h"

namespace mt2mp::game::building_npc_owner {
struct Scope {uintptr_t image=0,root=0,region=0;};
inline bool fail(std::string& error,const char* cause){error=cause;return false;}

// The enclosing runtime proves the current pending Controller lease and world
// lifetime before passing this complete Region census. Lookup precedes every
// dereference of a retained Building or NPC pointer.
template<class Read,class Find>
bool read(Read& at,Find& find,const Scope& scope,const building_npc_lifecycle::Parent& parent,
          const npc_census::State& census,npc_census::Row& out,bool& present,std::string& error){
    out={};present=false;uintptr_t registered=0;
    if(!scope.image||!scope.root||!scope.region||!parent.object||!parent.uid||parent.saved_region<0||!building_npc_key::parent(parent.key))
        return fail(error,"Building child NPC requires exact current canonical parent/scope");
    if(!find(parent.uid,registered,error))return false;
    if(registered!=parent.object)return fail(error,"Building child NPC full parent UID registry differs");
    uintptr_t ap=0,root=0,region=0,manager=0,reverse=0;uint64_t uid=0,child_uid=0;int32_t saved=-1;
    if(!at(parent.object,0,ap,error)||!at(parent.object,0x18,uid,error)||!at(parent.object,0x170,root,error)||
       !at(parent.object,0x178,region,error)||!at(parent.object,0xa30,child_uid,error)||
       !at(scope.region,0x70,reverse,error)||!at(scope.region,0x834,saved,error)||!at(scope.region,0x998,manager,error))return false;
    if(ap!=scope.image+0x16d93c0||uid!=parent.uid||root!=scope.root||region!=scope.region||reverse!=scope.root||
       saved!=parent.saved_region||manager!=census.manager)return fail(error,"Building child NPC parent class/full UID/Region/census owner differs");
    const npc_census::Row* selected=nullptr;std::set<uintptr_t> objects;std::set<uint64_t> uids;
    for(const auto& row:census.rows){uintptr_t actor_owner=0;
        if(!row.local_pointer||!row.local_uid||!objects.insert(row.local_pointer).second||!uids.insert(row.local_uid).second)
            return fail(error,"Building child NPC complete census repeats a native pointer or full UID");
        if(!find(row.local_uid,registered,error))return false;
        if(registered!=row.local_pointer)return fail(error,"Building child NPC current census full UID lookup differs");
        if(!at(row.local_pointer,0x268,actor_owner,error))return false;
        if(row.local_uid==child_uid||actor_owner==parent.object){
            if(selected||!child_uid||row.local_uid!=child_uid||actor_owner!=parent.object)
                return fail(error,"Building child NPC reciprocal parent/full UID is absent, duplicated or foreign");
            selected=&row;
        }
    }
    if(child_uid&&!selected)return fail(error,"Building child NPC a30 full UID is absent from complete Region census");
    uint64_t after_uid=0,after_child=0;
    if(!find(parent.uid,registered,error))return false;
    if(registered!=parent.object)return fail(error,"Building child NPC parent full UID registry changed during read");
    if(!at(parent.object,0x18,after_uid,error)||!at(parent.object,0xa30,after_child,error))return false;
    if(after_uid!=parent.uid||after_child!=child_uid)return fail(error,"Building child NPC parent/full UID changed during read");
    if(selected){uintptr_t actor_owner=0;if(!find(selected->local_uid,registered,error))return false;
        if(registered!=selected->local_pointer)return fail(error,"Building child NPC full UID registry changed during read");
        if(!at(selected->local_pointer,0x268,actor_owner,error))return false;
        if(actor_owner!=parent.object)return fail(error,"Building child NPC reciprocal owner changed during read");
        out=*selected;present=true;
    }
    error.clear();return true;
}
template<class Find>
bool absent(Find& find,const npc_census::State& current,const npc_census::Row& old,std::string& error){
    if(!old.local_pointer||!old.local_uid)return fail(error,"Building child NPC retirement lacks its exact before identity");
    uintptr_t registered=0;if(!find(old.local_uid,registered,error))return false;
    if(registered)return fail(error,"Building child NPC original queued deletion still has a full UID registry member");
    for(const auto& row:current.rows)if(row.local_uid==old.local_uid||row.local_pointer==old.local_pointer)
        return fail(error,"Building child NPC original queued deletion still has a complete-census member");
    error.clear();return true;
}
inline bool restored_uid(const building_npc_lifecycle::Parent& parent,uint64_t inverse_uid,const npc_census::Row& before,const npc_census::Row& after,std::string& error){
    if(inverse_uid){if(inverse_uid!=before.local_uid||after.local_uid!=inverse_uid||(parent.stored_child_uid&&parent.stored_child_uid!=inverse_uid))
        return fail(error,"Original MakeNPC inverse did not restore its complete saved child UID");return true;}
    return fail(error,"Building child NPC restoration lacks its validated complete original MakeNPC inverse UID");
}
template<class Read,class Text>
bool make_npc_inverse(Read& at,Text& text,uintptr_t image,uintptr_t node,const building_npc_lifecycle::Receipt& before,
                      building_npc_lifecycle::MakeNPCInverse& out,std::string& error){
    out={};if(!before.present||!before.child.row.local_uid||!before.original_character_uid||before.original_record_sha.empty())
        return fail(error,"Building MakeNPC inverse lacks its complete original before child receipt");
    uintptr_t ap=0,shard=0,region=0,array_ap=0,data=0;uint64_t uid=0;int32_t type=-1,level=-1,count=-1,capacity=-1;
    std::string name;std::array<unsigned char,0x6c> transform{};std::array<int32_t,2> character{};
    if(!at(node,0,ap,error)||!at(node,0xb8,uid,error)||!text(node,0xc0,name,error)||!at(node,0xe0,type,error)||
       !at(node,0xe8,shard,error)||!at(node,0xf0,region,error)||!at(node,0xf8,transform,error)||!at(node,0x164,level,error)||
       !at(node,0x168,character,error)||!at(node,0x170,array_ap,error)||!at(node,0x178,data,error)||!at(node,0x180,count,error)||!at(node,0x184,capacity,error))return false;
    const auto& row=before.child.row;
    if(ap!=image+0x16de8a0||uid!=row.local_uid||name!=before.original_display_name||type!=row.type||shard!=before.original_shard||region!=before.original_region||
       transform!=before.original_transform||level!=row.level||character!=std::array<int32_t,2>{row.character_type,row.character_slot})
        return fail(error,"Original Building child MakeNPC inverse field differs from complete before witness");
    if(array_ap!=image+0x17469a0||count!=0||capacity!=4||!data)
        return fail(error,"Original MakeNPC constructor QuestData array is not its empty exact native array");
    out={node,uid,before.original_character_uid};error.clear();return true;
}

// MakeNPC inserts exactly one child UID into its produced set and exactly one
// CharacterType UID into its required set. Check both native representations.
template<class Read>
bool inverse_uid_set(Read& at,uintptr_t object,uint64_t expected,std::string& error){
    uintptr_t table=0,values=0;int32_t buckets=0,shift=-1,count=-1,capacity=-1;
    if(!expected)return fail(error,"Original MakeNPC dependency full UID is absent");
    if(!at(object,0,table,error)||!at(object,8,buckets,error)||!at(object,0xc,shift,error)||
       !at(object,0x18,values,error)||!at(object,0x20,count,error)||!at(object,0x24,capacity,error))return false;
    if(!table||!values||buckets<=0||buckets>1000000||(buckets&(buckets-1))||shift<0||shift>31||
       uint64_t(buckets)!=(uint64_t(1)<<(32-shift))||count!=1||capacity<count||capacity>1000000||
       table>UINTPTR_MAX-size_t(buckets)*24)return fail(error,"Original MakeNPC singleton dependency set header differs");
    uint64_t value=0;if(!at(values,0,value,error))return false;
    if(value!=expected)return fail(error,"Original MakeNPC dependency vector full UID differs");
    size_t found=0;std::set<uintptr_t> seen;
    for(int32_t i=0;i<buckets;++i){uintptr_t link=0;if(!at(table,size_t(i)*24+0x10,link,error))return false;
        while(link){uint64_t uid=0;uint32_t hash=0;uintptr_t next=0;
            if(!seen.insert(link).second||seen.size()>1)return fail(error,"Original MakeNPC dependency hash repeats or has an extra member");
            if(!at(link,0,uid,error)||!at(link,8,hash,error)||!at(link,0x10,next,error))return false;
            if(uid!=expected||hash!=uint32_t(uid)||int32_t((hash*0x9e3779ffu)>>shift)!=i)
                return fail(error,"Original MakeNPC dependency hash/full UID/bucket differs");
            ++found;link=next;
        }
    }
    if(found!=1)return fail(error,"Original MakeNPC dependency hash lacks its exact singleton UID");
    uintptr_t final_table=0,final_values=0;int32_t final_buckets=0,final_shift=0,final_count=0,final_capacity=0;
    if(!at(object,0,final_table,error)||!at(object,8,final_buckets,error)||!at(object,0xc,final_shift,error)||
       !at(object,0x18,final_values,error)||!at(object,0x20,final_count,error)||!at(object,0x24,final_capacity,error)||!at(values,0,value,error))return false;
    return (table==final_table&&values==final_values&&buckets==final_buckets&&shift==final_shift&&count==final_count&&capacity==final_capacity&&value==expected)||
        fail(error,"Original MakeNPC dependency set changed during read");
}
}
