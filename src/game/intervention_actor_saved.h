#pragma once
#include "intervention_actor_read.h"

namespace mt2mp::game::interventions::actors {
template<class Read,class Find>
bool actor_link(Read& at,Find& find,uintptr_t image,const Links& links,uintptr_t pointer,Target& out,std::string& error){
    const auto found=links.actors.find(pointer);if(found==links.actors.end()){error="Actor saved effect user lacks its complete current canonical Actor owner";return false;}
    const auto& expected=found->second;uintptr_t registered=0,ap=0;uint64_t uid=0;
    const uintptr_t expected_ap=expected.target.family==Family::Boss?0x1743040:expected.target.family==Family::Npc?0x1742980:expected.target.family==Family::Monster?0x16c0770:0x1744e90;
    if(!at(pointer,0,ap,error)||ap!=image+expected_ap||!at(pointer,0x18,uid,error)||uid!=expected.uid||!find(uid,registered,error)||registered!=pointer){if(error.empty())error="Actor saved effect user class/current full UID differs";return false;}
    out=expected.target;return true;
}
template<class Read,class Find>
bool ability_link(Read& at,Find& find,uintptr_t image,const Links& links,uintptr_t pointer,std::string& key,uint64_t& out_uid,std::string& error){
    const auto found=links.abilities.find(pointer);if(found==links.abilities.end()){error="Actor saved effect/item prototype lacks its current Character ability slot";return false;}
    uintptr_t ap=0,registered=0;uint64_t uid=0;
    if(!at(pointer,0,ap,error)||ap!=image+0x171c930||!at(pointer,0x18,uid,error)||uid!=found->second.uid||!find(uid,registered,error)||registered!=pointer){if(error.empty())error="Actor saved effect/item prototype class/current full UID differs";return false;}
    key=found->second.key;out_uid=uid;return true;
}
template<class Read,class Copy,class Find>
bool read_saved(Read& at,Copy& copy,Find& find,uintptr_t image,const Local& owner,Row& row,Local& local,std::string& error){
    const auto failed=[&](const char* cause){if(error.empty())error=cause;return false;};
    if(!owner.links)return failed("Actor original saved fields require complete current canonical links");const auto& links=*owner.links;
    for(size_t n=0;n<4;++n){const uintptr_t object=owner.object+0x4e8+n*0x40;uintptr_t ap=0,user=0,ability=0;auto& value=row.status_effects[n];
        if(!at(object,0,ap,error)||ap!=image+0x1720600||!at(object,0x18,value.type,error)||!at(object,0x1c,value.parameter_int,error)||!at(object,0x20,value.parameter_float,error)||!at(object,0x24,value.timer,error)||!at(object,0x28,value.duration,error)||!at(object,0x30,user,error)||!at(object,0x38,ability,error))return failed("Actor original StatusEffectData class/fields differ");
        if(user){Target target;if(!actor_link(at,find,image,links,user,target,error))return false;value.user=std::move(target);}else value.user.reset();
        uint64_t ability_uid=0;if(ability){if(!ability_link(at,find,image,links,ability,value.ability,ability_uid,error))return false;}else value.ability.clear();
    }
    const uintptr_t set=owner.object+0x7a8;uintptr_t ap=0,character=0;
    if(!at(set,0,ap,error)||ap!=image+0x16e3c30||!at(set,8,local.item_data,error)||!at(set,0x10,local.item_count,error)||!at(set,0x14,local.item_capacity,error)||!at(set,0x30,character,error)||character!=owner.object)return failed("Actor original AbilitySet class/physical owner differs");
    if(local.item_count<0||local.item_count>10000||local.item_capacity<local.item_count||(local.item_count&&(!local.item_data||local.item_data>UINTPTR_MAX-size_t(local.item_count)*8)))return failed("Actor original AbilitySet native item extent invalid");
    std::vector<uintptr_t> members(size_t(local.item_count),uintptr_t{});if(local.item_count&&!copy(local.item_data,members.data(),members.size()*8,error))return false;
    row.abilities.clear();local.items.clear();std::set<uintptr_t> pointers;std::set<uint64_t> ids;
    for(const auto pointer:members){Item item;ItemLocal binding;binding.pointer=pointer;uintptr_t registered=0,item_character=0,weak_ap=0,target_slot=0;int32_t references=-1;
        if(!pointer||!pointers.insert(pointer).second||!at(pointer,0,ap,error)||ap!=image+0x17437f0||!at(pointer,0x18,binding.uid,error)||!binding.uid||!ids.insert(binding.uid).second||!find(binding.uid,registered,error)||registered!=pointer||!at(pointer,0x80,item_character,error)||item_character!=owner.object||!at(pointer,0x38,item.state,error)||!at(pointer,0x3c,item.cooldown,error)||!at(pointer,0x40,weak_ap,error)||weak_ap!=image+0x17117b0||!at(pointer,0x68,binding.prototype,error)||!binding.prototype||!at(pointer,0x70,binding.proxy,error)||!binding.proxy)return failed("Actor original Item class/full UID/owner/prototype weak link differs");
        uintptr_t cast=0;if(!at(pointer,0x30,cast,error)||cast!=binding.prototype||!at(binding.proxy,0,target_slot,error)||target_slot!=binding.prototype+0x38||!at(binding.proxy,8,references,error)||references<1)return failed("Actor original Item Ability prototype cast/weak lifetime differs");
        if(!ability_link(at,find,image,links,binding.prototype,item.prototype,binding.prototype_uid,error))return false;
        row.abilities.push_back(std::move(item));local.items.push_back(binding);
    }
    uintptr_t data=0;int32_t count=-1,capacity=-1;if(!at(set,8,data,error)||!at(set,0x10,count,error)||!at(set,0x14,capacity,error)||data!=local.item_data||count!=local.item_count||capacity!=local.item_capacity)return failed("Actor original AbilitySet membership changed during read");
    return valid(State{row},error);
}
}
