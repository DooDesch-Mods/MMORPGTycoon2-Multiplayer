bool npc_quest_removal_valid(const NpcQuestRemovalProof& proof,const std::string& target){
    const auto& value=proof.quests;
    if(!game::npc_target_key::valid(target)||!digest(value.before_sha256)||!digest(value.after_sha256)||
       !digest(proof.references_before)||!digest(proof.references_after)||value.before_count>4096||
       value.after_count>value.before_count||value.before_count-value.after_count!=value.removed.size()||
       value.completed_before<0||value.completed_after<0||uint32_t(value.completed_before)>value.before_count||
       uint32_t(value.completed_after)>value.after_count)return false;
    int64_t completed=value.completed_before;std::set<std::pair<std::string,int32_t>> unique;
    for(const auto& row:value.removed){
        if(!game::npc_target_key::valid(row.owner_key)||!valid_quest(row.record)||
           (row.owner_key!=target&&row.record.destination_key!=target)||
           !unique.emplace(row.owner_key,row.record.sequence_id).second)return false;
        if(row.record.completed.total>0)--completed;
    }
    return completed==value.completed_after&&
        (value.removed.empty()?value.before_sha256==value.after_sha256:value.before_sha256!=value.after_sha256);
}
json npc_quest_removal_json(const NpcQuestRemovalProof& proof){
    return {{"quests",monster_quests_json(proof.quests)},{"references_before",proof.references_before},
        {"references_after",proof.references_after}};
}
NpcQuestRemovalProof npc_quest_removal_read(const json& source){
    exact(source,{"quests","references_before","references_after"});NpcQuestRemovalProof proof;
    proof.quests=monster_quests_read(source.at("quests"));
    proof.references_before=source.at("references_before").get<std::string>();
    proof.references_after=source.at("references_after").get<std::string>();return proof;
}
bool npc_quest_removal_packet(const json& source){return u64(source.at("request").at("action"))==uint64_t(Action::RemoveNPC);}
bool npc_quest_removal_message_read(const json& source,Message& out,std::string& error){
    exact(source,{"schema","kind","session","revision","ack_peer","required_mask","outcome","cause","request","evidence"});
    if(u64(source.at("schema"))!=36)throw std::runtime_error("NPC Quest removal requires schema36");
    const auto& request=source.at("request");
    exact(request,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid"});
    if(u64(request.at("action"))!=uint64_t(Action::RemoveNPC))throw std::runtime_error("NPC Quest removal schema36 contains another action");
    const auto kind=u64(source.at("kind")),outcome=u64(source.at("outcome"));
    if(kind<1||kind>5||outcome>3)throw std::runtime_error("NPC Quest removal message enum differs");
    Message next;next.kind=Kind(kind);next.outcome=Outcome(outcome);next.session=u64(source.at("session"));
    next.revision=u64(source.at("revision"));next.ack_peer=u64(source.at("ack_peer"));
    next.required_mask=u64(source.at("required_mask"));next.cause=source.at("cause").get<std::string>();
    auto& r=next.request;r.action=Action::RemoveNPC;r.peer=u64(request.at("peer"));r.sequence=u64(request.at("sequence"));
    r.host_revision=u64(request.at("host_revision"));r.action_uuid=request.at("action_uuid").get<std::string>();
    r.snapshot_uuid=request.at("snapshot_uuid").get<std::string>();r.canonical_record_key=request.at("canonical_record_key").get<std::string>();
    r.target_action_uuid=request.at("target_action_uuid").get<std::string>();
    const auto& evidence=source.at("evidence");
    exact(evidence,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","npc_before","npc_after","npc_quest_removal"});
    auto& e=next.evidence;e.before=ledger_read(evidence.at("before"));e.after=ledger_read(evidence.at("after"));
    e.target_retired=ability_bool(evidence.at("target_retired"));e.undo_confirmed=ability_bool(evidence.at("undo_confirmed"));
    e.selection_confirmed=ability_bool(evidence.at("selection_confirmed"));e.retirement_confirmed=ability_bool(evidence.at("retirement_confirmed"));
    e.npc_before=npc_state_read(evidence.at("npc_before"),true);e.npc_after=npc_state_read(evidence.at("npc_after"),true);
    e.npc_quest_removal=npc_quest_removal_read(evidence.at("npc_quest_removal"));
    out=std::move(next);error.clear();return true;
}
