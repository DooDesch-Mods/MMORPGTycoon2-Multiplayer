#pragma once
#include "gm_ticket_reader.h"
#include "vendor/nlohmann/json.hpp"
#include <stdexcept>

namespace mt2mp::game::gm_ticket {
struct TicketValue {
    int32_t type=0,master=-1;std::array<float,3> position{};bool valid=false;
    std::string toon,region;bool operator==(const TicketValue&)const=default;
};
struct MasterValue {
    int32_t slot=-1,state=0,ticket=-1;std::array<float,3> position{},timers{},scale{},arrow_target{};std::array<float,4> orientation{};
    bool employed=false,active=false,on_map=false,evidence=false,arrow_has_target=false;std::string region;
    bool operator==(const MasterValue&)const=default;
};
struct State {
    std::vector<TicketValue> tickets;std::vector<MasterValue> masters;
    bool operator==(const State&)const=default;
};
inline bool valid(const State& value,std::string& error){
    if(value.tickets.size()>4096||value.masters.size()>50)return fail(error,"GM authoritative Ticket/master extent invalid");
    const auto key=[](const std::string& text){return text.size()<=512&&text.find('\0')==std::string::npos;};
    for(size_t i=0;i<value.masters.size();++i){const auto& row=value.masters[i];
        if(row.slot!=int32_t(i)||row.state<0||row.state>4||row.ticket< -1||row.ticket>=int32_t(value.tickets.size())||!key(row.region))return fail(error,"GM authoritative fixed slot/state/Ticket index/Region domain differs");
        for(const auto values:{row.position,row.timers,row.scale,row.arrow_target})for(float number:values)if(!std::isfinite(number))return fail(error,"GM authoritative position/timer/scale/arrow target is not finite");
        for(float number:row.orientation)if(!std::isfinite(number))return fail(error,"GM authoritative orientation is not finite");
        for(float number:row.scale)if(number<=0)return fail(error,"GM authoritative scale is not positive");
        const auto& q=row.orientation;const float norm=(q[1]*q[1]+q[3]*q[3])+(q[0]*q[0]+q[2]*q[2]);
        if(norm<0.99f||norm>1.01f)return fail(error,"GM authoritative orientation is not normalized");
        if(row.ticket>=0&&value.tickets[size_t(row.ticket)].master!=row.slot)return fail(error,"GM authoritative Ticket assignment is not reciprocal");
    }
    for(size_t i=0;i<value.tickets.size();++i){const auto& row=value.tickets[i];
        if(row.type<0||row.type>3||row.master< -1||row.master>=int32_t(value.masters.size())||!key(row.toon)||!key(row.region))return fail(error,"GM authoritative Ticket type/master/target key domain differs");
        for(float number:row.position)if(!std::isfinite(number))return fail(error,"GM authoritative Ticket position is not finite");
        if(row.master>=0&&value.masters[size_t(row.master)].ticket!=int32_t(i))return fail(error,"GM authoritative master assignment is not reciprocal");
    }
    error.clear();return true;
}

// Region and Toon keys come from the current saved-world identity owners. Frame
// ticket indices join the complete queue and fixed master rows atomically.
template<class ToonKey,class RegionKey>bool portable(const Census& source,ToonKey&& toon_key,RegionKey&& region_key,State& out,std::string& error){
    out={};State value;std::map<uintptr_t,int32_t> masters,tickets;
    for(const auto& row:source.masters)if(!masters.emplace(row.object,row.slot).second)return fail(error,"GM portable projection duplicate fixed master object");
    for(size_t i=0;i<source.values.size();++i)if(!tickets.emplace(source.values[i].object,int32_t(i)).second)return fail(error,"GM portable projection duplicate queue object");
    for(const auto& row:source.masters){MasterValue target;target.slot=row.slot;target.state=row.state;target.position=row.position;target.orientation=row.orientation;target.scale=row.scale;target.timers=row.timers;target.employed=row.employed!=0;target.active=row.active!=0;target.on_map=row.on_map!=0;target.evidence=row.evidence!=0;target.arrow_target=row.arrow_target;target.arrow_has_target=row.arrow_has_target!=0;
        if(row.ticket){const auto found=tickets.find(row.ticket);if(found==tickets.end())return fail(error,"GM portable master Ticket absent from this exact queue census");target.ticket=found->second;}
        if(row.region&&!region_key(row.region,target.region,error))return false;value.masters.push_back(std::move(target));}
    for(const auto& row:source.values){TicketValue target;target.type=row.type;target.position=row.position;target.valid=row.valid!=0;
        if(row.master){const auto found=masters.find(row.master);if(found==masters.end())return fail(error,"GM portable Ticket master absent from this exact fixed pool");target.master=found->second;}
        if(row.toon_alive&&!toon_key(row.toon,target.toon,error))return false;
        if(row.region&&!region_key(row.region,target.region,error))return false;value.tickets.push_back(std::move(target));}
    if(!valid(value,error))return false;out=std::move(value);return true;
}
inline nlohmann::json encode(const State& value){auto masters=nlohmann::json::array(),tickets=nlohmann::json::array();
    for(const auto& row:value.masters)masters.push_back({{"slot",row.slot},{"state",row.state},{"ticket",row.ticket},{"position",row.position},{"orientation",row.orientation},{"scale",row.scale},{"timers",row.timers},{"employed",row.employed},{"active",row.active},{"on_map",row.on_map},{"evidence",row.evidence},{"arrow_target",row.arrow_target},{"arrow_has_target",row.arrow_has_target},{"region",row.region}});
    for(const auto& row:value.tickets)tickets.push_back({{"type",row.type},{"master",row.master},{"position",row.position},{"valid",row.valid},{"toon",row.toon},{"region",row.region}});
    return {{"masters",masters},{"tickets",tickets}};
}
inline bool decode(const nlohmann::json& source,State& out,std::string& error){
    out={};
    try{
        const auto shape=[](const nlohmann::json& object,std::initializer_list<const char*> fields){
            if(!object.is_object()||object.size()!=fields.size())throw std::runtime_error("GM exact object fields required");
            for(const char* field:fields)if(!object.contains(field))throw std::runtime_error("GM required field missing");};
        const auto integer=[](const nlohmann::json& value){if(!value.is_number_integer()||(value.is_number_unsigned()&&value.get<uint64_t>()>uint64_t(INT32_MAX)))throw std::runtime_error("GM exact int32 required");
            const auto result=value.get<int64_t>();if(result<INT32_MIN||result>INT32_MAX)throw std::runtime_error("GM integer outside int32");return int32_t(result);};
        const auto boolean=[](const nlohmann::json& value){if(!value.is_boolean())throw std::runtime_error("GM exact Boolean required");return value.get<bool>();};
        const auto text=[](const nlohmann::json& value){if(!value.is_string())throw std::runtime_error("GM canonical key must be a string");auto result=value.get<std::string>();if(result.size()>512||result.find('\0')!=std::string::npos)throw std::runtime_error("GM canonical key domain invalid");return result;};
        const auto floats=[](const nlohmann::json& value){if(!value.is_array()||value.size()!=3)throw std::runtime_error("GM exact float triple required");std::array<float,3> result{};
            for(size_t i=0;i<3;++i){if(!value[i].is_number())throw std::runtime_error("GM finite numeric value required");result[i]=value[i].get<float>();if(!std::isfinite(result[i]))throw std::runtime_error("GM numeric value outside finite float32");}return result;};
        shape(source,{"masters","tickets"});const auto& masters=source.at("masters");const auto& tickets=source.at("tickets");
        if(!masters.is_array()||masters.size()>50||!tickets.is_array()||tickets.size()>4096)throw std::runtime_error("GM complete array extent invalid");State result;
        for(const auto& row:masters){shape(row,{"slot","state","ticket","position","orientation","scale","timers","employed","active","on_map","evidence","arrow_target","arrow_has_target","region"});MasterValue item;
            item.slot=integer(row.at("slot"));item.state=integer(row.at("state"));item.ticket=integer(row.at("ticket"));item.position=floats(row.at("position"));item.scale=floats(row.at("scale"));
            const auto& orientation=row.at("orientation");if(!orientation.is_array()||orientation.size()!=4)throw std::runtime_error("GM exact quaternion required");
            for(size_t i=0;i<4;++i){if(!orientation[i].is_number())throw std::runtime_error("GM numeric quaternion required");item.orientation[i]=orientation[i].get<float>();}
            item.timers=floats(row.at("timers"));item.employed=boolean(row.at("employed"));item.active=boolean(row.at("active"));item.on_map=boolean(row.at("on_map"));item.evidence=boolean(row.at("evidence"));item.arrow_target=floats(row.at("arrow_target"));item.arrow_has_target=boolean(row.at("arrow_has_target"));item.region=text(row.at("region"));result.masters.push_back(std::move(item));}
        for(const auto& row:tickets){shape(row,{"type","master","position","valid","toon","region"});TicketValue item;
            item.type=integer(row.at("type"));item.master=integer(row.at("master"));item.position=floats(row.at("position"));item.valid=boolean(row.at("valid"));item.toon=text(row.at("toon"));item.region=text(row.at("region"));result.tickets.push_back(std::move(item));}
        if(!valid(result,error))return false;out=std::move(result);return true;
    }catch(const std::exception& exception){error=std::string("GM state decode: ")+exception.what();return false;}
}
}
