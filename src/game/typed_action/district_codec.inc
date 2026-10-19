bool district_intent_valid(const DistrictTextIntent& v){
    return (v.kind==DistrictTextKind::Name||v.kind==DistrictTextKind::Description)&&prop_text_valid(v.text);
}
bool district_state_valid(const DistrictState& v){
    if(v.region<0||v.geo<0||v.geo>2||!prop_text_valid(v.name)||!prop_text_valid(v.description))return false;
    for(auto n:v.pose)if(!std::isfinite(std::bit_cast<float>(n)))return false;
    for(auto n:v.values)if(!std::isfinite(std::bit_cast<float>(n)))return false;
    for(auto n:v.flags)if(n>1)return false;
    if(!v.flags[11]&&!v.manual_title_inside)return false;
    size_t points=0;
    for(const auto& path:v.shape){if(path.size()>1000000-points)return false;points+=path.size();
        for(const auto& p:path)if(p.x<-(INT64_MAX>>2)||p.x>(INT64_MAX>>2)||p.z<-(INT64_MAX>>2)||p.z>(INT64_MAX>>2))return false;}
    return v.shape.size()<=1000000;
}
json district_intent_json(const DistrictTextIntent& v){return {{"kind",uint8_t(v.kind)},{"text",v.text}};}
DistrictTextIntent district_intent_read(const json& j){
    exact(j,{"kind","text"});const auto kind=u64(j.at("kind"));
    if(kind<1||kind>2)throw std::runtime_error("District text kind invalid");
    DistrictTextIntent v{DistrictTextKind(kind),j.at("text").get<std::string>()};
    if(!district_intent_valid(v))throw std::runtime_error("District text intent invalid");return v;
}
json district_state_json(const std::optional<DistrictState>& v){
    if(!v)return nullptr;json shape=json::array();
    for(const auto& path:v->shape){json points=json::array();for(const auto& p:path){points.push_back(p.x);points.push_back(p.z);}shape.push_back(std::move(points));}
    return {{"region",v->region},{"geo",v->geo},{"constructed_day",v->constructed_day},{"saved_version",v->saved_version},
        {"name",v->name},{"description",v->description},{"pose",v->pose},{"values",v->values},{"flags",v->flags},{"manual_title_inside",v->manual_title_inside},{"shape",std::move(shape)}};
}
std::optional<DistrictState> district_state_read(const json& j){
    if(j.is_null())return {};exact(j,{"region","geo","constructed_day","saved_version","name","description","pose","values","flags","manual_title_inside","shape"});
    DistrictState v;v.region=i32(j.at("region"));v.geo=i32(j.at("geo"));v.constructed_day=i32(j.at("constructed_day"));v.saved_version=i32(j.at("saved_version"));
    v.name=j.at("name").get<std::string>();v.description=j.at("description").get<std::string>();
    if(!j.at("manual_title_inside").is_boolean())throw std::runtime_error("District title containment is not boolean");
    v.manual_title_inside=j.at("manual_title_inside").get<bool>();
    auto array=[&](const char* name,auto& out,uint64_t max){const auto& a=j.at(name);if(!a.is_array()||a.size()!=out.size())throw std::runtime_error("District scalar array size differs");
        for(size_t i=0;i<a.size();++i){const auto n=u64(a[i]);if(n>max)throw std::runtime_error("District scalar array value outside domain");out[i]=static_cast<typename std::remove_reference_t<decltype(out)>::value_type>(n);}};
    array("pose",v.pose,UINT32_MAX);array("values",v.values,UINT32_MAX);array("flags",v.flags,1);
    const auto& shape=j.at("shape");if(!shape.is_array()||shape.size()>1000000)throw std::runtime_error("District shape section span invalid");
    size_t remaining=1000000;for(const auto& points:shape){if(!points.is_array()||points.size()%2||points.size()/2>remaining)throw std::runtime_error("District shape vertex span invalid");
        remaining-=points.size()/2;TerrainPath path;for(size_t i=0;i<points.size();i+=2)path.push_back({terrain_i64(points[i]),terrain_i64(points[i+1])});v.shape.push_back(std::move(path));}
    if(!district_state_valid(v))throw std::runtime_error("District complete state invalid");return v;
}
std::string district_text_value(const DistrictTextIntent& v){return v.kind==DistrictTextKind::Name&&v.text.empty()?"{district_unnamed}":v.text;}
bool district_effect(const Request& r,const Evidence& e){
    if(!district_intent_valid(r.district_text)||!e.district_before||!e.district_after||!district_state_valid(*e.district_before)||
       !district_state_valid(*e.district_after)||e.before!=e.after||e.target_retired||e.undo_confirmed||e.selection_confirmed||
       e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    auto expected=*e.district_before;auto& value=r.district_text.kind==DistrictTextKind::Name?expected.name:expected.description;
    const auto required=district_text_value(r.district_text);if(value==required)return false;value=required;
    if(r.district_text.kind==DistrictTextKind::Name&&!expected.manual_title_inside){expected.flags[11]=0;expected.manual_title_inside=true;}
    return *e.district_after==expected;
}
