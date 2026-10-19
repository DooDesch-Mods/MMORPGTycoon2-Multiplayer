#pragma once
#include "service_live_state.h"
#include "service_membership_reader.h"

namespace mt2mp::game::service_live {
inline action_session::ServiceMember effective_row(const action_session::ServiceMembershipState& state,const action_session::ServiceMember& row){
    auto out=row;
    if(!row.flags[0]||!state.config_present||row.config_key.empty())return out;
    const action_session::ServiceConfigValue* config=nullptr;
    for(const auto& value:state.configs)if(value.key==row.config_key){if(config)return out;config=&value;}
    // GetPrice resolves the existing Config and copies its price into the local cache.
    // Missing or ambiguous Configs remain exact; sampling must not create them.
    if(config){out.price=config->price;out.config=true;}
    return out;
}
inline nlohmann::json metadata_fields(const action_session::ServiceMembershipState& state,bool effective=true){
    auto rows=nlohmann::json::array(),characters=nlohmann::json::array(),configs=nlohmann::json::array();
    for(const auto& raw:state.rows){const auto r=effective?effective_row(state,raw):raw;rows.push_back({r.type,r.character,r.index,r.destination,r.config_key,r.price,r.flags,r.prototype,r.config});}
    for(const auto& c:state.characters)characters.push_back({c.id,c.name,c.deleted});
    for(const auto& c:state.configs)configs.push_back({c.key,c.price});
    return nlohmann::json{state.building_definition,state.spawn_type,rows,characters,state.config_present,configs};
}
template<class Hash>
std::string metadata(const action_session::ServiceMembershipState& state,Hash&& hash){return hash(metadata_fields(state).dump());}
template<class Hash>
nlohmann::json sample_diagnostic(const action_session::ServiceMembershipState& state,Hash&& hash){
    const auto raw=metadata_fields(state,false),effective=metadata_fields(state);
    auto rows=nlohmann::json::array();
    for(size_t i=0;i<std::min(size_t{8},state.rows.size());++i){const auto& row=state.rows[i];const auto projected=effective_row(state,row);
        rows.push_back({{"ordinal",i},{"type",row.type},{"flags",row.flags},{"price",row.price},{"effective_price",projected.price},
            {"cached_config",row.config},{"effective_config",projected.config},{"config_key_sha256",hash(row.config_key)}});
    }
    return {{"raw_metadata",hash(raw.dump())},{"effective_metadata",hash(effective.dump())},{"row_count",state.rows.size()},{"rows",rows},
        {"config_present",state.config_present},{"config_count",state.configs.size()},{"configs_sha256",hash(raw[5].dump())},
        {"character_count",state.characters.size()},{"characters_sha256",hash(raw[3].dump())}};
}
template<class Hash>
Building project(const std::string& key,const action_session::ServiceMembershipState& state,const service_membership_reader::Local& local,Hash&& hash){
    Building out;out.key=key;out.metadata=metadata(state,hash);
    for(size_t i=0;i<state.rows.size();++i){Row row;for(size_t j=0;j<2;++j){const auto& from=state.rows[i].counts[j];row.counts[j]={from.today,from.total,local.rows[i].daily[j][4],from.highest,from.history,from.warning};}out.rows.push_back(std::move(row));}return out;
}
}
