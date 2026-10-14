#include "observer_mcp.h"
#include "node_observer.h"
#include "observer_hooks.h"
namespace mt2mp::game::node_observer {
namespace {
using json=nlohmann::json;
bool uuid(const std::string& s){if(s.size()!=36)return false;for(size_t i=0;i<s.size();++i){
    if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}
    else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;}return true;}
bool digest(const std::string& s){if(s.size()!=64)return false;for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
bool int32(const json& v){return v.is_number_integer()&&(v.is_number_unsigned()?v.get<uint64_t>()<=INT32_MAX:(v.get<int64_t>()>=0&&v.get<int64_t>()<=INT32_MAX));}
json observation(const Observation& o){
    json out={{"sequence",o.sequence},{"thread",o.thread},{"caller_rva",o.caller_rva},
        {"argument_present",o.argument_present},{"original_returned",o.original_returned},
        {"original_fault",o.original_fault},{"before_present",o.before_present},{"after_present",o.after_present},
        {"read_before_fault",o.read_before_fault},{"read_after_fault",o.read_after_fault}};
    if(o.argument_present)out["argument"]=o.argument;
    auto fields=[](const Scalars& s){return json{{"current",s.values[0]},{"last_current",s.values[1]},
        {"pending_delta",s.values[2]},{"startup_timer",s.values[3]},{"provides_to_pipes",s.flags[0]!=0},
        {"provides_supply",s.flags[1]!=0},{"activated",s.flags[2]!=0}};};
    if(o.before_present)out["before"]=fields(o.before);if(o.after_present)out["after"]=fields(o.after);return out;
}
json status(const Status& s){json leaves=json::array();constexpr const char* names[]={"presentation","network","finish","drain","charge"};
    for(size_t i=0;i<s.leaves.size();++i){const auto& l=s.leaves[i];leaves.push_back({{"leaf",names[i]},
        {"entered",l.entered},{"returned",l.returned},{"original_failed",l.original_failed},{"foreign_thread",l.foreign_thread},
        {"read_failed",l.read_failed},{"changed",l.changed},{"first",observation(l.first)},{"last",observation(l.last)}});}
    return {{"prepared",s.prepared},{"bound",s.bound},{"fresh_owner_confirmed",s.fresh_owner_confirmed},
        {"main_thread",s.main_thread},{"hook_mask",s.hook_mask},{"binding_revision",s.binding_revision},
        {"action_uuid",s.action_uuid},{"snapshot_uuid",s.snapshot},{"inflight",s.inflight},{"cause",s.cause},
        {"snapshot_binding",s.snapshot_binding},{"region_id",s.region_id},{"node_index",s.node_index},
        {"rejected",s.rejected},{"foreign_receivers",s.foreign_receivers},{"invalid_leaf",s.invalid_leaf},
        {"ticket_exhausted",s.ticket_exhausted},{"observation_capacity",s.observation_capacity},
        {"unknown_ticket",s.unknown_ticket},{"invalid_caller",s.invalid_caller},{"leaves",leaves},
        {"original_policy","always_forward_once"},{"native_state_writes",0},{"simulation_suppressed",false}};
}
}
bool dispatch(const std::string& op,const json& args,json& result,std::string& e){
    result=json::object();e.clear();if(!args.is_object()){e="node observer arguments must be an object";return false;}
    if(op=="enable_node_observer"){
        const bool action=args.size()==1&&args.contains("action_uuid")&&args.at("action_uuid").is_string()&&uuid(args.at("action_uuid").get<std::string>());
        const bool snapshot=args.size()==4&&args.contains("snapshot_uuid")&&args.at("snapshot_uuid").is_string()&&uuid(args.at("snapshot_uuid").get<std::string>())&&
            args.contains("region_id")&&int32(args.at("region_id"))&&args.contains("node_index")&&int32(args.at("node_index"))&&
            args.contains("graph_sha256")&&args.at("graph_sha256").is_string()&&digest(args.at("graph_sha256").get<std::string>());
        if(!action&&!snapshot){e="node observer enable requires canonical action_uuid or exact snapshot_uuid/region_id/node_index/graph_sha256";return false;}
        if(!install(e)){result["cause"]=e;return false;}
        const bool bound=action?bind_action(args.at("action_uuid").get<std::string>(),e):
            bind_snapshot(args.at("snapshot_uuid").get<std::string>(),args.at("region_id").get<int32_t>(),args.at("node_index").get<int32_t>(),args.at("graph_sha256").get<std::string>(),e);
        if(!bound){result["cause"]=e;return false;}
    }else if(op!="read_node_observer"||!args.empty()){e="node observer read requires empty args; operation must be enable/read";return false;}
    Status s;const bool okay=read_status(s,e);result=status(s);if(!okay)result["cause"]=e;return okay;
}
}
