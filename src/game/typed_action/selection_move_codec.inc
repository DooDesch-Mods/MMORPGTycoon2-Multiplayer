bool selection_move_state_valid(const SelectionMoveState&);
bool selection_history(const Request& r){
    return (r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)&&
        r.canonical_record_key=="selection:"+r.target_action_uuid;
}
bool selection_history_effect(const Evidence& e){
    if(!e.selection_move_before||!e.selection_move_after||
       !selection_move_state_valid(*e.selection_move_before)||!selection_move_state_valid(*e.selection_move_after)||
       e.selection_move_before->rows.size()!=e.selection_move_after->rows.size()||e.before!=e.after||
       !e.undo_confirmed||e.selection_confirmed||e.target_retired||e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    bool changed=false;
    for(size_t i=0;i<e.selection_move_before->rows.size();++i){const auto& before=e.selection_move_before->rows[i];const auto& after=e.selection_move_after->rows[i];
        if(before.type!=after.type||before.variant!=after.variant)return false;changed|=before!=after;}
    return changed;
}
bool selection_move_intent_valid(const SelectionMoveIntent& v){
    if(v.rows.size()<2||v.rows.size()>max_payload)return false;
    std::set<std::string> keys;bool changed=false;
    for(const auto& row:v.rows){
        if(!key(row.key)||!keys.insert(row.key).second||!valid_building(row.before)||!valid_building(row.after)||
           !row.before.alive||!row.after.alive||row.before.type!=row.after.type||row.before.variant!=row.after.variant)return false;
        changed|=row.before!=row.after;
    }
    return changed;
}
json selection_move_intent_json(const SelectionMoveIntent& v){
    json rows=json::array();for(const auto& row:v.rows)rows.push_back({{"key",row.key},{"before",building_json(row.before)},{"after",building_json(row.after)}});return rows;
}
SelectionMoveIntent selection_move_intent_read(const json& j){
    if(!j.is_array()||j.size()<2||j.size()>max_payload)throw std::runtime_error("Selection Move row span invalid");
    SelectionMoveIntent v;for(const auto& row:j){exact(row,{"key","before","after"});v.rows.push_back({row.at("key").get<std::string>(),building_read(row.at("before"),false),building_read(row.at("after"),false)});}
    if(!selection_move_intent_valid(v))throw std::runtime_error("Selection Move complete intent invalid");return v;
}
json selection_move_state_json(const std::optional<SelectionMoveState>& v){
    if(!v)return nullptr;json rows=json::array();for(const auto& row:v->rows)rows.push_back(building_json(row));return rows;
}
std::optional<SelectionMoveState> selection_move_state_read(const json& j){
    if(j.is_null())return {};if(!j.is_array()||j.size()<2||j.size()>max_payload)throw std::runtime_error("Selection Move evidence span invalid");
    SelectionMoveState v;for(const auto& row:j){auto value=building_read(row,false);if(!value.alive)throw std::runtime_error("Selection Move evidence target absent");v.rows.push_back(std::move(value));}return v;
}
bool selection_move_state_valid(const SelectionMoveState& v){
    return v.rows.size()>=2&&v.rows.size()<=max_payload&&std::all_of(v.rows.begin(),v.rows.end(),[](const auto& row){return valid_building(row)&&row.alive;});
}
bool selection_move_effect(const Request& r,const Evidence& e){
    if(!selection_move_intent_valid(r.selection_move)||!e.selection_move_before||!e.selection_move_after||
       !selection_move_state_valid(*e.selection_move_before)||!selection_move_state_valid(*e.selection_move_after)||
       e.selection_move_before->rows.size()!=r.selection_move.rows.size()||e.selection_move_after->rows.size()!=r.selection_move.rows.size()||
       e.before!=e.after||e.target_retired||!e.undo_confirmed||!e.selection_confirmed||e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    for(size_t i=0;i<r.selection_move.rows.size();++i)if(e.selection_move_before->rows[i]!=r.selection_move.rows[i].before||e.selection_move_after->rows[i]!=r.selection_move.rows[i].after)return false;
    return true;
}
