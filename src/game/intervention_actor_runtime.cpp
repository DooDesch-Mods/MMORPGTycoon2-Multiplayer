#include "intervention_actor_runtime.h"
#include "intervention_census.h"
#include "intervention_actor_saved.h"
#include "root_saved_building.h"
#include "monster_live_codes.h"
#include "mt2mp/core/hash.h"
#include <Windows.h>

namespace mt2mp::game::interventions::actors {
namespace {
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
struct Read {template<class T>bool operator()(uintptr_t object,size_t offset,T& out,std::string& error){return root_saved_building::bytes(reinterpret_cast<void*>(object),offset,&out,sizeof out,error);}};
bool code(uintptr_t image,std::string& error){
    constexpr uintptr_t leaves[]={0x9ff370,0xc67c80,0x39e750,0x3b1b50,0x39d670,0x287d0,0x28970,0x21df0,0x28730,0x28750,0x28760,0x23890,0x237e0,0x23860,0x39f6e0,0x3bbc00,0x39a920,0x23770,0x23840,0x237a0,0x238b0,0x9ca3a0};
    for(const auto rva:leaves){const auto pin=std::find_if(std::begin(monster_live_codes::values),std::end(monster_live_codes::values),[&](const auto& value){return value.rva==rva;});
        if(pin==std::end(monster_live_codes::values)||pin->tail)return fail(error,"Actor Frame native leaf has no original complete function pin RVA="+std::to_string(rva));
        std::vector<unsigned char> bytes(pin->size);if(!root_saved_building::bytes(reinterpret_cast<void*>(image+rva),0,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=pin->sha)return fail(error,"Actor Frame native original leaf body differs RVA="+std::to_string(rva));}
    struct Pin{uintptr_t rva;size_t size;const char* sha;};
    constexpr Pin npc_leaves[]={{0x3c2a00,64,"f5920de3004cfb920a54ca852cdffde6904e2122c7db69ba64893d6a35500bda"},{0x3c27b0,592,"062090540eac606dc417a74cf12782ff421d2a041eb62a060daa419d3dd98b6e"}};
    for(const auto& pin:npc_leaves){std::vector<unsigned char> bytes(pin.size);if(!root_saved_building::bytes(reinterpret_cast<void*>(image+pin.rva),0,bytes.data(),bytes.size(),error))return false;if(mt2mp::sha256(bytes)!=pin.sha)return fail(error,"Actor Frame native NPC presentation/map leaf body differs RVA="+std::to_string(pin.rva));}
    return true;
}
bool same(const Census& a,const Census& b){
    if(a.actors!=b.actors||a.zones!=b.zones||a.owners!=b.owners||a.users!=b.users||a.bosses.live!=b.bosses.live||a.bosses.pools.size()!=b.bosses.pools.size())return false;
    const auto& x=a.bosses.world;const auto& y=b.bosses.world;
    if(x.game!=y.game||x.mode!=y.mode||x.state!=y.state||x.world!=y.world||x.map!=y.map||x.generation!=y.generation||x.world_epoch!=y.world_epoch||x.uuid!=y.uuid||x.sha256!=y.sha256)return false;
    for(size_t i=0;i<a.bosses.pools.size();++i)if(a.bosses.pools[i].scope!=b.bosses.pools[i].scope||a.bosses.pools[i].native!=b.bosses.pools[i].native)return false;return true;
}
}
bool read_bound(const Local& owner,Row& row,Local& local,std::string& error){
    Read at;const auto image=uintptr_t(GetModuleHandleW(nullptr));uintptr_t registered=0;const auto lookup=native::registered(image,owner.uid,registered);
    if(lookup||registered!=owner.object)return fail(error,"Actor point current full UID no longer resolves its confirmed native object SEH="+std::to_string(lookup));
    auto copy=[](uintptr_t address,void* data,size_t size,std::string& cause){return root_saved_building::bytes(reinterpret_cast<void*>(address),0,data,size,cause);};
    auto maximum=[&](uintptr_t object,int32_t index,float& value,std::string& cause){const auto code=native::maximum(image,object,index,value);return !code||fail(cause,"Actor point original attribute maximum SEH="+std::to_string(code));};
    auto clip=[&](const native::Render& render,int32_t index,Clip& value,std::string& cause){const auto code=native::clip(image,render,index,value);return !code||fail(cause,"Actor point original semantic clip read SEH="+std::to_string(code));};
    auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& cause){const auto code=native::registered(image,uid,pointer);return !code||fail(cause,"Actor point original saved link/item UID lookup SEH="+std::to_string(code));};
    return read_row(at,copy,maximum,clip,image,owner,row,local,error)&&read_saved(at,copy,find,image,owner,row,local,error);
}
bool read_current(const action_session::Context& context,const action_session::Binding& binding,State& out,Locals& locals,std::string& error){
    out.clear();locals.clear();Census before;if(!interventions::census(context,binding,before,error))return false;
    State values;Locals native_rows;Read at;const auto image=uintptr_t(GetModuleHandleW(nullptr));if(!code(image,error))return false;
    auto links=std::make_shared<Links>();
    for(const auto& actor:before.actors){if(!links->actors.emplace(actor.object,ActorLink{actor.target,actor.object,actor.uid}).second||!links->actor_keys.emplace(std::pair{uint8_t(actor.target.family),actor.target.key},actor.object).second)return fail(error,"Actor current complete canonical user links repeat");}
    for(const auto& [pointer,key]:before.owners)if(ability_key(key)){uint64_t uid=0;if(!at(pointer,0x18,uid,error)||!uid)return fail(error,error.empty()?"Actor current Ability prototype full UID absent":error);
        if(!links->abilities.emplace(pointer,AbilityLink{key,pointer,uid}).second||!links->ability_keys.emplace(key,pointer).second)return fail(error,"Actor current complete canonical Ability links repeat");}
    auto copy=[](uintptr_t address,void* data,size_t size,std::string& cause){return root_saved_building::bytes(reinterpret_cast<void*>(address),0,data,size,cause);};
    auto maximum=[&](uintptr_t object,int32_t index,float& value,std::string& cause){const auto code=native::maximum(image,object,index,value);return !code||fail(cause,"Actor Frame original attribute maximum SEH="+std::to_string(code));};
    auto clip=[&](const native::Render& render,int32_t index,Clip& value,std::string& cause){const auto code=native::clip(image,render,index,value);return !code||fail(cause,"Actor Frame original semantic clip read SEH="+std::to_string(code));};
    auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& cause){const auto code=native::registered(image,uid,pointer);return !code||fail(cause,"Actor Frame original saved link/item full UID lookup SEH="+std::to_string(code));};
    for(const auto& actor:before.actors){if(actor.target.family==Family::Toon||(actor.target.family==Family::Monster&&!zone_monster_key(actor.target.key)))continue;
        uintptr_t region=0;if(!at(actor.object,0x178,region,error))return false;
        const auto pool=std::find_if(before.bosses.pools.begin(),before.bosses.pools.end(),[&](const auto& value){return value.scope.region==region;});
        if(pool==before.bosses.pools.end())return fail(error,"Actor Frame canonical actor Region absent from current Root/Dungeon scene census");
        Local owner;owner.target=actor.target;owner.object=actor.object;owner.uid=actor.uid;owner.scope=pool->scope;owner.links=links;
        if(actor.target.family==Family::Monster){if(!at(actor.object,0x8d0,owner.zone,error))return false;const auto zone=std::find_if(before.zones.begin(),before.zones.end(),[&](const auto& value){return value.object==owner.zone;});
            if(zone==before.zones.end())return fail(error,"Actor Frame natural Monster canonical Zone absent from complete current census");owner.zone_uid=zone->uid;
            const auto separator=actor.target.key.rfind(":monster:");const auto index=std::string_view(actor.target.key).substr(separator+9);uint32_t ordinal=0;for(const char digit:index)ordinal=ordinal*10+uint32_t(digit-'0');owner.zone_index=int32_t(ordinal);}
        Row first,second;Local first_local,second_local;
        if(!read_row(at,copy,maximum,clip,image,owner,first,first_local,error)||!read_saved(at,copy,find,image,owner,first,first_local,error)||!read_row(at,copy,maximum,clip,image,owner,second,second_local,error)||!read_saved(at,copy,find,image,owner,second,second_local,error))return false;
        if(first!=second||first_local!=second_local)return fail(error,"Actor Frame complete current row changed during native read key="+actor.target.key);
        if(!native_rows.emplace(actor.target.key,std::move(second_local)).second)return fail(error,"Actor Frame complete canonical identity repeats");values.push_back(std::move(second));
    }
    Census after;if(!interventions::census(context,binding,after,error)||!same(before,after))return fail(error,error.empty()?"Actor Frame complete canonical owner census changed during read":error);
    std::sort(values.begin(),values.end(),[](const auto& a,const auto& b){return a.target.key<b.target.key;});if(!valid(values,error))return false;
    out=std::move(values);locals=std::move(native_rows);error.clear();return true;
}
}
