namespace {
namespace scenery_history=scenery_make_history;
struct HistoryMemory {
    region::Runtime& owner;
    bool read(uintptr_t address,void* out,size_t bytes,std::string& e){return owner.memory.copy(address,out,bytes,e);}
};
struct TerrainHistoryOperation {
    action::Binding binding;action::Request request;action::Evidence evidence;
    uintptr_t inverse=0;bool attempted=false,confirmed=false,retired=false;
    std::vector<std::string> keys;std::string cause;
};
std::map<std::string,TerrainHistoryOperation> terrain_history_operations;
uint32_t history_find_raw(uintptr_t image,uint64_t uid,uintptr_t& object){
    __try{object=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x47ce20)(uid);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool history_find(region::Runtime& r,uint64_t uid,uintptr_t& object,std::string& e){
    const auto code=history_find_raw(r.image,uid,object);
    return !code||fail(e,"Terrain history original FindUID SEH="+std::to_string(code));
}
bool history_capture(region::Runtime& r,const region::LocalView& before,const json& bindings,TerrainOperation& op,std::string& e){
    HistoryMemory memory{r};
    op.history_before_uids=before.scenery_uids;
    auto find=[&](uint64_t uid,uintptr_t& value,std::string& why){return history_find(r,uid,value,why);};
    for(auto uid:before.scenery_uids){uintptr_t object=0;scenery_history::Saved saved;
        if(!find(uid,object,e)||!scenery_history::live(memory,r.image,object,uid,before.world.root,before.region,find,saved,e))return false;
        op.history_scenery.emplace(uid,std::move(saved));}
    for(const auto& row:bindings.at("bindings")){
        const auto uid=row.at("local_uid").get<uint64_t>();
        if(!op.history_scenery.contains(uid)||row.at("revoked").get<bool>())continue;
        if(row.at("unknown").get<bool>()||!op.history_keys.emplace(uid,row.at("record_key").get<std::string>()).second)
            return fail(e,"Terrain history canonical before binding is Unknown or ambiguous");
    }
    return true;
}
bool history_graph(region::Runtime& r,uintptr_t graph,uintptr_t receiver,const std::string& contours,
                   const TerrainOperation& source,bool making,std::string& e){
    HistoryMemory memory{r};undo::GraphWitness witness;uintptr_t array=0;size_t painted=0;std::set<uint64_t> seen;
    if(!undo::read_graph(r.memory,r.image,graph,witness,e)||!region::at(r,graph,0x20,array,e))return false;
    if(!witness.locked||witness.pending_delete||witness.operations!=source.removed_scenery_uids.size()+1)
        return fail(e,"Terrain history complete original graph count/lifetime differs");
    for(int32_t i=0;i<witness.operations;++i){uintptr_t node=0,ap=0;
        if(!region::at(r,array,size_t(i)*8,node,e)||!region::at(r,node,0,ap,e))return false;
        if(ap==r.image+0x1723330){uintptr_t method=0;if(!region::at(r,ap,0x10,method,e))return false;
            if(++painted!=1||method!=r.image+0x55e150)return fail(e,"Terrain history original contour inverse method/count differs");continue;}
        uint64_t uid=0;if(!region::at(r,node,0xb8,uid,e))return false;
        const auto saved=source.history_scenery.find(uid);
        if(saved==source.history_scenery.end()||!seen.insert(uid).second)
            return fail(e,"Terrain history original graph changes unrelated or repeated Scenery UID");
        if(making?!scenery_history::make(memory,r.image,node,saved->second,e):!scenery_history::destroy(memory,r.image,node,uid,e))return false;
    }
    return (painted==1&&seen.size()==source.removed_scenery_uids.size()&&inverse(r,graph,receiver,contours,e))||
        fail(e,e.empty()?"Terrain history complete inverse UID set differs":e);
}
bool history_source(region::Runtime& r,const region::LocalView& before,const region::LocalView& after,TerrainOperation& op,std::string& e){
    op.history_after_uids=after.scenery_uids;
    const std::set<uint64_t> removed(op.removed_scenery_uids.begin(),op.removed_scenery_uids.end());
    std::erase_if(op.history_scenery,[&](const auto& row){return !removed.contains(row.first);});
    std::erase_if(op.history_keys,[&](const auto& row){return !removed.contains(row.first);});
    return history_graph(r,op.graph,before.region,op.evidence.terrain_before.contours_sha256,op,true,e);
}
struct HistorySelection {std::vector<std::pair<uintptr_t,uint64_t>> rows;std::array<uint8_t,9> filter{};uintptr_t camera=0;};
bool history_selection(region::Runtime& r,const region::LocalView& view,HistorySelection& out,std::string& e){
    HistoryMemory memory{r};std::vector<uintptr_t> raw,filtered;uintptr_t manager=0;
    if(!group_reader::selection(memory,r.image,view.world.selection,raw,filtered,e)||
       !region::at(r,view.world.selection,0xc8,out.filter,e)||!region::at(r,r.image,0x13df440,manager,e)||!region::at(r,manager,0x60,out.camera,e))return false;
    if(std::any_of(out.filter.begin(),out.filter.end(),[](auto n){return n>1;}))return fail(e,"Terrain history local Selection filter invalid");
    for(auto pointer:raw){uint64_t uid=0;uintptr_t found=0;
        if(!region::at(r,pointer,0x18,uid,e)||!uid||!history_find(r,uid,found,e)||found!=pointer)return fail(e,"Terrain history local selected UID differs: "+e);
        out.rows.emplace_back(pointer,uid);}
    return true;
}
uint32_t history_filter(uintptr_t image,uintptr_t selection,const std::array<uint8_t,9>& values){
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x2d58f0)(reinterpret_cast<void*>(selection),values.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool history_restore_selection(region::Runtime& r,const region::LocalView& view,const HistorySelection& before,const TerrainOperation& source,std::string& e){
    std::vector<uintptr_t> desired;
    for(const auto& [pointer,uid]:before.rows){uintptr_t found=0;if(!history_find(r,uid,found,e))return false;
        if(found==pointer)desired.push_back(found);
        else if(found||!source.history_scenery.contains(uid))return fail(e,"Terrain history replaced unrelated local selected UID");}
    const duplicate_calls::Array array{r.image+0x1748c50,desired.data(),int32_t(desired.size()),int32_t(desired.size())};
    const auto selected=duplicate_calls::select(r.image,view.world.selection,array);
    const auto filter=history_filter(r.image,view.world.selection,before.filter);
    if(selected||filter)return fail(e,"Terrain history Selection restore failed select_seh="+std::to_string(selected)+" filter_seh="+std::to_string(filter));
    HistorySelection after;if(!history_selection(r,view,after,e))return false;
    std::vector<uintptr_t> actual;for(const auto& row:after.rows)actual.push_back(row.first);
    return (desired==actual&&before.filter==after.filter&&before.camera==after.camera)||fail(e,"Terrain history changed local Selection/filter/camera");
}
}

action::Outcome perform_history(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=runtime(e);uint64_t revision=0;
    if(!r||!terrain.ready||request.snapshot_uuid!=net::current_snapshot_id()||bool(authority)==Config::instance().is_host||
       request.canonical_record_key!="terrain:"+request.target_action_uuid||
       (request.action!=action::Action::UndoBuilding&&request.action!=action::Action::RedoBuilding)||
       !ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,e)){
        if(e.empty())e="Terrain history owner/direction/authority differs";return action::Outcome::RejectedNoEffect;}
    if(terrain_history_operations.contains(request.action_uuid)){e="Terrain history already attempted; no replay";return action::Outcome::Unknown;}
    const auto found=terrain.operations.find(request.target_action_uuid);
    if(found==terrain.operations.end()||!found->second.confirmed||!found->second.retired||found->second.request.terrain.height||
       found->second.binding!=binding||found->second.request.snapshot_uuid!=request.snapshot_uuid||!found->second.graph){
        e="Terrain history lacks current confirmed retired original stroke";return action::Outcome::RejectedNoEffect;}
    auto& source=found->second;const bool undoing=request.action==action::Action::UndoBuilding;
    region::LocalView before;action::TerrainState state;action::Ledger money;uintptr_t global=0,graph=0;HistorySelection selection;
    const auto& desired_region=undoing?source.evidence.region_before:source.evidence.region_after;
    const auto& expected_region=undoing?source.evidence.region_after:source.evidence.region_before;
    const auto& desired_terrain=undoing?source.evidence.terrain_before:source.evidence.terrain_after;
    const auto& expected_terrain=undoing?source.evidence.terrain_after:source.evidence.terrain_before;
    if(!region::read(*r,source.request.terrain.id,0,before,e)||!region::bound(before,binding,request.snapshot_uuid,e)||
       !terrain_state(*r,before,state,global,e)||!region::ledger(money,e)||!native_code(*r,true,e)||
       !building_history_slice::graph_for(request.action,request.target_action_uuid,graph,e)||
       !history_graph(*r,graph,before.region,desired_terrain.contours_sha256,source,undoing,e)||!history_selection(*r,before,selection,e))return action::Outcome::RejectedNoEffect;
    if(!action::same_region_content(before.state,expected_region)||state.contours_sha256!=expected_terrain.contours_sha256||
       before.scenery_uids!=(undoing?source.history_after_uids:source.history_before_uids)){
        e="Terrain history current full Region/Scenery/contours differ from original direction";return action::Outcome::RejectedNoEffect;}
    if(authority&&(money!=authority->before||!action::same_region_content(before.state,authority->region_before)||!action::same_terrain_content(state,authority->terrain_before))){
        e="Terrain history Client complete current state differs from Host";return action::Outcome::RejectedNoEffect;}
    for(const auto& [uid,key]:source.history_keys){
        if(undoing){if(!remove_diagnostics::history_target_absent(request.snapshot_uuid,key,uid,e))return action::Outcome::RejectedNoEffect;}
        else{remove_diagnostics::LocalTarget target;if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,key,target,e))return action::Outcome::RejectedNoEffect;
            if(target.uid!=uid){e="Terrain history current canonical Scenery full UID differs";return action::Outcome::RejectedNoEffect;}}
    }
    if(undoing)for(const auto& [uid,saved]:source.history_scenery){
        (void)uid;std::array<uint32_t,3> position{};std::memcpy(position.data(),saved.transform.data()+0x10,sizeof position);uintptr_t resolved=0;
        const auto code=duplicate_calls::region(r->image,saved.root,true,position,resolved);
        if(code||resolved!=before.region){e="Terrain history original Make RegionNear differs SEH="+std::to_string(code);return action::Outcome::RejectedNoEffect;}
    }
    auto& op=terrain_history_operations.try_emplace(request.action_uuid).first->second;
    op.binding=binding;op.request=request;op.evidence.before=money;op.evidence.region_before=authority?authority->region_before:before.state;
    op.evidence.terrain_before=authority?authority->terrain_before:state;
    auto unknown=[&](const std::string& why){e=op.cause=why;return action::Outcome::Unknown;};
    op.attempted=true;if(authority&&!native_error(seed(before.random,global,*authority),"history authority RNG seed",e))return unknown(e);
    const auto result=building_history_slice::perform(request.action,request.target_action_uuid,e);
    if(result!=action::Outcome::Confirmed)return unknown(e);
    if(!history_restore_selection(*r,before,selection,source,e))return unknown(e);
    const auto opposite=undoing?action::Action::RedoBuilding:action::Action::UndoBuilding;
    region::LocalView after;uintptr_t after_global=0;
    if(!building_history_slice::graph_for(opposite,request.target_action_uuid,op.inverse,e)||
       !history_graph(*r,op.inverse,before.region,state.contours_sha256,source,!undoing,e)||
       !region::read(*r,source.request.terrain.id,before.region,after,e)||!region::bound(after,binding,request.snapshot_uuid,e)||
       !terrain_state(*r,after,op.evidence.terrain_after,after_global,e)||!region::ledger(op.evidence.after,e))return unknown(e);
    op.evidence.region_after=after.state;op.evidence.undo_confirmed=true;
    if(global!=after_global||!action::same_region_content(after.state,desired_region)||
       after.scenery_uids!=(undoing?source.history_before_uids:source.history_after_uids)||
       op.evidence.terrain_after.contours_sha256!=desired_terrain.contours_sha256||
       op.evidence.terrain_after.release_edits!=state.release_edits||op.evidence.after!=money)return unknown("Terrain history original complete consequence differs");
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& why){return history_find(*r,uid,object,why);};
    HistoryMemory memory{*r};
    for(const auto& [uid,saved]:source.history_scenery){uintptr_t object=0;if(!find(uid,object,e))return unknown(e);
        if(undoing){scenery_history::Saved actual;if(!scenery_history::live(memory,r->image,object,uid,before.world.root,before.region,find,actual,e))return unknown(e);
            auto expected=saved;expected.object=object;if(actual!=expected)return unknown("Terrain history original restored full Scenery payload differs");}
        else if(object)return unknown("Terrain history original removed Scenery UID still exists");}
    if(undoing){for(const auto& [uid,key]:source.history_keys){
        if(!remove_diagnostics::restore_redone_building(request.snapshot_uuid,key,uid,e))return unknown(e);op.keys.push_back(key);}}
    else if(!remove_diagnostics::retire_region_scenery(request.snapshot_uuid,request.action_uuid,source.removed_scenery_uids,op.keys,e))return unknown(e);
    std::sort(op.keys.begin(),op.keys.end());
    if(!region::sha256(json(op.keys).dump(),op.evidence.region_retired_sha256,e))return unknown(e);
    if(authority&&(op.evidence.region_after!=authority->region_after||op.evidence.terrain_after!=authority->terrain_after||
       op.evidence.after!=authority->after||op.evidence.region_retired_sha256!=authority->region_retired_sha256))return unknown("Terrain history Client original result differs from Host");
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe_history(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=runtime(e);const auto found=terrain_history_operations.find(request.action_uuid);
    if(!r||found==terrain_history_operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)
        return fail(e,"Terrain history observation lacks exact confirmed operation");
    const auto& op=found->second;const auto& source=terrain.operations.at(request.target_action_uuid);
    region::LocalView current;action::TerrainState state;action::Ledger money;uintptr_t global=0,graph=0;
    const auto opposite=request.action==action::Action::UndoBuilding?action::Action::RedoBuilding:action::Action::UndoBuilding;
    if(!region::read(*r,source.request.terrain.id,0,current,e)||!region::bound(current,binding,request.snapshot_uuid,e)||
       !terrain_state(*r,current,state,global,e)||!region::ledger(money,e)||
       !building_history_slice::graph_for(opposite,request.target_action_uuid,graph,e))return false;
    if(graph!=op.inverse)return fail(e,"Terrain history current inverse graph changed before ACK");
    if(current.state!=op.evidence.region_after||state!=op.evidence.terrain_after||money!=op.evidence.after)return fail(e,"Terrain history complete result changed before ACK");
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool history_keys(const action::Binding& binding,const action::Request& request,std::vector<std::string>& keys,std::string& e){
    action::Evidence actual;if(!observe_history(binding,request,actual,e))return false;keys=terrain_history_operations.at(request.action_uuid).keys;return true;
}
bool retire_history(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence actual;if(!observe_history(binding,request,actual,e))return false;
    auto& op=terrain_history_operations.at(request.action_uuid);if(op.retired)return fail(e,"Terrain history already retired");op.retired=true;return true;
}
