namespace district_stroke {
namespace {
bool history_source(const action::Binding& binding,const action::Request& request,Operation*& out,std::string& error){
    const auto found=operations.find(request.target_action_uuid);
    if((request.action!=action::Action::UndoBuilding&&request.action!=action::Action::RedoBuilding)||
       request.canonical_record_key!="district-stroke:"+request.target_action_uuid||found==operations.end()||
       found->second.binding!=binding||found->second.request.snapshot_uuid!=request.snapshot_uuid||
       !found->second.confirmed||!found->second.retired)
        return q::fail(error,"District history lacks its exact confirmed retired stroke");
    out=&found->second;return true;
}
bool restored_keys(const Operation& source,const inverse::Graph& graph,
                   std::map<int32_t,std::map<uint64_t,std::string>>& out,std::string& error){
    for(const auto& node:graph.nodes)if(node.rva==0x17132f0){
        const auto found=source.keys.find(node.uid);
        if(found==source.keys.end())return q::fail(error,"District history Make full UID has no retained canonical owner");
        if(!district_lifecycle::valid_key(found->second))continue;
        const auto old=source.before.regions.find(node.stored.region),born=source.after.regions.find(node.stored.region);
        if(old==source.before.regions.end()&&born==source.after.regions.end())return q::fail(error,"District history Make Region is not retained");
        const auto region=old!=source.before.regions.end()?old->second:born->second;
        out[region].emplace(node.uid,found->second);
    }return true;
}
}
action::Outcome perform_history(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& error){
    auto* runtime=published.load(std::memory_order_acquire);Operation* original=nullptr;uint64_t revision=0;
    if(!runtime||!runtime->ready||!installed||runtime->thread!=GetCurrentThreadId()||request.snapshot_uuid!=net::current_snapshot_id()||
       bool(authority)==Config::instance().is_host||!history_source(binding,request,original,error)||!code(*runtime,error)||
       !contract::pending(typed_action_runtime::controller_status(),request,revision,error)){
        if(error.empty())error="District history current request/role/Controller unavailable";return action::Outcome::RejectedNoEffect;}
    if(history_operations.contains(request.action_uuid)){error="District history already attempted; no replay";return action::Outcome::Unknown;}
    Snapshot before;uintptr_t pointer=0;inverse::Graph graph;
    if(!snapshot(*runtime,binding,before,error)||!building_history_slice::graph_for(request.action,request.target_action_uuid,pointer,error)||
       !inverse::read(runtime->memory,runtime->image,pointer,graph,error))return action::Outcome::RejectedNoEffect;
    uintptr_t region_pointer=0;if(!resolve_region(*runtime,before,original->region,region_pointer,error))return action::Outcome::RejectedNoEffect;
    before.regions.emplace(region_pointer,original->region);
    const auto retained=original->inverses.find(pointer);
    if(retained==original->inverses.end()||retained->second!=graph){error="District history original graph differs from retained inverse";return action::Outcome::RejectedNoEffect;}
    if(authority&&(!action::valid_district_stroke_effect(request,*authority)||authority->district_stroke->before!=before.wire||authority->before!=before.money)){
        error="District history Client complete current values/Ledger differ from Host";return action::Outcome::RejectedNoEffect;}
    if(!action::district_stroke_wire_fits(request,before.money,before.wire,error))return action::Outcome::RejectedNoEffect;
    std::map<int32_t,std::map<uint64_t,std::string>> restored;
    if(!restored_keys(*original,graph,restored,error))return action::Outcome::RejectedNoEffect;
    auto& value=history_operations.try_emplace(request.action_uuid).first->second;
    value.binding=binding;value.request=request;value.before=std::move(before);value.keys=original->keys;value.region=original->region;
    auto failed=[&](){value.cause=error;return action::Outcome::Unknown;};
    if(!value.selection.capture(*runtime,value.before.world.selection,error)||!identity::prepare_edit(request.action_uuid,value.edit,error)){
        value.cause=error;return action::Outcome::RejectedNoEffect;}
    value.attempted=true;
    if(building_history_slice::perform(request.action,request.target_action_uuid,error)!=action::Outcome::Confirmed)return failed();
    value.returned=true;uintptr_t opposite_graph=0;
    const auto direction=request.action==action::Action::UndoBuilding?action::Action::RedoBuilding:action::Action::UndoBuilding;
    if(!building_history_slice::graph_for(direction,request.target_action_uuid,opposite_graph,error)||
       !after_original(*runtime,value,authority,opposite_graph,restored,error,&graph))return failed();
    original->inverses[value.graph.pointer]=value.graph;
    if(!merge_keys(value.keys,original->keys,error)){value.confirmed=false;return failed();}
    return action::Outcome::Confirmed;
}
bool observe_history(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error){
    auto* runtime=published.load(std::memory_order_acquire);Operation* original=nullptr;const auto found=history_operations.find(request.action_uuid);
    if(!runtime||found==history_operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||
       !history_source(binding,request,original,error))return q::fail(error,error.empty()?"District history lacks exact confirmed receipt":error);
    uintptr_t graph=0;const auto direction=request.action==action::Action::UndoBuilding?action::Action::RedoBuilding:action::Action::UndoBuilding;
    if(!building_history_slice::graph_for(direction,request.target_action_uuid,graph,error)||graph!=found->second.graph.pointer)
        return q::fail(error,error.empty()?"District history current opposite graph differs":error);
    return observed(*runtime,found->second,out,error);
}
bool retire_history(const action::Binding& binding,const action::Request& request,std::string& error){
    action::Evidence value;if(!observe_history(binding,request,value,error))return false;
    auto& operation=history_operations.at(request.action_uuid);
    if(operation.retired)return q::fail(error,"District history already retired");operation.retired=true;return true;
}
}
