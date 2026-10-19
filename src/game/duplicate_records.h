#pragma once
#include "subscriber_lifecycle/record_graph.h"
#include "duplicate_record_witness.h"
#include "mt2mp/core/hash.h"
#include "vendor/nlohmann/json.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace mt2mp::game::duplicate_records {
namespace record=subscriber_lifecycle;
// SaveContext allocates IDs from one in PrepareSave traversal order. Preserve
// every original token, including ordinary integer values equal to link IDs.
// Only the separate pointer manifest is represented with canonical owners.
template<class Owner>bool summarize(const record::Graph& graph,uintptr_t root,Owner&& owner,Witness& out,std::string& error){
    out={};auto fail=[&](const std::string& why){error=why;return false;};
    if(graph.records.empty()||!root)return fail("Duplicate original record/root absent");
    nlohmann::json records=nlohmann::json::array(),references=nlohmann::json::array();
    std::map<int32_t,size_t> definitions;
    std::set<std::pair<size_t,size_t>> positions;
    size_t token_count=0;
    auto token=[&](const record::Token& t,nlohmann::json& result){
        if(t.type<0||t.type>4)return fail("Duplicate original record token type invalid");
        if(t.type==1||t.type==2){result=nlohmann::json::array({t.type,t.text});return true;}
        if(!t.text.empty())return fail("Duplicate scalar token contains foreign text");
        result=nlohmann::json::array({t.type,t.bits});return true;
    };
    for(size_t i=0;i<graph.records.size();++i){const auto& r=graph.records[i];
        if((i==0&&(r.parent!=SIZE_MAX||r.child_index))||(i&&r.parent>=i)||
           !positions.emplace(r.parent,r.child_index).second)return fail("Duplicate original record tree/order invalid");
        nlohmann::json label,values=nlohmann::json::array();if(!token(r.label,label))return false;
        for(const auto& t:r.tokens){nlohmann::json value;if(!token(t,value))return false;values.push_back(std::move(value));++token_count;}
        if((r.label.type==1||r.label.type==2)&&r.label.text=="linkId"){
            if(r.tokens.size()!=1||r.tokens[0].type!=4||!r.tokens[0].bits||r.tokens[0].bits>INT32_MAX||
               !definitions.emplace(int32_t(r.tokens[0].bits),r.parent).second)return fail("Duplicate inline link definition invalid/duplicate");
        }
        records.push_back(nlohmann::json::array({i?r.parent:SIZE_MAX,r.child_index,std::move(label),std::move(values)}));
    }
    std::map<int32_t,const record::PrivateLink*> ordered;std::set<uintptr_t> objects;
    for(const auto& link:graph.private_links){
        if(link.saved_link_id<=0||!link.object||!ordered.emplace(link.saved_link_id,&link).second||!objects.insert(link.object).second)
            return fail("Duplicate original SaveContext link owner/ID repeats");
        if(link.has_inline_definition!=definitions.contains(link.saved_link_id))return fail("Duplicate original inline link manifest differs");
    }
    if(ordered.size()<definitions.size())return fail("Duplicate inline definition has no original SaveContext owner");
    int32_t expected=1;
    for(const auto& [id,link]:ordered){
        if(id!=expected++)return fail("Duplicate original SaveContext IDs are not complete from one");
        if(link->has_inline_definition)references.push_back(nlohmann::json::array({id,"inline",definitions.at(id)}));
        else if(link->object==root)references.push_back(nlohmann::json::array({id,"self"}));
        else{std::string key;if(!owner(link->object,key,error))return false;
            if(key.empty()||key.size()>4096)return fail("Duplicate external reference canonical owner invalid id="+std::to_string(id));
            references.push_back(nlohmann::json::array({id,"external",std::move(key)}));}
    }
    try{const auto bytes=nlohmann::json::array({std::move(records),std::move(references)}).dump();
        out={mt2mp::sha256(std::vector<unsigned char>(bytes.begin(),bytes.end())),graph.records.size(),token_count,ordered.size()};return true;
    }catch(const std::exception& ex){return fail(std::string("Duplicate original complete record serialization failed: ")+ex.what());}
}
}
