bool district_property_history(const Request& r){
    return (r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)&&
        r.canonical_record_key=="district-edit:"+r.target_action_uuid;
}
bool district_property_history_effect(const Request& r,const Evidence& e){
    return district_property_history(r)&&e.district_before&&e.district_after&&
        district_state_valid(*e.district_before)&&district_state_valid(*e.district_after)&&
        e.before==e.after&&e.undo_confirmed&&!e.target_retired&&!e.selection_confirmed&&
        !e.district_continent_height&&e.object_before==BuildingState{}&&e.object_after==BuildingState{};
}
uint32_t district_property_u32(const json& value){
    const auto number=u64(value);if(number>UINT32_MAX)throw std::runtime_error("District property scalar exceeds uint32");
    return uint32_t(number);
}
json district_property_json(const game::district_property::Value& value){
    return {{"kind",uint8_t(value.kind)},{"bits",value.bits}};
}
game::district_property::Value district_property_read(const json& j){
    exact(j,{"kind","bits"});const auto kind=u64(j.at("kind"));
    if(kind<1||kind>game::district_property::fields.size())throw std::runtime_error("District property kind invalid");
    const auto& values=j.at("bits");if(!values.is_array()||values.size()!=4)throw std::runtime_error("District property value width differs");
    game::district_property::Value out;out.kind=game::district_property::Kind(kind);
    for(size_t i=0;i<4;++i)out.bits[i]=district_property_u32(values[i]);
    if(!game::district_property::valid(out))throw std::runtime_error("District property value domain invalid");
    return out;
}
bool district_property_effect(const Request& request,const Evidence& evidence){
    namespace property=game::district_property;
    const auto* field=property::field(request.district_property.kind);
    std::string error;
    return request.action==Action::EditDistrict&&field&&evidence.district_before&&evidence.district_after&&
        district_state_valid(*evidence.district_before)&&district_state_valid(*evidence.district_after)&&
        evidence.before==evidence.after&&!evidence.target_retired&&!evidence.selection_confirmed&&
        evidence.undo_confirmed==field->original_history&&evidence.object_before==BuildingState{}&&evidence.object_after==BuildingState{}&&
        property::effect(*evidence.district_before,request.district_property,evidence.district_continent_height,*evidence.district_after,error);
}
