#pragma once
#include "subscriber_lifecycle/record_graph.h"
#include "mt2mp/core/hash.h"
#include "vendor/nlohmann/json.hpp"
#include <set>

namespace mt2mp::game::duplicate_owned_history {
namespace native=subscriber_lifecycle;
// Undo Make nodes retain this payload without SaveContext. It is a value
// witness for the exact owned Layout/Plan/Bug record, not a portable live link
// resolver. Every original label, token bit and ordered child is retained.
inline bool summarize(const std::vector<native::Record>& records,std::string& sha,std::string& e){
    sha.clear();auto fail=[&](const char* cause){e=cause;return false;};
    if(records.empty())return fail("Duplicate owned original record is empty");
    nlohmann::json rows=nlohmann::json::array();std::set<std::pair<size_t,size_t>> positions;std::set<uint32_t> definitions;
    auto token=[&](const native::Token& t,nlohmann::json& value){
        if(t.type<0||t.type>4)return fail("Duplicate owned original token type invalid");
        if(t.type==1||t.type==2)value=nlohmann::json::array({t.type,t.text});
        else{if(!t.text.empty())return fail("Duplicate owned scalar contains text");value=nlohmann::json::array({t.type,t.bits});}
        return true;};
    for(size_t i=0;i<records.size();++i){const auto& record=records[i];
        if((i==0&&(record.parent!=SIZE_MAX||record.child_index))||(i&&record.parent>=i)||
           !positions.emplace(record.parent,record.child_index).second)
            return fail("Duplicate owned original tree order invalid");
        if(record.label.type!=0&&record.label.type!=1&&record.label.type!=2)return fail("Duplicate owned original label type invalid");
        nlohmann::json label,values=nlohmann::json::array();if(!token(record.label,label))return false;
        for(const auto& t:record.tokens){nlohmann::json value;if(!token(t,value))return false;values.push_back(std::move(value));}
        if(record.label.text=="linkId"){
            if(record.tokens.size()!=1||record.tokens[0].type!=4||!record.tokens[0].bits||record.tokens[0].bits>INT32_MAX||
               !definitions.insert(record.tokens[0].bits).second)return fail("Duplicate owned inline definition invalid/duplicate");
        }
        rows.push_back(nlohmann::json::array({record.parent,record.child_index,std::move(label),std::move(values)}));
    }
    try{const auto bytes=rows.dump();sha=mt2mp::sha256(std::vector<unsigned char>(bytes.begin(),bytes.end()));}
    catch(const std::exception& ex){e=std::string("Duplicate owned original record serialization failed: ")+ex.what();return false;}
    return true;
}
inline bool captured(const native::Graph& graph,std::string& sha,std::string& e){
    // A detached Undo record cannot retain the original private pointer map.
    // Its original serializer must have embedded all objects it needs.
    for(const auto& link:graph.private_links)if(!link.has_inline_definition){
        e="Duplicate owned record has an external SaveContext reference id="+std::to_string(link.saved_link_id);return false;}
    return summarize(graph.records,sha,e);
}
}
