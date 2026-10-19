#pragma once
#include "npc_quest_targets.h"
#include <bit>
#include <cmath>
#include <set>
#include "mt2mp/core/hash.h"
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::game::duplicate_localized_history {
namespace q=npc_quest_targets;
struct Argument {
    std::string name;
    int32_t type=0,integer=0;
    uint32_t real=0;
    size_t child=SIZE_MAX;
    bool operator==(const Argument&)const=default;
};
struct String {
    std::string text;
    std::vector<Argument> arguments;
    bool operator==(const String&)const=default;
};
using Value=std::vector<String>;
struct Budget {size_t strings=0,arguments=0,text_bytes=0;};

inline bool summarize(const Value& value,std::string& sha,std::string& e){
    sha.clear();if(value.empty())return q::fail(e,"Duplicate localized description value is empty");
    nlohmann::json strings=nlohmann::json::array();size_t next=1;
    for(size_t i=0;i<value.size();++i){const auto& row=value[i];nlohmann::json args=nlohmann::json::array();
        for(const auto& arg:row.arguments){
            if(arg.type==0){if(arg.child!=next++||arg.child<=i||arg.child>=value.size())return q::fail(e,"Duplicate localized nested value order invalid");
                args.push_back(nlohmann::json::array({arg.name,arg.type,arg.child}));}
            else if(arg.type==1)args.push_back(nlohmann::json::array({arg.name,arg.type,arg.integer}));
            else if(arg.type==2){if(!std::isfinite(std::bit_cast<float>(arg.real)))return q::fail(e,"Duplicate localized numeric value is not finite");
                args.push_back(nlohmann::json::array({arg.name,arg.type,arg.integer,arg.real}));}
            else return q::fail(e,"Duplicate localized argument value type invalid");
        }
        strings.push_back(nlohmann::json::array({row.text,std::move(args)}));
    }
    if(next!=value.size())return q::fail(e,"Duplicate localized description has an unowned nested value");
    try{const auto bytes=strings.dump();sha=mt2mp::sha256(std::vector<unsigned char>(bytes.begin(),bytes.end()));}
    catch(const std::exception& ex){e=std::string("Duplicate localized description serialization failed: ")+ex.what();return false;}
    return true;
}

// MakeBuilding copies vsLocString, including nested named arguments. Reading
// only its format text loses Region/date substitutions in native descriptions.
// Inactive union fields are not part of vsLocString's original equality.
template<class Memory,class Text>
bool read(Memory& memory,uintptr_t pointer,const Budget& budget,Text&& text,Value& out,std::string& e){
    out.clear();std::vector<uintptr_t> pending{pointer};std::set<uintptr_t> seen;
    size_t arguments=0,bytes=0;
    auto string=[&](uintptr_t p,std::string& value){
        if(!text(p,value,e))return false;
        if(value.size()>budget.text_bytes||bytes>budget.text_bytes-value.size())
            return q::fail(e,"Duplicate localized description text exceeds supplied budget");
        bytes+=value.size();return true;};
    for(size_t index=0;index<pending.size();++index){
        if(pending.size()>budget.strings)return q::fail(e,"Duplicate localized description exceeds supplied string budget");
        const auto current=pending[index];
        if(!current||!seen.insert(current).second)return q::fail(e,"Duplicate localized description repeats an owned string");
        String row;std::array<uintptr_t,3> bounds{};
        if(!string(current,row.text)||!q::at(memory,current,0x20,bounds,e))return false;
        const auto [begin,end,capacity]=bounds;
        if(begin>end||end>capacity||(!begin&&(end||capacity))||(end-begin)%0x68||(capacity-begin)%0x68)
            return q::fail(e,"Duplicate localized argument vector extent invalid");
        const auto count=size_t((end-begin)/0x68);
        if(count>budget.arguments||arguments>budget.arguments-count)
            return q::fail(e,"Duplicate localized description exceeds supplied argument budget");
        arguments+=count;
        for(size_t i=0;i<count;++i){const auto arg=begin+i*0x68;Argument value;
            if(!string(arg,value.name)||!q::at(memory,arg,0x60,value.type,e))return false;
            if(value.type==0){
                if(pending.size()>=budget.strings)return q::fail(e,"Duplicate localized nested string exceeds supplied budget");
                value.child=pending.size();pending.push_back(arg+0x20);
            }else if(value.type==1){if(!q::at(memory,arg,0x58,value.integer,e))return false;
            }else if(value.type==2){
                if(!q::at(memory,arg,0x58,value.integer,e)||!q::at(memory,arg,0x5c,value.real,e))return false;
                if(!std::isfinite(std::bit_cast<float>(value.real)))return q::fail(e,"Duplicate localized numeric argument is not finite");
            }else return q::fail(e,"Duplicate localized argument type invalid actual="+std::to_string(value.type));
            row.arguments.push_back(std::move(value));
        }
        std::array<uintptr_t,3> actual{};
        if(!q::at(memory,current,0x20,actual,e))return false;
        if(actual!=bounds)return q::fail(e,"Duplicate localized argument vector changed during read");
        out.push_back(std::move(row));
    }
    return true;
}
}
