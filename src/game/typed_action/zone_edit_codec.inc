bool zone_edit_history(const Request& request){
    return (request.action==Action::UndoBuilding||request.action==Action::RedoBuilding)&&
        request.canonical_record_key=="zone-edit:"+request.target_action_uuid;
}
json zone_edit_optional(const std::optional<ZoneEditEffect>& value){
    return value?zone_edit_codec::effect_json(*value):json(nullptr);
}
std::optional<ZoneEditEffect> zone_edit_optional_read(const json& value){
    if(value.is_null())return {};
    return zone_edit_codec::effect_read(value);
}
bool zone_edit_effect(const Request& request,const Evidence& evidence){
    if((request.action!=Action::EditZone&&!zone_edit_history(request))||!evidence.zone_edit||
       !valid_zone_edit_effect(*evidence.zone_edit)||evidence.before!=evidence.after||evidence.target_retired||evidence.selection_confirmed||
       evidence.object_before!=BuildingState{}||evidence.object_after!=BuildingState{})return false;
    auto unrelated=std::make_unique<Evidence>(evidence);
    unrelated->before={};unrelated->after={};unrelated->undo_confirmed=false;
    unrelated->retirement_confirmed=false;unrelated->zone_edit.reset();
    if(*unrelated!=*std::make_unique<Evidence>())return false;
    if(request.action==Action::EditZone){
        if(!valid_zone_edit_effect(request.zone_edit,*evidence.zone_edit))return false;
        if(request.zone_edit.kind==ZoneEditKind::Respawn)
            return !evidence.undo_confirmed&&!evidence.zone_edit->inverse_operations&&evidence.before==evidence.after;
    }
    return evidence.undo_confirmed&&evidence.zone_edit->inverse_operations>0;
}
