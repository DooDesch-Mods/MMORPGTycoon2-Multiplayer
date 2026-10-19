#pragma once
#include "network_action.h"
#include "../network_object_identity.h"
#include <algorithm>
#include <set>
#include <vector>
namespace mt2mp::action_session {
struct NetworkRemoveIntent {int32_t region=-1;std::string graph_before_sha256;int32_t quoted_refund=0;bool operator==(const NetworkRemoveIntent&)const=default;};
struct NetworkRemoval {NetworkState graph;std::string identities_sha256;bool operator==(const NetworkRemoval&)const=default;};
inline bool valid_network_remove(const NetworkRemoveIntent& value){return value.region>=0&&value.quoted_refund>=0&&game::network_object_identity::sha(value.graph_before_sha256);}
inline bool valid_network_removal(const NetworkRemoval& value){
    return game::network_object_identity::sha(value.graph.graph_sha256)&&game::network_object_identity::sha(value.identities_sha256)&&
        value.graph.nodes<=65536&&value.graph.pipes<=65536;
}
inline bool network_remove_transition(const std::string& target,const NetworkRemoveIntent& intent,const NetworkRemoval& before,const NetworkRemoval& after){
    return game::network_object_identity::key(target)&&valid_network_remove(intent)&&valid_network_removal(before)&&valid_network_removal(after)&&
        before.graph.graph_sha256==intent.graph_before_sha256&&before.graph.graph_sha256!=after.graph.graph_sha256&&
        before.graph.nodes+before.graph.pipes>after.graph.nodes+after.graph.pipes&&before.identities_sha256!=after.identities_sha256&&
        before.graph.nodes>=after.graph.nodes&&before.graph.pipes>=after.graph.pipes;
}
}
