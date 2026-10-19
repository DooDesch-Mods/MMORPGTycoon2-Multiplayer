#pragma once
#include "dungeon_scene_key.h"
#include "party_identity.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace mt2mp::game::dungeon_instance_live {
struct State {
    std::string scene,exterior,party;
    int32_t index=-1;
    float online_fraction=0,timeout=0,completion=0;
    bool wants_online=false,timing_out=false,used=false,completed=false;
    bool operator==(const State&)const=default;
};
struct Local {
    uintptr_t instance=0,dungeon=0,shard=0,region=0,party=0;
    State state;
    bool operator==(const Local&)const=default;
};
inline bool fail(std::string& error,const std::string& cause){error="Dungeon Instance "+cause;return false;}
inline bool valid(const State& value,std::string& error){
    if(!dungeon_scene_key::building_key(value.exterior)||value.index<0||
       value.scene!=value.exterior+"/instance/"+std::to_string(value.index))
        return fail(error,"canonical exterior/scene/native index differs");
    if(!value.party.empty()&&!party_identity::valid_key(value.party))return fail(error,"assigned Party key invalid");
    if(!std::isfinite(value.online_fraction)||value.online_fraction<0||value.online_fraction>1||
       !std::isfinite(value.timeout)||!std::isfinite(value.completion)||value.completion<0)
        return fail(error,"original saved float values invalid");
    // The original timeout can cross zero before ForcePlayersOut is dispatched.
    // Its sign is part of the authoritative state, not a local clamping rule.
    error.clear();return true;
}
template<class Read,class Identify>
bool read(Read& at,Identify& identify,uintptr_t image,const dungeon_scene_key::Scene& scene,Local& out,std::string& error){
    out={};Local value;uintptr_t ap=0,owner=0;int32_t index=-1;
    if(scene.type!=2||!scene.instance||!scene.dungeon||!scene.shard||!scene.region||
       !at(scene.instance,0,ap,error)||ap!=image+0x1719450||
       !at(scene.instance,0x18,index,error)||index!=scene.instance_index||
       !at(scene.instance,0x30,owner,error)||owner!=scene.dungeon||
       !at(scene.instance,0x38,owner,error)||owner!=scene.shard||
       !at(scene.instance,0x40,owner,error)||owner!=scene.region)
        return fail(error,error.empty()?"actual class/index/scene owner differs":error);
    value.instance=scene.instance;value.dungeon=scene.dungeon;value.shard=scene.shard;value.region=scene.region;
    value.state.scene=scene.key;value.state.exterior=scene.exterior_key;value.state.index=index;
    if(!at(value.instance,0x1c,value.state.online_fraction,error)||!at(value.instance,0x24,value.state.timeout,error)||
       !at(value.instance,0x70,value.state.completion,error)||!at(value.instance,0x68,value.party,error))return false;
    std::array<uint8_t,4> flags{};
    constexpr std::array<size_t,4> offsets{0x20,0x28,0x74,0x75};
    for(size_t i=0;i<flags.size();++i)if(!at(value.instance,offsets[i],flags[i],error)||flags[i]>1)
        return fail(error,error.empty()?"original saved Boolean outside0/1":error);
    value.state.wants_online=flags[0]!=0;value.state.timing_out=flags[1]!=0;value.state.used=flags[2]!=0;value.state.completed=flags[3]!=0;
    if(value.party&&!identify(value.party,value.state.party,error))return false;
    if(!valid(value.state,error))return false;
    out=std::move(value);error.clear();return true;
}
struct Receipt {
    bool attempted=false,confirmed=false,unknown=false;
    uint32_t attempted_stores=0,stores=0,party_calls=0;
    std::string cause;
};
// Native membership changes run before this value adoption. The caller holds
// the original drained SaveScope and rechecks the complete current scene roster.
// ReserveForParty is the original b5fe0 leaf, used instead of a foreign pointer.
template<class Resolve,class Store,class Reserve,class Readback>
bool adopt(const Local& before,const State& target,Resolve& resolve,Store& store,Reserve& reserve,Readback& readback,
           Receipt& receipt,std::string& error){
    receipt={};
    const auto reject=[&](const std::string& cause){receipt.unknown=receipt.attempted;receipt.cause=cause;return fail(error,cause);};
    if(!valid(before.state,error)||!valid(target,error))return false;
    if(!before.instance||!before.dungeon||!before.shard||!before.region||before.state.scene!=target.scene||
       before.state.exterior!=target.exterior||before.state.index!=target.index)
        return reject("value adoption cannot create, replace or move an original Instance");
    uintptr_t target_party=0;
    if(!target.party.empty()&&!resolve(target.party,target_party,error))return false;
    if((target.party.empty())!=(target_party==0))return reject("assigned Party resolver returned an invalid binding");
    const auto scalar=[&](size_t offset,const auto& old_value,const auto& new_value){
        if(std::memcmp(&old_value,&new_value,sizeof(old_value))==0)return true;
        receipt.attempted=true;++receipt.attempted_stores;
        if(!store(before,offset,&old_value,&new_value,sizeof(old_value),error))return false;
        ++receipt.stores;return true;
    };
    if(!scalar(0x1c,before.state.online_fraction,target.online_fraction)||
       !scalar(0x24,before.state.timeout,target.timeout)||!scalar(0x70,before.state.completion,target.completion))
        return reject(error.empty()?"original saved float store failed":error);
    const std::array<uint8_t,4> old_flags{uint8_t(before.state.wants_online),uint8_t(before.state.timing_out),uint8_t(before.state.used),uint8_t(before.state.completed)};
    const std::array<uint8_t,4> new_flags{uint8_t(target.wants_online),uint8_t(target.timing_out),uint8_t(target.used),uint8_t(target.completed)};
    constexpr std::array<size_t,4> offsets{0x20,0x28,0x74,0x75};
    for(size_t i=0;i<offsets.size();++i)if(!scalar(offsets[i],old_flags[i],new_flags[i]))
        return reject(error.empty()?"original saved Boolean store failed":error);
    if(before.party!=target_party){
        receipt.attempted=true;++receipt.party_calls;
        if(!reserve(before,target_party,error))return reject(error.empty()?"original ReserveForParty failed":error);
    }
    Local after;
    if(!readback(after,error)||after.instance!=before.instance||after.dungeon!=before.dungeon||
       after.shard!=before.shard||after.region!=before.region||after.party!=target_party||after.state!=target)
        return reject(error.empty()?"complete original saved value readback differs":error);
    receipt.confirmed=true;error.clear();return true;
}
}
