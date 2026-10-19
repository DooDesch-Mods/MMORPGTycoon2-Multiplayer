namespace group_object {
namespace {
struct HistoryOperation {
    action::Binding binding;action::Request request;action::Evidence evidence;Sample after;
    uintptr_t graph=0,inverse=0;bool attempted=false,returned=false,selection_restored=false,confirmed=false,retired=false;
    bool move_kind=false,restoring=false;std::string cause,cleanup_cause;
};
std::map<std::string,HistoryOperation> history_operations,history_quarantined;
bool history_request(const action::Request& request){return (request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&
    request.canonical_record_key=="group-object:"+request.target_action_uuid;}
bool history_source(const action::Binding& binding,const action::Request& request,Operation*& out,std::string& e){
    const auto found=operations.find(request.target_action_uuid);
    if(!history_request(request)||found==operations.end()||found->second.binding!=binding||found->second.request.snapshot_uuid!=request.snapshot_uuid||
       !found->second.confirmed||!found->second.retired)return q::fail(e,"Group history lacks its exact confirmed retired original source");out=&found->second;return true;
}
bool live_known(Runtime& r,const Sample& sample,duplicate_edit::HistorySource& owned,std::string& e){
    duplicate_edit::Sample subset;subset.world=sample.props.world;subset.state.created=sample.state.records.sources;
    for(const auto& row:subset.state.created){const auto found=sample.props.handles.find(row.key);if(found==sample.props.handles.end())return q::fail(e,"Group live original Record lacks its native full UID");subset.handles.emplace(*found);}
    return duplicate_edit::history_known(r,subset,owned,e);
}
bool restored_pointers(Runtime& r,const duplicate_edit::HistorySource& original,duplicate_edit::HistorySource& out,std::string& e){
    out=original;
    for(auto& [uid,saved]:out.known){uintptr_t actual=0,root=0,region=0;uint64_t current_uid=0;
        if(!group_edit::find(r,uid,actual,e)||!actual||!q::typed(r.memory,r.image,actual,saved.row.family==Family::Group?0x1749de0:0x16c1390,e)||
           !q::at(r.memory,actual,0x18,current_uid,e)||current_uid!=uid||!q::at(r.memory,actual,0x170,root,e)||root!=saved.root||
           !q::at(r.memory,actual,0x178,region,e)||region!=saved.region)return q::fail(e,e.empty()?"Group restored original full UID/root/Region differs":e);
        saved.object=actual;
    }
    return true;
}
bool settle_restoration(Runtime& r,const Operation& source,const action::Request& request,const Sample& before,Sample& after,std::string& e){
    host::WorldState world_value;duplicate_reader::Census census;
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& why){return group_edit::find(r,uid,object,why);};
    if(!world(r,world_value,e)||!host::same_world(before.props.world.origin,world_value.origin)||
       !duplicate_reader::census(r.memory,r.image,uintptr_t(world_value.origin.map),world_value.root,find,census,e))return false;
    std::set<uint64_t> alive;for(const auto& [object,row]:census.objects){(void)object;if(row.family==Family::Scenery)alive.insert(row.handle.uid);}
    std::map<uint64_t,std::string> restorable;
    for(const auto& [uid,saved]:source.owned.known)if(saved.row.family==Family::Scenery){if(!alive.contains(uid))return q::fail(e,"Group native Make omitted an original member");restorable.emplace(uid,saved.row.key);}
    auto retire=[](const auto&,auto&,std::string& why){return q::fail(why,"Group native restoration cannot retire an unrelated alias");};
    auto restore=[&](uint64_t uid,const std::string& key,std::string& why){const auto found=source.owned.known.find(uid);
        if(found==source.owned.known.end()||found->second.row.key!=key)return q::fail(why,"Group restored canonical member differs");
        const auto& expected=found->second;uintptr_t object=0;scenery_make_history::Saved current;
        if(!find(uid,object,why)||!scenery_make_history::live(r.memory,r.image,object,uid,expected.root,expected.region,find,current,why))return false;
        std::array<uint32_t,10> pose{};std::memcpy(pose.data(),current.transform.data(),sizeof pose);
        if(current.type!=expected.scenery||pose!=expected.row.pose||current.height!=expected.height||current.automatic!=expected.automatic)
            return q::fail(why,"Group restored original member type/pose/height/automatic fields differ");
        return remove_diagnostics::restore_redone_building(request.snapshot_uuid,key,uid,why);
    };
    auto records=[&](std::string& why){return sample(r,source.binding,source.request.group_object,after,why);};
    scenery_alias_order::Receipt receipt;return scenery_alias_order::capture({},alive,restorable,retire,restore,records,receipt,e);
}
bool history_graph(Runtime& r,const Operation& source,uintptr_t graph,bool move_kind,bool destroying,
                   const duplicate_edit::HistorySource& owned,std::string& e){
    duplicate_history::Graph checked;
    if(move_kind){const auto proof=source.move_proofs.find(graph);if(proof==source.move_proofs.end())return q::fail(e,"Group Move history lacks its retained exact native inverse payload");
        return group_object_graph::move(r.memory,r.image,graph,source.remove_binding.uid,proof->second,checked,e);}
    const auto proof=source.cash_proofs.find(graph);if(proof==source.cash_proofs.end())return q::fail(e,"Group history lacks its retained signed native refund inverse");
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& why){return group_edit::find(r,uid,object,why);};
    auto text=[&](uintptr_t object,std::string& value,std::string& why){return prop_text::text(r,object,value,why);};
    return group_object_graph::destroy_restore(r.memory,r.image,graph,destroying,source.request.group_object.key,owned.known,proof->second,find,text,checked,e);
}
}
action::Outcome perform_history(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);Operation* source=nullptr;uint64_t revision=0;
    if(!r||!r->ready||r->thread!=GetCurrentThreadId()||request.snapshot_uuid!=net::current_snapshot_id()||bool(authority)==Config::instance().is_host||
       !history_source(binding,request,source,e)||!contract::pending(typed_action_runtime::controller_status(),request,revision,e)){
        if(e.empty())e="Group history requires its exact pending action and current role";return action::Outcome::RejectedNoEffect;}
    if(history_operations.contains(request.action_uuid)||history_quarantined.contains(request.action_uuid)){e="Group history already attempted; no replay";return action::Outcome::Unknown;}
    if(authority&&!action::valid_group_object_effect(request,*authority)){e="Group history Host complete evidence is invalid";return action::Outcome::RejectedNoEffect;}
    const bool move_kind=moving(source->request),undo=request.action==action::Action::UndoBuilding,restoring=!move_kind&&undo;
    Sample before;uintptr_t graph=0;
    if(!sample(*r,binding,source->request.group_object,before,e)||!building_history_slice::graph_for(request.action,request.target_action_uuid,graph,e))return action::Outcome::RejectedNoEffect;
    const auto parent=before.props.objects.find(source->request.group_object.key);
    if(restoring){if(parent!=before.props.objects.end()){e="Group history restoration expected an absent canonical Group";return action::Outcome::RejectedNoEffect;}}
    else if(parent==before.props.objects.end()||parent->second.pose!=(move_kind?(undo?source->request.group_object.after:source->request.group_object.before):source->request.group_object.before)){
        e="Group history complete live source pose differs";return action::Outcome::RejectedNoEffect;}
    if(!move_kind&&!restoring&&!live_known(*r,before,source->owned,e))return action::Outcome::RejectedNoEffect;
    if(!history_graph(*r,*source,graph,move_kind,!restoring,source->owned,e))return action::Outcome::RejectedNoEffect;
    if(authority&&(before.state!=authority->group_object_before||before.props.money!=authority->before)){
        e="Group history Client complete before state differs from Host";return action::Outcome::RejectedNoEffect;}
    Operation scratch;scratch.binding=binding;scratch.request=source->request;scratch.request.action_uuid=request.action_uuid;scratch.before=std::move(before);scratch.owned=source->owned;
    scratch.remove_binding=source->remove_binding;
    if(!duplicate_edit::history_local_selection(*r,scratch.before.props,scratch.selection,scratch.filter,scratch.camera,e))return action::Outcome::RejectedNoEffect;
    if(move_kind&&!q::at(r->memory,source->remove_binding.object,0x100,scratch.stored_pose,e))return action::Outcome::RejectedNoEffect;
    auto& op=history_operations.emplace(request.action_uuid,HistoryOperation{binding,request}).first->second;op.graph=graph;op.move_kind=move_kind;op.restoring=restoring;
    op.evidence.before=scratch.before.props.money;op.evidence.group_object_before=scratch.before.state;
    auto unknown=[&](const std::string& why){e=op.cause=why;std::string cleanup;
        if(!op.selection_restored){if(duplicate_edit::history_restore(*r,scratch.before.props,scratch.selection,scratch.filter,scratch.camera,scratch.owned,cleanup))op.selection_restored=true;
            else{op.cleanup_cause=cleanup;e=op.cause+="; cleanup: "+cleanup;}}return action::Outcome::Unknown;};
    op.attempted=true;if(building_history_slice::perform(request.action,request.target_action_uuid,e)!=action::Outcome::Confirmed)return unknown(e);op.returned=true;
    const auto opposite=undo?action::Action::RedoBuilding:action::Action::UndoBuilding;
    if(!building_history_slice::graph_for(opposite,request.target_action_uuid,op.inverse,e))return unknown(e);
    if(move_kind){source->move_proofs[op.inverse]=scratch.stored_pose;
        if(!history_graph(*r,*source,op.inverse,true,false,source->owned,e)||!refresh_move(scratch,e)||!sample(*r,binding,source->request.group_object,op.after,e))return unknown(e);
    }else{
        const auto cash=source->cash_proofs.find(graph);if(cash==source->cash_proofs.end())return unknown("Group history original signed cash proof disappeared");
        std::vector<action::DuplicateCash> inverse=cash->second;for(auto& row:inverse)row.amount=-row.amount;source->cash_proofs[op.inverse]=inverse;
        op.evidence.duplicate_cash=inverse;
        if(restoring){duplicate_edit::HistorySource present;
            if(!restored_pointers(*r,source->owned,present,e)||!history_graph(*r,*source,op.inverse,false,true,present,e)||
               !settle_restoration(*r,*source,request,scratch.before,op.after,e)||!live_known(*r,op.after,source->owned,e))return unknown(e);
        }else{
            if(!history_graph(*r,*source,op.inverse,false,false,source->owned,e)||!settle_destroy(*r,scratch,e))return unknown(e);op.after=std::move(scratch.after);
        }
    }
    if(!same_binding(scratch.before,op.after)||!duplicate_edit::history_survivors(scratch.before.props,op.after.props,scratch.owned,e))return unknown(e.empty()?"Group history current World or unrelated native identity changed":e);
    if(!duplicate_edit::history_restore(*r,scratch.before.props,scratch.selection,scratch.filter,scratch.camera,scratch.owned,e))return unknown(e);op.selection_restored=true;
    op.evidence.after=op.after.props.money;op.evidence.group_object_after=op.after.state;op.evidence.undo_confirmed=op.evidence.selection_confirmed=true;op.evidence.target_retired=!move_kind&&!restoring;
    if(!action::valid_group_object_effect(request,op.evidence)||!action::group_object_wire_fits(request,op.evidence,e)||
       (authority&&(op.evidence.group_object_after!=authority->group_object_after||op.evidence.after!=authority->after||!duplicate_history::cash_equal(op.evidence.duplicate_cash,authority->duplicate_cash))))
        return unknown(e.empty()?"Group history complete native result differs from Host":e);
    trim(op.after);op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe_history(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);Operation* source=nullptr;const auto found=history_operations.find(request.action_uuid);
    if(!r||r->thread!=GetCurrentThreadId()||found==history_operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||
       !history_source(binding,request,source,e))return q::fail(e,e.empty()?"Group history observation lacks its exact confirmed operation":e);
    const auto& op=found->second;Sample current;uintptr_t inverse=0;const auto opposite=request.action==action::Action::UndoBuilding?action::Action::RedoBuilding:action::Action::UndoBuilding;
    if(!sample(*r,binding,source->request.group_object,current,e)||!same_binding(op.after,current)||current.state!=op.after.state||current.props.money!=op.after.props.money||current.props.undo!=op.after.props.undo||
       !building_history_slice::graph_for(opposite,request.target_action_uuid,inverse,e)||inverse!=op.inverse||!history_graph(*r,*source,inverse,op.move_kind,op.restoring,source->owned,e))
        return q::fail(e,e.empty()?"Group history complete current state or opposite original graph changed before ACK":e);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire_history(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence actual;if(!observe_history(binding,request,actual,e))return false;auto& op=history_operations.at(request.action_uuid);
    if(op.retired)return q::fail(e,"Group history already retired");op.retired=true;return true;
}
}
