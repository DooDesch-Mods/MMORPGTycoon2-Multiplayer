bool district_stroke_history(const Request& r){
    return (r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)&&r.canonical_record_key=="district-stroke:"+r.target_action_uuid;
}
json district_stroke_shape_json(const TerrainShape& shape){
    json result=json::array();
    for(const auto& path:shape){json points=json::array();for(const auto& p:path){points.push_back(p.x);points.push_back(p.z);}result.push_back(std::move(points));}
    return result;
}
TerrainShape district_stroke_shape_read(const json& value,size_t& remaining){
    if(!value.is_array()||value.size()>remaining)throw std::runtime_error("District stroke shape section budget exceeded");
    remaining-=value.size();TerrainShape out;
    for(const auto& row:value){
        if(!row.is_array()||row.size()%2||row.size()/2>remaining)throw std::runtime_error("District stroke point budget exceeded");
        remaining-=row.size()/2;TerrainPath path;
        for(size_t i=0;i<row.size();i+=2)path.push_back({terrain_i64(row[i]),terrain_i64(row[i+1])});
        out.push_back(std::move(path));
    }
    return out;
}
json district_stroke_intent_json(const DistrictStrokeIntent& value){
    json stamps=json::array();for(const auto& stamp:value.stamps)stamps.push_back(json::array({stamp.route,district_stroke_shape_json(stamp.shape)}));
    return {{"kind",uint8_t(value.kind)},{"region",value.region},{"geo",value.geo},{"selected",value.selected},
        {"press",value.press},{"finish",value.finish},{"color",value.color},{"water_color",value.water_color},
        {"change_color",value.change_color},{"change_water_color",value.change_water_color},
        {"clip",district_stroke_shape_json(value.clip)},{"stamps",std::move(stamps)}};
}
DistrictStrokeIntent district_stroke_intent_read(const json& j){
    exact(j,{"kind","region","geo","selected","press","finish","color","water_color","change_color","change_water_color","clip","stamps"});
    const auto kind=u64(j.at("kind"));if(kind<1||kind>3)throw std::runtime_error("District stroke kind invalid");
    DistrictStrokeIntent out;out.kind=DistrictStrokeKind(kind);out.region=i32(j.at("region"));out.geo=i32(j.at("geo"));out.selected=j.at("selected").get<std::string>();
    auto bits=[&](const char* name,auto& result){const auto& values=j.at(name);
        if(!values.is_array()||values.size()!=result.size())throw std::runtime_error("District stroke scalar extent differs");
        for(size_t i=0;i<result.size();++i)result[i]=district_property_u32(values[i]);};
    bits("press",out.press);bits("finish",out.finish);bits("color",out.color);bits("water_color",out.water_color);
    if(!j.at("change_color").is_boolean()||!j.at("change_water_color").is_boolean())throw std::runtime_error("District stroke option is not Boolean");
    out.change_color=j.at("change_color").get<bool>();out.change_water_color=j.at("change_water_color").get<bool>();
    size_t remaining=1000000;out.clip=district_stroke_shape_read(j.at("clip"),remaining);
    const auto& stamps=j.at("stamps");if(!stamps.is_array()||stamps.size()>100000)throw std::runtime_error("District stroke stamp extent exceeds budget");
    for(const auto& stamp:stamps){
        if(!stamp.is_array()||stamp.size()!=2||u64(stamp[0])>2)throw std::runtime_error("District original stamp route invalid");
        out.stamps.push_back({uint8_t(u64(stamp[0])),district_stroke_shape_read(stamp[1],remaining)});
    }
    if(!game::district_stroke_state::intent(out))throw std::runtime_error("District stroke intent is invalid");return out;
}
json district_stroke_state_json(const DistrictStrokeState& value){return json::array({value.count,value.values_sha});}
DistrictStrokeState district_stroke_state_read(const json& j){
    if(!j.is_array()||j.size()!=2)throw std::runtime_error("District stroke state extent differs");
    DistrictStrokeState out{district_property_u32(j[0]),j[1].get<std::string>()};
    if(!game::district_stroke_state::state(out))throw std::runtime_error("District stroke state invalid");return out;
}
json district_stroke_effect_json(const std::optional<DistrictStrokeEffect>& value){
    if(!value)return nullptr;
    return json::array({district_stroke_state_json(value->before),district_stroke_state_json(value->after),value->inverse_sha,value->inverse_operations});
}
std::optional<DistrictStrokeEffect> district_stroke_effect_read(const json& j){
    if(j.is_null())return {};
    if(!j.is_array()||j.size()!=4)throw std::runtime_error("District stroke effect extent differs");
    DistrictStrokeEffect out{district_stroke_state_read(j[0]),district_stroke_state_read(j[1]),j[2].get<std::string>(),district_property_u32(j[3])};
    if(!game::district_stroke_state::effect(out))throw std::runtime_error("District stroke effect invalid");return out;
}
bool district_stroke_effect(const Request& r,const Evidence& e){
    if((r.action!=Action::DistrictStroke&&!district_stroke_history(r))||!e.district_stroke||!game::district_stroke_state::effect(*e.district_stroke)||
       !e.undo_confirmed||e.target_retired||e.selection_confirmed||e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    if(e.before!=e.after||(r.action==Action::DistrictStroke&&!game::district_stroke_state::intent(r.district_stroke)))return false;
    return true;
}
