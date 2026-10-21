#pragma once
#include "character_calendar_state.h"
#include <algorithm>
#include <charconv>
#include <map>
#include <optional>
#include <string_view>
namespace mt2mp::game::subscriber_demand {
struct Row {int32_t id=-1;std::optional<std::string> key;bool operator==(const Row&)const=default;};
using State=std::vector<Row>;
inline bool key(const std::string& value){
    constexpr std::string_view prefix="character:";if(!value.starts_with(prefix)||value.size()>128)return false;
    const auto separator=value.find(':',prefix.size()),suffix=value.find("/demand/",separator);
    if(separator!=prefix.size()+1||suffix==std::string::npos||value[prefix.size()]<'0'||value[prefix.size()]>'4')return false;
    int32_t id=-1;const auto begin=value.data()+separator+1,end=value.data()+suffix;const auto parsed=std::from_chars(begin,end,id);
    return begin!=end&&parsed.ec==std::errc{}&&parsed.ptr==end&&id>=0&&(end-begin==1||*begin!='0')&&(value.substr(suffix)=="/demand/buff"||value.substr(suffix)=="/demand/nerf");
}
inline bool valid(const State& value,std::string& e){int32_t previous=-1;if(value.size()>1000000){e="Subscriber Demand complete census exceeds native bound";return false;}
    for(const auto& row:value){if(row.id<=previous||row.id<0||(row.key&&!key(*row.key))){e="Subscriber Demand sorted ID/canonical owner key invalid";return false;}previous=row.id;}e.clear();return true;}
inline bool reciprocal(const State& value,const character_calendar::State& calendar,std::string& e){
    if(!valid(value,e)||!character_calendar::valid(calendar,e))return false;std::map<int32_t,std::string> members;
    for(const auto& definition:calendar.definitions)for(size_t i=0;i<2;++i)if(definition.demands[i])for(auto id:definition.demands[i]->members)
        if(!members.emplace(id,character_calendar::demand_key(definition,i)).second){e="Subscriber Demand occurs in multiple current owning lists";return false;}
    for(const auto& row:value){const auto found=members.find(row.id);if(row.key){if(found==members.end()||found->second!=*row.key){e="Subscriber current Demand pointer/key differs from complete owning member list id="+std::to_string(row.id);return false;}members.erase(found);}else if(found!=members.end()){e="Subscriber null Demand retains owning list membership id="+std::to_string(row.id);return false;}}
    if(!members.empty()){e="Demand owning list contains a Subscriber outside the current manager";return false;}e.clear();return true;
}
inline nlohmann::json encode(const State& value){auto rows=nlohmann::json::array();for(const auto& row:value)rows.push_back(nlohmann::json::array({row.id,row.key?nlohmann::json(*row.key):nlohmann::json(nullptr)}));return rows;}
inline bool decode(const nlohmann::json& j,State& out,std::string& e){out.clear();try{if(!j.is_array()||j.size()>1000000)throw std::runtime_error("complete Subscriber Demand array required");State value;
    for(const auto& row:j){if(!row.is_array()||row.size()!=2||!row[0].is_number_integer()||(row[0].is_number_unsigned()?row[0].get<uint64_t>()>uint64_t(INT32_MAX):row[0].get<int64_t>()<0||row[0].get<int64_t>()>INT32_MAX))throw std::runtime_error("Subscriber Demand exact ID/reference pair required");Row next;next.id=row[0].get<int32_t>();if(!row[1].is_null())next.key=row[1].get<std::string>();value.push_back(std::move(next));}
    if(!valid(value,e))return false;out=std::move(value);e.clear();return true;
}catch(const std::exception& cause){e=std::string("Subscriber Demand decode: ")+cause.what();return false;}}
struct Local {uintptr_t object=0,demand=0;int32_t id=-1;bool operator==(const Local&)const=default;};
struct Owners {uintptr_t manager=0,data=0;int32_t count=0,capacity=0;std::vector<Local> rows;bool operator==(const Owners&)const=default;};
template<class Read>bool read(Read& at,uintptr_t image,const std::map<uintptr_t,std::string>& keys,Owners& owners,State& out,std::string& e){
    owners={};out.clear();uintptr_t ap=0;
    if(!at(image,0x13df690,owners.manager,e)||!at(owners.manager,0,ap,e)||ap!=image+0x1725f40||!at(owners.manager,0x40,owners.data,e)||!at(owners.manager,0x48,owners.count,e)||!at(owners.manager,0x4c,owners.capacity,e)){
        if(e.empty())e="Subscriber Demand current Manager singleton/class differs";return false;}
    if(owners.count<0||owners.count>1000000||owners.capacity<owners.count||owners.capacity>1000000||(!owners.data&&owners.capacity)||owners.data>UINTPTR_MAX-size_t(owners.capacity)*8){e="Subscriber Demand complete manager extent invalid";return false;}
    std::set<uintptr_t> unique;std::map<int32_t,std::pair<Local,Row>> sorted;
    for(int32_t i=0;i<owners.count;++i){Local local;Row value;
        if(!at(owners.data,size_t(i)*8,local.object,e)||!at(local.object,0,ap,e)||ap!=image+0x16e75f0||!at(local.object,0x20,local.id,e)||!at(local.object,0xc0,local.demand,e)){if(e.empty())e="Subscriber Demand actual member class/ID differs";return false;}
        if(local.id<0||!unique.insert(local.object).second){e="Subscriber Demand native allocation or ID duplicated";return false;}value.id=local.id;
        if(local.demand){const auto found=keys.find(local.demand);if(found==keys.end()||!key(found->second)){e="Subscriber Demand current pointer has no exact registered Character owner";return false;}value.key=found->second;}
        if(!sorted.emplace(local.id,std::pair{local,value}).second){e="Subscriber Demand serialized ID duplicated";return false;}
    }
    for(const auto& entry:sorted){owners.rows.push_back(entry.second.first);out.push_back(entry.second.second);}return valid(out,e);
}
}
