    if(Config::instance().is_host){
        const auto id=job.value("action_uuid",std::string{});const auto found=state.operations.find(id);
        const auto ordered=typed_action_runtime::controller_status();
        const auto unknown=[&](const std::string& cause){job["cause"]=cause;job["unknown"]=true;result=job;return fail(error,cause);};
        if(found==state.operations.end()||!ordered.value("started",false)||ordered.value("locked",true)||ordered.value("action_uuid",std::string{})!=id)
            return unknown("NPC Remove UI lacks exact synchronous Host operation");
        const auto& op=found->second;
        if(op.binding!=binding||op.request.action!=action::Action::RemoveNPC||op.request.action_uuid!=id||
           op.request.snapshot_uuid!=local.snapshot_uuid||!npc_target_key::request(op.request.canonical_record_key,op.request.target_action_uuid)||
           npc_target_key::argument(op.request.canonical_record_key)!=target||op.request.canonical_record_key!=local.canonical_record_key||
           op.local_pointer!=local.object||op.local_uid!=local.uid||op.attempted!=3||op.returned!=3||!op.confirmed||op.destroyprops_original_calls!=1||
           op.evidence.npc_before!=before||op.evidence.before!=money||op.quest_tasks_before!=before_tasks)
            return unknown(op.cause.empty()?"NPC Remove UI synchronous Host binding or original receipt differs":op.cause);
        action::Evidence current;
        if(!remove_observe(binding,op.request,current,error))return unknown(error);
        job["execution_confirmed"]=true;
    }else{
    npc_census::State after_census;action::NpcState after;action::Ledger after_money;undo::UndoWitness after_undo;
    json after_tasks=json::array();
    if(!read_effect(memory,state.image,region,local.uid,after,after_money,after_undo,after_census,error,
                    quest_target,&after_tasks)||
       after!=before||after_money!=money||after_undo!=undo_before||after_tasks!=before_tasks){
        job["cause"]=error.empty()?"NPC Client UI changed world before Host authority":error;
        job["unknown"]=true;result=job;return fail(error,job.at("cause").get<std::string>());}
        job["execution_confirmed"]=false;
    }
