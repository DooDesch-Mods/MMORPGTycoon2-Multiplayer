bool world_path_action(Action action){return action==Action::PlaceWall||action==Action::PlacePass||action==Action::RemoveWorldPath;}
bool world_path_history(const Request& request){
    return (request.action==Action::UndoBuilding||request.action==Action::RedoBuilding)&&
           request.canonical_record_key=="path:"+request.target_action_uuid;
}
json world_path_json(const WorldPathIntent& value){
    auto result=json::array({uint8_t(value.kind),value.start_region,value.end_region,value.type,value.node_type,
        value.name,value.start_node,value.end_node,value.start_split,value.end_split,value.start,value.end,
        value.quoted_length,value.segments,value.preview,value.preview_valid,value.graph_before_sha256});
    if(value.kind==WorldPathKind::Remove)result.push_back(value.target_path);
    return result;
}
WorldPathIntent world_path_read(const json& value){
    if(!value.is_array()||(value.size()!=17&&value.size()!=18))throw std::runtime_error("Wall/Pass semantic intent width differs");
    WorldPathIntent out;
    const auto kind=u64(value.at(0));if(kind<1||kind>3)throw std::runtime_error("Wall/Pass kind differs");
    out.kind=WorldPathKind(kind);
    if(value.size()!=(kind==3?18u:17u))throw std::runtime_error("mixed Path kind/intent width differs");
    if(kind==3)out.target_path=value.at(17).get<std::string>();
    out.start_region=i32(value.at(1));out.end_region=i32(value.at(2));
    out.type=i32(value.at(3));out.node_type=i32(value.at(4));
    out.name=value.at(5).get<std::string>();out.start_node=value.at(6).get<std::string>();
    out.end_node=value.at(7).get<std::string>();out.start_split=value.at(8).get<std::string>();
    out.end_split=value.at(9).get<std::string>();
    out.start=duplicate_bits<3>(value.at(10));out.end=duplicate_bits<3>(value.at(11));
    const auto quoted=u64(value.at(12));if(quoted>UINT32_MAX)throw std::runtime_error("Wall/Pass quoted length bits overflow");
    out.quoted_length=uint32_t(quoted);
    if(!value.at(13).is_array()||value.at(13).size()>10||!value.at(14).is_array()||value.at(14).size()>1024)
        throw std::runtime_error("Wall/Pass native geometry exceeds bounded wire");
    for(const auto& row:value.at(13))out.segments.push_back(duplicate_bits<12>(row));
    for(const auto& row:value.at(14))out.preview.push_back(duplicate_bits<3>(row));
    if(!value.at(15).is_boolean())throw std::runtime_error("Wall/Pass ghost SetWaypoints validity differs");
    out.preview_valid=value.at(15).get<bool>();
    out.graph_before_sha256=value.at(16).get<std::string>();
    if(!world_path_valid(out))throw std::runtime_error("Wall/Pass native intent invalid");
    return out;
}
json world_path_state_json(const std::optional<WorldPathState>& value){
    return value?json::array({value->complete_sha256,value->world_effect_sha256,value->paths,value->nodes}):json(nullptr);
}
std::optional<WorldPathState> world_path_state_read(const json& value){
    if(value.is_null())return {};
    if(!value.is_array()||value.size()!=4)throw std::runtime_error("Wall/Pass complete state width differs");
    const auto paths=u64(value.at(2)),nodes=u64(value.at(3));
    if(paths>UINT32_MAX||nodes>UINT32_MAX)throw std::runtime_error("Wall/Pass state counts overflow");
    WorldPathState out{value.at(0).get<std::string>(),value.at(1).get<std::string>(),
                       uint32_t(paths),uint32_t(nodes)};
    if(!world_path_state_valid(out))throw std::runtime_error("Wall/Pass complete state invalid");
    return out;
}
bool world_path_effect(const Request& request,const Evidence& evidence){
    if(!evidence.world_path_before||!evidence.world_path_after||
       !world_path_state_valid(*evidence.world_path_before)||!world_path_state_valid(*evidence.world_path_after)||
       !evidence.undo_confirmed||evidence.object_before!=BuildingState{}||evidence.object_after!=BuildingState{}||
       evidence.target_retired!=(request.action==Action::RemoveWorldPath)||
        evidence.selection_confirmed!=(request.action==Action::RemoveWorldPath))return false;
    const auto& before=*evidence.world_path_before;const auto& after=*evidence.world_path_after;
    if(before.complete_sha256==after.complete_sha256||before.paths==after.paths&&before.nodes==after.nodes)return false;
    if(world_path_action(request.action)){
        if(!world_path_valid(request.world_path)||before.complete_sha256!=request.world_path.graph_before_sha256||
            (request.action==Action::PlaceWall)!=(request.world_path.kind==WorldPathKind::Wall)||
            (request.action==Action::RemoveWorldPath)!=(request.world_path.kind==WorldPathKind::Remove))return false;
        if(request.action==Action::RemoveWorldPath){
             if(after.paths+1!=before.paths||after.nodes>before.nodes||before.nodes-after.nodes>2||
                after.world_effect_sha256!=before.world_effect_sha256)return false;
        }else if(after.paths<=before.paths||after.nodes<before.nodes)return false;
    }else if(!world_path_history(request))return false;
    return true;
}
