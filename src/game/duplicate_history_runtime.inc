#include "duplicate_history_values_runtime.inc"
struct HistorySource {
    std::map<uint64_t,duplicate_history::Known> known;
    action::DuplicateState last_live;
    std::optional<HistoryBuildingPlan> buildings;
    monster_quest_retirement::Witness quest_previous;
};
struct HistoryOperation {
    action::Binding binding;action::Request request;Sample before,after;action::Evidence evidence;
    uintptr_t graph=0,inverse=0;bool attempted=false,returned=false,selection_restored=false,confirmed=false,retired=false;
    std::string cause,cleanup_cause;
    bool building=false,undo=false;HistoryServices services;monster_quest_retirement::Witness quests;
    std::map<uint64_t,child_npc::Receipt> children;std::set<std::string> related_keys;
};
std::map<std::string,HistorySource> history_sources;
std::map<std::string,HistoryOperation> history_operations;
bool history_known(Runtime& r,const Sample& value,HistorySource& source,std::string& e){
    auto next=source.known;
    for(const auto& row:value.state.created){const auto found=value.handles.find(row.key);
        if(found==value.handles.end())return q::fail(e,"Duplicate history live result lacks full UID key="+row.key);
        const auto old=source.known.find(found->second.uid);
        if(!source.known.empty()&&(old==source.known.end()||old->second.row.key!=row.key))return q::fail(e,"Duplicate history restored full UID/key differs");
        duplicate_history::Known saved;saved.object=found->second.object;saved.uid=found->second.uid;saved.row=row;
        if(!q::at(r.memory,saved.object,0x170,saved.root,e)||!q::at(r.memory,saved.object,0x178,saved.region,e)||
           !q::at(r.memory,saved.object,0x100,saved.transform,e)||saved.root!=value.world.root)return q::fail(e,e.empty()?"Duplicate history live root/Region differs":e);
        if(row.family==Family::Scenery){if(!q::at(r.memory,saved.object,0x268,saved.scenery,e)||!q::at(r.memory,saved.object,0x270,saved.height,e)||
            !q::at(r.memory,saved.object,0x275,saved.automatic,e))return false;}
        else if(row.family==Family::Group&&!prop_text::text(r,saved.object+0x38,saved.name,e))return false;
        next[saved.uid]=std::move(saved);
    }
    if(next.size()!=value.state.created.size())return q::fail(e,"Duplicate history live complete created count differs");
    source.known=std::move(next);source.last_live=value.state;return true;
}
bool history_graph(Runtime& r,uintptr_t graph,bool undo,const HistorySource& source,const action::DuplicateIntent& intent,std::string& e){
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& why){return group_edit::find(r,uid,object,why);};
    auto text=[&](uintptr_t pointer,std::string& result,std::string& why){return prop_text::text(r,pointer,result,why);};
    duplicate_history::Graph value;
    if(!duplicate_history::code(r.memory,r.image,e)||!duplicate_history::read(r.memory,r.image,graph,undo,source.known,find,text,value,e))return false;
    if(!duplicate_history::cash_equal(value.cash,duplicate_history::cash(intent,undo)))return q::fail(e,"Duplicate history original signed cash node set differs");
    std::set<std::string> expected,actual(value.targets.begin(),value.targets.end());
    for(const auto& row:intent.rows)expected.insert(row.family==Family::Group?"group:"+row.created_keys.front():row.created_keys.front());
    return expected==actual||q::fail(e,"Duplicate history original top-level created set differs");
}
bool remember_history(Runtime& r,const Operation& op,std::string& e){
    if(history_sources.contains(op.request.action_uuid))return q::fail(e,"Duplicate source history already registered");
    HistorySource source;if(!history_known(r,op.after,source,e)||!history_graph(r,op.native.graph,true,source,op.request.duplicate,e)||
       !building_history_slice::remember_duplicate(op.request.action_uuid,op.native.graph,e))return false;
    history_sources.emplace(op.request.action_uuid,std::move(source));return true;
}
bool history_local_selection(Runtime& r,const Sample& before,std::vector<Handle>& selection,std::array<uint8_t,9>& filter,uintptr_t& camera,std::string& e){
    std::vector<uintptr_t> raw,filtered;uintptr_t manager=0;
    if(!group_reader::selection(r.memory,r.image,before.world.selection,raw,filtered,e)||!q::at(r.memory,before.world.selection,0xc8,filter,e)||
       !q::at(r.memory,r.image,0x13df440,manager,e)||!q::at(r.memory,manager,0x60,camera,e))return false;
    if(std::any_of(filter.begin(),filter.end(),[](auto n){return n>1;}))return q::fail(e,"Duplicate history local Selection filter invalid");
    for(auto pointer:raw){uint64_t uid=0;uintptr_t found=0;
        if(!q::at(r.memory,pointer,0x18,uid,e)||!uid||!group_edit::find(r,uid,found,e)||found!=pointer)return q::fail(e,e.empty()?"Duplicate history local Selection full UID differs":e);
        selection.push_back({pointer,uid});}return true;
}
bool history_restore(Runtime& r,const Sample& before,const std::vector<Handle>& selection,const std::array<uint8_t,9>& filter,
                     uintptr_t camera,const HistorySource& source,std::string& e){
    std::vector<uintptr_t> desired;
    for(const auto& row:selection){uintptr_t found=0;if(!group_edit::find(r,row.uid,found,e))return false;
        if(found==row.object)desired.push_back(found);
        else if(found||!source.known.contains(row.uid))return q::fail(e,"Duplicate history changed unrelated selected full UID");}
    const duplicate_calls::Array array{r.image+0x1748c50,desired.data(),int32_t(desired.size()),int32_t(desired.size())};
    const auto selected=duplicate_calls::select(r.image,before.world.selection,array);
    const auto restored=group_edit::filter_call(r.image,before.world.selection,filter);
    if(selected||restored)return q::fail(e,"Duplicate history Selection/filter restore failed select_seh="+std::to_string(selected)+" filter_seh="+std::to_string(restored));
    std::vector<uintptr_t> raw,filtered;std::array<uint8_t,9> actual{};uintptr_t manager=0,current=0;
    if(!group_reader::selection(r.memory,r.image,before.world.selection,raw,filtered,e)||!q::at(r.memory,before.world.selection,0xc8,actual,e)||
       !q::at(r.memory,r.image,0x13df440,manager,e)||!q::at(r.memory,manager,0x60,current,e))return false;
    return (desired==raw&&actual==filter&&current==camera)||q::fail(e,"Duplicate history restored local Selection/filter/camera differs");
}
bool history_survivors(const Sample& before,const Sample& after,const HistorySource& source,std::string& e){
    if(!same_binding(before,after))return q::fail(e,"Duplicate history current World/manager owners changed");
    for(const auto& [key,handle]:before.handles){if(source.known.contains(handle.uid))continue;
        const auto found=after.handles.find(key);
        if(found==after.handles.end()||found->second!=handle)return q::fail(e,"Duplicate history replaced unrelated full UID key="+key);}
    return true;
}
action::Outcome perform_history(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);uint64_t revision=0;
    if(!r||!r->ready||GetCurrentThreadId()!=r->thread||request.snapshot_uuid!=net::current_snapshot_id()||
       (request.action!=action::Action::UndoBuilding&&request.action!=action::Action::RedoBuilding)||
       request.canonical_record_key!="duplicate:"+request.target_action_uuid||bool(authority)==Config::instance().is_host||
       !contract::pending(typed_action_runtime::controller_status(),request,revision,e)){
        if(e.empty())e="Duplicate history exact pending direction/role/current snapshot unavailable";return action::Outcome::RejectedNoEffect;}
    if(history_operations.contains(request.action_uuid)){e="Duplicate history already attempted; no replay";return action::Outcome::Unknown;}
    const auto original=operations.find(request.target_action_uuid);const auto remembered=history_sources.find(request.target_action_uuid);
    if(original==operations.end()||remembered==history_sources.end()||original->second.binding!=binding||!original->second.confirmed||!original->second.retired){
        e="Duplicate history lacks current confirmed original whole transaction";return action::Outcome::RejectedNoEffect;}
    auto& source=remembered->second;const auto& birth=original->second;const bool undo=request.action==action::Action::UndoBuilding;
    const bool building=std::any_of(source.known.begin(),source.known.end(),[](const auto& value){return value.second.row.family==Family::Building;});
    Sample before;uintptr_t graph=0;
    if(!duplicate_code::check(r->memory,r->image,e)||!sample_duplicate(*r,binding,&birth.request.duplicate,undo?birth.created:std::map<uint64_t,std::string>{},before,e)||
       !same_binding(birth.after,before)||!building_history_slice::graph_for(request.action,request.target_action_uuid,graph,e))return action::Outcome::RejectedNoEffect;
    if(undo&&!history_known(*r,before,source,e))return action::Outcome::RejectedNoEffect;
    HistoryBuildingPlan plan;HistoryServices before_services;monster_quest_retirement::Witness before_quests;
    const auto lease=child_lease(binding,request,request.target_action_uuid);std::vector<uintptr_t> quest_order;
    if(building){
        if(undo){if(!history_building_plan(*r,binding,before,graph,source.known,request,plan,e))return action::Outcome::RejectedNoEffect;
            before_services=plan.services;before_quests=plan.quests;
        }else{
            if(!source.buildings){e="Duplicate Building Redo lacks its complete original inverse receipts";return action::Outcome::RejectedNoEffect;}
            plan=*source.buildings;
            if(!history_building_graph(*r,graph,source.known,lease,plan,quest_order,e)||
               !history_service_world(*r,before,before_services,e)||
               !npc_probe::world_quest_history::read(binding,request.snapshot_uuid,before_quests,e,&source.quest_previous))return action::Outcome::RejectedNoEffect;
        }
        if(!history_effect_witness(before,before_services,before_quests,plan.related_keys,history_children(plan.children,!undo),before.state,e))
            return action::Outcome::RejectedNoEffect;
    }
    if((!building||undo)&&!history_graph(*r,graph,undo,source,birth.request.duplicate,e))return action::Outcome::RejectedNoEffect;
    const auto cash=duplicate_history::cash(birth.request.duplicate,undo);auto projected=before.state;
    if(undo)projected.created.clear();else projected.created=source.last_live.created;
    if(authority&&(!action::valid_duplicate_history_effect(request,*authority)||authority->before!=before.money||
       authority->duplicate_before!=before.state||authority->duplicate_cash!=cash)){
        e="Duplicate history Client complete prior/cash differs from Host";return action::Outcome::RejectedNoEffect;}
    if(!action::duplicate_history_wire_fits(request,before.state,projected,cash,before.money,e))return action::Outcome::RejectedNoEffect;
    std::vector<Handle> selection;std::array<uint8_t,9> filter{};uintptr_t camera=0;
    if(!history_local_selection(*r,before,selection,filter,camera,e))return action::Outcome::RejectedNoEffect;
    auto& op=history_operations.emplace(request.action_uuid,HistoryOperation{binding,request,before}).first->second;
    op.graph=graph;op.attempted=true;op.building=building;op.undo=undo;op.related_keys=plan.related_keys;
    const auto result=building_history_slice::perform(request.action,request.target_action_uuid,e);
    if(result==action::Outcome::RejectedNoEffect){op.attempted=false;op.cause=e;return result;}
    op.returned=result==action::Outcome::Confirmed;const auto original_cause=e;
    auto fail=[&](const std::string& why){op.cause=why;std::string cleanup;
        if(history_restore(*r,before,selection,filter,camera,source,cleanup))op.selection_restored=true;
        else{op.cleanup_cause=cleanup;op.cause+="; cleanup: "+cleanup;}e=op.cause;return action::Outcome::Unknown;};
    if(!op.returned)return fail(original_cause);
    const auto opposite=undo?action::Action::RedoBuilding:action::Action::UndoBuilding;
    if(!building_history_slice::graph_for(opposite,request.target_action_uuid,op.inverse,e))return fail(e);
    if(building&&undo){
        if(!history_building_graph(*r,op.inverse,source.known,lease,plan,quest_order,e))return fail(e);
        for(const auto& [uid,child]:plan.children){(void)uid;if(!child_npc::confirm_retired(lease,child,e))return fail(e);}
        op.children=plan.children;
    }
    auto restore_children=[&](Sample& current,std::string& why){
        if(!building||undo)return true;
        auto owner=[&](uintptr_t p,std::string& key,std::string& cause){return external_owner(*r,current,p,key,cause);};
        for(const auto& [uid,child]:plan.children){child_npc::Parent parent;child_npc::Receipt restored;
            if(!child_parent(*r,current,child.parent.key,parent,why)||
               !child_npc::confirm_restored(lease,child,parent,owner,restored,why)||!child_alias(current,restored,why))return false;
            op.children.emplace(uid,std::move(restored));
        }return true;
    };
    if(!sample_duplicate(*r,binding,&birth.request.duplicate,undo?std::map<uint64_t,std::string>{}:birth.created,op.after,e,restore_children))return fail(e);
    if(building){
        std::set<uintptr_t> retired;std::vector<duplicate_quest_history::OwnerRebind> restored;
        std::set<std::string> changed_keys,retired_keys,restored_keys;
        for(auto pointer:plan.quest_owners)changed_keys.insert(plan.quests.owners.at(pointer).key);
        for(const auto& [uid,child]:plan.children){if(!child.present)continue;
            if(undo){retired.insert(child.child.row.local_pointer);retired_keys.insert(child.child.key);}
            else{const auto& actual=op.children.at(uid);restored.push_back({child.child.row.local_pointer,actual.child.row.local_pointer,
                actual.child.row.local_uid,actual.child.key});restored_keys.insert(actual.child.key);}
        }
        monster_quest_retirement::Witness previous;
        if(!duplicate_quest_history::previous_owners(before_quests,plan.quests,restored,retired,previous,e)||
           !history_service_world(*r,op.after,op.services,e)||
           !npc_probe::world_quest_history::read(binding,request.snapshot_uuid,op.quests,e,&previous)||
           !duplicate_quest_history::unchanged_owners(before_quests,op.quests,changed_keys,retired_keys,restored_keys,e)||
           !history_effect_witness(op.after,op.services,op.quests,plan.related_keys,history_children(op.children,undo),op.after.state,e))return fail(e);
        if(undo&&!duplicate_service_history::absent(plan.service_removal,op.services.saved,e))return fail(e);
        if(!undo){auto find=[&](uint64_t uid,uintptr_t& p,std::string& why){return group_edit::find(*r,uid,p,why);};
            for(const auto& [uid,saved]:plan.service_removal.saved){uintptr_t object=0,owner=0;
                if(!find(uid,object,e)||!find(saved.owner_uid,owner,e)||!object||!owner||
                   !duplicate_service_history::restored(r->memory,r->image,object,owner,saved,find,e))return fail(e);}
        }
    }
    if(!history_survivors(before,op.after,source,e)||!duplicate_history::effect(undo,before.state,op.after.state,e)||
       !duplicate_history::ledger(before.money,op.after.money,cash))return fail(e.empty()?"Duplicate history original signed AddCash Ledger differs":e);
    if(!undo&&!history_known(*r,op.after,source,e))return fail(e);
    if((!building||!undo)&&!history_graph(*r,op.inverse,!undo,source,birth.request.duplicate,e))return fail(e);
    for(const auto& [uid,key]:birth.created){
        if(undo?!remove_diagnostics::retire_undone_building(request.snapshot_uuid,key,uid,e):
                 !remove_diagnostics::restore_redone_building(request.snapshot_uuid,key,uid,e))return fail(e);}
    if(!history_restore(*r,before,selection,filter,camera,source,e)){op.cause=e;return action::Outcome::Unknown;}op.selection_restored=true;
    op.evidence.before=before.money;op.evidence.after=op.after.money;op.evidence.undo_confirmed=true;
    op.evidence.duplicate_before=before.state;op.evidence.duplicate_after=op.after.state;op.evidence.duplicate_cash=cash;
    if(!action::valid_duplicate_history_effect(request,op.evidence)||(authority&&(authority->after!=op.evidence.after||authority->duplicate_after!=op.evidence.duplicate_after))){
        e=op.cause="Duplicate history complete original result differs from Host";return action::Outcome::Unknown;}
    if(building){source.buildings=std::move(plan);source.quest_previous=op.quests;}
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe_history(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);const auto found=history_operations.find(request.action_uuid);
    if(!r||GetCurrentThreadId()!=r->thread||found==history_operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)
        return q::fail(e,"Duplicate history observation lacks exact confirmed operation");
    auto& op=found->second;const auto& birth=operations.at(request.target_action_uuid);Sample current;auto children=op.children;
    auto capture_children=[&](Sample& value,std::string& why){if(!op.building||op.undo)return true;
        auto owner=[&](uintptr_t p,std::string& key,std::string& cause){return external_owner(*r,value,p,key,cause);};
        for(auto& [uid,child]:children){(void)uid;child_npc::Parent parent;child_npc::Receipt actual;
            if(!child_parent(*r,value,child.parent.key,parent,why)||
               !child_npc::capture_before(child_lease(binding,request,request.target_action_uuid),parent,owner,actual,why)||
               !child_alias(value,actual,why))return false;child=std::move(actual);}
        return true;
    };
    if(!sample_duplicate(*r,binding,&birth.request.duplicate,op.undo?std::map<uint64_t,std::string>{}:birth.created,current,e,capture_children))return false;
    if(op.building){HistoryServices services;monster_quest_retirement::Witness quests;
        if(!history_service_world(*r,current,services,e)||
           !npc_probe::world_quest_history::read(binding,request.snapshot_uuid,quests,e,&op.quests)||
           !history_effect_witness(current,services,quests,op.related_keys,history_children(children,op.undo),current.state,e))return false;
    }
    if(!same_binding(op.after,current)||op.after.state!=current.state||op.after.money!=current.money||op.after.handles!=current.handles||op.after.undo!=current.undo)
        return q::fail(e,e.empty()?"Duplicate history actual full records/UID/graph/Ledger changed before ACK":e);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire_history(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence value;if(!observe_history(binding,request,value,e))return false;auto& op=history_operations.at(request.action_uuid);
    if(op.retired)return q::fail(e,"Duplicate history already retired");op.retired=true;return true;
}
bool history_targets(const action::Binding& binding,const action::Request& request,std::map<uint64_t,std::string>& out,std::string& e){
    const auto found=operations.find(request.target_action_uuid);
    if(found==operations.end()||found->second.binding!=binding||!found->second.confirmed||!found->second.retired||
       found->second.request.snapshot_uuid!=request.snapshot_uuid||!history_sources.contains(request.target_action_uuid))
        return q::fail(e,"Duplicate history target set lacks exact current source binding");
    out=found->second.created;return true;
}
