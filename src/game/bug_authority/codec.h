#pragma once
#include "state.h"
#include "../snapshot_building_key.h"
#include "../snapshot_npc_key.h"
#include "vendor/nlohmann/json.hpp"
#include <cmath>
#include <set>
#include <stdexcept>
#include <tuple>

namespace mt2mp::game::bug_authority {
namespace codec_detail {
inline bool uuid(const std::string& s){
    if(s.size()!=36)return false;
    for(size_t i=0;i<s.size();++i){
        if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}
        else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;
    }
    return true;
}
inline bool sha(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
inline bool parent(const Row& r){
    if(r.parent_kind!=ParentKind::Building&&r.parent_kind!=ParentKind::Npc)return false;
    if(r.parent_key.starts_with("action:"))return uuid(r.parent_key.substr(7));
    return r.parent_kind==ParentKind::Building?snapshot_building_key::valid(r.parent_key):snapshot_npc_key::valid(r.parent_key);
}
inline int32_t i32(const nlohmann::json& j){
    if(!j.is_number_integer()||(j.is_number_unsigned()?j.get<uint64_t>()>uint64_t(INT32_MAX):j.get<int64_t>()<INT32_MIN||j.get<int64_t>()>INT32_MAX))
        throw std::runtime_error("Bug int32 required");
    return j.get<int32_t>();
}
inline uint64_t u64(const nlohmann::json& j){
    if(!j.is_number_integer()||(!j.is_number_unsigned()&&j.get<int64_t>()<0))throw std::runtime_error("Bug uint64 required");
    return j.get<uint64_t>();
}
inline bool boolean(const nlohmann::json& j){if(!j.is_boolean())throw std::runtime_error("Bug boolean required");return j.get<bool>();}
}
inline bool valid(const State& state,std::string& error){
    using BirthKey=std::tuple<BirthKind,std::string,uint64_t,uint64_t>;
    using Key=std::tuple<ParentKind,std::string,BirthKey>;
    std::set<Key> keys;std::set<BirthKey> global;
    for(const auto& row:state.rows){
        const auto& b=row.birth;const auto& v=row.value;
        if(!codec_detail::parent(row)){error="Bug canonical parent kind/key invalid";return false;}
        if((b.kind==BirthKind::Snapshot&&(!codec_detail::sha(b.source)||b.sequence!=0))||
           (b.kind==BirthKind::Action&&(!codec_detail::uuid(b.source)||b.sequence!=0))||
           (b.kind==BirthKind::Publication&&(!codec_detail::uuid(b.source)||b.sequence==0))||
           (b.kind!=BirthKind::Snapshot&&b.kind!=BirthKind::Action&&b.kind!=BirthKind::Publication)){
            error="Bug birth kind/source/sequence invalid";return false;
        }
        if(v.type<0||v.type>2||v.created< -1||v.discovered< -1||v.toons<0||v.variant<0||
           !std::isfinite(v.reproducibility)||v.reproducibility<0||v.reproducibility>1){error="Bug scalar domain invalid";return false;}
        const BirthKey birth{b.kind,b.source,b.sequence,b.occurrence};
        if(!keys.emplace(row.parent_kind,row.parent_key,birth).second){error="Bug duplicate parent/birth identity";return false;}
        if(b.kind!=BirthKind::Snapshot&&!global.insert(birth).second){error="Bug duplicate global action/publication occurrence";return false;}
    }
    error.clear();return true;
}
// Flat rows keep the existing HostFrame nesting and Channel9 payload bounds.
inline nlohmann::json encode(const State& state){
    std::string error;if(!valid(state,error))throw std::runtime_error(error);
    auto rows=nlohmann::json::array();
    for(const auto& r:state.rows){const auto& b=r.birth;const auto& v=r.value;
        rows.push_back(nlohmann::json::array({static_cast<int>(r.parent_kind),r.parent_key,static_cast<int>(b.kind),b.source,b.sequence,b.occurrence,
            v.type,v.created,v.discovered,v.toons,v.variant,v.reproducibility,v.known,v.manual,v.debugged}));
    }
    return rows;
}
inline bool decode(const nlohmann::json& input,State& out,std::string& error){
    out={};try{
        if(!input.is_array())throw std::runtime_error("Bug complete row array required");
        State next;
        for(const auto& j:input){
            if(!j.is_array()||j.size()!=15)throw std::runtime_error("Bug exact 15-field row required");
            Row r;r.parent_kind=static_cast<ParentKind>(codec_detail::i32(j.at(0)));r.parent_key=j.at(1).get<std::string>();
            r.birth.kind=static_cast<BirthKind>(codec_detail::i32(j.at(2)));r.birth.source=j.at(3).get<std::string>();
            r.birth.sequence=codec_detail::u64(j.at(4));r.birth.occurrence=codec_detail::u64(j.at(5));
            auto& v=r.value;v.type=codec_detail::i32(j.at(6));v.created=codec_detail::i32(j.at(7));v.discovered=codec_detail::i32(j.at(8));
            v.toons=codec_detail::i32(j.at(9));v.variant=codec_detail::i32(j.at(10));
            if(!j.at(11).is_number())throw std::runtime_error("Bug numeric reproducibility required");
            const auto repro=j.at(11).get<double>();
            if(!std::isfinite(repro)||repro<0||repro>1)throw std::runtime_error("Bug reproducibility domain invalid");
            v.reproducibility=static_cast<float>(repro);v.known=codec_detail::boolean(j.at(12));
            v.manual=codec_detail::boolean(j.at(13));v.debugged=codec_detail::boolean(j.at(14));next.rows.push_back(std::move(r));
        }
        if(!valid(next,error))return false;
        out=std::move(next);error.clear();return true;
    }catch(const std::exception& x){error=std::string("Bug decode: ")+x.what();return false;}
}
}
