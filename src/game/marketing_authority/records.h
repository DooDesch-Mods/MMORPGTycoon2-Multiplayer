#pragma once
#include "state.h"
#include "subscriber_lifecycle/record_graph.h"
#include <bit>
namespace mt2mp::game::marketing_authority {
using Records=std::vector<mt2mp::subscriber_lifecycle::Record>;
using Token=mt2mp::subscriber_lifecycle::Token;
inline Records records(const Json& row,bool award){
    Records out{{SIZE_MAX,0,{1,0,award?"mmoAward":"mmoAdCampaign"},{}}};
    const auto append=[&](const char* name,Token t){out.push_back({0,out.size()-1,{1,0,name},{std::move(t)}});};
    const auto string=[&](const char* key,const std::string& value){append(key,{2,0,value});};
    const auto number=[&](const char* key){append(key,{4,std::bit_cast<uint32_t>(integer(row.at(key))),{}});};
    if(award){string("name",row.at("name").get<std::string>());string("source",row.at("source").get<std::string>());number("day");}
    else {string("customName",row.at("customName").get<std::string>());string("type",campaign_names.at(size_t(integer(row.at("type")))));number("startedOn");number("duration");}
    number("impressions");number("conversions");
    if(award)string("isGood",row.at("isGood").get<bool>()?"true":"false");else number("hype");
    return out;
}
inline bool same_records(const Records& a,const Records& b){
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i){const auto token=[](const Token& x,const Token& y){return x.type==y.type&&x.bits==y.bits&&x.text==y.text;};
        if(a[i].parent!=b[i].parent||a[i].child_index!=b[i].child_index||!token(a[i].label,b[i].label)||a[i].tokens.size()!=b[i].tokens.size())return false;
        for(size_t t=0;t<a[i].tokens.size();++t)if(!token(a[i].tokens[t],b[i].tokens[t]))return false;
    }return true;
}
}
