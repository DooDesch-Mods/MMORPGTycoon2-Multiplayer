#pragma once
#include "subscriber_lifecycle/record_graph.h"
#include "duplicate_record_witness.h"
#include "duplicate_record_diagnostic.h"
#include "building_uptime.h"
#include <optional>
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
template<class Owner>bool summarize(const record::Graph& graph,uintptr_t root,Owner&& owner,Witness& out,std::string& error,nlohmann::json* diagnostic=nullptr){
    out={};if(diagnostic)*diagnostic=nullptr;auto fail=[&](const std::string& why){error=why;return false;};
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
    try{const auto original=nlohmann::json::array({std::move(records),std::move(references)});const auto bytes=original.dump();
        out={mt2mp::sha256(std::vector<unsigned char>(bytes.begin(),bytes.end())),graph.records.size(),token_count,ordered.size()};
        if(diagnostic)*diagnostic=diagnose(original.at(0),original.at(1),out);return true;
    }catch(const std::exception& ex){return fail(std::string("Duplicate original complete record serialization failed: ")+ex.what());}
}
struct UptimeWitness {
    std::optional<building_uptime::Values> captured;
    building_uptime::Values actual;
    std::string historical_sha;
};
// The original writer emits Building.uptime.history as an ordered sequence of
// unnamed Boolean int32 leaves. Rebuild only that sequence in a separate graph;
// the actual graph and its complete current hash remain the action evidence.
inline bool historical_uptime(const record::Graph& graph,const std::optional<building_uptime::Values>& captured,
                              building_uptime::Values& actual,record::Graph& historical,std::string& error){
    actual.clear();historical={};auto fail=[&](const char* cause){error=cause;return false;};
    if(graph.records.empty()||graph.records.size()>100000||(captured&&!building_uptime::valid(*captured)))
        return fail("Duplicate Building uptime graph/history budget invalid");
    std::vector<std::vector<size_t>> children(graph.records.size());
    for(size_t i=0;i<graph.records.size();++i){const auto& row=graph.records[i];
        if(!i){if(row.parent!=SIZE_MAX||row.child_index)return fail("Duplicate Building uptime root invalid");}
        else{if(row.parent>=i||row.child_index!=children[row.parent].size())return fail("Duplicate Building uptime tree child order invalid");
            children[row.parent].push_back(i);}
    }
    // The full hash includes BFS parent indices, including inline link owners.
    // Require the original traversal rather than silently reorder a foreign tree.
    std::vector<size_t> order{0};
    for(size_t i=0;i<order.size();++i){if(order[i]!=i)return fail("Duplicate Building uptime original BFS order differs");
        order.insert(order.end(),children[i].begin(),children[i].end());}
    if(order.size()!=graph.records.size())return fail("Duplicate Building uptime graph incomplete");
    auto named=[&](size_t parent,const char* name,size_t& found){found=SIZE_MAX;
        for(auto child:children[parent]){const auto& label=graph.records[child].label;
            if((label.type==1||label.type==2)&&label.text==name){if(found!=SIZE_MAX)return false;found=child;}}
        return found!=SIZE_MAX;
    };
    size_t uptime=0,history=0;
    if(!named(0,"uptime",uptime)||!named(uptime,"history",history)||children[uptime].size()!=1||
       !graph.records[uptime].tokens.empty()||!graph.records[history].tokens.empty()||children[history].size()>10)
        return fail("Duplicate Building uptime named original history shape differs");
    for(auto child:children[history]){const auto& row=graph.records[child];
        if(!children[child].empty()||row.label.type!=0||row.label.bits||!row.label.text.empty()||row.tokens.size()!=1||
           row.tokens[0].type!=4||row.tokens[0].bits>1||!row.tokens[0].text.empty())
            return fail("Duplicate Building uptime original history leaf is not Boolean int32");
        actual.push_back(int32_t(row.tokens[0].bits));
    }
    if(!captured)return true;
    if(graph.records.size()-actual.size()+captured->size()>100000)return fail("Duplicate historical Building record exceeds original graph budget");
    struct Pending {size_t original,parent,index;int32_t bit=0;};
    std::vector<Pending> pending{{0,SIZE_MAX,0}};
    historical.private_links=graph.private_links;
    for(size_t i=0;i<pending.size();++i){const auto item=pending[i];record::Record row;
        if(item.original==SIZE_MAX)row.tokens.push_back({4,uint32_t(item.bit),{}});
        else row=graph.records[item.original];
        row.parent=item.parent;row.child_index=item.index;historical.records.push_back(std::move(row));
        if(item.original==history){for(size_t n=0;n<captured->size();++n)pending.push_back({SIZE_MAX,i,n,(*captured)[n]});}
        else if(item.original!=SIZE_MAX)for(auto child:children[item.original])
            pending.push_back({child,i,graph.records[child].child_index});
    }
    return true;
}
template<class Owner>bool summarize_source(const record::Graph& graph,uintptr_t root,Owner&& owner,Witness& out,
                                          std::string& error,nlohmann::json* diagnostic=nullptr,UptimeWitness* uptime=nullptr){
    if(!summarize(graph,root,owner,out,error,diagnostic))return false;
    if(!uptime)return true;
    uptime->actual.clear();uptime->historical_sha.clear();record::Graph historical;
    if(!historical_uptime(graph,uptime->captured,uptime->actual,historical,error))return false;
    if(uptime->captured){Witness proof;if(!summarize(historical,root,owner,proof,error))return false;
        uptime->historical_sha=std::move(proof.sha);}
    return true;
}
}
