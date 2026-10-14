#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::ui_network_runtime {
struct Admission {
    action_session::Binding binding;
    std::string snapshot,action_key,type,key;
    int32_t region_id=-1;
    uint64_t local_uid=0;
    uintptr_t object=0,manager=0,region=0,definition=0;
    std::string topology_sha256;
};
bool admitted_node(const std::string& action_uuid,Admission&,std::string&);
bool admitted_snapshot_node(const std::string& snapshot,int32_t region_id,int32_t node_index,const std::string& expected_graph_sha256,Admission&,std::string&);
bool refresh_snapshot_node(const Admission&,Admission&,std::string&);
bool enable(std::string&);
bool enabled()noexcept;
void tick();
bool unresolved();
nlohmann::json status();
bool query(const nlohmann::json&,nlohmann::json&,std::string&);
bool native_pipe(const nlohmann::json&,nlohmann::json&,std::string&);
bool native_place(const nlohmann::json&,nlohmann::json&,std::string&);
action_session::Outcome perform(const action_session::Binding&,const action_session::Request&,const action_session::Evidence*,std::string&);
bool observe(const action_session::Binding&,const action_session::Request&,action_session::Evidence&,std::string&);
bool retire(const action_session::Binding&,const action_session::Request&,std::string&);
}
