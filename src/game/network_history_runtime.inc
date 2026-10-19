#include "network_history_runtime.h"
#include "network_history_graph.h"
#include "building_history_slice/native_bridge.h"
#include "building_history_slice/cash_effect.h"
namespace mt2mp::game::network_history_runtime {
namespace {
namespace a=action_session;namespace nr=ui_network_runtime;namespace region=ui_region_runtime;
struct Source {a::Binding binding;a::Request request;Snapshot before,after;uintptr_t graph=0;a::NetworkHistoryProof proof;bool retired=false;};
struct Operation {a::Binding binding;a::Request request;a::Evidence evidence;Snapshot before,after;int32_t region=-1;bool attempted=false,confirmed=false,retired=false;std::string cause;};
std::map<std::string,Source> sources;std::map<std::string,Operation> operations;
a::Binding selected_binding;std::string selected_snapshot;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool bind(const a::Binding& binding,const std::string& snapshot,std::string& error){
    if(!binding.native_epoch||!binding.native_generation||snapshot.empty())return fail(error,"Network history requires the current native World binding");
    if(selected_binding!=binding||selected_snapshot!=snapshot){sources.clear();operations.clear();selected_binding=binding;selected_snapshot=snapshot;}return true;
}
int32_t region_id(const a::Request& request){return request.action==a::Action::PlaceNetworkNode?request.network.region:request.action==a::Action::PlaceNetworkPipe?request.network_pipe.region:request.network_remove.region;}
std::map<uint64_t,std::string> keys(const Snapshot& snapshot){std::map<uint64_t,std::string> out;for(const auto& [key,row]:snapshot.objects)out.emplace(row.uid,key);return out;}
bool same_members(const Snapshot& actual,const Snapshot& target,std::string& error){
    if(actual.objects.size()!=target.objects.size())return fail(error,"Network history complete canonical membership extent differs");
    for(const auto& [key,row]:actual.objects){const auto found=target.objects.find(key);
        if(found==target.objects.end()||row.kind!=found->second.kind||row.uid!=found->second.uid||row.region!=found->second.region||row.manager!=found->second.manager||row.region_id!=found->second.region_id)return fail(error,"Network history canonical original local UID/owner membership differs");}
    return network_history_graph::shape(actual.topology)==network_history_graph::shape(target.topology)||fail(error,"Network history original complete topology/geometry differs");
}
bool current(const a::Binding& binding,const a::Request& request,int32_t id,const std::map<uint64_t,std::string>& aliases,Snapshot& snapshot,a::Ledger& money,std::string& error){
    auto* runtime=nr::runtime(error);region::LocalView view;connected_network_native::Graph graph;a::NetworkState unused;
    if(!runtime||GetCurrentThreadId()!=runtime->thread)return fail(error,"Network history requires its bound Mainthread runtime");
    return region::identity(*runtime,id,0,view,error)&&region::bound(view,binding,request.snapshot_uuid,error)&&
        nr::graph(*runtime,view,graph,unused,error)&&capture(graph,aliases,snapshot,error)&&region::ledger(money,error);
}
}
bool capture(const connected_network_native::Graph& graph,const std::map<uint64_t,std::string>& born,Snapshot& out,std::string& error){
    out={};Snapshot value;
    const auto add=[&](uintptr_t object,uint64_t uid,std::string& key,std::string& cause){network_identity::ObjectEntry row;
        const auto local=born.find(uid);if(local!=born.end()){key=local->second;if(!network_identity::inspect_history_object(object,uid,row,cause))return false;}
        else if(!network_identity::object_key(object,uid,key,row,cause))return false;
        if(!value.objects.emplace(key,row).second)return fail(cause,"Network history canonical census repeats a key");return true;};
    if(!network_history_graph::canonical(graph,add,value.topology,error))return false;
    auto names=nlohmann::json::array();for(const auto& [key,row]:value.objects){(void)row;names.push_back(key);}
    value.state.graph.nodes=graph.nodes.size();value.state.graph.pipes=graph.pipes;value.state.graph.connections=graph.connections;
    if(!region::sha256(value.topology.dump(),value.state.graph.graph_sha256,error)||!region::sha256(names.dump(),value.state.identities_sha256,error)||!a::valid_network_removal(value.state))return fail(error,error.empty()?"Network history complete canonical values invalid":error);
    out=std::move(value);error.clear();return true;
}
bool record(const a::Binding& binding,const a::Request& request,const Snapshot& before,const Snapshot& after,uintptr_t graph,int32_t amount,std::string& error){
    if(!bind(binding,request.snapshot_uuid,error))return false;
    if((request.action!=a::Action::PlaceNetworkNode&&request.action!=a::Action::PlaceNetworkPipe&&request.action!=a::Action::RemoveNetworkObject)||!graph||amount<0||sources.contains(request.action_uuid))return fail(error,"Network history original source/action/graph/amount invalid or already retained");
    Source value;value.binding=binding;value.request=request;value.before=before;value.after=after;value.graph=graph;
    value.proof={uint8_t(request.action),amount,request.action==a::Action::PlaceNetworkPipe&&request.network_pipe.end_node<0};
    sources.emplace(request.action_uuid,std::move(value));error.clear();return true;
}
bool retire_source(const a::Binding& binding,const a::Request& request,std::string& error){
    const auto found=sources.find(request.action_uuid);if(found==sources.end()||found->second.binding!=binding||found->second.request!=request||found->second.retired)return fail(error,"Network history retirement lacks its exact confirmed original source");
    Snapshot actual;a::Ledger money;if(!current(binding,request,region_id(request),{},actual,money,error)||actual.state!=found->second.after.state)return fail(error,error.empty()?"Network history original complete graph changed before source retirement":error);
    if(!building_history_slice::remember_network(request.action_uuid,found->second.graph,error))return false;
    found->second.retired=true;error.clear();return true;
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error){
    using Outcome=a::Outcome;auto* runtime=nr::runtime(error);
    if(!runtime||!a::network_history(request)||selected_binding!=binding||selected_snapshot!=request.snapshot_uuid||bool(authority)==Config::instance().is_host){if(error.empty())error="Network history current source/direction/authority unavailable";return Outcome::RejectedNoEffect;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Network history exact pending Controller differs";return Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Network history already attempted; no original graph replay";return Outcome::Unknown;}
    const auto found=sources.find(request.target_action_uuid);if(found==sources.end()||!found->second.retired||found->second.binding!=binding||found->second.request.snapshot_uuid!=request.snapshot_uuid){error="Network history confirmed retired original source missing";return Outcome::RejectedNoEffect;}
    const auto& source=found->second;const bool undo=request.action==a::Action::UndoBuilding;
    const auto& old=undo?source.after:source.before;const auto& desired=undo?source.before:source.after;
    const bool destroying=undo!=(source.request.action==a::Action::RemoveNetworkObject);
    Operation value;value.binding=binding;value.request=request;value.region=region_id(source.request);a::Ledger expected;uintptr_t original=0;
    if(!current(binding,request,value.region,{},value.before,value.evidence.before,error)||!same_members(value.before,old,error)||
       !building_history_slice::graph_for(request.action,request.target_action_uuid,original,error)||
       !building_history_slice::cash_effect(value.evidence.before,destroying?source.proof.amount:-source.proof.amount,expected,error))return Outcome::RejectedNoEffect;
    value.evidence.network_removal_before=value.before.state;value.evidence.network_history=source.proof;
    if(authority&&(authority->network_history!=value.evidence.network_history||authority->network_removal_before!=value.evidence.network_removal_before||authority->before!=value.evidence.before||authority->after!=expected)){error="Network history Client complete canonical BEFORE/full Ledger differs from Host";return Outcome::RejectedNoEffect;}
    auto& operation=operations.emplace(request.action_uuid,std::move(value)).first->second;
    const auto unknown=[&](const std::string& cause){error=operation.cause=cause;return Outcome::Unknown;};
    operation.attempted=true;const auto result=building_history_slice::perform(request.action,request.target_action_uuid,error);
    if(result!=Outcome::Confirmed)return unknown(error.empty()?"Network original whole DoGraph did not confirm":error);
    if(!current(binding,request,operation.region,keys(desired),operation.after,operation.evidence.after,error)||!same_members(operation.after,desired,error))return unknown(error);
    for(const auto& [key,row]:operation.before.objects){const auto survivor=operation.after.objects.find(key);if(survivor!=operation.after.objects.end()&&survivor->second!=row)return unknown("Network history surviving full UID/pointer/owner changed");}
    operation.evidence.network_removal_after=operation.after.state;operation.evidence.undo_confirmed=true;
    if(!a::network_history_effect(request,operation.evidence,error))return unknown(error);
    if(authority&&(operation.evidence.after!=authority->after||operation.evidence.network_removal_after!=authority->network_removal_after))return unknown("Network history complete original Client result differs from Host");
    operation.confirmed=true;error.clear();return Outcome::Confirmed;
}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& error){
    const auto found=operations.find(request.action_uuid);if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)return fail(error,"Network history observation lacks the confirmed exact original request");
    const auto& operation=found->second;Snapshot actual;a::Ledger money;
    if(!current(binding,request,operation.region,keys(operation.after),actual,money,error))return false;
    if(actual.state!=operation.after.state||actual.objects!=operation.after.objects||money!=operation.evidence.after)return fail(error,"Network history complete native topology/UIDs/Ledger changed before ACK");
    out=operation.evidence;out.retirement_confirmed=operation.retired;error.clear();return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){
    a::Evidence actual;if(!observe(binding,request,actual,error))return false;auto& operation=operations.at(request.action_uuid);
    if(operation.retired)return fail(error,"Network history original result already retired");
    if(!network_identity::reconcile_history(request.action_uuid,operation.before.objects,operation.after.objects,error))return false;
    operation.retired=true;error.clear();return true;
}
}
