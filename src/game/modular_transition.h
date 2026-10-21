#pragma once
#include "typed_action/modular_action.h"
#include "scenery_stroke_transition.h"
#include "scenery_stroke_state.h"
#include <bit>
#include <cmath>
namespace mt2mp::game::modular_transition {
namespace a=action_session;
inline bool fail(std::string& e,const char* why){e=why;return false;}
inline nlohmann::json records_json(const std::vector<subscriber_lifecycle::Record>& records){auto out=nlohmann::json::array();const auto token=[](const auto& value){return nlohmann::json::array({value.type,value.bits,value.text});};
    for(const auto& row:records){auto tokens=nlohmann::json::array();for(const auto& t:row.tokens)tokens.push_back(token(t));out.push_back(nlohmann::json::array({row.parent==SIZE_MAX?int64_t{-1}:int64_t(row.parent),row.child_index,token(row.label),tokens}));}return out;}
inline bool records(const nlohmann::json& j,bool plan,std::vector<subscriber_lifecycle::Record>& out,std::string& e)try{
    out.clear();if(!j.is_array()||j.empty()||j.size()>100000)return fail(e,"Modular typed owned Record count differs");size_t text=0,tokens=0;std::vector<size_t> children(j.size());std::set<uint32_t> definitions;
    const auto token=[&](const auto& value,subscriber_lifecycle::Token& result){if(!value.is_array()||value.size()!=3||!value[0].is_number_integer()||!value[1].is_number_unsigned()||!value[2].is_string())return false;
        const auto type=value[0].template get<int64_t>();const auto bits=value[1].template get<uint64_t>();if(type<0||type>4||bits>UINT32_MAX)return false;result={int32_t(type),uint32_t(bits),value[2].template get<std::string>()};text+=result.text.size();
        return text<=32*1024*1024&&result.text.find('\0')==std::string::npos&&((type==1||type==2)?bits==0:result.text.empty())&&(type!=3||std::isfinite(std::bit_cast<float>(result.bits)));};
    for(size_t i=0;i<j.size();++i){const auto& row=j[i];if(!row.is_array()||row.size()!=4||!row[0].is_number_integer()||!row[1].is_number_unsigned()||!row[3].is_array())return fail(e,"Modular exact native Record row required");
        const auto parent=row[0].get<int64_t>();const auto index=row[1].get<uint64_t>();if((!i&&(parent!=-1||index))||(i&&(parent<0||uint64_t(parent)>=i||index!=children[size_t(parent)]++)))return fail(e,"Modular complete native Record tree order differs");
        subscriber_lifecycle::Record result;result.parent=i?size_t(parent):SIZE_MAX;result.child_index=size_t(index);if(!token(row[2],result.label)||result.label.type>2)return fail(e,"Modular Record label type/bits differ");tokens+=row[3].size();if(tokens>1000000)return fail(e,"Modular Record token budget exceeded");
        for(const auto& t:row[3]){subscriber_lifecycle::Token value;if(!token(t,value))return fail(e,"Modular native Record token differs");result.tokens.push_back(std::move(value));}if(result.label.text=="linkId"&&(result.tokens.size()!=1||result.tokens[0].type!=4||!result.tokens[0].bits||result.tokens[0].bits>INT32_MAX||!definitions.insert(result.tokens[0].bits).second))return fail(e,"Modular inline Record definition is invalid or repeated");out.push_back(std::move(result));}
    if(out[0].label.type!=1||out[0].label.text!=(plan?"mmoBuildingPlan":"mmoBuildingLayout"))return fail(e,"Modular native Record typed root differs");e.clear();return true;
}catch(const std::exception& error){e=std::string("Modular owned Record decode: ")+error.what();return false;}
template<class Key>bool valid(const a::ModularIntent& v,Key&& key,std::string& e){
    if(v.mode>4||v.region<0)return fail(e,"Modular mode/Region outside original domain");for(auto bits:v.position)if(!std::isfinite(std::bit_cast<float>(bits)))return fail(e,"Modular resolved position is not finite");
    if(v.mode){if(!key(v.target)||!duplicate_transition::sha(v.before_record_sha)||!v.theme.empty()||v.pose!=std::array<uint32_t,10>{}||v.clearance!=std::array<uint32_t,6>{}||v.layout!=nlohmann::json::array()||v.plan!=nlohmann::json::array())return fail(e,"Modular edit exact canonical before/unused Prefab fields differ");}
    else{if(!v.target.empty()||!v.before_record_sha.empty()||v.theme.empty()||v.theme.size()>512||v.theme.find('\0')!=std::string::npos||!duplicate_transition::pose(v.pose))return fail(e,"Modular Prefab theme/pose or unused target fields differ");
        for(size_t i=0;i<3;++i)if(v.position[i]!=v.pose[4+i])return fail(e,"Modular Prefab purchase and preview position differ");
        for(auto bits:v.clearance)if(!std::isfinite(std::bit_cast<float>(bits)))return fail(e,"Modular Prefab clearance is not finite");for(size_t i=0;i<3;++i)if(std::bit_cast<float>(v.clearance[i])>std::bit_cast<float>(v.clearance[i+3]))return fail(e,"Modular Prefab clearance minimum exceeds maximum");
        std::vector<subscriber_lifecycle::Record> rows;if(!records(v.layout,false,rows,e)||!records(v.plan,true,rows,e))return false;}
    e.clear();return true;
}
template<class Hash>bool difference(const std::map<std::string,a::DuplicateObject>& before,const std::map<std::string,a::DuplicateObject>& after,Hash&& hash,a::ModularEffect& out,std::string& e){
    out={};auto untouched=nlohmann::json::array();for(const auto& [key,row]:before){const auto found=after.find(key);if(found==after.end())out.removed.push_back(row);else if(found->second!=row)out.changed.push_back({row,found->second});else untouched.push_back(scenery_stroke_state::row(row));}
    for(const auto& [key,row]:after)if(!before.contains(key))out.created.push_back(row);return hash(untouched.dump(),out.untouched_sha,e);
}
template<class Key>bool valid(const a::ModularEffect& v,Key&& key,std::string& e){
    if(!duplicate_transition::sha(v.untouched_sha)||v.removed.size()>1000000||v.created.size()>1000000||v.changed.size()>1000000)return fail(e,"Modular complete effect budget/hash differs");std::set<std::string> prior,next;
    for(const auto& row:v.removed)if(!scenery_stroke_transition::object(row,key)||!prior.insert(row.key).second)return fail(e,"Modular removed canonical owner repeats or is invalid");
    for(const auto& row:v.created)if(!scenery_stroke_transition::object(row,key)||!next.insert(row.key).second||prior.contains(row.key))return fail(e,"Modular created canonical owner repeats or is invalid");
    for(const auto& row:v.changed)if(!scenery_stroke_transition::object(row[0],key)||!scenery_stroke_transition::object(row[1],key)||row[0]==row[1]||row[0].key!=row[1].key||row[0].family!=row[1].family||row[0].region!=row[1].region||!prior.insert(row[0].key).second||!next.insert(row[1].key).second)return fail(e,"Modular changed exact owner pair differs");
    for(const auto& sha:v.dependency_sha)if(!duplicate_transition::sha(sha))return fail(e,"Modular complete Service/Quest witness hash differs");std::set<std::string> related;for(const auto& owner:v.related_buildings)if(!key(owner)||!related.insert(owner).second)return fail(e,"Modular reciprocal Building owner is invalid or repeated");
    for(const auto& cash:v.cash){if(cash.amount==INT32_MIN)return fail(e,"Modular native inverse cash cannot be negated");for(auto bits:cash.position)if(!std::isfinite(std::bit_cast<float>(bits)))return fail(e,"Modular native cash position is not finite");}e.clear();return true;
}
}
