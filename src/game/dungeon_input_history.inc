namespace dungeon_input {
namespace {
struct HistoryOperation {
    a::Binding binding;a::Request request;Sample before,after;dungeon_cells::BoundGraph inverse;
    npc_quest_target_runtime::DungeonCellCascade zones;a::Evidence evidence;
    bool attempted=false,confirmed=false,retired=false;std::string cause;
};
std::map<std::string,HistoryOperation> history_operations;
void clear_history(){history_operations.clear();}
json history_status(){json out=json::object();for(const auto& [id,op]:history_operations)out[id]={{"attempted",op.attempted},{"confirmed",op.confirmed},{"retired",op.retired},{"cause",op.cause}};return out;}
bool history_cash(Memory& memory,const dungeon_cells::BoundGraph& graph,bool undo,const a::DungeonInputIntent& intent,std::string& error){
    if(intent.controls){const auto& controls=*intent.controls;const auto count=dungeon_gizmo_control::cash_count(controls);
        size_t found=0;for(const auto& node:graph.value.nodes)if(node.kind==dungeon_cell_graph::Kind::Cash){int32_t amount=0;
            if(found>=count||!field(memory,node.pointer,0xb8,amount,error))return fail(error,error.empty()?"Gizmo history has an extra Cash node":error);
            const auto cost=controls.rows.size()>9?dungeon_gizmo_control::total(controls):controls.rows[found].cost;
            const int64_t expected=int64_t(cost)*(controls.command==ControlCommand::Destroy?-1:1)*(undo?1:-1);
            if(amount!=expected)return fail(error,"Gizmo history retained Cash amount differs from exact signed original cost");++found;}
        return found==count||fail(error,"Gizmo history retained Cash node count differs");}
    const bool required=intent.tool==a::DungeonInputTool::Door||intent.tool==a::DungeonInputTool::Treasure||(intent.tool==a::DungeonInputTool::Key&&intent.cost!=0);
    unsigned count=0;for(const auto& node:graph.value.nodes)if(node.kind==dungeon_cell_graph::Kind::Cash){
        ++count;int32_t amount=0;if(!field(memory,node.pointer,0xb8,amount,error))return false;
        if(int64_t(amount)!=(undo?int64_t(intent.cost):-int64_t(intent.cost)))return fail(error,"Dungeon input retained original Cash amount differs from exact signed Undo/Redo cost");
    }
    return count==unsigned(required)||fail(error,"Dungeon input retained original Cash node count differs from Door/Key contract");
}
}
a::Outcome perform_history(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    uint64_t revision=0;
    if(!ready||fault||GetCurrentThreadId()!=state.thread||request.snapshot_uuid!=net::current_snapshot_id()||
       bool(authority)==Config::instance().is_host||request.canonical_record_key!="dungeon-input:"+request.target_action_uuid||
       (request.action!=a::Action::UndoBuilding&&request.action!=a::Action::RedoBuilding)||
       !ui_move_runtime::contract::pending(typed_action_runtime::controller_status(),request,revision,error)){
        if(error.empty())error="Dungeon input history requires exact pending Mainthread world/authority";return a::Outcome::RejectedNoEffect;}
    if(history_operations.contains(request.action_uuid)){error="Dungeon input history already entered; replay forbidden";return a::Outcome::Unknown;}
    const auto found=operations.find(request.target_action_uuid);
    if(found==operations.end()||found->second.binding!=binding||found->second.request.snapshot_uuid!=request.snapshot_uuid||!found->second.confirmed||!found->second.retired){
        error="Dungeon input history requires its exact confirmed retired original source";return a::Outcome::RejectedNoEffect;}
    auto& source=found->second;const auto& intent=source.request.dungeon_input;const bool undo=request.action==a::Action::UndoBuilding;
    Memory memory;Sample before;uintptr_t graph=0;host::LookupApi api;dungeon_cells::Creating context;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,intent,before,error,&source.current_graph.quests,request.target_action_uuid)||
       !building_history_slice::graph_for(request.action,request.target_action_uuid,graph,error)||graph!=source.current_graph.value.pointer||
       !dungeon_cells::graph_ready(memory,source.current_graph,api,error)||!dungeon_cells::prepare_creating(source.current_graph,context,error)||
       !history_cash(memory,source.current_graph,undo,intent,error)){
        if(error.empty())error="Dungeon input retained original stack graph differs";return a::Outcome::RejectedNoEffect;}
    std::vector<std::string> restored_names;
    if(!undo&&intent.controls&&intent.controls->command==ControlCommand::Duplicate){
        const auto gizmos=size_t(std::count_if(intent.controls->rows.begin(),intent.controls->rows.end(),[](const auto& row){return !row.scenery;}));
        if(!source.after.value.edits||source.after.value.edits->size()!=gizmos)return fail(error,"Gizmo Redo retained original output names are incomplete"),a::Outcome::RejectedNoEffect;
        size_t output=0;for(size_t i=0;i<intent.controls->rows.size();++i){if(intent.controls->rows[i].scenery){restored_names.emplace_back();continue;}
            const auto& row=source.after.value.edits->at(output++);
            if(row.key!=dungeon_gizmo_identity::birth(request.target_action_uuid,uint32_t(i)))return fail(error,"Gizmo Redo retained original name owner differs"),a::Outcome::RejectedNoEffect;
            restored_names.push_back(row.value.name);}
    }
    if(!a::dungeon_input_wire_fits(request,before.value,before.source.palette.dungeon.dungeon.dungeon.money,error,&intent,!undo&&source.after.value.birth?&*source.after.value.birth:nullptr,restored_names.empty()?nullptr:&restored_names))return a::Outcome::RejectedNoEffect;
    if(authority){auto portable=before.value;portable.protected_state.dungeon.global_random=authority->dungeon_input_before.protected_state.dungeon.global_random;
        if(!authority->dungeon_input_origin||*authority->dungeon_input_origin!=intent||portable!=authority->dungeon_input_before||before.source.palette.dungeon.dungeon.dungeon.money!=authority->before){
            error="Dungeon input history Client retained source/complete native before differs from Host";return a::Outcome::RejectedNoEffect;}}
    auto& op=history_operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){dungeon_cells::creating=nullptr;replaying=false;error=op.cause=why.empty()?"Dungeon input original history failure without cause":why;return a::Outcome::Unknown;};
    if(authority){if(!original_result(action_runtime::seed(state.image,authority->dungeon_input_before.protected_state.dungeon.global_random),"history Host RNG adoption",error)||
        !sample(memory,binding,request.snapshot_uuid,intent,before,error,&source.current_graph.quests,request.target_action_uuid)||before.value!=authority->dungeon_input_before)
        return unknown(error.empty()?"Dungeon input history adopted RNG complete readback differs":error);}
    op.before=std::move(before);op.evidence.before=op.before.source.palette.dungeon.dungeon.dungeon.money;
    op.evidence.dungeon_input_before=op.before.value;op.evidence.dungeon_input_origin=intent;
    if(!npc_quest_target_runtime::prepare_dungeon_cells(binding,request,op.zones,error))return unknown(error);
    op.attempted=true;dungeon_cells::creating=&context;replaying=true;
    const auto result=building_history_slice::perform(request.action,request.target_action_uuid,error);
    dungeon_cells::creating=nullptr;replaying=false;if(result!=a::Outcome::Confirmed)return unknown(error);
    if(!dungeon_cells::reconcile_history(memory,binding,request,source.current_graph,op.before.source,context,op.zones,source.zones,error)||
       !sample(memory,binding,request.snapshot_uuid,intent,op.after,error,&op.before.source.quests,request.target_action_uuid)||
       (intent.controls&&!control_monsters(op.before,op.after,error))||
       !dungeon_cells::history_owner(op.before.source.palette,op.after.source.palette)||
       !a::dungeon_input_history_transition(request.target_action_uuid,undo,intent,op.before.value,op.after.value)||
       !a::dungeon_input_history_cash(undo,intent,op.evidence.before,op.after.source.palette.dungeon.dungeon.dungeon.money))
        return unknown(error.empty()?"Dungeon input original history complete scene/reference/ledger effect differs":error);
    const auto opposite=undo?a::Action::RedoBuilding:a::Action::UndoBuilding;uintptr_t inverse=0;
    if(!building_history_slice::graph_for(opposite,request.target_action_uuid,inverse,error)||
       !dungeon_cells::graph_capture(memory,op.before.source,op.after.source,inverse,op.inverse,error,source.current_graph.policy))return unknown(error);
    op.evidence.after=op.after.source.palette.dungeon.dungeon.dungeon.money;op.evidence.dungeon_input_after=op.after.value;op.evidence.undo_confirmed=true;
    if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Dungeon input history Client complete original result differs from Host");}
    source.current_graph=op.inverse;op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){dungeon_cells::creating=nullptr;replaying=false;error=std::string("Dungeon input history original boundary: ")+exception.what();return a::Outcome::Unknown;}
catch(...){dungeon_cells::creating=nullptr;replaying=false;error="Dungeon input history original non-standard boundary exception";return a::Outcome::Unknown;}
bool observe_history(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& error){
    const auto found=history_operations.find(request.action_uuid);
    const auto source=operations.find(request.target_action_uuid);
    if(GetCurrentThreadId()!=state.thread||found==history_operations.end()||source==operations.end()||found->second.binding!=binding||
       found->second.request!=request||!found->second.confirmed)return fail(error,"Dungeon input history observation lacks exact confirmed current receipt/source");
    const auto& op=found->second;Memory memory;Sample current;uintptr_t graph=0;
    const auto opposite=request.action==a::Action::UndoBuilding?a::Action::RedoBuilding:a::Action::UndoBuilding;
    if(!sample(memory,binding,request.snapshot_uuid,source->second.request.dungeon_input,current,error,&op.after.source.quests,request.target_action_uuid)||
       !dungeon_cells::history_owner(op.after.source.palette,current.source.palette)||current.value!=op.after.value||
       current.source.palette.dungeon.dungeon.dungeon.money!=op.evidence.after||!building_history_slice::graph_for(opposite,request.target_action_uuid,graph,error)||
       graph!=op.inverse.value.pointer||!dungeon_cells::graph_unchanged(memory,op.inverse,error))
        return fail(error,error.empty()?"Dungeon input complete original history result changed before ACK":error);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire_history(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence out;if(!observe_history(binding,request,out,error))return false;
    auto& op=history_operations.at(request.action_uuid);if(op.retired)return fail(error,"Dungeon input history receipt already retired");op.retired=true;return true;}
}
