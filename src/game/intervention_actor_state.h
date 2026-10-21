#pragma once
#include "intervention_state.h"
#include "dungeon_gizmo_identity.h"
#include "subscriber_motion.h"
#include <bit>
#include <cmath>
#include <variant>
#include <set>

namespace mt2mp::game::interventions::actors {
constexpr size_t animation_slots=49,max_rows=4096;
struct Clip {
    std::optional<uint32_t> length,time;
    bool paused=false;uint32_t speed=0;
    bool operator==(const Clip&)const=default;
};
struct Npc {
    int32_t marker_kind=0,online_state=0,state=0;
    uint32_t respawn=0;std::array<float,3> spawn{};
    uint32_t move_delay=0,interact_timer=0;int32_t tick_slot=0;
    bool operator==(const Npc&)const=default;
};
struct Boss {
    int32_t phase=0,state=0;
    uint32_t respawn=0;std::array<float,3> spawn{};
    bool operator==(const Boss&)const=default;
};
struct ZoneMonster {
    int32_t status=0,ai=0;bool ready=false;
    uint32_t respawn=0;std::array<float,3> spawn{};
    bool operator==(const ZoneMonster&)const=default;
};
struct StatusEffect {
    int32_t type=0,parameter_int=0;
    uint32_t parameter_float=0,timer=0,duration=0;
    std::optional<Target> user;std::string ability;
    bool operator==(const StatusEffect&)const=default;
};
struct Item {
    std::string prototype;int32_t state=0;uint32_t cooldown=0;
    bool operator==(const Item&)const=default;
};
struct Row {
    Target target;
    std::string dungeon_scene;
    int32_t slot=-1,type=-1,level=0;
    subscriber_motion::Pose pose;
    std::array<uint32_t,3> values{},recovery{},maxima{};
    bool dead=false,on_map=false;int32_t action=-1;
    std::array<Clip,animation_slots> clips{};
    std::variant<Npc,Boss,ZoneMonster> lifecycle;
    std::array<StatusEffect,4> status_effects{};std::vector<Item> abilities;
    bool operator==(const Row&)const=default;
};
using State=std::vector<Row>;
inline bool finite(uint32_t bits){return std::isfinite(std::bit_cast<float>(bits));}
inline bool spawn_valid(const std::array<float,3>& values){for(float v:values)if(!std::isfinite(v)||std::abs(v)>100000000)return false;return true;}
inline bool ability_key(std::string_view key){
    constexpr std::string_view prefix="character:";
    if(!key.starts_with(prefix))return false;key.remove_prefix(prefix.size());const auto kind=key.find(':');if(kind==std::string_view::npos||!snapshot_building_key::number(key.substr(0,kind),4,false))return false;
    key.remove_prefix(kind+1);const auto split=key.find("/ability/");if(split==std::string_view::npos||!snapshot_building_key::number(key.substr(0,split),INT32_MAX,false))return false;
    return snapshot_building_key::number(key.substr(split+9),9999,false);
}
inline bool direct_dungeon_monster(const Target& target,const std::string& scene){
    return target.family==Family::Monster&&monster_target_key::valid(target.key)&&dungeon_gizmo_identity::scene(scene);
}
inline bool valid(const State& state,std::string& error){
    if(state.size()>max_rows){error="Actor Frame complete row budget exceeded";return false;}
    std::string previous;
    for(const auto& row:state){
        if(!interventions::valid(row.target)||(!previous.empty()&&row.target.key<=previous)||row.slot<0||row.type<0||row.level<0||row.action<-1||row.action>=int32_t(animation_slots)){
            error="Actor Frame canonical sorted identity or Character fields invalid";return false;}
        previous=row.target.key;
        if(!row.dungeon_scene.empty()&&!direct_dungeon_monster(row.target,row.dungeon_scene)){
            error="Actor Frame explicit Dungeon scene requires a direct canonical Monster";return false;}

        if(!subscriber_motion::valid(row.pose,error))return false;
        for(const auto& values:{row.values,row.recovery,row.maxima})for(auto value:values)if(!finite(value)){error="Actor Frame attribute is not finite";return false;}
        for(const auto& clip:row.clips){
            if((clip.length&&(!finite(*clip.length)||std::bit_cast<float>(*clip.length)<=0))||
               (clip.time&&(!clip.length||!finite(*clip.time)))||!finite(clip.speed)||(!clip.time&&(clip.paused||clip.speed))){
                error="Actor Frame semantic clip phase or local asset length invalid";return false;}}
        for(const auto& effect:row.status_effects)if(effect.type<0||!finite(effect.parameter_float)||!finite(effect.timer)||!finite(effect.duration)||(effect.user&&!interventions::valid(*effect.user))||(!effect.ability.empty()&&!ability_key(effect.ability))){error="Actor Frame original saved StatusEffectData invalid";return false;}
        if(row.abilities.size()>10000){error="Actor Frame native AbilitySet item budget exceeded";return false;}std::set<std::string> prototypes;
        for(const auto& item:row.abilities)if(!ability_key(item.prototype)||item.state<0||!finite(item.cooldown)||!prototypes.insert(item.prototype).second){error="Actor Frame original AbilitySet item/prototype/state invalid";return false;}
        bool matching=false;
        if(const auto* npc=std::get_if<Npc>(&row.lifecycle))matching=row.target.family==Family::Npc&&finite(npc->respawn)&&finite(npc->move_delay)&&finite(npc->interact_timer)&&spawn_valid(npc->spawn);
        if(const auto* boss=std::get_if<Boss>(&row.lifecycle))matching=row.target.family==Family::Boss&&finite(boss->respawn)&&spawn_valid(boss->spawn);
        if(const auto* monster=std::get_if<ZoneMonster>(&row.lifecycle))matching=row.target.family==Family::Monster&&(zone_monster_key(row.target.key)||direct_dungeon_monster(row.target,row.dungeon_scene))&&finite(monster->respawn)&&spawn_valid(monster->spawn);
        if(!matching){error="Actor Frame family lifecycle differs or includes an already-owned Toon/root direct Monster";return false;}
    }
    error.clear();return true;
}
inline nlohmann::json encode(const State& state){
    nlohmann::json out=nlohmann::json::array();
    for(const auto& row:state){nlohmann::json clips=nlohmann::json::array(),life,effects=nlohmann::json::array(),abilities=nlohmann::json::array();
        for(const auto& clip:row.clips)clips.push_back(nlohmann::json::array({clip.length?nlohmann::json(*clip.length):nlohmann::json(nullptr),clip.time?nlohmann::json(*clip.time):nlohmann::json(nullptr),clip.paused,clip.speed}));
        if(const auto* npc=std::get_if<Npc>(&row.lifecycle))life=nlohmann::json::array({npc->marker_kind,npc->online_state,npc->state,npc->respawn,npc->spawn,npc->move_delay,npc->interact_timer,npc->tick_slot});
        if(const auto* boss=std::get_if<Boss>(&row.lifecycle))life=nlohmann::json::array({boss->phase,boss->state,boss->respawn,boss->spawn});
        if(const auto* monster=std::get_if<ZoneMonster>(&row.lifecycle))life=nlohmann::json::array({monster->status,monster->ai,monster->ready,monster->respawn,monster->spawn});
        for(const auto& effect:row.status_effects)effects.push_back(nlohmann::json::array({effect.type,effect.parameter_int,effect.parameter_float,effect.timer,effect.duration,effect.user?interventions::encode(*effect.user):nlohmann::json(nullptr),effect.ability}));
        for(const auto& item:row.abilities)abilities.push_back(nlohmann::json::array({item.prototype,item.state,item.cooldown}));
        out.push_back({{"target",interventions::encode(row.target)},{"character",{row.slot,row.type,row.level}},
            {"region",row.pose.region},{"position",row.pose.position},{"rotation",row.pose.orientation},{"scale",row.pose.scale},
            {"values",row.values},{"recovery",row.recovery},{"maxima",row.maxima},
            {"dead",row.dead},{"on_map",row.on_map},{"action",row.action},{"clips",clips},{"lifecycle",life},{"status_effects",effects},{"abilities",abilities}});
        if(!row.dungeon_scene.empty())out.back()["dungeon_scene"]=row.dungeon_scene;
    }return out;
}
namespace codec {
using Json=nlohmann::json;
inline int32_t integer(const Json& j){if(!j.is_number_integer()||(j.is_number_unsigned()?j.get<uint64_t>()>uint64_t(INT32_MAX):j.get<int64_t>()<INT32_MIN||j.get<int64_t>()>INT32_MAX))throw std::runtime_error("Actor Frame int32 required");return j.get<int32_t>();}
inline uint32_t bits(const Json& j){return interventions::codec::number(j,UINT32_MAX);}
inline bool boolean(const Json& j){if(!j.is_boolean())throw std::runtime_error("Actor Frame boolean required");return j.get<bool>();}
inline float real(const Json& j){if(!j.is_number())throw std::runtime_error("Actor Frame numeric transform required");const double v=j.get<double>();if(!std::isfinite(v)||std::abs(v)>100000000)throw std::runtime_error("Actor Frame transform outside finite domain");return float(v);}
template<class T,size_t N,class Read>void array(const Json& j,std::array<T,N>& out,Read&& read){if(!j.is_array()||j.size()!=N)throw std::runtime_error("Actor Frame exact array extent differs");for(size_t i=0;i<N;++i)out[i]=read(j[i]);}
inline State state(const Json& j){
    if(!j.is_array()||j.size()>max_rows)throw std::runtime_error("Actor Frame complete rows required");State out;
    for(const auto& v:j){
        if(v.contains("dungeon_scene"))interventions::codec::exact(v,{"target","dungeon_scene","character","region","position","rotation","scale","values","recovery","maxima","dead","on_map","action","clips","lifecycle","status_effects","abilities"});
        else interventions::codec::exact(v,{"target","character","region","position","rotation","scale","values","recovery","maxima","dead","on_map","action","clips","lifecycle","status_effects","abilities"});Row row;
        if(v.contains("dungeon_scene")){row.dungeon_scene=interventions::codec::text(v.at("dungeon_scene"),4096);if(row.dungeon_scene.empty())throw std::runtime_error("Actor Frame explicit Dungeon scene is empty");}
        row.target=interventions::codec::target(v.at("target"));std::array<int32_t,3> character{};array(v.at("character"),character,integer);row.slot=character[0];row.type=character[1];row.level=character[2];
        row.pose.region=integer(v.at("region"));array(v.at("position"),row.pose.position,real);array(v.at("rotation"),row.pose.orientation,real);array(v.at("scale"),row.pose.scale,real);
        array(v.at("values"),row.values,bits);array(v.at("recovery"),row.recovery,bits);array(v.at("maxima"),row.maxima,bits);row.dead=boolean(v.at("dead"));row.on_map=boolean(v.at("on_map"));row.action=integer(v.at("action"));
        const auto& clips=v.at("clips");if(!clips.is_array()||clips.size()!=animation_slots)throw std::runtime_error("Actor Frame semantic clip extent differs");
        for(size_t i=0;i<animation_slots;++i){const auto& source=clips[i];if(!source.is_array()||source.size()!=4)throw std::runtime_error("Actor Frame semantic clip four values required");auto& clip=row.clips[i];if(!source[0].is_null())clip.length=bits(source[0]);if(!source[1].is_null())clip.time=bits(source[1]);clip.paused=boolean(source[2]);clip.speed=bits(source[3]);}
        const auto& life=v.at("lifecycle");if(!life.is_array())throw std::runtime_error("Actor Frame family lifecycle array required");
        switch(row.target.family){
        case Family::Npc:{if(life.size()!=8)throw std::runtime_error("NPC Frame lifecycle extent differs");Npc value{integer(life[0]),integer(life[1]),integer(life[2]),bits(life[3]),{}};array(life[4],value.spawn,real);value.move_delay=bits(life[5]);value.interact_timer=bits(life[6]);value.tick_slot=integer(life[7]);row.lifecycle=value;break;}
        case Family::Boss:{if(life.size()!=4)throw std::runtime_error("Boss Frame lifecycle extent differs");Boss value{integer(life[0]),integer(life[1]),bits(life[2]),{}};array(life[3],value.spawn,real);row.lifecycle=value;break;}
        case Family::Monster:{if(life.size()!=5)throw std::runtime_error("Zone Monster Frame lifecycle extent differs");ZoneMonster value{integer(life[0]),integer(life[1]),boolean(life[2]),bits(life[3]),{}};array(life[4],value.spawn,real);row.lifecycle=value;break;}
        default:throw std::runtime_error("Actor Frame Toon is already owned by Economy");}
        const auto& effects=v.at("status_effects");if(!effects.is_array()||effects.size()!=4)throw std::runtime_error("Actor Frame original fixed four StatusEffectData required");
        for(size_t n=0;n<4;++n){const auto& source=effects[n];if(!source.is_array()||source.size()!=7)throw std::runtime_error("Actor Frame complete StatusEffectData fields required");auto& value=row.status_effects[n];value.type=integer(source[0]);value.parameter_int=integer(source[1]);value.parameter_float=bits(source[2]);value.timer=bits(source[3]);value.duration=bits(source[4]);if(!source[5].is_null())value.user=interventions::codec::target(source[5]);value.ability=interventions::codec::text(source[6],128);}
        const auto& abilities=v.at("abilities");if(!abilities.is_array()||abilities.size()>10000)throw std::runtime_error("Actor Frame bounded original AbilitySet items required");
        for(const auto& source:abilities){if(!source.is_array()||source.size()!=3)throw std::runtime_error("Actor Frame original Item fields required");row.abilities.push_back({interventions::codec::text(source[0],128),integer(source[1]),bits(source[2])});}
        out.push_back(std::move(row));}
    std::string error;if(!valid(out,error))throw std::runtime_error(error);return out;
}
}
inline bool preflight(const State& local,const State& target,std::string& error){
    if(!valid(local,error)||!valid(target,error))return false;
    if(local.size()!=target.size()){error="Actor Frame complete canonical census differs";return false;}
    for(size_t i=0;i<local.size();++i){const auto& a=local[i];const auto& b=target[i];
        if(a.target!=b.target||a.dungeon_scene!=b.dungeon_scene||a.slot!=b.slot||a.type!=b.type||a.level!=b.level||a.pose.region!=b.pose.region||(a.status_effects==b.status_effects&&a.maxima!=b.maxima)||a.lifecycle.index()!=b.lifecycle.index()){
            error="Actor Frame current identity, family, Region or native maxima differs key="+b.target.key;return false;}
        for(size_t c=0;c<animation_slots;++c)if(a.clips[c].length!=b.clips[c].length){error="Actor Frame local semantic asset differs key="+b.target.key;return false;}
        if(a.abilities.size()!=b.abilities.size()){error="Actor Frame AbilitySet membership requires its ordered original transaction";return false;}
        for(size_t slot=0;slot<a.abilities.size();++slot)if(a.abilities[slot].prototype!=b.abilities[slot].prototype){error="Actor Frame AbilitySet prototype/slot differs from its original transaction";return false;}
        if(const auto* npc=std::get_if<Npc>(&a.lifecycle)){const auto& other=std::get<Npc>(b.lifecycle);if(npc->marker_kind!=other.marker_kind){error="NPC Frame original marker kind differs key="+b.target.key;return false;}}
    }error.clear();return true;
}
}
