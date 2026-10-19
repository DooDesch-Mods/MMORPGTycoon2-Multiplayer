namespace group_object {
namespace {
struct Operation {
    action::Binding binding;action::Request request;Sample before,after;action::Evidence evidence;
    duplicate_edit::HistorySource owned;move_kiss::Receipt move;
    ui::LocalBinding remove_binding;uintptr_t graph=0;int32_t refund=0;
    std::vector<Handle> selection;std::array<uint8_t,9> filter{};uintptr_t camera=0;
    std::array<uint8_t,0x6c> stored_pose{};std::vector<action::DuplicateCash> inverse_cash;
    std::map<uintptr_t,std::array<uint8_t,0x6c>> move_proofs;
    std::map<uintptr_t,std::vector<action::DuplicateCash>> cash_proofs;
    bool attempted=false,returned=false,selection_restored=false,confirmed=false,retired=false;
    uint64_t original_calls=0;std::string cause,cleanup_cause;
};
std::map<std::string,Operation> operations,quarantined;
const ui::Replay* remove_replay=nullptr;
bool moving(const action::Request& request){return request.action==action::Action::MoveGroup;}
bool graph(Runtime& r,const Operation& op,const Sample& current,std::string& e){
    witness::GraphWitness checked;duplicate_history::Graph complete;
    if(!witness::read_graph(r.memory.process,r.image,op.graph,checked,e))return false;
    if(moving(op.request))return group_object_graph::move(r.memory,r.image,op.graph,op.remove_binding.uid,op.stored_pose,complete,e);
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& why){return group_edit::find(r,uid,object,why);};
    auto text=[&](uintptr_t object,std::string& value,std::string& why){return prop_text::text(r,object,value,why);};
    (void)current;
    return group_object_graph::destroy_restore(r.memory,r.image,op.graph,false,op.request.group_object.key,op.owned.known,op.inverse_cash,find,text,complete,e);
}
bool settle_destroy(Runtime& r,Operation& op,std::string& e){
    host::WorldState world_value;duplicate_reader::Census census;
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& why){return group_edit::find(r,uid,object,why);};
    if(!world(r,world_value,e)||!host::same_world(op.before.props.world.origin,world_value.origin)||
       !duplicate_reader::census(r.memory,r.image,uintptr_t(world_value.origin.map),world_value.root,find,census,e))return false;
    std::set<uint64_t> alive;for(const auto& [object,row]:census.objects){(void)object;if(row.family==Family::Scenery)alive.insert(row.handle.uid);}
    std::map<uint64_t,std::string> prior;
    for(const auto& key:op.request.group_object.members){const auto handle=op.before.props.handles.find(key);
        if(handle==op.before.props.handles.end()||alive.contains(handle->second.uid)||!prior.emplace(handle->second.uid,key).second)
            return q::fail(e,"Group original Destroy did not retire every exact ordered member");}
    uintptr_t actual=0;if(!find(op.remove_binding.uid,actual,e)||actual)return q::fail(e,e.empty()?"Group original Destroy retained its full Group UID":e);
    auto retire=[&](const auto& ids,auto& keys,std::string& why){return remove_diagnostics::retire_region_scenery(op.request.snapshot_uuid,op.request.action_uuid,ids,keys,why);};
    auto restore=[](uint64_t,const std::string&,std::string& why){return q::fail(why,"Group original Destroy cannot restore an alias");};
    auto records=[&](std::string& why){return sample(r,op.binding,op.request.group_object,op.after,why);};
    scenery_alias_order::Receipt aliases;return scenery_alias_order::capture(prior,alive,{},retire,restore,records,aliases,e);
}
bool refresh_move(Operation& op,std::string& e){
    for(const auto& key:op.request.group_object.members){const auto handle=op.before.props.handles.find(key);
        if(handle==op.before.props.handles.end()||!remove_diagnostics::refresh_moved_building(op.request.snapshot_uuid,key,handle->second.uid,e))return false;}
    return true;
}
}
bool authorize_remove_replay(const ui::Replay& replay,std::string& e){
    auto* r=published.load(std::memory_order_acquire);const auto found=operations.find(replay.intent.action_uuid);uint64_t revision=0;
    if(!r||!remove_replay||found==operations.end()||!r->ready||r->thread!=GetCurrentThreadId()||
       remove_replay->binding!=replay.binding||remove_replay->intent.action_uuid!=replay.intent.action_uuid||remove_replay->intent.snapshot_uuid!=replay.intent.snapshot_uuid||
       remove_replay->intent.canonical_record_key!=replay.intent.canonical_record_key||remove_replay->purpose!=replay.purpose||!replay.binding.supported_group||
       found->second.request.action!=action::Action::RemoveGroup||found->second.remove_binding!=replay.binding||
       found->second.request.snapshot_uuid!=replay.intent.snapshot_uuid||found->second.request.group_object.key!=replay.intent.canonical_record_key||
       replay.intent.snapshot_uuid!=net::current_snapshot_id()||(replay.purpose==ui::ReplayPurpose::HostExecution)!=Config::instance().is_host||
       !contract::pending(typed_action_runtime::controller_status(),found->second.request,revision,e))
        return q::fail(e,e.empty()?"Group Destroy private replay binding, pending action or role differs":e);
    ui::LocalBinding actual;return native_target(*r,found->second.binding,replay.binding.manager,replay.binding.object,actual,e)&&
        (actual==replay.binding||q::fail(e,"Group Destroy private replay full native binding changed"));
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);uint64_t revision=0;
    if(!r||!r->ready||r->thread!=GetCurrentThreadId()||(request.action!=action::Action::MoveGroup&&request.action!=action::Action::RemoveGroup)||
       request.snapshot_uuid!=net::current_snapshot_id()||bool(authority)==Config::instance().is_host||
       !action::valid_group_object(moving(request),request.group_object)||!contract::pending(typed_action_runtime::controller_status(),request,revision,e)){
        if(e.empty())e="Group exact pending current action or role is unavailable";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)||quarantined.contains(request.action_uuid)){e="Group action already attempted; no replay";return action::Outcome::Unknown;}
    if(authority&&!action::valid_group_object_effect(request,*authority)){e="Group Host complete evidence is invalid";return action::Outcome::RejectedNoEffect;}
    Sample before;move_kiss::Calls calls;
    if(!selection_move::calls(*r,calls,e)||!sample(*r,binding,request.group_object,before,e))return action::Outcome::RejectedNoEffect;
    const auto handle=before.props.handles.find(request.group_object.key);
    const auto group=std::find_if(before.state.ownership.groups.begin(),before.state.ownership.groups.end(),[&](const auto& row){return row.key==request.group_object.key;});
    if(handle==before.props.handles.end()||group==before.state.ownership.groups.end()||group->pose!=request.group_object.before||group->members.size()!=request.group_object.members.size()){
        e="Group original current complete source differs from request";return action::Outcome::RejectedNoEffect;}
    for(size_t index=0;index<group->members.size();++index)if(group->members[index].key!=request.group_object.members[index]){
        e="Group original ordered source members differ from request";return action::Outcome::RejectedNoEffect;}
    if(authority&&(before.state!=authority->group_object_before||before.props.money!=authority->before)){
        e="Group Client complete objects, ownership or Ledger differ from Host";return action::Outcome::RejectedNoEffect;}
    if(!group_object_calls::binding(r->memory,r->image,handle->second.object,e))return action::Outcome::RejectedNoEffect;
    Operation value;value.binding=binding;value.request=request;value.before=std::move(before);
    if(!native_target(*r,binding,value.before.props.world.selection,handle->second.object,value.remove_binding,e)||
       !q::at(r->memory,handle->second.object,0x100,value.stored_pose,e)||
       !duplicate_edit::history_local_selection(*r,value.before.props,value.selection,value.filter,value.camera,e))return action::Outcome::RejectedNoEffect;
    if(!moving(request)){
        duplicate_edit::Sample selected;selected.world=value.before.props.world;selected.state.created=value.before.state.records.sources;
        for(const auto& row:selected.state.created)selected.handles.emplace(row.key,value.before.props.handles.at(row.key));
        if(!ui_remove_runtime::enabled()||!duplicate_edit::history_known(*r,selected,value.owned,e))return action::Outcome::RejectedNoEffect;
        const auto code=group_object_calls::cost(r->image,handle->second.object,value.refund);
        if(code||value.refund<0){e="Group original aggregate native Cost failed SEH="+std::to_string(code);return action::Outcome::RejectedNoEffect;}
        value.inverse_cash={{-value.refund,{request.group_object.before[4],request.group_object.before[5],request.group_object.before[6]}}};
    }else value.owned.known.emplace(handle->second.uid,duplicate_history::Known{});
    auto& op=operations.emplace(request.action_uuid,std::move(value)).first->second;
    auto unknown=[&](const std::string& why){e=op.cause=why;std::string cleanup;
        if(!op.selection_restored){if(duplicate_edit::history_restore(*r,op.before.props,op.selection,op.filter,op.camera,op.owned,cleanup))op.selection_restored=true;
            else{op.cleanup_cause=cleanup;e=op.cause+="; cleanup: "+cleanup;}}return action::Outcome::Unknown;};
    op.evidence.before=op.before.props.money;op.evidence.group_object_before=op.before.state;
    op.attempted=true;
    if(moving(request)){
        std::array<unsigned char,0x6c> pose;
        if(!group_object_move_reader::transform(request.group_object.after,pose,e)||!move_kiss::execute_once(calls,op.remove_binding.object,pose,op.move,e))return unknown(e);
        op.graph=op.move.graph;op.original_calls=1;
    }else{
        ui::Replay replay{authority?ui::ReplayPurpose::ConfirmedHostApplication:ui::ReplayPurpose::HostExecution,
            {request.action_uuid,request.snapshot_uuid,request.group_object.key},op.remove_binding};
        if(remove_replay)return unknown("Group Destroy private replay is reentrant");
        const auto calls_before=ui_remove_runtime::status().value("replay_originals",uint64_t{});bool done=false;
        {remove_replay=&replay;struct Reset{~Reset(){remove_replay=nullptr;}} reset;
            done=ui_remove_runtime::with_replay(replay,[&](std::string& why){const auto code=group_object_calls::destroy(r->image,op.remove_binding.manager,op.remove_binding.object);
                return !code||q::fail(why,"Group original DestroyProp SEH="+std::to_string(code));},e);}
        const auto calls_after=ui_remove_runtime::status().value("replay_originals",uint64_t{});op.original_calls=calls_after>=calls_before?calls_after-calls_before:0;
        if(!done||op.original_calls!=1)return unknown(e.empty()?"Group Destroy expected one returned original DestroyProps":e);
        witness::UndoWitness undo;if(!witness::read_undo(r->memory.process,r->image,undo,e)||undo.undo.empty())return unknown(e.empty()?"Group original Destroy has no inverse graph":e);op.graph=undo.undo.back();
    }
    op.returned=true;
    if(!graph(*r,op,op.before,e))return unknown(e);
    if(moving(request)){if(!refresh_move(op,e)||!sample(*r,binding,request.group_object,op.after,e))return unknown(e);}
    else if(!settle_destroy(*r,op,e))return unknown(e);
    witness::GraphWitness committed;
    if(!same_binding(op.before,op.after)||!witness::read_graph(r->memory.process,r->image,op.graph,committed,e)||
       !witness::validate_undo_commit(op.before.props.undo,op.after.props.undo,committed,e))return unknown(e.empty()?"Group original complete graph commit or World binding differs":e);
    if(!duplicate_edit::history_restore(*r,op.before.props,op.selection,op.filter,op.camera,op.owned,e))return unknown(e);op.selection_restored=true;
    op.evidence.after=op.after.props.money;op.evidence.group_object_after=op.after.state;op.evidence.duplicate_cash=op.inverse_cash;
    op.evidence.undo_confirmed=op.evidence.selection_confirmed=true;op.evidence.target_retired=!moving(request);
    if(!action::valid_group_object_effect(request,op.evidence)||(authority&&(op.evidence.group_object_after!=authority->group_object_after||op.evidence.after!=authority->after||
       !duplicate_history::cash_equal(op.evidence.duplicate_cash,authority->duplicate_cash))))return unknown("Group actual original complete result differs from Host/request");
    if(moving(request))op.move_proofs.emplace(op.graph,op.stored_pose);else op.cash_proofs.emplace(op.graph,op.inverse_cash);
    trim(op.before);trim(op.after);op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);const auto found=operations.find(request.action_uuid);
    if(!r||r->thread!=GetCurrentThreadId()||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)
        return q::fail(e,"Group observation lacks its exact confirmed original operation");
    const auto& op=found->second;Sample current;
    if(!sample(*r,binding,request.group_object,current,e)||!same_binding(op.after,current)||current.state!=op.after.state||
       current.props.money!=op.after.props.money||current.props.undo!=op.after.props.undo||!graph(*r,op,current,e))
        return q::fail(e,e.empty()?"Group actual complete state or original inverse changed before ACK":e);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
}
