namespace dungeon_cells {
namespace {
struct HistoryOperation {
    a::Binding binding;a::Request request;Sample before,after;BoundGraph inverse;
    npc_quest_target_runtime::DungeonCellCascade zones;a::Evidence evidence;
    bool attempted=false,confirmed=false,retired=false;std::string cause;
};
std::map<std::string,HistoryOperation> history_operations;
void clear_history(){history_operations.clear();creating=nullptr;}
bool history_owner(const dungeon_palette::Sample& before,const dungeon_palette::Sample& after){
    // Native SetLayoutAndPlan can replace owned cell storage. The independently
    // read full layout binds its new slots to the same Dungeon, not old pointers.
    return dungeon_sound::same_local(before.dungeon,after.dungeon)&&before.dungeon_uid==after.dungeon_uid&&before.local.count==after.local.count;
}
bool graph_ready(Memory& memory,const BoundGraph& graph,host::LookupApi& api,std::string& error){
    if(!graph_unchanged(memory,graph,error)||!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
    std::set<uint64_t> produced;
    for(const auto& node:graph.value.nodes)produced.insert(node.dependencies[0].begin(),node.dependencies[0].end());
    for(const auto& node:graph.value.nodes){
        for(size_t set=1;set<3;++set)for(const auto uid:node.dependencies[set]){
            if(set==1&&produced.contains(uid))continue;uintptr_t object=0;uint64_t actual=0;
            const auto seh=lookup(api.find,uid,object);
            if(seh||!object||!field(memory,object,0x18,actual,error)||actual!=uid)
                return fail(error,error.empty()?"Dungeon original history dependency does not resolve its complete current UID SEH="+std::to_string(seh):error);
        }
        if(node.kind==cg::Kind::Cash)continue;
        const bool make=node.kind==cg::Kind::Scenery||node.kind==cg::Kind::Monster||node.kind==cg::Kind::Zone||node.kind==cg::Kind::Gizmo||node.kind==cg::Kind::Door;
        uintptr_t object=0;const auto seh=lookup(api.find,node.uid,object);
        if(seh||(make?bool(object):!object))return fail(error,"Dungeon original history target lifetime differs before mutation UID="+std::to_string(node.uid)+" SEH="+std::to_string(seh));
        if(!make&&node.kind!=cg::Kind::Quests&&node.kind!=cg::Kind::Paint&&node.kind!=cg::Kind::Layout){
            const auto actor=graph.actors.find(node.uid);
            if(actor==graph.actors.end()||actor->second.object!=object)return fail(error,"Dungeon original history live target replaced its retained local owner");
        }
    }
    return true;
}
bool prepare_creating(const BoundGraph& graph,Creating& out,std::string& error){
    out={};std::vector<dungeon_gizmo_restore::Expected> expected;
    for(const auto& node:graph.value.nodes)if(node.kind==cg::Kind::Gizmo||node.kind==cg::Kind::Door){
        const auto actor=graph.actors.find(node.uid);
        if(actor==graph.actors.end()||actor->second.kind!=cg::Kind::Gizmo||actor->second.key.empty())return fail(error,"Dungeon original Gizmo Make lacks its retained canonical scene actor");
        expected.push_back({node.pointer,actor->second.manager});
    }
    return out.receipts.prepare(expected,error);
}
bool reconcile_history(Memory& memory,const a::Binding& binding,const a::Request& request,const BoundGraph& graph,
                       const Sample& before,Creating& created,npc_quest_target_runtime::DungeonCellCascade& zones,
                       const npc_quest_target_runtime::DungeonCellCascade& original_zones,std::string& error){
    if(!created.cause.empty())return fail(error,created.cause);
    std::vector<dungeon_gizmo_restore::Created> receipts;if(!created.receipts.complete(receipts,error))return false;
    host::LookupApi api;if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
    std::vector<dungeon_scene_identity_runtime::GizmoRestore> restored;
    std::vector<std::pair<std::string,dungeon_gizmo_identity::Member>> retired;
    auto monsters=identity::registry;std::set<uint64_t> restored_zones;
    std::vector<remove_diagnostics::scene_buildings::Row> scenery_retired;
    std::vector<remove_diagnostics::scene_buildings::Restore> scenery_restored;
    const auto scenery_row=[](const Actor& actor,uintptr_t object){return remove_diagnostics::scene_buildings::Row{
        actor.key,actor.scene,object,actor.shard,actor.region,actor.manager,actor.uid,true};};
    for(const auto& node:graph.value.nodes){
        if(node.kind==cg::Kind::Paint||node.kind==cg::Kind::Layout||node.kind==cg::Kind::Quests||node.kind==cg::Kind::Cash)continue;
        const auto found=graph.actors.find(node.uid);if(found==graph.actors.end())return fail(error,"Dungeon original history completed an actor absent from its captured graph");
        const auto& actor=found->second;uintptr_t object=0;uint64_t uid=0;uintptr_t shard=0,region=0;
        const auto seh=lookup(api.find,node.uid,object);if(seh)return fail(error,"Dungeon original history target readback SEH="+std::to_string(seh));
        if(node.kind==cg::Kind::Destroy){
            if(object)return fail(error,"Dungeon original history queued actor deletion remains registered UID="+std::to_string(node.uid));
            if(actor.kind==cg::Kind::Scenery)scenery_retired.push_back(scenery_row(actor,actor.object));
            else if(actor.kind==cg::Kind::Gizmo)retired.emplace_back(actor.key,actor.gizmo());
            else if(actor.kind==cg::Kind::Monster&&!monsters.retire(actor.key,actor.monster,error))return false;
            continue;
        }
        if(node.kind==cg::Kind::Move){
            if(object!=actor.object)return fail(error,"Scene Move history replaced its original canonical lifetime");
            if(actor.kind==cg::Kind::Scenery&&!remove_diagnostics::refresh_moved_building(request.snapshot_uuid,actor.key,actor.uid,error))return false;
            continue;
        }
        if(node.kind==cg::Kind::Gizmo||node.kind==cg::Kind::Door){
            const auto receipt=std::find_if(receipts.begin(),receipts.end(),[&](const auto& row){return row.node==node.pointer;});
            if(receipt==receipts.end())return fail(error,"Dungeon original Gizmo Make has no actual return receipt");
            object=receipt->object;uintptr_t ap=0;
            if(!field(memory,object,0,ap,error)||ap!=state.image+0x1749ab0)return fail(error,error.empty()?"Dungeon original Gizmo return has a foreign concrete class":error);
        }
        if(!object||!field(memory,object,0x18,uid,error)||!field(memory,object,0x170,shard,error)||!field(memory,object,0x178,region,error)||
           !uid||shard!=actor.shard||region!=actor.region)return fail(error,error.empty()?"Dungeon original actor restoration lacks its complete reciprocal scene owner":error);
        uintptr_t registered=0;const auto current_seh=lookup(api.find,uid,registered);
        if(current_seh||registered!=object)return fail(error,"Dungeon original restored actor full UID does not resolve its actual returned object SEH="+std::to_string(current_seh));
        if(actor.kind==cg::Kind::Gizmo){
            auto actual=actor.gizmo();actual.object=object;actual.uid=uid;restored.push_back({actor.key,actor.gizmo(),actual});
        }else if(uid!=node.uid)return fail(error,"Dungeon original UID-preserving Make changed its saved full UID");
        else if(actor.kind==cg::Kind::Scenery)scenery_restored.push_back({scenery_row(actor,actor.object),scenery_row(actor,object)});
        else if(actor.kind==cg::Kind::Monster){auto actual=actor.monster;actual.pointer=object;actual.retired=false;
            if(!monsters.restore_retired(actor.key,actor.monster,actual,error))return false;
        }else if(actor.kind==cg::Kind::Zone)restored_zones.insert(uid);
    }
    dungeon_scene_identity_runtime::Census actual;
    if(!dungeon_scene_identity_runtime::confirm_cell_restoration(binding,request,before.scenes,restored,retired,actual,error)||
       !npc_quest_target_runtime::confirm_dungeon_cell_history(binding,request,zones,original_zones,restored_zones,error))return false;
    remove_diagnostics::scene_buildings::Preview scenery_preview;
    if(!remove_diagnostics::scene_buildings::preview(before.scenes.world,{},scenery_retired,scenery_restored,scenery_preview,error)||
       !remove_diagnostics::scene_buildings::publish(scenery_preview,error))return false;
    std::vector<std::pair<std::string,identity::Entry>> actual_monsters;
    if(!monster_members(memory,actual,actual_monsters,false,error,&monsters))return false;
    // Publish only after the complete current native Monster roster accepts the
    // prepared aliases, including unaffected members and all restored full UIDs.
    identity::registry=std::move(monsters);return true;
}
json history_status(){json out=json::object();for(const auto& [id,op]:history_operations)out[id]={{"attempted",op.attempted},{"confirmed",op.confirmed},{"retired",op.retired},{"cause",op.cause}};return out;}
}
a::Outcome perform_history(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    uint64_t revision=0;
    if(!ready||fault||GetCurrentThreadId()!=state.thread||request.snapshot_uuid!=net::current_snapshot_id()||
       bool(authority)==Config::instance().is_host||request.canonical_record_key!="dungeon-tiles:"+request.target_action_uuid||
       (request.action!=a::Action::UndoBuilding&&request.action!=a::Action::RedoBuilding)||
       !ui_move_runtime::contract::pending(typed_action_runtime::controller_status(),request,revision,error)){
        if(error.empty())error="Dungeon cell history requires exact pending Mainthread world and authority";return a::Outcome::RejectedNoEffect;}
    if(history_operations.contains(request.action_uuid)){error="Dungeon cell history already entered; replay forbidden";return a::Outcome::Unknown;}
    const auto found=operations.find(request.target_action_uuid);
    if(found==operations.end()||found->second.binding!=binding||found->second.request.snapshot_uuid!=request.snapshot_uuid||!found->second.confirmed||!found->second.retired){
        error="Dungeon cell history requires its confirmed retired current-world original source";return a::Outcome::RejectedNoEffect;}
    auto& source=found->second;const auto& intent=source.request.dungeon_palette;Memory memory;Sample before;uintptr_t pointer=0;host::LookupApi api;Creating context;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,intent,before,error,&source.current_graph.quests)||
       !building_history_slice::graph_for(request.action,request.target_action_uuid,pointer,error)||pointer!=source.current_graph.value.pointer||
       !graph_ready(memory,source.current_graph,api,error)||!prepare_creating(source.current_graph,context,error)){
        if(error.empty())error="Dungeon cell retained original stack graph differs";return a::Outcome::RejectedNoEffect;}
    if(!a::dungeon_palette_wire_fits(request,before.value,before.palette.dungeon.dungeon.dungeon.money,error))return a::Outcome::RejectedNoEffect;
    if(authority){auto portable=before.value;portable.protected_state.dungeon.global_random=authority->dungeon_palette_before.protected_state.dungeon.global_random;
        if(portable!=authority->dungeon_palette_before||before.palette.dungeon.dungeon.dungeon.money!=authority->before){error="Dungeon cell history Client complete before differs from Host";return a::Outcome::RejectedNoEffect;}}
    auto& op=history_operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){creating=nullptr;replaying=false;error=op.cause=why.empty()?"Dungeon original cell history failed without cause":why;return a::Outcome::Unknown;};
    if(authority){if(!original_result(action_runtime::seed(state.image,authority->dungeon_palette_before.protected_state.dungeon.global_random),"history Host RNG adoption",error)||
           !sample(memory,binding,request.snapshot_uuid,intent,before,error,&source.current_graph.quests)||before.value!=authority->dungeon_palette_before)return unknown(error.empty()?"Dungeon history Host RNG readback differs":error);}
    op.before=std::move(before);op.evidence.before=op.before.palette.dungeon.dungeon.dungeon.money;op.evidence.dungeon_palette_before=op.before.value;
    if(!npc_quest_target_runtime::prepare_dungeon_cells(binding,request,op.zones,error))return unknown(error);
    op.attempted=true;creating=&context;replaying=true;
    const auto result=building_history_slice::perform(request.action,request.target_action_uuid,error);creating=nullptr;replaying=false;
    if(result!=a::Outcome::Confirmed)return unknown(error);
    if(!reconcile_history(memory,binding,request,source.current_graph,op.before,context,op.zones,source.zone_cascade,error)||
       !sample(memory,binding,request.snapshot_uuid,intent,op.after,error,&op.before.quests)||!history_owner(op.before.palette,op.after.palette)||
       !a::dungeon_cells_history_transition(request.action==a::Action::UndoBuilding,op.before.value,op.after.value)||
       op.before.palette.dungeon.dungeon.dungeon.money!=op.after.palette.dungeon.dungeon.dungeon.money)return unknown(error.empty()?"Dungeon original history complete result or protected state differs":error);
    const auto opposite=request.action==a::Action::UndoBuilding?a::Action::RedoBuilding:a::Action::UndoBuilding;uintptr_t inverse=0;
    if(!building_history_slice::graph_for(opposite,request.target_action_uuid,inverse,error)||!graph_capture(memory,op.before,op.after,inverse,op.inverse,error))return unknown(error);
    op.evidence.after=op.after.palette.dungeon.dungeon.dungeon.money;op.evidence.dungeon_palette_after=op.after.value;op.evidence.undo_confirmed=true;
    if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Dungeon history Client complete native result differs from Host");}
    source.current_graph=op.inverse;op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){creating=nullptr;replaying=false;error=std::string("Dungeon cell history boundary: ")+exception.what();return a::Outcome::Unknown;}
catch(...){creating=nullptr;replaying=false;error="Dungeon cell history non-standard exception";return a::Outcome::Unknown;}
bool observe_history(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& error){
    const auto found=history_operations.find(request.action_uuid);
    if(GetCurrentThreadId()!=state.thread||found==history_operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)
        return fail(error,"Dungeon cell history observation lacks its exact confirmed current receipt");
    const auto& op=found->second;const auto source=operations.find(request.target_action_uuid);
    if(source==operations.end())return fail(error,"Dungeon cell history source disappeared before ACK");
    Memory memory;Sample current;uintptr_t graph=0;const auto opposite=request.action==a::Action::UndoBuilding?a::Action::RedoBuilding:a::Action::UndoBuilding;
    if(!sample(memory,binding,request.snapshot_uuid,source->second.request.dungeon_palette,current,error,&op.after.quests)||
       !history_owner(op.after.palette,current.palette)||current.value!=op.after.value||current.palette.dungeon.dungeon.dungeon.money!=op.evidence.after||
       !building_history_slice::graph_for(opposite,request.target_action_uuid,graph,error)||graph!=op.inverse.value.pointer||!graph_unchanged(memory,op.inverse,error))
        return fail(error,error.empty()?"Dungeon complete original history result changed before ACK":error);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire_history(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence out;if(!observe_history(binding,request,out,error))return false;
    auto& op=history_operations.at(request.action_uuid);if(op.retired)return fail(error,"Dungeon cell history receipt already retired");op.retired=true;return true;}
}
