#pragma once
#include "dungeon_building_runtime.h"
#include "duplicate_history_reader.h"
#include "duplicate_history_effects.h"
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "mt2mp/game/native_engine.h"
#include <Windows.h>

namespace mt2mp::game::dungeon_building_graph {
namespace sample=npc_probe::dungeon_building;
namespace host=build_actions::private_native::host;
namespace q=npc_quest_targets;
struct GraphMemory {
    host::ProcessMemory native;
    bool read(uintptr_t pointer,void* out,size_t size,std::string& error){
        return native.copy(pointer,out,size,error);
    }
};
struct Proof {
    std::string sha;
    uintptr_t held_node=0;
    std::vector<uintptr_t> operations,execution;
    std::vector<action_session::DuplicateCash> cash;
    bool operator==(const Proof&)const=default;
};
inline uint32_t find_pod(host::FindUid call,uint64_t uid,uintptr_t& out){
    __try{out=uintptr_t(call(uid));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline bool owner(const sample::Snapshot& captured,const std::string& key,sample::BuildingOwner& out,std::string& error){
    dungeon_scene_identity_runtime::Census census;std::vector<sample::BuildingOwner> owners;
    if(!sample::read_owners(captured,census,owners,error))return false;
    const auto found=std::find_if(owners.begin(),owners.end(),[&](const auto& row){return row.key==key;});
    if(found==owners.end())return q::fail(error,"Dungeon Building graph target has no retained complete current owner");
    out=*found;return true;
}
inline bool environment(GraphMemory& memory,uintptr_t& image,host::LookupApi& lookup,std::string& error){
    uint32_t thread=0;std::string hash;
    if(!native::read_resolver_receipt(image,thread,hash,error)||thread!=GetCurrentThreadId()||
       !host::resolve_lookup(memory.native,image,hash,lookup,error))
        return q::fail(error,error.empty()?"Dungeon Building graph current original native binding differs":error);
    return true;
}
inline bool held(const sample::Snapshot& captured,const std::string& key,uintptr_t graph,uintptr_t& node,std::string& error){
    node=0;sample::BuildingOwner target;
    if(!owner(captured,key,target,error))return false;
    if(target.scene!="world"||target.saved.type!="DungeonExterior"||!target.saved.dungeon)return true;
    GraphMemory memory;host::LookupApi lookup;uintptr_t image=0;q::Bounds nodes;
    if(!environment(memory,image,lookup,error)||!q::typed(memory,image,graph+0x18,0x16e2dc0,error)||
       !q::bounds(memory,graph+0x18,100000,nodes,error))return false;
    for(int32_t i=0;i<nodes.count;++i){
        uintptr_t operation=0,ap=0;uint64_t uid=0;
        if(!q::at(memory,nodes.data,size_t(i)*8,operation,error)||!q::at(memory,operation,0,ap,error))return false;
        if(ap!=image+0x173c5b0)continue;
        if(!q::at(memory,operation,0xb8,uid,error))return false;
        if(uid!=target.uid)continue;
        if(node||!sample::building_inverse(captured,operation,key,error))
            return q::fail(error,error.empty()?"Dungeon Exterior graph has repeated or invalid original Make ownership":error);
        node=operation;
    }
    if(!node)return q::fail(error,"Dungeon Exterior removal graph did not retain its original Dungeon");
    return q::unchanged(memory,graph+0x18,100000,nodes,error);
}
inline bool operations(uintptr_t graph,std::vector<uintptr_t>& out,std::string& error){
    out.clear();GraphMemory memory;host::LookupApi lookup;uintptr_t image=0;q::Bounds nodes;
    if(!environment(memory,image,lookup,error)||!q::typed(memory,image,graph+0x18,0x16e2dc0,error)||
       !q::bounds(memory,graph+0x18,100000,nodes,error))return false;
    std::vector<uintptr_t> first,second;
    const auto read=[&](auto& values){
        std::set<uintptr_t> unique;
        for(int32_t i=0;i<nodes.count;++i){
            uintptr_t node=0;
            if(!q::at(memory,nodes.data,size_t(i)*8,node,error))return false;
            if(!node||!unique.insert(node).second)return q::fail(error,"Dungeon Building inverse operation pointer is absent or repeated");
            values.push_back(node);
        }
        return q::unchanged(memory,graph+0x18,100000,nodes,error);
    };
    if(!read(first)||!read(second))return false;
    if(first!=second)return q::fail(error,"Dungeon Building inverse operation order changed during read");
    out=std::move(second);return true;
}
inline bool read_once(const sample::Snapshot& captured,const std::string& key,uintptr_t graph,bool destroying,
    bool moving,const action_session::BuildingState& destination,Proof& out,std::string& error){
    out={};sample::BuildingOwner target;
    if(!owner(captured,key,target,error))return false;
    GraphMemory memory;host::LookupApi lookup;uintptr_t image=0;
    if(!environment(memory,image,lookup,error))return false;
    auto find=[&](uint64_t uid,uintptr_t& value,std::string& cause){
        const auto seh=find_pod(lookup.find,uid,value);
        return !seh||q::fail(cause,"Dungeon Building graph original full UID lookup SEH="+std::to_string(seh));
    };
    nlohmann::json sequence=nlohmann::json::array(),cash=nlohmann::json::array();
    duplicate_history::Graph parsed;
    if(moving){
        q::Bounds nodes;uintptr_t operation=0,ap=0,method=0;uint64_t uid=0;
        action_session::BuildingState stored=destination;
        if(!q::typed(memory,image,graph+0x18,0x16e2dc0,error)||
           !q::bounds(memory,graph+0x18,100000,nodes,error)||nodes.count!=1||
           !q::at(memory,nodes.data,0,operation,error)||!q::at(memory,operation,0,ap,error)||
           ap!=image+0x1786ff0||!q::at(memory,ap,0x10,method,error)||method!=image+0x55ed80||
           !q::at(memory,operation,0xb8,uid,error)||uid!=target.uid||
           !q::at(memory,operation,0xc0,stored.rotation,error)||!q::at(memory,operation,0xd0,stored.x,error)||
           !q::at(memory,operation,0xd4,stored.y,error)||!q::at(memory,operation,0xd8,stored.z,error)||
           !q::at(memory,operation,0xdc,stored.scale,error)||stored!=destination||
           !q::unchanged(memory,graph+0x18,100000,nodes,error))
            return q::fail(error,error.empty()?"Dungeon Building Move inverse original full transform differs":error);
        uintptr_t current=0;if(!find(uid,current,error)||current!=target.object)
            return q::fail(error,error.empty()?"Dungeon Building Move inverse current full UID differs":error);
        parsed.operations={operation};parsed.top_operations={operation};
        sequence.push_back(nlohmann::json::array({"move",key,destination.rotation,
            std::array<float,3>{destination.x,destination.y,destination.z},destination.scale}));
    }else{
        duplicate_history::Known known;
        known.object=target.object;known.root=target.shard;known.region=target.region;known.uid=target.uid;
        known.row.key=key;known.row.family=action_session::DuplicateFamily::Building;
        known.transform=target.saved.transform;
        auto text=[](uintptr_t,std::string&,std::string& cause){return q::fail(cause,"Dungeon Building graph unexpectedly requested a Group text");};
        auto building=[&](uintptr_t node,const duplicate_history::Known& value,std::string& cause){
            return value.uid==target.uid&&sample::building_inverse(captured,node,key,cause);
        };
        auto related=[&](uintptr_t node,uintptr_t ap,std::string& cause){
            return sample::related_inverse(captured,node,ap,cause);
        };
        if(!duplicate_history::read(memory,image,graph,destroying,{{target.uid,known}},find,text,building,related,parsed,error))return false;
        if(parsed.cash.size()!=1)return q::fail(error,"Dungeon Building inverse requires its one original Cash operation");
        for(const auto& row:parsed.cash)cash.push_back(nlohmann::json::array({row.amount,row.position}));
        if(!destroying&&target.scene=="world"&&!held(captured,key,graph,out.held_node,error))return false;
    }
    if(!duplicate_history::execution_order(memory,image,graph,parsed,out.execution,error))return false;
    for(const auto operation:out.execution){
        uintptr_t ap=0;if(!q::at(memory,operation,0,ap,error)||ap<image)return false;
        sequence.push_back(nlohmann::json::array({uint64_t(ap-image),key}));
    }
    out.operations=parsed.operations;
    out.cash=parsed.cash;
    return duplicate_history_effects::digest(nlohmann::json::array({sequence,cash,captured.value.authored,
        captured.value.quests,captured.value.references}),out.sha,error);
}
inline bool read(const sample::Snapshot& captured,const std::string& key,uintptr_t graph,bool destroying,bool moving,
    const action_session::BuildingState& destination,Proof& out,std::string& error){
    Proof before,after;
    if(!read_once(captured,key,graph,destroying,moving,destination,before,error)||
       !read_once(captured,key,graph,destroying,moving,destination,after,error))return false;
    if(before!=after)return q::fail(error,"Dungeon Building complete inverse graph changed during its double read");
    out=std::move(after);return true;
}
}
