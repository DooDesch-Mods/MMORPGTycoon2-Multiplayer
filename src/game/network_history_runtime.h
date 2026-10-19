#pragma once
#include "typed_action/action_session.h"
#include "network_identity.h"
#include "connected_network_reader.h"
namespace mt2mp::game::network_history_runtime {
struct Snapshot {
    std::map<std::string,network_identity::ObjectEntry> objects;
    nlohmann::json topology=nlohmann::json::array();
    action_session::NetworkRemoval state;
};
bool capture(const connected_network_native::Graph&,const std::map<uint64_t,std::string>& born,Snapshot&,std::string&);
bool record(const action_session::Binding&,const action_session::Request&,const Snapshot&,const Snapshot&,uintptr_t graph,int32_t amount,std::string&);
bool retire_source(const action_session::Binding&,const action_session::Request&,std::string&);
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
}
