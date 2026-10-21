#pragma once
#include "world_zone_runtime.h"
#include "dungeon_zone_roster.h"
#include "world_zone_key.h"
#include "snapshot_zone_key.h"
#include <map>
#include <set>

namespace mt2mp::game::zone_edit_identity {
using Current=world_zone_runtime::Current;
inline bool key_valid(const std::string& value){return world_zone_key::valid(value)||snapshot_zone_key::valid(value);}
inline bool owner(const Current& row,const dungeon_zone_roster::Census& current){
    return row.root==current.shard&&row.region==current.region&&row.manager==current.manager&&row.scene_key=="root";
}
// The native operation supplies its actual newly created receiver, or exact
// retained Make history. Current survivors keep their assigned alias and UID.
template<class Find>
bool reconcile(const std::vector<Current>& before,const dungeon_zone_roster::Census& current,
    const std::string& operation,uintptr_t born,uint64_t born_uid,
    const std::map<uint64_t,Current>& restored,Find&& find,std::vector<Current>& out,std::string& error){
    const auto fail=[&](const char* reason){error=reason;return false;};
    out.clear();
    if(!monster_target_key::uuid(operation)||bool(born)!=bool(born_uid))return fail("Zone edit identity operation/birth receipt differs");
    std::map<uint64_t,Current> prior;std::set<uintptr_t> prior_pointers;std::set<std::string> prior_keys;
    for(const auto& row:before){
        if(!row.uid||!row.object||row.type<0||row.type>3||!owner(row,current)||!key_valid(row.key)||
           !prior.emplace(row.uid,row).second||!prior_pointers.insert(row.object).second||!prior_keys.insert(row.key).second)
            return fail("Zone edit before owner/full UID/canonical alias duplicated or invalid");
    }
    if(born_uid&&(prior.contains(born_uid)||prior_pointers.contains(born)||restored.contains(born_uid)))
        return fail("Zone edit new receiver reuses a current or restored identity");
    std::vector<Current> next;std::set<uint64_t> live;std::set<uintptr_t> pointers;std::set<std::string> keys;
    bool found_birth=false;
    for(const auto& member:current.members){
        if(!member.uid||!member.object||member.type<0||member.type>3||!live.insert(member.uid).second||!pointers.insert(member.object).second)
            return fail("Zone edit current array repeats an identity or has invalid type");
        uintptr_t registered=0;if(!find(member.uid,registered,error))return false;
        if(registered!=member.object)return fail("Zone edit current full UID registry differs from its actual receiver");
        Current row;const auto existing=prior.find(member.uid);
        if(existing!=prior.end()){
            row=existing->second;
            if(row.object!=member.object||row.type!=member.type)return fail("Zone edit survivor replaced its current receiver or native type");
        }else if(const auto history=restored.find(member.uid);history!=restored.end()){
            row=history->second;
            if(row.uid!=member.uid||row.type!=member.type||!owner(row,current)||!key_valid(row.key))
                return fail("Zone edit original Make authority has a different full UID/type/owner");
            row.object=member.object;
        }else{
            if(member.object!=born||member.uid!=born_uid||found_birth)return fail("Zone edit native member has no actual birth or original Make authority");
            found_birth=true;
            row={"action:"+operation,"root",member.object,current.shard,current.region,current.manager,member.uid,member.type};
        }
        if(!keys.insert(row.key).second)return fail("Zone edit current canonical alias duplicated");
        next.push_back(std::move(row));
    }
    if(bool(born)!=found_birth)return fail("Zone edit original new receiver is absent from its complete current array");
    for(const auto& [uid,row]:prior){
        (void)row;if(live.contains(uid))continue;uintptr_t registered=0;
        if(!find(uid,registered,error))return false;
        if(registered)return fail("Zone edit removed owner remains registered under its full UID");
    }
    out=std::move(next);error.clear();return true;
}
}
