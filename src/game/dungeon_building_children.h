#pragma once
#include "building_npc_lifecycle.h"
#include "duplicate_quest_history.h"
#include <map>

namespace mt2mp::game::dungeon_building_children {
namespace child=building_npc_lifecycle;
namespace qr=monster_quest_retirement;
using Children=std::map<std::string,child::Receipt>;
inline bool fail(std::string& e,const char* cause){e=cause;return false;}
inline const child::Receipt* owner(const Children& children,uintptr_t pointer,uint64_t uid,const std::string& key){
    const child::Receipt* result=nullptr;
    for(const auto& [parent,row]:children){(void)parent;
        if(row.present&&row.child.row.local_pointer==pointer&&row.child.row.local_uid==uid&&row.child.key==key){
            if(result)return nullptr;result=&row;
        }
    }
    return result;
}
inline bool same_identity(const child::Receipt& before,const child::Receipt& retained){
    return before.parent.key==retained.parent.key&&before.parent.uid==retained.parent.uid&&before.parent.scene==retained.parent.scene&&
        before.lease.snapshot==retained.lease.snapshot&&before.lease.duplicate_uuid==retained.lease.duplicate_uuid&&
        before.lease.epoch==retained.lease.epoch&&before.lease.generation==retained.lease.generation&&
        before.present==retained.present&&(!before.present||(before.child.key==retained.child.key&&
        before.child.row.local_uid==retained.child.row.local_uid));
}
// This seed is consumed only by the complete original Quest reader. A null UID
// is insufficient on its own: an existing confirmed retirement must own it.
template<class Port>
bool history_seed(const qr::Witness& original,const Children& children,Port& port,qr::Witness& out,std::string& e){
    std::set<uintptr_t> retired;std::vector<duplicate_quest_history::OwnerRebind> restored;
    for(const auto& [pointer,row]:original.owners){uintptr_t current=0;
        if(!port.find(row.uid,current,e))return false;
        if(current==pointer)continue;
        const auto* source=owner(children,pointer,row.uid,row.key);
        if(!source)return fail(e,"Dungeon Building previous Quest owner changed without a retained child lifecycle");
        child::Receipt retained;bool found=false,dead=false;
        if(!port.retained(source->parent.key,retained,found,dead,e))return false;
        if(!found||!same_identity(*source,retained)||retained.inverse_child_uid!=row.uid)
            return fail(e,"Dungeon Building previous Quest owner lacks its confirmed original child inverse");
        if(!current){
            if(!dead)return fail(e,"Dungeon Building previous Quest owner is absent without confirmed retirement");
            retired.insert(pointer);
        }else{
            if(dead||retained.child.row.local_pointer!=current)
                return fail(e,"Dungeon Building previous Quest owner changed receiver without confirmed restoration");
            restored.push_back({pointer,current,row.uid,row.key});
        }
    }
    return duplicate_quest_history::previous_owners(original,original,restored,retired,out,e);
}
// Port performs the existing original inverse validation and local registry
// admission. It never executes the graph or creates/destroys a native actor.
template<class Port>
bool retire_children(const Children& before,const std::vector<uintptr_t>& inverse_operations,Port& port,
                     Children& verified,std::set<uintptr_t>& retired,std::string& e){
    verified.clear();retired.clear();std::set<uintptr_t> nodes;
    for(auto node:inverse_operations)if(!node||!nodes.insert(node).second)return fail(e,"Dungeon Building original inverse operation is null or repeated");
    for(const auto& [key,saved]:before){
        if(key!=saved.parent.key)return fail(e,"Dungeon Building before child parent key differs");
        if(!saved.present)continue;
        uintptr_t actual=0;if(!port.find(saved.child.row.local_uid,actual,e))return false;
        if(actual==saved.child.row.local_pointer)continue;
        if(actual)return fail(e,"Dungeon Building surviving child receiver changed before original restoration admission");
        auto receipt=saved;
        if(!port.inverse(inverse_operations,receipt,e)||!port.retire(receipt,e))return false;
        retired.insert(saved.child.row.local_pointer);verified.emplace(key,std::move(receipt));
    }
    return true;
}
template<class Port>
bool publish(const Children& before,const Children* consumed,const std::vector<child::Parent>& current,
             const std::vector<uintptr_t>& inverse_operations,Port& port,Children& out,
             std::set<uintptr_t>& retired,std::vector<duplicate_quest_history::OwnerRebind>& restored,std::string& e){
    out.clear();retired.clear();restored.clear();std::set<std::string> parents;Children verified;
    if(!retire_children(before,inverse_operations,port,verified,retired,e))return false;
    for(const auto& parent:current){
        if(!parents.insert(parent.key).second)return fail(e,"Dungeon Building current child parent repeats");
        child::Receipt retained,actual;bool found=false,dead=false;
        if(!port.retained(parent.key,retained,found,dead,e))return false;
        if(found&&dead){
            if(!consumed)return fail(e,"Dungeon Building restored child lacks the consumed original history sample");
            const auto source=consumed->find(parent.key);
            if(source==consumed->end()||!same_identity(source->second,retained)||
               (retained.present&&retained.inverse_child_uid!=retained.child.row.local_uid))
                return fail(e,"Dungeon Building restored child differs from consumed original full UID/lease");
            if(!port.restore(retained,parent,actual,e))return false;
            if(actual.present)restored.push_back({source->second.child.row.local_pointer,actual.child.row.local_pointer,
                actual.child.row.local_uid,actual.child.key});
        }else if(!port.capture(parent,found?&retained:nullptr,actual,e))return false;
        if(actual.parent.key!=parent.key||actual.parent.object!=parent.object||actual.parent.uid!=parent.uid||actual.parent.scene!=parent.scene)
            return fail(e,"Dungeon Building published child parent differs from the complete current census");
        out.emplace(parent.key,std::move(actual));
    }
    return true;
}
}
