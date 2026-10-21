bool dungeon_world_rows(const npc_probe::dungeon_building::Snapshot& sample,
    std::vector<world_state::LocalBinding>& rows,std::string& error){
    rows.clear();dungeon_scene_identity_runtime::Census census;
    std::vector<npc_probe::dungeon_building::BuildingOwner> owners;
    if(!npc_probe::dungeon_building::read_owners(sample,census,owners,error))return false;
    for(const auto& owner:owners)rows.push_back({world_.origin(),owner.key,owner.uid,owner.state});
    return true;
}
action::Outcome perform_dungeon_building(const action::Binding& binding,const action::Request& request,
    const action::Evidence* authority,std::string& error){
    namespace sample=npc_probe::dungeon_building;
    using A=action::Action;
    const bool history=request.action==A::UndoBuilding||request.action==A::RedoBuilding;
    if(!request.dungeon_building||!action::valid_dungeon_building_scene(request.dungeon_scene)||
       !current(binding,request.snapshot_uuid,error)||!live_state_||active_dungeon_building_){
        if(error.empty())error="Dungeon Building requires its current idle Engine and Party owner";
        return action::Outcome::RejectedNoEffect;
    }
    if(dungeon_buildings_.contains(request.action_uuid)){
        error="Dungeon Building action already attempted; no replay";return action::Outcome::Unknown;
    }
    if(dungeon_buildings_.size()>=action::history_limit){
        error="Dungeon Building retained original receipt limit reached";return action::Outcome::RejectedNoEffect;
    }
    action::Context context;uint64_t revision=0;
    if(!action::selected_session().current(context,error)||
       !ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,error)||
       (context.role==action::Role::Client)!=(authority!=nullptr)||
       (authority&&!authority->dungeon_building_effect)){
        if(error.empty())error="Dungeon Building current role or complete Host proof differs";
        return action::Outcome::RejectedNoEffect;
    }
    DungeonOperation* consumed=nullptr;uintptr_t consumed_graph=0;
    if(history){
        const auto original=dungeon_buildings_.find(request.target_action_uuid);
        if(original==dungeon_buildings_.end()||!original->second->confirmed||!original->second->retired||
           original->second->binding!=binding||original->second->request.snapshot_uuid!=request.snapshot_uuid||
           original->second->request.canonical_record_key!=request.canonical_record_key||
           original->second->request.dungeon_scene!=request.dungeon_scene){
            error="Dungeon Building history has no matching complete retired source";return action::Outcome::RejectedNoEffect;
        }
        if(!building_history_slice::graph_for(request.action,request.target_action_uuid,consumed_graph,error))
            return action::Outcome::RejectedNoEffect;
        for(auto& [id,entry]:dungeon_buildings_){
            (void)id;
            const auto& source=entry->request;
            const bool same_source=source.action_uuid==request.target_action_uuid||source.target_action_uuid==request.target_action_uuid;
            if(!same_source||entry->inverse_graph!=consumed_graph)continue;
            if(consumed||!entry->confirmed||!entry->retired||entry->binding!=binding){
                error="Dungeon Building current inverse graph has ambiguous or unconfirmed ownership";
                return action::Outcome::RejectedNoEffect;
            }
            consumed=entry.get();
        }
        if(!consumed){error="Dungeon Building native history top has no retained full inverse";return action::Outcome::RejectedNoEffect;}
        dungeon_building_graph::Proof proof;
        if(!dungeon_building_graph::read(consumed->inverse_sample,request.canonical_record_key,consumed_graph,
            consumed->inverse_destroying,consumed->inverse_moving,consumed->inverse_destination,proof,error)||
           proof.sha!=consumed->effect.inverse){
            if(error.empty())error="Dungeon Building current complete inverse differs from confirmed graph";
            return action::Outcome::RejectedNoEffect;
        }
    }
    auto operation=std::make_unique<DungeonOperation>();operation->request=request;operation->binding=binding;
    if(!read_ledger(operation->ledger_before,error))return action::Outcome::RejectedNoEffect;
    if(authority&&operation->ledger_before!=authority->before){
        error="Dungeon Building complete Client Ledger BEFORE differs from Host";return action::Outcome::RejectedNoEffect;
    }
    sample::Snapshot history_seed,children_seed;
    if(consumed&&!sample::prepare_history_seed(binding,request.snapshot_uuid,consumed->inverse_sample,history_seed,error))
        return action::Outcome::RejectedNoEffect;
    if(!sample::prepare_children(binding,request.snapshot_uuid,request,children_seed,error,consumed?&history_seed:nullptr)||
       !sample::capture(binding,request.snapshot_uuid,operation->before,error,&children_seed))
        return action::Outcome::RejectedNoEffect;
    std::vector<world_state::LocalBinding> initial_rows;
    if(!dungeon_world_rows(operation->before,initial_rows,error))return action::Outcome::RejectedNoEffect;
    for(const auto& row:initial_rows)if(world_.rows().contains(row.object_key)&&
       !world_.refresh_current(world_.origin(),row.object_key,row,error))return action::Outcome::RejectedNoEffect;
    if(authority){
        auto compared=operation->before.value;compared.random=authority->dungeon_building_effect->before.random;
        if(compared!=authority->dungeon_building_effect->before){
            error="Dungeon Building complete Client authored/Quest/reference BEFORE differs from Host";
            return action::Outcome::RejectedNoEffect;
        }
    }
    if(request.action!=A::PlaceBuilding){
        if(!history||consumed->inverse_destroying||consumed->inverse_moving){
            sample::BuildingOwner target;
            if(!dungeon_building_graph::owner(operation->before,request.canonical_record_key,target,error)||
               target.scene!=request.dungeon_scene){
                if(error.empty())error="Dungeon Building current target scene differs from request";
                return action::Outcome::RejectedNoEffect;
            }
        }
    }
    if(!live_state_->begin_dungeon_building(request,error))return action::Outcome::RejectedNoEffect;
    const auto prior_random=operation->before.value.random;
    if(authority&&!sample::seed(binding,request.snapshot_uuid,authority->dungeon_building_effect->before.random,error)){
        const auto original=error;std::string cancellation;
        if(!live_state_->cancel_dungeon_building(request,cancellation))error=cancellation+" original="+original;
        return action::Outcome::Unknown;
    }
    if(authority)operation->before.value.random=authority->dungeon_building_effect->before.random;
    auto& saved=*dungeon_buildings_.emplace(request.action_uuid,std::move(operation)).first->second;
    active_dungeon_building_=&saved.request;
    struct Reset {NativeEngine& engine;~Reset(){engine.active_dungeon_building_=nullptr;}} reset{*this};
    action::Outcome outcome=action::Outcome::RejectedNoEffect;
    if(request.action==A::PlaceBuilding)outcome=perform_place(binding,request,authority,error);
    else if(request.action==A::MoveBuilding)outcome=perform_move(binding,request,authority,error);
    else if(request.action==A::RemoveSnapshotBuilding)
        outcome=perform_remove(binding,request,authority?action::ui_capture::ReplayPurpose::ConfirmedHostApplication:
            action::ui_capture::ReplayPurpose::HostExecution,authority,error);
    else if(history)outcome=perform_building_history(binding,request,authority,error);
    else error="Dungeon Building original action family is invalid";
    if(outcome!=action::Outcome::Confirmed){
        if(outcome==action::Outcome::RejectedNoEffect){
            const auto original=error;std::string cancellation;
            if(!live_state_->cancel_dungeon_building(request,cancellation)||
               (authority&&!sample::seed(binding,request.snapshot_uuid,prior_random,cancellation))){
                error=cancellation+" original="+original;return action::Outcome::Unknown;
            }
        }
        return outcome;
    }
    if(request.action==A::RemoveSnapshotBuilding){
        if(!retire_base(binding,request,error))return action::Outcome::Unknown;
        saved.removal_finalized=true;
    }
    const auto inverse_direction=history?(request.action==A::UndoBuilding?A::RedoBuilding:A::UndoBuilding):A::UndoBuilding;
    if(!building_history_slice::graph_for(inverse_direction,history?request.target_action_uuid:request.action_uuid,
        saved.inverse_graph,error))return action::Outcome::Unknown;
    action::BuildingState before_object,after_object;
    if(request.action==A::PlaceBuilding||request.action==A::MoveBuilding){
        const auto& native=buildings_.at(request.action_uuid);before_object=native.object_before;after_object=native.state;
    }else if(request.action==A::RemoveSnapshotBuilding){
        before_object=operations_.at(request.action_uuid).object_before;after_object=before_object;after_object.alive=false;
    }else{
        const auto& native=history_.at(request.action_uuid);before_object=native.object_before;after_object=native.object_after;
    }
    uintptr_t held_node=0;
    if(!after_object.alive&&request.dungeon_scene=="world"&&
       !dungeon_building_graph::held(saved.before,request.canonical_record_key,saved.inverse_graph,held_node,error))
        return action::Outcome::Unknown;
    bool publication_attempted=false;
    if(!live_state_->finish_dungeon_building(request,authority?&authority->dungeon_building_effect->mapping:nullptr,
        saved.effect.mapping,publication_attempted,error,held_node))return action::Outcome::Unknown;
    std::vector<uintptr_t> inverse_operations;sample::Snapshot after_seed;
    if(!dungeon_building_graph::operations(saved.inverse_graph,inverse_operations,error)||
       !sample::publish_children(binding,request.snapshot_uuid,request,saved.before,
           consumed?&consumed->inverse_sample:nullptr,inverse_operations,after_seed,error)||
       !sample::capture(binding,request.snapshot_uuid,saved.after,error,&after_seed))return action::Outcome::Unknown;
    saved.effect.before=saved.before.value;saved.effect.after=saved.after.value;
    saved.inverse_moving=before_object.alive&&after_object.alive;
    saved.inverse_destroying=after_object.alive&&!saved.inverse_moving;
    saved.inverse_sample=after_object.alive?saved.after:saved.before;
    saved.inverse_destination=before_object;
    dungeon_building_graph::Proof inverse;
    if(!dungeon_building_graph::read(saved.inverse_sample,request.canonical_record_key,saved.inverse_graph,
        saved.inverse_destroying,saved.inverse_moving,saved.inverse_destination,inverse,error))return action::Outcome::Unknown;
    if(inverse.held_node!=held_node){
        error="Dungeon Building complete inverse lost its exact retained scene node";return action::Outcome::Unknown;
    }
    saved.effect.inverse=inverse.sha;
    if(!read_ledger(saved.ledger_after,error))return action::Outcome::Unknown;
    const int64_t inverse_cash=int64_t(saved.ledger_before.cash)-saved.ledger_after.cash;
    if((saved.inverse_moving&&(!inverse.cash.empty()||saved.ledger_before!=saved.ledger_after))||
       (!saved.inverse_moving&&(inverse.cash.size()!=1||int64_t(inverse.cash.front().amount)!=inverse_cash))){
        error="Dungeon Building inverse Cash differs from the actual complete original effect";return action::Outcome::Unknown;
    }
    if(authority&&saved.ledger_after!=authority->after){
        error="Dungeon Building actual complete Ledger AFTER differs from Host";return action::Outcome::Unknown;
    }
    if(authority&&saved.effect!=*authority->dungeon_building_effect){
        error="Dungeon Building actual complete authored/Quest/reference/mapping/inverse result differs from Host";
        return action::Outcome::Unknown;
    }
    std::vector<world_state::LocalBinding> before_rows,after_rows;
    if(!dungeon_world_rows(saved.before,before_rows,error)||!dungeon_world_rows(saved.after,after_rows,error)||
       world_.commit_dungeon_census(world_.origin(),revision,request.action_uuid,request.canonical_record_key,
           before_rows,after_rows,error)!=world_state::Admission::New)return action::Outcome::Unknown;
    if(consumed)consumed->inverse_graph=0;
    saved.confirmed=true;error.clear();return action::Outcome::Confirmed;
}
bool observe_dungeon_building(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error){
    const auto found=dungeon_buildings_.find(request.action_uuid);
    if(found==dungeon_buildings_.end()||found->second->request!=request||found->second->binding!=binding||
       !found->second->confirmed||!current(binding,request.snapshot_uuid,error)){
        if(error.empty())error="Dungeon Building observation has no complete original action receipt";return false;
    }
    const auto& saved=*found->second;npc_probe::dungeon_building::Snapshot actual;
    if(!npc_probe::dungeon_building::capture(binding,request.snapshot_uuid,actual,error,&saved.after)||
       actual.value!=saved.effect.after){
        if(error.empty())error="Dungeon Building current complete authored/Quest/reference state differs";return false;
    }
    if(!observe_action_base(binding,request,out,error))return false;
    if(out.before!=saved.ledger_before||out.after!=saved.ledger_after){
        error="Dungeon Building current complete Ledger differs from original receipt";return false;
    }
    out.retirement_confirmed=saved.retired;out.dungeon_building_effect=saved.effect;return true;
}
bool retire_dungeon_building(const action::Binding& binding,const action::Request& request,std::string& error){
    const auto found=dungeon_buildings_.find(request.action_uuid);
    if(found==dungeon_buildings_.end()||found->second->request!=request||found->second->binding!=binding||
       !found->second->confirmed||found->second->retired||!current(binding,request.snapshot_uuid,error)){
        if(error.empty())error="Dungeon Building retirement requires its exact unretired original receipt";return false;
    }
    auto& saved=*found->second;action::Evidence observed;
    if(!observe_dungeon_building(binding,request,observed,error))return false;
    if(!saved.removal_finalized&&!retire_base(binding,request,error))return false;
    if(request.action==action::Action::PlaceBuilding&&request.building.type=="DungeonExterior"){
        npc_probe::dungeon_building::BuildingOwner target;
        if(!dungeon_building_graph::owner(saved.after,request.canonical_record_key,target,error)||
           !ui_build_runtime::complete_dungeon_local_ui(request,target.object,target.uid,error))return false;
    }
    saved.retired=true;error.clear();return true;
}
