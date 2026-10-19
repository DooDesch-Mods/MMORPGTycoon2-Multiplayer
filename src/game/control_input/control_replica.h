#pragma once
#include "../intervention_state.h"
#include <set>

namespace mt2mp::control_input {
struct ReplicaRow {
    game::interventions::Target actor;
    game::interventions::ControlProof state;
    bool operator==(const ReplicaRow&)const=default;
};
using ReplicaState=std::vector<ReplicaRow>;
inline bool valid(const ReplicaState& rows,std::string& error){
    if(rows.size()>4){error="Control Frame has more than four peer leases";return false;}
    uint64_t prior=0;std::set<std::pair<uint8_t,std::string>> actors;std::set<std::string> ids;
    for(const auto& row:rows){if(!game::interventions::valid(row.actor)||!game::interventions::valid(row.state)||!row.state.controlled||row.state.peer<=prior||
        !actors.emplace(uint8_t(row.actor.family),row.actor.key).second||!ids.insert(row.state.lease).second){error="Control Frame lease ownership/actor/order is invalid";return false;}prior=row.state.peer;}
    error.clear();return true;
}
inline nlohmann::json encode(const ReplicaState& rows){nlohmann::json out=nlohmann::json::array();for(const auto& row:rows)out.push_back({{"actor",game::interventions::encode(row.actor)},{"state",game::interventions::encode(row.state)}});return out;}
inline bool decode(const nlohmann::json& value,ReplicaState& out,std::string& error){
    out.clear();try{if(!value.is_array()||value.size()>4)throw std::runtime_error("Control Frame bounded lease array required");
        for(const auto& row:value){game::interventions::codec::exact(row,{"actor","state"});out.push_back({game::interventions::codec::target(row.at("actor")),game::interventions::codec::control_proof(row.at("state"))});}
        return valid(out,error);
    }catch(const std::exception& e){error=std::string("Control Frame decode: ")+e.what();return false;}
}
}
