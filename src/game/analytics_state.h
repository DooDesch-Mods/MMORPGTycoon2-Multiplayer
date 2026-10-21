#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::analytics_state {
struct Reason {int32_t type=0;std::vector<std::array<uint32_t,3>> positions;bool operator==(const Reason&)const=default;};
struct Day {int32_t day=0;std::vector<Reason> reasons;bool operator==(const Day&)const=default;};
struct State {
    // Complete native integer block at Analytics +0x40..+0x58. The two view
    // flags at +0x5c/+0x5d and EventListener storage stay local.
    std::array<int32_t,7> counters{};Day today;std::vector<Day> history;
    bool operator==(const State&)const=default;
};
inline bool valid(const State& s,std::string& e){
    if(s.today.day<0||s.history.size()>10){e="Analytics current Day/history extent invalid";return false;}
    size_t total=0;
    const auto day=[&](const Day& value){
        if(value.day<0||value.reasons.size()>10){e="Analytics Reason Day/type extent invalid";return false;}
        std::set<int32_t> types;
        for(const auto& row:value.reasons){
            if(row.type<0||row.type>=10||!types.insert(row.type).second){e="Analytics Reason native type duplicate/outside0..9";return false;}
            if(row.positions.size()>65536||total>65536-row.positions.size()){e="Analytics complete position payload exceeds65536 resource bound";return false;}
            total+=row.positions.size();for(const auto& position:row.positions)for(auto bits:position)
                if(!std::isfinite(std::bit_cast<float>(bits))){e="Analytics Reason position is not finite";return false;}
        }return true;
    };
    if(!day(s.today))return false;int32_t previous=-1;
    for(const auto& row:s.history){if(row.day<=previous||row.day>=s.today.day){e="Analytics retained history Day order differs";return false;}if(!day(row))return false;previous=row.day;}
    e.clear();return true;
}
inline nlohmann::json encode(const Day& d){auto rows=nlohmann::json::array();for(const auto& row:d.reasons)rows.push_back({{"type",row.type},{"positions",row.positions}});return {{"day",d.day},{"reasons",rows}};}
inline nlohmann::json encode(const State& s){auto rows=nlohmann::json::array();for(const auto& d:s.history)rows.push_back(encode(d));return {{"counters",s.counters},{"today",encode(s.today)},{"history",rows}};}
inline bool decode(const nlohmann::json& j,State& out,std::string& e){
    out={};try{
        const auto shape=[](const auto& v,size_t n){if(!v.is_object()||v.size()!=n)throw std::runtime_error("Analytics exact object fields required");};
        const auto integer=[](const auto& v){if(!v.is_number_integer()||(v.is_number_unsigned()?v.template get<uint64_t>()>uint64_t(INT32_MAX):v.template get<int64_t>()<INT32_MIN||v.template get<int64_t>()>INT32_MAX))throw std::runtime_error("Analytics int32 outside native domain");return v.template get<int32_t>();};
        const auto day=[&](const auto& value){shape(value,2);Day result;result.day=integer(value.at("day"));const auto& rows=value.at("reasons");if(!rows.is_array()||rows.size()>10)throw std::runtime_error("Analytics Reason array extent invalid");
            for(const auto& row:rows){shape(row,2);Reason item;item.type=integer(row.at("type"));const auto& positions=row.at("positions");if(!positions.is_array()||positions.size()>65536)throw std::runtime_error("Analytics position array extent invalid");
                for(const auto& position:positions){if(!position.is_array()||position.size()!=3)throw std::runtime_error("Analytics exact vec3 bits required");std::array<uint32_t,3> bits{};for(size_t i=0;i<3;++i){const auto& word=position[i];if(!word.is_number_integer()||(word.is_number_unsigned()?word.template get<uint64_t>()>UINT32_MAX:word.template get<int64_t>()<0||uint64_t(word.template get<int64_t>())>UINT32_MAX))throw std::runtime_error("Analytics position bits outside uint32");bits[i]=word.template get<uint32_t>();}item.positions.push_back(bits);}result.reasons.push_back(std::move(item));}return result;};
        shape(j,3);State value;const auto& counters=j.at("counters");if(!counters.is_array()||counters.size()!=7)throw std::runtime_error("Analytics complete seven native counters required");for(size_t i=0;i<7;++i)value.counters[i]=integer(counters[i]);
        value.today=day(j.at("today"));const auto& history=j.at("history");if(!history.is_array()||history.size()>10)throw std::runtime_error("Analytics complete retained history required");for(const auto& row:history)value.history.push_back(day(row));
        if(!valid(value,e))return false;out=std::move(value);e.clear();return true;
    }catch(const std::exception& cause){e=std::string("Analytics decode: ")+cause.what();return false;}
}
struct Owners {
    uintptr_t analytics=0,statistics=0,history_data=0;int32_t history_capacity=0;
    std::vector<uintptr_t> days,reasons,position_data,reason_data;std::vector<int32_t> capacities;
    bool operator==(const Owners&)const=default;
};
template<class Read,class Copy>
bool read(Read& at,Copy& copy,uintptr_t image,uintptr_t state,Owners& owners,State& out,std::string& e){
    owners={};out={};uintptr_t reciprocal=0,ap=0;
    const auto type=[&](uintptr_t object,uintptr_t rva){if(!object){e="Analytics native class owner is null";return false;}return at(object,0,ap,e)&&(ap==image+rva||(e="Analytics native class differs",false));};
    if(!at(image,0x13df280,owners.analytics,e)||!at(state,0x208,reciprocal,e)||reciprocal!=owners.analytics||!type(owners.analytics,0x16dd3b0)||
       !at(owners.analytics,0x38,owners.statistics,e)||!type(owners.statistics,0x17386b0)||!at(owners.analytics,0x40,out.counters,e)){
        if(e.empty())e="Analytics State/current singleton ownership differs";return false;
    }
    const auto array=[&](uintptr_t object,size_t offset,int32_t maximum,uintptr_t& data,int32_t& count,int32_t& capacity){
        if(!at(object,offset,data,e)||!at(object,offset+8,count,e)||!at(object,offset+12,capacity,e))return false;
        if(count<0||count>maximum||capacity<count||capacity>1000000||(!data&&capacity)||data>UINTPTR_MAX-size_t(capacity)*12){e="Analytics complete native array extent invalid";return false;}return true;
    };
    const auto day=[&](uintptr_t object,Day& value){
        uintptr_t data=0;int32_t count=0,capacity=0;
        if(!type(object,0x172e460)||!at(object,0x18,value.day,e)||!type(object+0x20,0x16ec160)||!array(object,0x28,10,data,count,capacity))return false;
        owners.days.push_back(object);owners.reason_data.push_back(data);owners.capacities.push_back(capacity);
        for(int32_t i=0;i<count;++i){uintptr_t reason=0,positions=0;int32_t amount=0,extent=0;Reason row;
            if(!at(data,size_t(i)*8,reason,e)||!type(reason,0x1711d40)||!at(reason,0x18,row.type,e)||!type(reason+0x20,0x1746160)||!array(reason,0x28,65536,positions,amount,extent))return false;
            owners.reasons.push_back(reason);owners.position_data.push_back(positions);owners.capacities.push_back(extent);row.positions.resize(size_t(amount));
            if(amount&&!copy(positions,row.positions.data(),row.positions.size()*12,e))return false;value.reasons.push_back(std::move(row));
        }return true;
    };
    if(!day(owners.statistics+0x38,out.today)||!type(owners.statistics+0x88,0x16ee200))return false;
    int32_t count=0;if(!array(owners.statistics,0x90,10,owners.history_data,count,owners.history_capacity))return false;
    std::set<uintptr_t> unique;
    for(int32_t i=0;i<count;++i){uintptr_t object=0;Day row;if(!at(owners.history_data,size_t(i)*8,object,e))return false;if(!unique.insert(object).second){e="Analytics duplicate native history Day allocation";return false;}if(!day(object,row))return false;out.history.push_back(std::move(row));}
    std::set<uintptr_t> reasons(owners.reasons.begin(),owners.reasons.end());
    if(reasons.size()!=owners.reasons.size()){e="Analytics duplicate native Reason allocation";return false;}
    return valid(out,e);
}
}
