bool scenery_stroke_history(const Request& r){return (r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)&&
    uuid(r.target_action_uuid)&&r.canonical_record_key=="scenery-stroke:"+r.target_action_uuid;}
bool scenery_stroke_history_effect(const Request& r,const Evidence& e){
    if(!scenery_stroke_history(r)||!e.scenery_stroke_before||!e.scenery_stroke_after||!e.scenery_stroke_result||
       !scenery_stroke_state_valid(*e.scenery_stroke_before)||!scenery_stroke_state_valid(*e.scenery_stroke_after)||
       !scenery_stroke_result_valid(*e.scenery_stroke_result,true)||e.duplicate_cash.size()>max_payload||
       !e.undo_confirmed||!e.selection_confirmed||e.target_retired||e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    const auto& before=*e.scenery_stroke_before;const auto& after=*e.scenery_stroke_after;const auto& effect=*e.scenery_stroke_result;
    if(effect.created.empty()&&effect.removed.empty())return false;
    if(before.props_sha==after.props_sha||before.release!=after.release||before.bugs_sha!=after.bugs_sha||before.bug_count!=after.bug_count)return false;
    for(const auto& row:e.duplicate_cash){if(!row.amount)return false;
        for(auto value:row.position)if(!std::isfinite(std::bit_cast<float>(value)))return false;}
    return game::duplicate_history::ledger(e.before,e.after,e.duplicate_cash);
}
