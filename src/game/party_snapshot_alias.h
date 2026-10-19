#pragma once
#include "party_identity.h"
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::party_snapshot_alias {
struct Row {std::string record,key;bool operator==(const Row&)const=default;};
struct Manifest {bool present=false;std::vector<Row> rows;bool operator==(const Manifest&)const=default;};
inline bool valid(const Manifest& manifest,const std::string& native_sha,std::string& error){
    if(!manifest.present){if(!manifest.rows.empty()){error="Absent Party snapshot aliases contain rows";return false;}return true;}
    if(native_sha.size()!=64||native_sha.find_first_not_of("0123456789abcdef")!=std::string::npos||manifest.rows.size()>INT32_MAX){error="Party snapshot aliases require exact native SHA and bounded records";return false;}
    std::set<std::string> keys;std::string previous;
    for(const auto& row:manifest.rows){if(!snapshot_party_key::valid(row.record)||row.record.substr(0,64)!=native_sha||!party_identity::valid_key(row.key)||(!previous.empty()&&row.record<=previous)||!keys.insert(row.key).second){error="Party snapshot aliases repeat or change the native saved/canonical record domain";return false;}previous=row.record;}
    error.clear();return true;
}
inline nlohmann::json encode(const Manifest& manifest){nlohmann::json rows=nlohmann::json::array();for(const auto& row:manifest.rows)rows.push_back({{"record",row.record},{"key",row.key}});return rows;}
inline bool decode(const nlohmann::json& wire,const std::string& native_sha,Manifest& out,std::string& error)try{
    out={};if(!wire.is_array()||wire.size()>INT32_MAX){error="Party snapshot alias rows exceed original schema bound";return false;}Manifest next;next.present=true;
    for(const auto& row:wire){if(!row.is_object()||row.size()!=2||!row.contains("record")||!row.contains("key")||!row.at("record").is_string()||!row.at("key").is_string()){error="Party snapshot aliases require exact saved/canonical string pair";return false;}next.rows.push_back({row.at("record").get<std::string>(),row.at("key").get<std::string>()});}
    if(!valid(next,native_sha,error))return false;out=std::move(next);return true;
}catch(const std::exception& ex){error=std::string("Party snapshot alias decode failed: ")+ex.what();return false;}
}
