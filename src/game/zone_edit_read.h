#pragma once
#include "world_zone_edit.h"
#include "native_polyshape.h"
#include "zone_edit_identity.h"

namespace mt2mp::game::zone_edit_read {
struct Row {
    world_zone_runtime::Current owner;
    npc_quest_targets::Content content;
    action_session::TerrainShape shape;
    bool operator==(const Row&)const=default;
};
template<class Memory,class Find>
bool read(Memory& memory,uintptr_t image,const std::vector<world_zone_runtime::Current>& owners,
    Find&& find,std::vector<Row>& out,std::string& error){
    namespace q=npc_quest_targets;out.clear();std::vector<Row> next;
    const q::Budget budget{4096,4096,1048576,4096};
    std::set<uint64_t> ids;std::set<uintptr_t> pointers;std::set<std::string> keys;
    for(const auto& owner:owners){
        if(!owner.uid||!owner.object||owner.type<0||owner.type>3||!zone_edit_identity::key_valid(owner.key)||
           !ids.insert(owner.uid).second||!pointers.insert(owner.object).second||!keys.insert(owner.key).second)
            return q::fail(error,"Zone authored read repeats or invalidates a canonical current owner");
        Row row;row.owner=owner;uint64_t uid=0;uintptr_t root=0,region=0,registered=0;
        if(!q::at(memory,owner.object,0x18,uid,error)||!q::at(memory,owner.object,0x170,root,error)||
           !q::at(memory,owner.object,0x178,region,error)||!find(uid,registered,error)||
           !q::content(memory,image,owner.object,budget,row.content,error,owner.type)||
           !ui_terrain_runtime::poly::read(memory,owner.object+0x2f0,row.shape,error))return false;
        if(uid!=owner.uid||registered!=owner.object||root!=owner.root||region!=owner.region)
            return q::fail(error,"Zone authored read full UID or reciprocal owner differs");
        next.push_back(std::move(row));
    }
    for(const auto& row:next){
        Row after;after.owner=row.owner;uint64_t uid=0;uintptr_t root=0,region=0,registered=0;
        if(!q::at(memory,row.owner.object,0x18,uid,error)||!q::at(memory,row.owner.object,0x170,root,error)||
           !q::at(memory,row.owner.object,0x178,region,error)||!find(uid,registered,error)||
           !q::content(memory,image,row.owner.object,budget,after.content,error,row.owner.type)||
           !ui_terrain_runtime::poly::read(memory,row.owner.object+0x2f0,after.shape,error))return false;
        if(after!=row||uid!=row.owner.uid||registered!=row.owner.object||root!=row.owner.root||region!=row.owner.region)
            return q::fail(error,"Zone complete authored values or reciprocal ownership changed during read");
    }
    out=std::move(next);error.clear();return true;
}
}
