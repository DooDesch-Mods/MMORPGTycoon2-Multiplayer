bool quest_history_effect(const Request& request,const Evidence& e){
    const auto& before=e.quest_list_before;const auto& after=e.quest_list_after;
    if(!valid_npc_quest_list_state(before)||!valid_npc_quest_list_state(after)||before==NpcQuestListState{}||after==NpcQuestListState{}||
       !e.undo_confirmed||e.target_retired||e.selection_confirmed||e.before!=e.after||e.object_before!=BuildingState{}||e.object_after!=BuildingState{}||
       before.records.size()>5||after.records.size()>5||before.targets_sha256!=after.targets_sha256)return false;
    const auto expected_count=uint64_t(before.count)-before.records.size()+after.records.size();
    int32_t expected_completed=before.completed;
    for(const auto& row:before.records)if(row.completed.total>0)--expected_completed;
    for(const auto& row:after.records){if(row.action==8||row.accepted!=npc_quest_records::DailyCount{}||row.completed!=npc_quest_records::DailyCount{})return false;}
    const auto expected_undo=request.action==Action::UndoBuilding?(before.undo_count?before.undo_count-1:UINT64_MAX):std::min(uint64_t{20},before.undo_count+1);
    return before.undo_count<=20&&after.undo_count==expected_undo&&expected_count==after.count&&expected_completed==after.completed;
}
