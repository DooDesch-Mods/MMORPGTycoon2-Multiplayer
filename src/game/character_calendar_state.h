#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace mt2mp::game::character_calendar {
struct History {int32_t limit=0;std::vector<int32_t> values;bool operator==(const History&)const=default;};
struct Rating {std::array<uint32_t,3> bits{};bool set=false;bool operator==(const Rating&)const=default;};
struct Statistics {
    std::array<int32_t,2> tiers{},last_edit_days{};bool tier_valid=false;
    Rating rating;uint32_t optimal_dps=0;std::array<History,2> durations;
    bool operator==(const Statistics&)const=default;
};
struct Demand {std::string name;std::vector<int32_t> members;bool operator==(const Demand&)const=default;};
struct Definition {
    int32_t kind=-1,id=-1;bool deleted=false;std::optional<Statistics> statistics;
    std::array<std::optional<Demand>,2> demands;
    bool operator==(const Definition&)const=default;
};
struct Zone {
    std::string key,scene;int32_t type=-1;std::array<int32_t,3> constructed{},counts{};
    std::array<std::vector<int32_t>,2> kills;Rating rating;
    bool operator==(const Zone&)const=default;
};
struct State {std::vector<Definition> definitions;std::vector<std::string> demand_order;std::vector<Zone> zones;bool operator==(const State&)const=default;};
struct Receipt {bool attempted=false,confirmed=false,unknown=false;uint32_t attempted_stores=0,stores=0;std::string cause;};
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
inline std::string definition_key(int32_t kind,int32_t id){return "character:"+std::to_string(kind)+":"+std::to_string(id);}
inline std::string demand_key(const Definition& row,size_t index){return definition_key(row.kind,row.id)+(index?"/demand/nerf":"/demand/buff");}
inline bool valid(const Rating& value,std::string& error){
    for(auto bits:value.bits)if(!std::isfinite(std::bit_cast<float>(bits)))return fail(error,"Character/Zone rating is not finite");return true;
}
inline bool valid(const State& value,std::string& error){
    if(value.definitions.size()>100000||value.zones.size()>100000||value.demand_order.size()>200000)
        return fail(error,"Character/Zone complete state exceeds bounded native census");
    std::array<int32_t,5> next{};int32_t previous_kind=-1;std::set<std::string> demands;std::set<int32_t> subscribers;
    for(const auto& row:value.definitions){
        if(row.kind<0||row.kind>=5||row.kind<previous_kind||row.id!=next[size_t(row.kind)]++||row.deleted==row.statistics.has_value())
            return fail(error,"Character calendar original kind/global ID/tombstone order differs");
        previous_kind=row.kind;
        if(row.statistics){const auto& stats=*row.statistics;
            if(!valid(stats.rating,error))return false;
            if(!std::isfinite(std::bit_cast<float>(stats.optimal_dps)))
                return fail(error,"Character original optimal DPS is not finite");
            for(const auto& history:stats.durations)if(history.limit<0||history.limit>1000000||history.values.size()>size_t(history.limit))
                return fail(error,"Character original IntegerHistory limit/count differs");
        }
        for(size_t index=0;index<2;++index){
            if(!row.demands[index]){if(!row.deleted)return fail(error,"Live Character lacks its registered embedded Demand");continue;}
            const auto& demand=*row.demands[index];
            if(demand.name.size()>65536||demand.name.find('\0')!=std::string::npos||demand.members.size()>1000000)
                return fail(error,"Character embedded Demand name/member bound differs");
            if(!demands.insert(demand_key(row,index)).second)return fail(error,"Character embedded Demand canonical owner repeats");
            for(auto id:demand.members)if(id<0||!subscribers.insert(id).second)
                return fail(error,"Subscriber belongs to repeated or multiple Character Demands");
        }
    }
    std::set<std::string> ordered;
    for(const auto& key:value.demand_order)if(!demands.contains(key)||!ordered.insert(key).second)
        return fail(error,"Character DemandManager order contains a foreign or repeated original owner");
    if(ordered!=demands)return fail(error,"Character DemandManager omits a registered embedded owner");
    std::string previous;
    for(const auto& row:value.zones){
        if(row.key.empty()||row.key<=previous||row.scene.empty()||row.type<0||row.type>3)
            return fail(error,"Zone calendar canonical identity/order/type differs");
        if(!valid(row.rating,error))return false;
        previous=row.key;
        for(auto count:row.counts)if(count<0)return fail(error,"Zone kill counter is negative");
        for(const auto& history:row.kills){if(history.size()>10)return fail(error,"Zone native rolling kill history exceeds ten days");
            for(auto count:history)if(count<0)return fail(error,"Zone rolling kill count is negative");}
    }
    error.clear();return true;
}
inline nlohmann::json encode(const Rating& value){return nlohmann::json::array({value.bits,value.set});}
inline nlohmann::json encode(const State& value){
    using Json=nlohmann::json;Json definitions=Json::array(),zones=Json::array();
    for(const auto& row:value.definitions){Json stats=nullptr,demands=Json::array();
        if(row.statistics){const auto& v=*row.statistics;Json histories=Json::array();
            for(const auto& history:v.durations)histories.push_back(Json::array({history.limit,history.values}));
            stats={{"tiers",v.tiers},{"last_edit_days",v.last_edit_days},{"tier_valid",v.tier_valid},{"rating",encode(v.rating)},
                {"optimal_dps",v.optimal_dps},{"durations",histories}};}
        for(const auto& demand:row.demands)demands.push_back(demand?Json{{"name",demand->name},{"members",demand->members}}:Json(nullptr));
        definitions.push_back({{"kind",row.kind},{"id",row.id},{"deleted",row.deleted},{"statistics",stats},{"demands",demands}});
    }
    for(const auto& row:value.zones)zones.push_back({{"key",row.key},{"scene",row.scene},{"type",row.type},
        {"constructed",row.constructed},{"counts",row.counts},{"kills",row.kills},{"rating",encode(row.rating)}});
    return {{"definitions",definitions},{"demand_order",value.demand_order},{"zones",zones}};
}
namespace codec {
using Json=nlohmann::json;
inline void exact(const Json& value,std::initializer_list<const char*> keys){
    if(!value.is_object()||value.size()!=keys.size())throw std::runtime_error("Character calendar exact object fields required");
    for(const auto* key:keys)if(!value.contains(key))throw std::runtime_error(std::string("Character calendar missing field ")+key);
}
inline int32_t integer(const Json& value){
    if(!value.is_number_integer()||(value.is_number_unsigned()?value.get<uint64_t>()>uint64_t(INT32_MAX):value.get<int64_t>()<INT32_MIN||value.get<int64_t>()>INT32_MAX))
        throw std::runtime_error("Character calendar int32 required");return value.get<int32_t>();
}
inline uint32_t bits(const Json& value){
    if(!value.is_number_integer()||(value.is_number_unsigned()?value.get<uint64_t>()>UINT32_MAX:value.get<int64_t>()<0||uint64_t(value.get<int64_t>())>UINT32_MAX))
        throw std::runtime_error("Character calendar uint32 bits required");return value.get<uint32_t>();
}
inline bool boolean(const Json& value){if(!value.is_boolean())throw std::runtime_error("Character calendar Boolean required");return value.get<bool>();}
inline std::string text(const Json& value){if(!value.is_string())throw std::runtime_error("Character calendar string required");return value.get<std::string>();}
template<class T,size_t N,class Read>void array(const Json& value,std::array<T,N>& out,Read read){
    if(!value.is_array()||value.size()!=N)throw std::runtime_error("Character calendar exact array extent required");
    for(size_t i=0;i<N;++i)out[i]=read(value[i]);
}
inline std::vector<int32_t> integers(const Json& value,size_t maximum){
    if(!value.is_array()||value.size()>maximum)throw std::runtime_error("Character calendar integer array bound differs");
    std::vector<int32_t> out;for(const auto& row:value)out.push_back(integer(row));return out;
}
inline Rating rating(const Json& value){
    if(!value.is_array()||value.size()!=2)throw std::runtime_error("Character calendar rating pair required");
    Rating out;array(value[0],out.bits,bits);out.set=boolean(value[1]);return out;
}
}
inline bool decode(const nlohmann::json& wire,State& out,std::string& error)try{
    using namespace codec;out={};exact(wire,{"definitions","demand_order","zones"});State next;
    if(!wire.at("definitions").is_array()||wire.at("definitions").size()>100000||!wire.at("zones").is_array()||wire.at("zones").size()>100000||
       !wire.at("demand_order").is_array()||wire.at("demand_order").size()>200000)throw std::runtime_error("Character calendar complete census array required");
    for(const auto& row:wire.at("definitions")){exact(row,{"kind","id","deleted","statistics","demands"});Definition value;
        value.kind=integer(row.at("kind"));value.id=integer(row.at("id"));value.deleted=boolean(row.at("deleted"));
        if(!row.at("statistics").is_null()){const auto& stats=row.at("statistics");exact(stats,{"tiers","last_edit_days","tier_valid","rating","optimal_dps","durations"});Statistics v;
            array(stats.at("tiers"),v.tiers,integer);array(stats.at("last_edit_days"),v.last_edit_days,integer);v.tier_valid=boolean(stats.at("tier_valid"));
            v.rating=rating(stats.at("rating"));v.optimal_dps=bits(stats.at("optimal_dps"));
            const auto& histories=stats.at("durations");if(!histories.is_array()||histories.size()!=2)throw std::runtime_error("Character two duration histories required");
            for(size_t i=0;i<2;++i){if(!histories[i].is_array()||histories[i].size()!=2)throw std::runtime_error("Character duration history pair required");
                v.durations[i].limit=integer(histories[i][0]);v.durations[i].values=integers(histories[i][1],1000000);}
            value.statistics=std::move(v);}
        const auto& demands=row.at("demands");if(!demands.is_array()||demands.size()!=2)throw std::runtime_error("Character two embedded Demand slots required");
        for(size_t i=0;i<2;++i)if(!demands[i].is_null()){exact(demands[i],{"name","members"});value.demands[i]=Demand{text(demands[i].at("name")),integers(demands[i].at("members"),1000000)};}
        next.definitions.push_back(std::move(value));
    }
    for(const auto& key:wire.at("demand_order"))next.demand_order.push_back(text(key));
    for(const auto& row:wire.at("zones")){exact(row,{"key","scene","type","constructed","counts","kills","rating"});Zone value;
        value.key=text(row.at("key"));value.scene=text(row.at("scene"));value.type=integer(row.at("type"));array(row.at("constructed"),value.constructed,integer);array(row.at("counts"),value.counts,integer);
        const auto& histories=row.at("kills");if(!histories.is_array()||histories.size()!=2)throw std::runtime_error("Zone two rolling histories required");
        for(size_t i=0;i<2;++i)value.kills[i]=integers(histories[i],10);value.rating=rating(row.at("rating"));next.zones.push_back(std::move(value));}
    if(!valid(next,error))return false;out=std::move(next);error.clear();return true;
}catch(const std::exception& exception){return fail(error,std::string("Character calendar decode: ")+exception.what());}
}
