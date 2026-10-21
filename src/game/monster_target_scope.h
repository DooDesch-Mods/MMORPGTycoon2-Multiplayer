#pragma once
#include "direct_monster_identity.h"
#include "monster_target_key.h"

namespace mt2mp::game::monster_target_scope {
using Entry=direct_monster_identity::Entry;
using Scope=direct_monster_identity::Scope;
inline bool fail(std::string& error,const char* cause){error=cause;return false;}
inline bool same_world(const Scope& current,const Scope& owner){
    return current.snapshot==owner.snapshot&&current.epoch==owner.epoch&&current.generation==owner.generation&&current.root==owner.root;
}
inline bool target(const Scope& world,const std::vector<std::pair<std::string,Entry>>& rows,const std::string& argument,
                   bool retired,Entry& out,std::string& error){
    out={};if(world.snapshot.empty()||!world.epoch||!world.generation||!world.root)
        return fail(error,"Monster target requires complete current world ownership");
    if(!monster_target_key::input(argument))return fail(error,"Monster target canonical argument is invalid");
    unsigned matches=0;
    for(const auto& [key,row]:rows)if(key==argument){
        ++matches;
        if(!same_world(world,row.scope)||!direct_monster_identity::valid_scene(row.scope)||!row.confirmed||
           (!retired&&row.retired)||!row.pointer||!row.uid||!row.scope.region||!row.scope.manager)
            return fail(error,"Monster target has a foreign, retired or unconfirmed scene/full-UID owner");
        out=row;
    }
    if(matches!=1)return fail(error,"Monster target is absent or repeated in the confirmed current registry");
    error.clear();return true;
}
inline bool receiver(const Scope& world,const std::vector<std::pair<std::string,Entry>>& rows,uintptr_t object,uint64_t uid,
                     std::string& argument,std::string& error){
    argument.clear();
    for(const auto& [key,row]:rows)if(row.pointer==object&&row.uid==uid&&!row.retired){
        if(!argument.empty())return fail(error,"Monster current receiver/full UID has two canonical owners");
        Entry confirmed;if(!target(world,rows,key,false,confirmed,error))return false;argument=key;
    }
    return !argument.empty()||fail(error,"Monster current receiver/full UID has no confirmed canonical owner");
}
}
