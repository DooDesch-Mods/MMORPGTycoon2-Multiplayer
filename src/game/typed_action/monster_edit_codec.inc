json monster_history_json(const MonsterHistoryProof& value){
    const auto& origin=value.origin;
    return {{"origin",{{"action",origin.action},{"key",origin.key},{"scene",origin.scene},{"region",origin.region},
        {"before",monster_actor_json(origin.before)},{"after",monster_actor_json(origin.after)}}},
        {"quests_before",value.quests_before},{"quests_after",value.quests_after},
        {"references_before",value.references_before},{"references_after",value.references_after},
        {"count_before",value.count_before},{"count_after",value.count_after},
        {"completed_before",value.completed_before},{"completed_after",value.completed_after}};
}
MonsterHistoryProof monster_history_read(const json& source){
    exact(source,{"origin","quests_before","quests_after","references_before","references_after","count_before","count_after","completed_before","completed_after"});
    const auto& origin=source.at("origin");exact(origin,{"action","key","scene","region","before","after"});
    const auto action=u64(origin.at("action"));if(action!=29&&action!=30)throw std::runtime_error("Monster history original action is not Move/Remove");
    MonsterHistoryProof out;out.origin={uint8_t(action),origin.at("key").get<std::string>(),origin.at("scene").get<std::string>(),
        i32(origin.at("region")),monster_actor_read(origin.at("before")),monster_actor_read(origin.at("after"))};
    out.quests_before=source.at("quests_before").get<std::string>();out.quests_after=source.at("quests_after").get<std::string>();
    out.references_before=source.at("references_before").get<std::string>();out.references_after=source.at("references_after").get<std::string>();
    const auto before=u64(source.at("count_before")),after=u64(source.at("count_after"));
    if(before>4096||after>4096)throw std::runtime_error("Monster history complete Quest count exceeds current pool bound");
    out.count_before=uint32_t(before);out.count_after=uint32_t(after);
    out.completed_before=i32(source.at("completed_before"));out.completed_after=i32(source.at("completed_after"));
    if(!valid_monster_history(out))throw std::runtime_error("Monster history fixed original proof invalid");return out;
}
bool monster_history_effect(const Request& request,const Evidence& evidence){
    return monster_history(request)&&evidence.monster_history_proof&&
        monster_history_transition(request.action==Action::UndoBuilding,*evidence.monster_history_proof,evidence.monster_before,evidence.monster_after)&&
        evidence.before==evidence.after&&evidence.undo_confirmed&&!evidence.selection_confirmed&&
        evidence.target_retired==(evidence.monster_after.actor==MonsterActor{})&&
        evidence.object_before==BuildingState{}&&evidence.object_after==BuildingState{};
}
bool monster_edit_packet(const json& source){
    const auto& request=source.at("request");const auto action=u64(request.at("action"));
    return action==29||action==30||((action==10||action==11)&&request.at("canonical_record_key").get<std::string>().starts_with("monster-history:"));
}
bool monster_edit_message_read(const json& source,Message& out,std::string& error){
    exact(source,{"schema","kind","session","revision","ack_peer","required_mask","outcome","cause","request","evidence"});
    if(u64(source.at("schema"))!=36)throw std::runtime_error("Monster edit requires schema36");
    const auto& request=source.at("request");const auto action=u64(request.at("action"));
    if(action==29)exact(request,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid","monster_move"});
    else if(action==30)exact(request,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid","monster_remove"});
    else if(action==10||action==11)exact(request,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid"});
    else throw std::runtime_error("Monster edit schema36 contains another action");
    const auto kind=u64(source.at("kind")),outcome=u64(source.at("outcome"));
    if(kind<1||kind>5||outcome>3)throw std::runtime_error("Monster edit message enum differs");
    Message next;next.kind=Kind(kind);next.outcome=Outcome(outcome);next.session=u64(source.at("session"));next.revision=u64(source.at("revision"));
    next.ack_peer=u64(source.at("ack_peer"));next.required_mask=u64(source.at("required_mask"));next.cause=source.at("cause").get<std::string>();
    auto& r=next.request;r.action=Action(action);r.peer=u64(request.at("peer"));r.sequence=u64(request.at("sequence"));r.host_revision=u64(request.at("host_revision"));
    r.action_uuid=request.at("action_uuid").get<std::string>();r.snapshot_uuid=request.at("snapshot_uuid").get<std::string>();
    r.canonical_record_key=request.at("canonical_record_key").get<std::string>();r.target_action_uuid=request.at("target_action_uuid").get<std::string>();
    const auto& evidence=source.at("evidence");
    if(action==29){exact(evidence,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","monster_before","monster_after"});r.monster_move=monster_move_read(request.at("monster_move"));}
    else if(action==30){exact(evidence,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","monster_before","monster_after","monster_quests"});
        exact(request.at("monster_remove"),{"region"});r.monster_remove.region=i32(request.at("monster_remove").at("region"));next.evidence.monster_quests=monster_quests_read(evidence.at("monster_quests"));}
    else{exact(evidence,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","monster_before","monster_after","monster_history"});
        if(!evidence.at("monster_history").is_null())next.evidence.monster_history_proof=monster_history_read(evidence.at("monster_history"));}
    auto& e=next.evidence;e.before=ledger_read(evidence.at("before"));e.after=ledger_read(evidence.at("after"));
    e.target_retired=ability_bool(evidence.at("target_retired"));e.undo_confirmed=ability_bool(evidence.at("undo_confirmed"));
    e.selection_confirmed=ability_bool(evidence.at("selection_confirmed"));e.retirement_confirmed=ability_bool(evidence.at("retirement_confirmed"));
    e.monster_before=monster_state_read(evidence.at("monster_before"));e.monster_after=monster_state_read(evidence.at("monster_after"));
    out=std::move(next);error.clear();return true;
}
