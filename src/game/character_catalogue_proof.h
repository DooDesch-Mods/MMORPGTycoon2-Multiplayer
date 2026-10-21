#pragma once
#include "character_definition_identity.h"
#include "vendor/nlohmann/json.hpp"
#include <algorithm>

namespace mt2mp::game::character_catalogue_proof {
namespace definitions=character_definition_identity;
inline bool rows(const definitions::Census& census,const std::map<std::string,std::string>& records,
                 nlohmann::json& out,std::string& error){
    nlohmann::json result=nlohmann::json::array();std::array<int32_t,5> next{};size_t live=0;
    for(const auto& definition:census.definitions){
        if(definition.kind<0||definition.kind>=5||definition.id!=next[size_t(definition.kind)]++||
           definition.key!=definitions::key(definition.kind,definition.id))
            return definitions::fail(error,"Character catalogue original kind/global ID/order differs");
        int32_t parent=-1;std::vector<int32_t> variants;
        if(!definition.deleted&&definition.parent){
            const auto found=std::find_if(census.definitions.begin(),census.definitions.end(),[&](const auto& row){return row.object==definition.parent;});
            if(found==census.definitions.end()||found->uid!=definition.parent_uid||found->kind!=definition.kind||found->deleted)
                return definitions::fail(error,"Character catalogue current parent full UID/kind differs");
            parent=found->id;
        }
        for(const auto& collection:census.collections)if(collection.owner==definition.object)
            for(const auto& id:collection.ids){
                if(id[0]!=definition.kind)return definitions::fail(error,"Character catalogue variant belongs to another native kind");
                variants.push_back(id[1]);
            }
        std::string record;
        const auto found=records.find(definition.key);
        if(!definition.deleted){
            if(found==records.end()||found->second.size()!=64||
               !std::all_of(found->second.begin(),found->second.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))
                return definitions::fail(error,"Character catalogue live definition lacks its complete original Record SHA256");
            record=found->second;++live;
        }else if(found!=records.end())return definitions::fail(error,"Character catalogue tombstone has a fabricated live Record");
        result.push_back({{"kind",definition.kind},{"id",definition.id},{"parent",parent},{"variants",variants},
            {"deleted",definition.deleted},{"edited",definition.edited},{"released",definition.released},{"record_sha256",record}});
    }
    for(size_t kind=0;kind<next.size();++kind)
        if(next[kind]!=census.arrays[kind].count||census.arrays[kind].members.size()!=size_t(next[kind]))
            return definitions::fail(error,"Character catalogue complete original pool count differs");
    if(records.size()!=live)return definitions::fail(error,"Character catalogue contains a foreign complete Record");
    out=std::move(result);error.clear();return true;
}
}
