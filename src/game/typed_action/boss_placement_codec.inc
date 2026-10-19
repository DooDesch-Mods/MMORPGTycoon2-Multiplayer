bool boss_placement_effect(const Request& request,const Evidence& evidence){
    return request.action==Action::PlaceBoss&&evidence.boss_placement_before&&evidence.boss_placement_after&&
        !evidence.target_retired&&evidence.undo_confirmed&&evidence.selection_confirmed&&
        evidence.object_before==BuildingState{}&&evidence.object_after==BuildingState{}&&
        game::boss_placement_state::effect(request.boss_placement,request.action_uuid,*evidence.boss_placement_before,*evidence.boss_placement_after)&&
        game::boss_placement_state::payment(evidence.before,evidence.after);
}
json boss_placement_json(const BossPlacementIntent& value){return json::array({value.type,value.region,value.pose,value.point,value.character_sha});}
BossPlacementIntent boss_placement_read(const json& value){
    if(!value.is_array()||value.size()!=5)throw std::runtime_error("Boss placement exact semantic intent width differs");
    BossPlacementIntent out{i32(value[0]),i32(value[1]),duplicate_bits<10>(value[2]),duplicate_bits<3>(value[3]),value[4].get<std::string>()};
    if(!game::boss_placement_reader::valid(out))throw std::runtime_error("Boss placement type/Region/pose/release point/Character witness invalid");return out;
}
json boss_placement_state_json(const std::optional<BossPlacementState>& value){
    if(!value)return nullptr;json created=nullptr;
    if(value->created)created=game::interventions::actors::encode({*value->created})[0];
    return json::array({value->random,value->release,value->untouched_sha,value->record_sha,std::move(created)});
}
std::optional<BossPlacementState> boss_placement_state_read(const json& value){
    if(value.is_null())return {};if(!value.is_array()||value.size()!=5||!value[0].is_array()||value[0].size()!=4||!value[1].is_array()||value[1].size()!=8)
        throw std::runtime_error("Boss placement complete state width differs");
    BossPlacementState out;
    for(size_t i=0;i<4;++i)out.random[i]=u64(value[0][i]);
    for(size_t i=0;i<8;++i)out.release[i]=i32(value[1][i]);
    out.untouched_sha=value[2].get<std::string>();out.record_sha=value[3].get<std::string>();
    if(!value[4].is_null())out.created=game::interventions::actors::codec::state(json::array({value[4]})).at(0);
    if(!game::boss_placement_state::valid(out))throw std::runtime_error("Boss placement canonical complete state is invalid");return out;
}
