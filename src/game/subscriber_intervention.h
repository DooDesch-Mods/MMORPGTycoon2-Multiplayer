#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <set>
#include <string>
#include <vector>
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::subscriber_intervention {
// Existing Economy rewards already own Subscriber+54/+5c/+60. These are the
// additional original Warn/Ban fields; satisfaction and Needs stay there too.
struct Value {uint32_t warnings=0,watch_timer_bits=0;bool leaving=false,banned=false;bool operator==(const Value&)const=default;};
struct Row {int32_t id=-1;Value value;bool operator==(const Row&)const=default;};
struct Owners {uintptr_t manager=0,data=0;int32_t count=0,capacity=0;std::vector<std::pair<uintptr_t,Row>> rows;bool operator==(const Owners&)const=default;};
using State=std::vector<Row>;
inline bool valid(const Value& value,std::string& error){if(!std::isfinite(std::bit_cast<float>(value.watch_timer_bits))){error="Subscriber intervention native watch timer is not finite";return false;}error.clear();return true;}
inline bool valid(const State& state,std::string& error){if(state.size()>1000000){error="Subscriber intervention actual manager budget exceeded";return false;}int32_t previous=-1;for(const auto& row:state){if(row.id<0||row.id<=previous){error="Subscriber intervention full serialized IDs must be sorted and unique";return false;}if(!valid(row.value,error))return false;previous=row.id;}error.clear();return true;}
template<class Read>bool read(Read& at,uintptr_t image,uintptr_t object,int32_t id,Value& out,std::string& error){
    out={};uintptr_t ap=0;int32_t actual_id=-1;uint8_t leaving=0,banned=0;Value next;
    if(!at(object,0,ap,error)||ap!=image+0x16e75f0||!at(object,0x20,actual_id,error)||actual_id!=id||id<0){if(error.empty())error="Subscriber intervention current native class/serialized ID differs";return false;}
    if(!at(object,0x50,next.warnings,error)||!at(object,0x70,next.watch_timer_bits,error)||!at(object,0xc8,leaving,error)||!at(object,0x125,banned,error))return false;
    if(leaving>1||banned>1){error="Subscriber intervention native leaving/banned member is not Boolean";return false;}next.leaving=leaving!=0;next.banned=banned!=0;if(!valid(next,error))return false;out=next;return true;
}
template<class Read,class Copy>bool read_all(Read& at,Copy& copy,uintptr_t image,State& out,Owners& owners,std::string& error){
    out.clear();owners={};Owners next;uintptr_t ap=0;
    if(!at(image,0x13df690,next.manager,error)||!at(next.manager,0,ap,error)||ap!=image+0x1725f40||!at(next.manager,0x40,next.data,error)||!at(next.manager,0x48,next.count,error)||!at(next.manager,0x4c,next.capacity,error)){if(error.empty())error="Subscriber intervention current actual manager class/header differs";return false;}
    if(next.count<0||next.count>1000000||next.capacity<next.count||(next.count&&(!next.data||next.data>UINTPTR_MAX-size_t(next.count)*8))){error="Subscriber intervention current complete array extent invalid";return false;}
    std::vector<uintptr_t> objects(size_t(next.count),uintptr_t{});if(next.count&&!copy(next.data,objects.data(),objects.size()*8,error))return false;
    std::set<uintptr_t> pointers;std::set<int32_t> ids;State values;
    for(const auto object:objects){Row row;if(!at(object,0x20,row.id,error))return false;
        if(!pointers.insert(object).second||!ids.insert(row.id).second){error="Subscriber intervention current native owner/ID duplicated";return false;}
        if(!read(at,image,object,row.id,row.value,error))return false;next.rows.emplace_back(object,row);values.push_back(row);}
    uintptr_t manager=0,data=0;int32_t count=-1,capacity=-1;
    if(!at(image,0x13df690,manager,error)||!at(next.manager,0x40,data,error)||!at(next.manager,0x48,count,error)||!at(next.manager,0x4c,capacity,error))return false;
    if(manager!=next.manager||data!=next.data||count!=next.count||capacity!=next.capacity){error="Subscriber intervention original current manager changed during read";return false;}
    std::sort(values.begin(),values.end(),[](const auto& a,const auto& b){return a.id<b.id;});if(!valid(values,error))return false;out=std::move(values);owners=std::move(next);error.clear();return true;
}
template<class Sample,class Guard,class Compare,class Receipt>bool adopt(const State& desired,Sample& sample,Guard& guard,Compare& compare,Receipt& receipt,std::string& error){
    if(!valid(desired,error))return false;State before;Owners owners;if(!sample(before,owners,error))return false;
    if(before.size()!=desired.size()){error="Subscriber intervention current native membership differs; original lifecycle adoption missing";return false;}
    for(size_t n=0;n<before.size();++n)if(before[n].id!=desired[n].id){error="Subscriber intervention current serialized ID set differs from Host";return false;}
    for(size_t n=0;n<owners.rows.size();++n){auto& [object,row]=owners.rows[n];const auto found=std::lower_bound(desired.begin(),desired.end(),row.id,[](const auto& value,int32_t id){return value.id<id;});
        if(found==desired.end()||found->id!=row.id){error="Subscriber intervention current member has no exact Host ID";return false;}const auto& target=found->value;
        const auto store=[&](size_t offset,unsigned bytes,uint32_t old,uint32_t value){if(old==value)return true;if(!guard(owners,n,error))return false;
            receipt.attempted=true;++receipt.attempted_stores;if(!compare(object+offset,bytes,old,value,error))return false;++receipt.stores;return true;};
        if(!store(0x50,4,row.value.warnings,target.warnings))return false;row.value.warnings=target.warnings;
        if(!store(0x70,4,row.value.watch_timer_bits,target.watch_timer_bits))return false;row.value.watch_timer_bits=target.watch_timer_bits;
        if(!store(0xc8,1,uint32_t(row.value.leaving),uint32_t(target.leaving)))return false;row.value.leaving=target.leaving;
        if(!store(0x125,1,uint32_t(row.value.banned),uint32_t(target.banned)))return false;row.value.banned=target.banned;}
    State actual;Owners final;if(!sample(actual,final,error))return false;
    if(actual!=desired||final!=owners){error="Subscriber intervention complete native state/identity readback differs from Host";return false;}
    error.clear();return true;
}
inline nlohmann::json encode(const State& state){auto rows=nlohmann::json::array();for(const auto& row:state)rows.push_back({{"id",row.id},{"warnings",row.value.warnings},{"watch_timer_bits",row.value.watch_timer_bits},{"leaving",row.value.leaving},{"banned",row.value.banned}});return rows;}
inline bool decode(const nlohmann::json& wire,State& out,std::string& error)try{
    out.clear();if(!wire.is_array()||wire.size()>1000000){error="Subscriber intervention rows exceed actual manager bound";return false;}State next;
    const auto number=[](const nlohmann::json& value,uint32_t maximum){if(!value.is_number_integer()||(value.is_number_unsigned()?value.get<uint64_t>()>maximum:value.get<int64_t>()<0||uint64_t(value.get<int64_t>())>maximum))throw std::runtime_error("Subscriber intervention integer outside domain");return value.get<uint32_t>();};
    for(const auto& value:wire){if(!value.is_object()||value.size()!=5||!value.at("leaving").is_boolean()||!value.at("banned").is_boolean())throw std::runtime_error("Subscriber intervention exact original ID/four values required");Row row;row.id=int32_t(number(value.at("id"),INT32_MAX));row.value={number(value.at("warnings"),UINT32_MAX),number(value.at("watch_timer_bits"),UINT32_MAX),value.at("leaving").get<bool>(),value.at("banned").get<bool>()};next.push_back(row);}
    if(!valid(next,error))return false;out=std::move(next);error.clear();return true;
}catch(const std::exception& exception){error=std::string("Subscriber intervention decode: ")+exception.what();return false;}
}
