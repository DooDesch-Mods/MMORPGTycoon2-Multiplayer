#pragma once
#include "vendor/nlohmann/json.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::game::region_rng_snapshot {
inline constexpr size_t maximum_rows=4096;
struct Row {
    int32_t region=-1;
    std::array<uint64_t,4> words{};
    int32_t extra=0;
    bool operator==(const Row&)const=default;
};
struct Manifest {
    bool present=false;
    std::vector<Row> rows;
    bool operator==(const Manifest&)const=default;
};
struct Proof {
    uint32_t mode=0,count=0,index=UINT32_MAX;
    Row row;
    std::string sha256;
};
inline std::string commitment(const Manifest& value){
    auto rows=nlohmann::json::array();
    for(const auto& row:value.rows)rows.push_back(nlohmann::json::array({row.region,row.words,row.extra}));
    return nlohmann::json::array({value.present?2:1,rows}).dump();
}
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
inline bool valid(const Manifest& value,std::string& error){
    if(!value.present)return value.rows.empty()||fail(error,"Region RNG absent manifest has rows");
    if(value.rows.empty()||value.rows.size()>maximum_rows)return fail(error,"Region RNG complete root census row count invalid");
    int32_t previous=-1;
    for(const auto& row:value.rows){
        if(row.region<0||row.region<=previous)return fail(error,"Region RNG saved ID is invalid, duplicate or unsorted");
        if(std::all_of(row.words.begin(),row.words.end(),[](uint64_t word){return !word;}))
            return fail(error,"Region RNG state is all zero id="+std::to_string(row.region));
        previous=row.region;
    }
    error.clear();return true;
}
inline nlohmann::json encode(const Manifest& value){
    auto rows=nlohmann::json::array();
    for(const auto& row:value.rows)rows.push_back({{"region",row.region},{"words",row.words},{"extra",row.extra}});
    return {{"rows",rows}};
}
inline bool decode(const nlohmann::json& value,Manifest& out,std::string& error)try{
    out={};
    if(!value.is_object()||value.size()!=1||!value.contains("rows")||!value.at("rows").is_array()||
       value.at("rows").empty()||value.at("rows").size()>maximum_rows)
        return fail(error,"Region RNG exact manifest schema differs");
    Manifest next;next.present=true;
    for(const auto& row:value.at("rows")){
        if(!row.is_object()||row.size()!=3||!row.at("region").is_number_integer()||
           row.at("region").get<int64_t>()<0||row.at("region").get<uint64_t>()>INT32_MAX||
           !row.at("words").is_array()||row.at("words").size()!=4||!row.at("extra").is_number_integer())
            return fail(error,"Region RNG exact row schema differs");
        const auto& extra=row.at("extra");
        if((extra.is_number_unsigned()&&extra.get<uint64_t>()>INT32_MAX)||
           (!extra.is_number_unsigned()&&(extra.get<int64_t>()<INT32_MIN||extra.get<int64_t>()>INT32_MAX)))
            return fail(error,"Region RNG extra state is outside int32");
        Row next_row;next_row.region=row.at("region").get<int32_t>();next_row.extra=extra.get<int32_t>();
        for(size_t index=0;index<4;++index){
            if(!row.at("words").at(index).is_number_unsigned())return fail(error,"Region RNG word is not uint64");
            next_row.words[index]=row.at("words").at(index).get<uint64_t>();
        }
        next.rows.push_back(next_row);
    }
    if(!valid(next,error))return false;out=std::move(next);return true;
}catch(const std::exception& cause){return fail(error,std::string("Region RNG decode failed: ")+cause.what());}
}
