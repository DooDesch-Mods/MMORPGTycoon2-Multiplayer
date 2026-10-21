json npc_history_json(const NpcHistoryProof& value){
    const auto& origin=value.origin;
    return {{"origin",{{"action",origin.action},{"key",origin.key},{"region",origin.region},
        {"before",npc_actor_json(origin.before,true)},{"after",npc_actor_json(origin.after,true)}}},
        {"cash",value.cash},{"quests_before",value.quests_before},{"quests_after",value.quests_after},
        {"references_before",value.references_before},{"references_after",value.references_after},
        {"count_before",value.count_before},{"count_after",value.count_after},
        {"completed_before",value.completed_before},{"completed_after",value.completed_after}};
}
NpcHistoryProof npc_history_read(const json& source){
    exact(source,{"origin","cash","quests_before","quests_after","references_before","references_after","count_before","count_after","completed_before","completed_after"});
    const auto& origin=source.at("origin");exact(origin,{"action","key","region","before","after"});
    const auto action=u64(origin.at("action"));if(action<15||action>17)throw std::runtime_error("NPC history original action is not Place, Remove or Move");
    NpcHistoryProof out;out.origin={uint8_t(action),origin.at("key").get<std::string>(),i32(origin.at("region")),
        npc_actor_read(origin.at("before"),true),npc_actor_read(origin.at("after"),true)};
    out.cash=i32(source.at("cash"));out.quests_before=source.at("quests_before").get<std::string>();out.quests_after=source.at("quests_after").get<std::string>();
    out.references_before=source.at("references_before").get<std::string>();out.references_after=source.at("references_after").get<std::string>();
    const auto before=u64(source.at("count_before")),after=u64(source.at("count_after"));
    if(before>4096||after>4096)throw std::runtime_error("NPC history complete Quest count exceeds its actual reader bound");
    out.count_before=uint32_t(before);out.count_after=uint32_t(after);
    out.completed_before=i32(source.at("completed_before"));out.completed_after=i32(source.at("completed_after"));
    if(!valid_npc_history(out))throw std::runtime_error("NPC history complete original proof invalid");return out;
}
bool npc_history_effect(const Request& request,const Evidence& evidence){
    if(!npc_history(request)||!evidence.npc_history_proof||
       !npc_history_membership(request.action==Action::UndoBuilding,*evidence.npc_history_proof,evidence.npc_before,evidence.npc_after)||
       !valid_npc_actor(evidence.npc_history_proof->origin.before)||!valid_npc_actor(evidence.npc_history_proof->origin.after)||
       !evidence.undo_confirmed||evidence.selection_confirmed||
       evidence.target_retired!=(evidence.npc_after.created==NpcActor{})||
       evidence.object_before!=BuildingState{}||evidence.object_after!=BuildingState{})return false;
    const auto& origin=evidence.npc_history_proof->origin;
    const std::array<int32_t,3> costs{1000,100,500};
    const auto type=origin.action==15?origin.after.type:origin.before.type;
    if(type<0||type>2)return false;
    const auto amount=origin.action==15?(request.action==Action::UndoBuilding?costs[size_t(type)]:-costs[size_t(type)]):0;
    Ledger predicted;std::string cause;
    if(evidence.npc_history_proof->cash!=amount||!game::building_history_slice::cash_effect(evidence.before,amount,predicted,cause)||predicted!=evidence.after)return false;
    const auto& expected=request.action==Action::UndoBuilding?origin.before:origin.after;
    if(expected==NpcActor{})return true;
    auto actual=evidence.npc_after.created,authored=expected;
    // SetNPCQuests reconstructs original valid definitions and computes runtime
    // rewards, rating and daily counters. Their complete actual values still
    // cross the Host result and exact Client comparison.
    actual.quests.clear();authored.quests.clear();actual.quest_count=authored.quest_count=0;
    return actual==authored;
}
bool npc_history_packet(const json& source){
    const auto& request=source.at("request");const auto action=u64(request.at("action"));
    return (action==10||action==11)&&request.at("canonical_record_key").get<std::string>().starts_with("npc-history:");
}
bool npc_history_message_read(const json& source,Message& out,std::string& error){
    if(u64(source.at("schema"))!=36)throw std::runtime_error("NPC history requires schema36");
    const auto& request=source.at("request");
    exact(request,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid"});
    const auto kind=u64(source.at("kind")),outcome=u64(source.at("outcome")),action=u64(request.at("action"));
    if(kind<1||kind>5||outcome>3||(action!=10&&action!=11))throw std::runtime_error("NPC history message enum differs");
    auto next=std::make_unique<Message>();next->kind=Kind(kind);next->outcome=Outcome(outcome);
    next->session=u64(source.at("session"));next->revision=u64(source.at("revision"));next->ack_peer=u64(source.at("ack_peer"));
    next->required_mask=u64(source.at("required_mask"));next->cause=source.at("cause").get<std::string>();
    auto& r=next->request;r.action=Action(action);r.peer=u64(request.at("peer"));r.sequence=u64(request.at("sequence"));r.host_revision=u64(request.at("host_revision"));
    r.action_uuid=request.at("action_uuid").get<std::string>();r.snapshot_uuid=request.at("snapshot_uuid").get<std::string>();
    r.canonical_record_key=request.at("canonical_record_key").get<std::string>();r.target_action_uuid=request.at("target_action_uuid").get<std::string>();
    const auto& evidence=source.at("evidence");
    exact(evidence,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","npc_before","npc_after","npc_history"});
    auto& e=next->evidence;e.before=ledger_read(evidence.at("before"));e.after=ledger_read(evidence.at("after"));
    e.target_retired=ability_bool(evidence.at("target_retired"));e.undo_confirmed=ability_bool(evidence.at("undo_confirmed"));
    e.selection_confirmed=ability_bool(evidence.at("selection_confirmed"));e.retirement_confirmed=ability_bool(evidence.at("retirement_confirmed"));
    e.npc_before=npc_state_read(evidence.at("npc_before"),true);e.npc_after=npc_state_read(evidence.at("npc_after"),true);
    if(!evidence.at("npc_history").is_null())e.npc_history_proof=npc_history_read(evidence.at("npc_history"));
    out=std::move(*next);error.clear();return true;
}
