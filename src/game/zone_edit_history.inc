namespace zone_edit_runtime {
namespace {
bool history_source(const a::Binding& binding,const a::Request& request,Operation*& source,std::string& error){
    const auto found=operations.find(request.target_action_uuid);
    if((request.action!=a::Action::UndoBuilding&&request.action!=a::Action::RedoBuilding)||
       request.canonical_record_key!="zone-edit:"+request.target_action_uuid||found==operations.end()||
       found->second.binding!=binding||found->second.request.snapshot_uuid!=request.snapshot_uuid||
       !found->second.confirmed||!found->second.retired||!found->second.graph.native.pointer)
        return fail(error,"Zone history lacks its exact confirmed retired original operation");
    source=&found->second;return true;
}
}
a::Outcome perform_history(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    Operation* source=nullptr;uint64_t revision=0;
    if(!ready||capture_fault||GetCurrentThreadId()!=za::state.thread||request.snapshot_uuid!=net::current_snapshot_id()||
       bool(authority)==Config::instance().is_host||!history_source(binding,request,source,error)||
       !ui_move_runtime::contract::pending(typed_action_runtime::controller_status(),request,revision,error)){
        if(error.empty())error="Zone history current Controller/role/owner differs";return a::Outcome::RejectedNoEffect;}
    if(history_operations.contains(request.action_uuid)){error="Zone history original already entered; replay forbidden";return a::Outcome::Unknown;}
    Memory memory;Sample before;uintptr_t pointer=0;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,source->region,request.action_uuid,before,error)||
       !building_history_slice::graph_for(request.action,request.target_action_uuid,pointer,error))return a::Outcome::RejectedNoEffect;
    const auto found=source->inverses.find(pointer);
    if(found==source->inverses.end()||!graph_unchanged(memory,found->second,error)){
        if(error.empty())error="Zone history current inverse differs from retained complete graph";return a::Outcome::RejectedNoEffect;}
    const auto consumed=found->second;
    if(!sample(memory,binding,request.snapshot_uuid,source->region,request.action_uuid,before,error,
        nullptr,nullptr,&consumed.quests))return a::Outcome::RejectedNoEffect;
    if(authority){auto comparable=before.value;
        comparable.global_random=authority->zone_edit?authority->zone_edit->before.global_random:std::array<uint64_t,4>{};
        if(!a::valid_zone_edit_evidence(request,*authority)||comparable!=authority->zone_edit->before||before.money!=authority->before){
            error="Zone history Client complete current state/Ledger differs from Host";return a::Outcome::RejectedNoEffect;}}
    if(!a::zone_edit_wire_fits(request,before.money,before.value,error))return a::Outcome::RejectedNoEffect;
    std::map<uint64_t,identity::Current> restored;
    for(const auto& node:consumed.native.nodes)if(node.kind==cg::Kind::Zone||node.kind==cg::Kind::EditZone){
        const auto known=source->known_zones.find(node.uid);
        if(known==source->known_zones.end()){error="Zone original Make/Edit lacks its retained full UID alias";return a::Outcome::RejectedNoEffect;}
        restored.emplace(node.uid,known->second);
    }
    auto& operation=history_operations.try_emplace(request.action_uuid).first->second;
    operation.binding=binding;operation.request=request;operation.region=source->region;operation.known_zones=source->known_zones;
    const auto unknown=[&](){replaying=false;operation.cause=error.empty()?"Zone history failed without cause":error;error=operation.cause;return a::Outcome::Unknown;};
    if(authority){const auto region_random=before.value.region_random;operation.rng_attempted=true;
        if(!za::native_ok(action_runtime::seed(za::state.image,authority->zone_edit->before.global_random),"history Host RNG adoption",error))return unknown();
        operation.rng_returned=true;
        if(!sample(memory,binding,request.snapshot_uuid,operation.region,request.action_uuid,before,error,nullptr,nullptr,&consumed.quests)||
           before.value.region_random!=region_random||before.value!=authority->zone_edit->before)return unknown();}
    operation.before=std::move(before);
    if(!operation.selection.capture(memory,operation.before.world.selection,error)){operation.cause=error;return a::Outcome::RejectedNoEffect;}
    operation.attempted=true;replaying=true;
    const auto result=building_history_slice::perform(request.action,request.target_action_uuid,error);
    replaying=false;if(result!=a::Outcome::Confirmed)return unknown();
    operation.returned=true;uintptr_t inverse=0;
    const auto opposite=request.action==a::Action::UndoBuilding?a::Action::RedoBuilding:a::Action::UndoBuilding;
    // Original DoCash(0) has no effect and emits no inverse Cash operation.
    if(!building_history_slice::graph_for(opposite,request.target_action_uuid,inverse,error)||
       !finish_result(memory,operation,authority,inverse,0,restored,error,&consumed))return unknown();
    source->known_zones=operation.known_zones;source->inverses[inverse]=operation.graph;
    return a::Outcome::Confirmed;
}catch(const std::exception& exception){replaying=false;error=std::string("Zone original history boundary: ")+exception.what();return a::Outcome::Unknown;}
catch(...){replaying=false;error="Zone original history non-standard boundary exception";return a::Outcome::Unknown;}
bool observe_history(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& error){
    Operation* source=nullptr;const auto found=history_operations.find(request.action_uuid);
    if(GetCurrentThreadId()!=za::state.thread||!history_source(binding,request,source,error)||found==history_operations.end()||
       found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)
        return fail(error,error.empty()?"Zone history observation lacks exact confirmed original receipt":error);
    const auto opposite=request.action==a::Action::UndoBuilding?a::Action::RedoBuilding:a::Action::UndoBuilding;
    uintptr_t graph=0;
    if(!building_history_slice::graph_for(opposite,request.target_action_uuid,graph,error)||graph!=found->second.graph.native.pointer)
        return fail(error,error.empty()?"Zone history opposite graph changed before ACK":error);
    Memory memory;return observed(memory,found->second,out,error);
}
bool retire_history(const a::Binding& binding,const a::Request& request,std::string& error){
    auto out=std::make_unique<a::Evidence>();if(!observe_history(binding,request,*out,error))return false;auto& operation=history_operations.at(request.action_uuid);
    if(operation.retired)return fail(error,"Zone original history receipt already retired");operation.retired=true;return true;
}
}
