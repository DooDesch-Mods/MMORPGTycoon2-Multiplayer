#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace mt2mp::game::toon_thought {
struct Row {
    int32_t type=0;std::array<int32_t,4> time{};std::array<uint32_t,3> position{};
    std::array<std::optional<std::string>,3> targets;bool registered=false;
    bool operator==(const Row&)const=default;
};
struct Toon {std::string key;std::vector<Row> thoughts;bool operator==(const Toon&)const=default;};
struct Links {int32_t kind=0;std::string key;std::vector<std::string> members;bool operator==(const Links&)const=default;};
struct State {std::vector<Toon> toons;std::vector<Links> links;bool operator==(const State&)const=default;};
inline std::string thought_key(const std::string& toon,size_t index){return toon+"/thought/"+std::to_string(index);}
inline bool fail(std::string& e,const std::string& cause){e=cause;return false;}
inline bool valid(const State& value,std::string& e){
    if(value.toons.size()>1000000||value.links.size()>1000000)return fail(e,"Toon Thought complete census exceeds native resource bound");
    std::string previous;std::map<std::string,std::array<std::optional<std::string>,3>> thoughts;size_t total=0;
    for(const auto& toon:value.toons){if(toon.key.empty()||toon.key<=previous||toon.thoughts.size()>1000000||total>1000000-toon.thoughts.size())return fail(e,"Toon Thought canonical owner/order or complete row budget differs");previous=toon.key;total+=toon.thoughts.size();
        for(size_t i=0;i<toon.thoughts.size();++i){const auto& row=toon.thoughts[i];if(row.type<0||row.type>49)return fail(e,"Toon Thought original enum exceeds its fifty native values");for(auto bits:row.position)if(!std::isfinite(std::bit_cast<float>(bits)))return fail(e,"Toon Thought original position is not finite");
            for(const auto& key:row.targets)if(key&&(key->empty()||key->size()>512||key->find('\0')!=std::string::npos))return fail(e,"Toon Thought canonical target key invalid");thoughts.emplace(thought_key(toon.key,i),row.targets);}}
    std::set<std::string> toon_keys;for(const auto& toon:value.toons)toon_keys.insert(toon.key);for(const auto& [key,refs]:thoughts){(void)key;if(refs[2]&&!toon_keys.contains(*refs[2]))return fail(e,"Thought Toon target lacks its complete canonical owner");}
    std::set<std::pair<int32_t,std::string>> targets;std::array<std::map<std::string,std::string>,2> members;
    for(const auto& list:value.links){if(list.kind<0||list.kind>1||list.key.empty()||!targets.emplace(list.kind,list.key).second||list.members.size()>1000000)return fail(e,"Thought reciprocal target class/key/list bound differs");
        for(const auto& key:list.members){const auto found=thoughts.find(key);if(found==thoughts.end()||found->second[size_t(list.kind)]!=std::optional<std::string>(list.key)||!members[size_t(list.kind)].emplace(key,list.key).second)return fail(e,"Thought reciprocal list contains foreign or repeated owning Thought");}}
    for(const auto& [key,refs]:thoughts)for(size_t i=0;i<2;++i)if(refs[i]&&(!members[i].contains(key)||members[i].at(key)!=*refs[i]))return fail(e,"Thought canonical target lacks exact reciprocal current membership");
    e.clear();return true;
}
inline nlohmann::json encode(const State& value){using Json=nlohmann::json;Json toons=Json::array(),links=Json::array();
    for(const auto& toon:value.toons){auto thoughts=Json::array();for(const auto& row:toon.thoughts){auto targets=Json::array();for(const auto& key:row.targets)targets.push_back(key?Json(*key):Json(nullptr));thoughts.push_back(Json::array({row.type,row.time,row.position,targets,row.registered}));}toons.push_back({{"key",toon.key},{"thoughts",thoughts}});}
    for(const auto& row:value.links)links.push_back(Json::array({row.kind,row.key,row.members}));return {{"toons",toons},{"links",links}};
}
inline bool decode(const nlohmann::json& j,State& out,std::string& e)try{
    out={};State next;const auto i32=[](const nlohmann::json& v){if(!v.is_number_integer()||(v.is_number_unsigned()?v.get<uint64_t>()>uint64_t(INT32_MAX):v.get<int64_t>()<INT32_MIN||v.get<int64_t>()>INT32_MAX))throw std::runtime_error("Thought int32 required");return v.get<int32_t>();};
    const auto bits=[](const nlohmann::json& v){if(!v.is_number_integer()||(v.is_number_unsigned()?v.get<uint64_t>()>UINT32_MAX:v.get<int64_t>()<0||uint64_t(v.get<int64_t>())>UINT32_MAX))throw std::runtime_error("Thought uint32 bits required");return v.get<uint32_t>();};
    if(!j.is_object()||j.size()!=2||!j.at("toons").is_array()||j.at("toons").size()>1000000||!j.at("links").is_array()||j.at("links").size()>1000000)throw std::runtime_error("Thought exact complete census fields required");
    size_t total=0;for(const auto& toon:j.at("toons")){if(!toon.is_object()||toon.size()!=2||!toon.at("key").is_string()||!toon.at("thoughts").is_array()||toon.at("thoughts").size()>1000000-total)throw std::runtime_error("Thought exact Toon row/budget required");Toon value;value.key=toon.at("key").get<std::string>();total+=toon.at("thoughts").size();
        for(const auto& row:toon.at("thoughts")){if(!row.is_array()||row.size()!=5||!row[1].is_array()||row[1].size()!=4||!row[2].is_array()||row[2].size()!=3||!row[3].is_array()||row[3].size()!=3||!row[4].is_boolean())throw std::runtime_error("Thought exact original fields/extents required");Row v;v.type=i32(row[0]);for(size_t i=0;i<4;++i)v.time[i]=i32(row[1][i]);for(size_t i=0;i<3;++i){v.position[i]=bits(row[2][i]);if(!row[3][i].is_null())v.targets[i]=row[3][i].get<std::string>();}v.registered=row[4].get<bool>();value.thoughts.push_back(std::move(v));}next.toons.push_back(std::move(value));}
    for(const auto& row:j.at("links")){if(!row.is_array()||row.size()!=3||!row[1].is_string()||!row[2].is_array()||row[2].size()>1000000)throw std::runtime_error("Thought exact reciprocal target list required");Links v;v.kind=i32(row[0]);v.key=row[1].get<std::string>();for(const auto& key:row[2])v.members.push_back(key.get<std::string>());next.links.push_back(std::move(v));}
    if(!valid(next,e))return false;out=std::move(next);e.clear();return true;
}catch(const std::exception& error){return fail(e,std::string("Thought decode: ")+error.what());}
struct Array {uintptr_t owner=0,data=0;int32_t count=0,capacity=0;bool operator==(const Array&)const=default;};
struct LocalRow {uintptr_t object=0;std::array<uintptr_t,3> targets{},proxies{};bool operator==(const LocalRow&)const=default;};
struct LocalToon {uintptr_t object=0;Array array;std::vector<LocalRow> thoughts;bool operator==(const LocalToon&)const=default;};
struct Owners {std::vector<LocalToon> toons;std::vector<Array> links;bool operator==(const Owners&)const=default;};
struct Bindings {std::map<std::string,uintptr_t> toons;std::array<std::map<std::string,uintptr_t>,2> targets;bool operator==(const Bindings&)const=default;};
template<class Read>bool array(Read& at,uintptr_t owner,size_t data_offset,Array& out,std::string& e){
    out.owner=owner;if(!at(owner,data_offset,out.data,e)||!at(owner,data_offset+8,out.count,e)||!at(owner,data_offset+12,out.capacity,e))return false;
    return (out.count>=0&&out.count<=1000000&&out.capacity>=out.count&&out.capacity<=1000000&&(!out.capacity||out.data)&&out.data<=UINTPTR_MAX-size_t(out.capacity)*8)||fail(e,"Thought complete native array extent invalid");
}
template<class Read>bool read_row(Read& at,uintptr_t image,uintptr_t expected_thinker,uintptr_t object,const std::array<std::map<uintptr_t,std::string>,3>& keys,LocalRow& owned,Row& value,std::string& e){
    owned={};value={};owned.object=object;uintptr_t ap=0,thinker=0;uint8_t registered=0;
if(!at(owned.object,0,ap,e)||ap!=image+0x16fdef0||!at(owned.object,0x18,thinker,e)||thinker!=expected_thinker||!at(owned.object,0x20,value.type,e)||
               !at(owned.object,0xd0,ap,e)||ap!=image+0x1744d10||!at(owned.object,0xe8,value.time,e)||!at(owned.object,0xf8,value.position,e)||!at(owned.object,0x104,registered,e)||registered>1)return fail(e,e.empty()?"Thought actual class/thinker/time/registration differs":e);
            value.registered=registered!=0;
            constexpr std::array<size_t,3> offsets{0x28,0x60,0x98};constexpr std::array<uintptr_t,3> classes{0x17116f0,0x1711b70,0x1711ab0};
            for(size_t kind=0;kind<3;++kind){if(!at(owned.object,offsets[kind],ap,e)||ap!=image+classes[kind]||!at(owned.object,offsets[kind]+0x28,owned.targets[kind],e)||!at(owned.object,offsets[kind]+0x30,owned.proxies[kind],e))return fail(e,e.empty()?"Thought weak target class/header differs":e);
                uintptr_t current=0;if(owned.proxies[kind]&&!at(owned.proxies[kind],0,current,e))return false;
                if(current){const auto found=keys[kind].find(owned.targets[kind]);if(found==keys[kind].end()||current!=owned.targets[kind])return fail(e,"Thought live weak target lacks exact current canonical owner");value.targets[kind]=found->second;}
            }
    for(auto bits:value.position)if(!std::isfinite(std::bit_cast<float>(bits)))return fail(e,"Thought original position is not finite");e.clear();return true;
}
template<class Read>bool read(Read& at,uintptr_t image,const Bindings& bindings,Owners& owners,State& out,std::string& e){
    owners={};out={};std::array<std::map<uintptr_t,std::string>,3> keys;
    for(size_t i=0;i<2;++i)for(const auto& [key,pointer]:bindings.targets[i])if(!pointer||!keys[i].emplace(pointer,key).second)return fail(e,"Thought canonical target allocation aliases two owners");
    for(const auto& [key,pointer]:bindings.toons)if(!pointer||!keys[2].emplace(pointer,key).second)return fail(e,"Thought canonical Toon allocation aliases two owners");
    std::map<uintptr_t,std::string> thought_keys;
    for(const auto& [key,pointer]:bindings.toons){uintptr_t ap=0;LocalToon local;local.object=pointer;Toon toon;toon.key=key;
        if(!at(pointer,0,ap,e)||ap!=image+0x1744e90||!at(pointer,0x8c0,ap,e)||ap!=image+0x16eaae0||!array(at,pointer,0x8c8,local.array,e))return fail(e,e.empty()?"Thought owning current Toon/Array class differs":e);
        for(int32_t i=0;i<local.array.count;++i){LocalRow owned;Row value;uintptr_t object=0;
            if(!at(local.array.data,size_t(i)*8,object,e)||!read_row(at,image,pointer,object,keys,owned,value,e))return false;
            if(!thought_keys.emplace(object,thought_key(key,size_t(i))).second)return fail(e,"Thought allocation occurs in multiple owning slots");
            local.thoughts.push_back(owned);toon.thoughts.push_back(std::move(value));
        }
        owners.toons.push_back(std::move(local));out.toons.push_back(std::move(toon));
    }
    for(size_t kind=0;kind<2;++kind)for(const auto& [key,pointer]:bindings.targets[kind]){Array local;Links links;links.kind=int32_t(kind);links.key=key;uintptr_t ap=0;const size_t base=kind?0x580:0xce0;
        if(!at(pointer,base,ap,e)||ap!=image+0x1717220||!array(at,pointer,base+0x20,local,e))return fail(e,e.empty()?"Thought reciprocal target link-array class differs":e);
        for(int32_t i=0;i<local.count;++i){uintptr_t thought=0;if(!at(local.data,size_t(i)*8,thought,e))return false;const auto found=thought_keys.find(thought);if(found==thought_keys.end())return fail(e,"Thought reciprocal target list contains an allocation outside complete live owning Toon arrays; no dereference");links.members.push_back(found->second);}
        owners.links.push_back(local);out.links.push_back(std::move(links));
    }
    return valid(out,e);
}
}
