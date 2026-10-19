#pragma once
#include "party_record.h"
#include "scene_clone/state.h"
#include <optional>
#include "dungeon_instance_live.h"
#include "dungeon_instance_lifecycle.h"
#include "live_host_state/frame_limits.h"
#include <algorithm>
#include <array>
#include <set>

namespace mt2mp::game::party_frame {
using Json=nlohmann::json;
struct Pool {
    std::string exterior;
    int32_t counter=-1;
    std::vector<int32_t> indices;
    bool operator==(const Pool&)const=default;
};
struct State {
    // Original record tables stay compact strings, as Subscriber birth graphs do.
    // Their complete original token/link checksum is checked before admission.
    std::vector<std::string> parties;
    std::vector<Pool> pools;
    std::vector<dungeon_instance_live::State> instances;
    marketing_authority::notice::Batch lifecycle;
    std::optional<scene_clone::Mapping> scenes;
    bool operator==(const State&)const=default;
};
inline bool fail(std::string& error,const std::string& cause){error="Party Frame "+cause;return false;}
inline void exact(const Json& value,std::initializer_list<const char*> fields){
    if(!value.is_object()||value.size()!=fields.size())throw std::runtime_error("Party Frame exact object fields required");
    for(const char* field:fields)if(!value.contains(field))throw std::runtime_error(std::string("Party Frame field missing: ")+field);
}
inline uint64_t unsigned_value(const Json& value,uint64_t maximum){
    if(!value.is_number_integer()||(!value.is_number_unsigned()&&value.get<int64_t>()<0))throw std::runtime_error("Party Frame unsigned integer required");
    const auto number=value.get<uint64_t>();if(number>maximum)throw std::runtime_error("Party Frame integer outside domain");return number;
}
inline Json token(const subscriber_lifecycle::Token& value){return Json::array({value.type,value.type==1||value.type==2?Json(value.text):Json(value.bits)});}
inline subscriber_lifecycle::Token token(const Json& value){
    if(!value.is_array()||value.size()!=2)throw std::runtime_error("Party Frame original token arity differs");
    subscriber_lifecycle::Token out;out.type=int32_t(unsigned_value(value[0],4));
    if(out.type==1||out.type==2)out.text=value[1].get<std::string>();else out.bits=uint32_t(unsigned_value(value[1],UINT32_MAX));return out;
}
inline Json encode_record(const party_live::Record& record){
    Json rows=Json::array(),external=Json::array(),managers=Json::array();
    for(const auto& row:record.records){Json values=Json::array();for(const auto& value:row.tokens)values.push_back(token(value));
        rows.push_back(Json::array({uint64_t(row.parent),uint64_t(row.child_index),token(row.label),std::move(values)}));}
    for(const auto& row:record.external)external.push_back(Json::array({row.id,row.key}));
    for(const auto& row:record.managers)managers.push_back(Json::array({row.region,row.removal,row.release}));
    return {{"key",record.key},{"sha",record.sha},{"shard",record.shard},{"region",record.region},{"manager_region",record.manager_region},
        {"managers",std::move(managers)},{"records",std::move(rows)},{"local_ids",record.local_ids},
        {"external",std::move(external)},{"toons",record.toons},{"root_link",record.root_link}};
}
inline party_live::Record decode_record(const Json& value){
    exact(value,{"key","sha","shard","region","manager_region","managers","records","local_ids","external","toons","root_link"});
    party_live::Record out;out.key=value.at("key").get<std::string>();out.sha=value.at("sha").get<std::string>();
    out.shard=value.at("shard").get<std::string>();out.region=value.at("region").get<std::string>();out.manager_region=value.at("manager_region").get<std::string>();
    for(const char* field:{"records","local_ids","external","toons","managers"})if(!value.at(field).is_array())throw std::runtime_error("Party Frame original manifest requires arrays");
    if(value.at("records").size()>100000)throw std::runtime_error("Party Frame original record budget exceeded");
    for(const auto& row:value.at("records")){
        if(!row.is_array()||row.size()!=4||!row[3].is_array())throw std::runtime_error("Party Frame original record row differs");
        subscriber_lifecycle::Record record;record.parent=size_t(unsigned_value(row[0],SIZE_MAX));record.child_index=size_t(unsigned_value(row[1],SIZE_MAX));record.label=token(row[2]);
        for(const auto& scalar:row[3])record.tokens.push_back(token(scalar));out.records.push_back(std::move(record));
    }
    for(const auto& id:value.at("local_ids"))out.local_ids.push_back(int32_t(unsigned_value(id,INT32_MAX)));
    for(const auto& row:value.at("external")){
        if(!row.is_array()||row.size()!=2)throw std::runtime_error("Party Frame external manifest row differs");
        out.external.push_back({int32_t(unsigned_value(row[0],INT32_MAX)),row[1].get<std::string>()});
    }
    for(const auto& row:value.at("managers")){
        if(!row.is_array()||row.size()!=3||!row[1].is_boolean()||!row[2].is_boolean())throw std::runtime_error("Party Frame manager row differs");
        out.managers.push_back({row[0].get<std::string>(),row[1].get<bool>(),row[2].get<bool>()});
    }
    for(const auto& toon:value.at("toons"))out.toons.push_back(toon.get<std::string>());
    out.root_link=int32_t(unsigned_value(value.at("root_link"),INT32_MAX));
    std::string error;if(!party_live::validate(out,error))throw std::runtime_error(error);return out;
}
inline party_live::Record record(const std::string& wire){
    if(wire.empty()||wire.size()>live_host_state::max_logical_frame_payload)throw std::runtime_error("Party Frame original record length invalid");
    std::vector<std::set<std::string>> fields;
    const auto value=Json::parse(wire,[&](int depth,Json::parse_event_t event,Json& item){
        if(depth>5)throw std::runtime_error("Party Frame original record nesting exceeds5");
        if(event==Json::parse_event_t::object_start)fields.emplace_back();
        if(event==Json::parse_event_t::key&&!fields.back().insert(item.get<std::string>()).second)throw std::runtime_error("Party Frame repeated original record field");
        if(event==Json::parse_event_t::object_end)fields.pop_back();return true;});
    auto out=decode_record(value);if(encode_record(out).dump()!=wire)throw std::runtime_error("Party Frame original record encoding is not canonical");return out;
}
inline bool valid(const State& state,std::string& error)try{
    if(!dungeon_instance_lifecycle::valid(state.lifecycle,error))return false;
    if(state.scenes&&!scene_clone::valid(*state.scenes,error))return false;
    std::set<std::string> party_keys;std::string prior;size_t bytes=0;
    for(const auto& wire:state.parties){
        if(wire.size()>live_host_state::max_logical_frame_payload-bytes)return fail(error,"original record resource bound exceeded");bytes+=wire.size();
        const auto row=record(wire);if((!prior.empty()&&row.key<=prior)||!party_keys.insert(row.key).second)return fail(error,"complete Party keys repeat or are unordered");prior=row.key;
    }
    std::map<std::string,std::set<int32_t>> pools;prior.clear();
    for(const auto& pool:state.pools){
        if(!dungeon_scene_key::building_key(pool.exterior)||pool.counter<0||(!prior.empty()&&pool.exterior<=prior)||
           !dungeon_instance_lifecycle::indices(pool.indices))return fail(error,"complete Dungeon pool key/index/counter invalid");
        pools.emplace(pool.exterior,std::set<int32_t>(pool.indices.begin(),pool.indices.end()));prior=pool.exterior;
    }
    prior.clear();
    for(const auto& instance:state.instances){
        if(!dungeon_instance_live::valid(instance,error)||(!prior.empty()&&instance.scene<=prior))return fail(error,error.empty()?"Instance values repeat or are unordered":error);
        const auto owner=pools.find(instance.exterior);
        if(owner==pools.end()||!owner->second.erase(instance.index))return fail(error,"saved Instance values are absent from complete pool membership");
        if(!instance.party.empty()&&!party_keys.contains(instance.party))return fail(error,"assigned Party is absent from complete Party state");prior=instance.scene;
    }
    for(const auto& pool:pools)if(!pool.second.empty())return fail(error,"complete Dungeon pool lacks saved Instance values");
    error.clear();return true;
}catch(const std::exception& e){return fail(error,e.what());}
inline Json encode(const State& state){
    Json pools=Json::array(),instances=Json::array();
    for(const auto& pool:state.pools)pools.push_back(Json::array({pool.exterior,pool.counter,pool.indices}));
    for(const auto& row:state.instances)instances.push_back({{"scene",row.scene},{"exterior",row.exterior},{"index",row.index},
        {"online_fraction",row.online_fraction},{"timeout",row.timeout},{"completion",row.completion},{"wants_online",row.wants_online},
        {"timing_out",row.timing_out},{"used",row.used},{"completed",row.completed},{"party",row.party}});
    return {{"parties",state.parties},{"pools",std::move(pools)},{"instances",std::move(instances)},
        {"lifecycle",marketing_authority::notice::encode(state.lifecycle)},{"scenes",state.scenes?scene_clone::encode(*state.scenes):Json(nullptr)}};
}
inline bool decode(const Json& value,State& out,std::string& error)try{
    out={};exact(value,{"parties","pools","instances","lifecycle","scenes"});State state;
    if(!value.at("scenes").is_null()){scene_clone::Mapping mapping;if(!scene_clone::decode(value.at("scenes"),mapping,error))return false;state.scenes=std::move(mapping);}
    for(const char* field:{"parties","pools","instances"})if(!value.at(field).is_array())return fail(error,"complete state requires arrays");
    for(const auto& wire:value.at("parties"))state.parties.push_back(wire.get<std::string>());
    for(const auto& row:value.at("pools")){
        if(!row.is_array()||row.size()!=3||!row[2].is_array())return fail(error,"complete Dungeon pool row differs");
        Pool pool;pool.exterior=row[0].get<std::string>();pool.counter=int32_t(unsigned_value(row[1],INT32_MAX));
        for(const auto& index:row[2])pool.indices.push_back(int32_t(unsigned_value(index,INT32_MAX)));state.pools.push_back(std::move(pool));
    }
    for(const auto& row:value.at("instances")){
        exact(row,{"scene","exterior","index","online_fraction","timeout","completion","wants_online","timing_out","used","completed","party"});
        dungeon_instance_live::State instance;instance.scene=row.at("scene").get<std::string>();instance.exterior=row.at("exterior").get<std::string>();
        instance.index=int32_t(unsigned_value(row.at("index"),INT32_MAX));instance.party=row.at("party").get<std::string>();
        for(const char* field:{"online_fraction","timeout","completion"})if(!row.at(field).is_number())return fail(error,"saved Instance scalar requires number");
        instance.online_fraction=row.at("online_fraction").get<float>();instance.timeout=row.at("timeout").get<float>();instance.completion=row.at("completion").get<float>();
        for(const char* field:{"wants_online","timing_out","used","completed"})if(!row.at(field).is_boolean())return fail(error,"saved Instance flag requires Boolean");
        instance.wants_online=row.at("wants_online").get<bool>();instance.timing_out=row.at("timing_out").get<bool>();
        instance.used=row.at("used").get<bool>();instance.completed=row.at("completed").get<bool>();state.instances.push_back(std::move(instance));
    }
    if(!marketing_authority::notice::decode(value.at("lifecycle"),state.lifecycle,error)||!valid(state,error))return false;
    out=std::move(state);error.clear();return true;
}catch(const std::exception& e){out={};return fail(error,e.what());}
}
