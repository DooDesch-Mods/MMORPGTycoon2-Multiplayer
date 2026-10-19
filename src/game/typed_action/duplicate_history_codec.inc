bool duplicate_history(const Request& r){return (r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)&&
    uuid(r.target_action_uuid)&&r.canonical_record_key=="duplicate:"+r.target_action_uuid;}
json duplicate_cash_json(const std::vector<DuplicateCash>& rows){json result=json::array();
    for(const auto& row:rows)result.push_back(json::array({row.amount,row.position}));return result;}
std::vector<DuplicateCash> duplicate_cash_read(const json& values){
    if(!values.is_array()||values.size()>max_payload)throw std::runtime_error("Duplicate history cash array count differs");
    std::vector<DuplicateCash> result;for(const auto& row:values){
        if(!row.is_array()||row.size()!=2)throw std::runtime_error("Duplicate history cash row width differs");
        result.push_back({i32(row[0]),duplicate_bits<3>(row[1])});}return result;
}
bool duplicate_history_effect(const Request& r,const Evidence& e){
    if(!duplicate_history(r)||!e.duplicate_before||!e.duplicate_after||!duplicate_state_valid(*e.duplicate_before)||
       !duplicate_state_valid(*e.duplicate_after)||e.duplicate_cash.empty()||e.duplicate_cash.size()>max_payload||
       !e.undo_confirmed||e.selection_confirmed||e.target_retired||e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    const bool undo=r.action==Action::UndoBuilding;
    for(const auto& row:e.duplicate_cash){if(undo?row.amount<0:row.amount>0)return false;
        for(auto value:row.position)if(!std::isfinite(std::bit_cast<float>(value)))return false;}
    std::string error;return game::duplicate_history::effect(undo,*e.duplicate_before,*e.duplicate_after,error)&&
        game::duplicate_history::ledger(e.before,e.after,e.duplicate_cash);
}
