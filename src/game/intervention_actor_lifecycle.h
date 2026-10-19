#pragma once
#include "intervention_state.h"
#include "marketing_authority/notices.h"
#include <algorithm>

namespace mt2mp::game::interventions::actors::lifecycle {
namespace journal=marketing_authority::notice;
enum class Effect:uint8_t {OnSpawn=1,ClearLivePointers=2,CharacterDie=3};
enum class Source:uint8_t {
    BossWait=1,BossRespawn=2,BossGenerate=3,BossRegenerate=4,BossAdd=5,
    NpcRespawn=6,NpcGenerate=7,NpcAdd=8,
    MonsterRespawn=9,MonsterGenerate=10,MonsterRegenerate=11,NpcDeath=12,MonsterDeath=13,
    BossBaseDeath=14,NpcBaseDeath=15,MonsterBaseDeath=16
};
struct Event {
    Target target;Effect effect=Effect::OnSpawn;Source source=Source::BossWait;
    std::string action;uint32_t ordinal=0;
    bool operator==(const Event&)const=default;
};
using Batch=journal::Batch;
using Host=journal::Host;
using Client=journal::Client;
inline uintptr_t call_return(Source source){switch(source){
    case Source::BossWait:return 0x397afa;case Source::BossRespawn:return 0x3996cd;case Source::BossGenerate:return 0x398b48;case Source::BossRegenerate:return 0x398bdd;case Source::BossAdd:return 0x2395de;
    case Source::NpcRespawn:return 0x3c98b9;case Source::NpcGenerate:return 0x3c71c2;case Source::NpcAdd:return 0x296a58;
    case Source::MonsterRespawn:return 0x3bc43a;case Source::MonsterGenerate:return 0x3bea41;case Source::MonsterRegenerate:return 0x3bec9d;case Source::NpcDeath:return 0x3c206c;case Source::MonsterDeath:return 0x3be435;
    case Source::NpcBaseDeath:return 0x3c200d;case Source::MonsterBaseDeath:return 0x3be1b8;
    // Boss uses the base Die directly through its actual native virtual slot.
    case Source::BossBaseDeath:return 0;
    default:return 0;}}
inline bool source_for(Family family,Effect effect,uintptr_t caller,Source& out){
    if(effect==Effect::CharacterDie){
        if(family!=Family::Boss&&family!=Family::Npc&&family!=Family::Monster)return false;
        out=family==Family::Boss?Source::BossBaseDeath:family==Family::Npc?Source::NpcBaseDeath:Source::MonsterBaseDeath;
        return family==Family::Boss||call_return(out)==caller;
    }
    for(uint8_t n=1;n<=13;++n){const auto source=Source(n);if(call_return(source)!=caller)continue;
        if(effect==Effect::OnSpawn&&((family==Family::Boss&&n<=5)||(family==Family::Npc&&n>=6&&n<=8)||(family==Family::Monster&&n>=9&&n<=11))){out=source;return true;}
        if(effect==Effect::ClearLivePointers&&((family==Family::Npc&&source==Source::NpcDeath)||(family==Family::Monster&&source==Source::MonsterDeath))){out=source;return true;}}
    return false;
}
inline bool authored(Source source){switch(source){
    case Source::BossGenerate:case Source::BossRegenerate:case Source::BossAdd:
    case Source::NpcGenerate:case Source::NpcAdd:case Source::MonsterGenerate:case Source::MonsterRegenerate:return true;
    default:return false;}}
inline bool ordered_receipt(bool original_scope,bool intervention_original,Source source){return original_scope&&(intervention_original||authored(source));}
inline bool valid(const Event& event){
    if(!interventions::valid(event.target)||(!event.action.empty()&&!monster_target_key::uuid(event.action))||(event.action.empty()?event.ordinal!=0:event.ordinal==0)||uint8_t(event.source)<1||uint8_t(event.source)>16)return false;
    switch(event.source){
    case Source::BossWait:case Source::BossRespawn:case Source::BossGenerate:case Source::BossRegenerate:case Source::BossAdd:return event.target.family==Family::Boss&&event.effect==Effect::OnSpawn;
    case Source::NpcRespawn:case Source::NpcGenerate:case Source::NpcAdd:return event.target.family==Family::Npc&&event.effect==Effect::OnSpawn;
    case Source::MonsterRespawn:case Source::MonsterGenerate:case Source::MonsterRegenerate:return event.target.family==Family::Monster&&zone_monster_key(event.target.key)&&event.effect==Effect::OnSpawn;
    case Source::NpcDeath:return event.target.family==Family::Npc&&event.effect==Effect::ClearLivePointers;
    case Source::MonsterDeath:return event.target.family==Family::Monster&&zone_monster_key(event.target.key)&&event.effect==Effect::ClearLivePointers;
    case Source::BossBaseDeath:return event.target.family==Family::Boss&&event.effect==Effect::CharacterDie;
    case Source::NpcBaseDeath:return event.target.family==Family::Npc&&event.effect==Effect::CharacterDie;
    case Source::MonsterBaseDeath:return event.target.family==Family::Monster&&zone_monster_key(event.target.key)&&event.effect==Effect::CharacterDie;
    default:return false;}
}
inline nlohmann::json encode(const Event& event){return {{"target",interventions::encode(event.target)},{"effect",uint8_t(event.effect)},{"source",uint8_t(event.source)},{"action",event.action},{"ordinal",event.ordinal}};}
inline Event decode_event(const nlohmann::json& value){
    interventions::codec::exact(value,{"target","effect","source","action","ordinal"});
    Event event{interventions::codec::target(value.at("target")),Effect(interventions::codec::number(value.at("effect"),3)),Source(interventions::codec::number(value.at("source"),16)),interventions::codec::text(value.at("action"),36),interventions::codec::number(value.at("ordinal"),UINT32_MAX)};
    if(!valid(event))throw std::runtime_error("Actor lifecycle original invocation family/source/action receipt invalid");return event;
}
inline Event event(const journal::Row& row){
    if(row.message.size()>8192)throw std::runtime_error("Actor lifecycle original invocation payload exceeds8192 bytes");
    unsigned depth=0;const auto guard=[&](int level,nlohmann::json::parse_event_t,nlohmann::json&){depth=std::max(depth,unsigned(level));return level<=3;};
    const auto json=nlohmann::json::parse(row.message.begin(),row.message.end(),guard);
    if(depth>3)throw std::runtime_error("Actor lifecycle original invocation payload nesting exceeds3");
    const auto decoded=decode_event(json);if(encode(decoded).dump()!=row.message)throw std::runtime_error("Actor lifecycle original invocation payload is not canonical");return decoded;
}
inline bool valid(const Batch& batch,std::string& error){
    if(!journal::valid(batch,error))return false;
    try{for(const auto& row:batch.rows)(void)event(row);error.clear();return true;}
    catch(const std::exception& exception){error=std::string("Actor lifecycle journal: ")+exception.what();return false;}
}
inline bool decode(const nlohmann::json& value,Batch& out,std::string& error){Batch candidate;if(!journal::decode(value,candidate,error)||!lifecycle::valid(candidate,error))return false;out=std::move(candidate);return true;}
inline nlohmann::json encode(const Batch& value){return journal::encode(value);}
inline bool append(Host& host,const journal::Origin& origin,const Event& value,std::string& error){
    if(!valid(value)){error="Actor lifecycle append lacks actual original invocation receipt";return false;}
    const auto payload=encode(value).dump();if(payload.size()>8192){error="Actor lifecycle original invocation payload exceeds8192 bytes";return false;}
    return host.append(origin,payload,error);
}
}
