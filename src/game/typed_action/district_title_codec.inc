bool district_title_valid(const DistrictTitleIntent& v){
    for(const auto value:v.position)if(!std::isfinite(std::bit_cast<float>(value)))return false;
    return true;
}
DistrictTitleIntent district_title_read(const json& j){
    if(!j.is_array()||j.size()!=3)throw std::runtime_error("District title point size differs");
    DistrictTitleIntent v;for(size_t i=0;i<3;++i){const auto n=u64(j[i]);if(n>UINT32_MAX)throw std::runtime_error("District title coordinate bits invalid");v.position[i]=uint32_t(n);}
    if(!district_title_valid(v))throw std::runtime_error("District title point is not finite");return v;
}
DistrictState district_title_after(DistrictState before,const DistrictTitleIntent& intent){
    for(size_t i=0;i<3;++i)before.values[30+i]=intent.position[i];
    before.flags[11]=1;before.manual_title_inside=true;return before;
}
bool district_title_effect(const Request& r,const Evidence& e){
    if(!district_title_valid(r.district_title)||!e.district_before||!e.district_after||!district_state_valid(*e.district_before)||
       !district_state_valid(*e.district_after)||e.before!=e.after||e.target_retired||e.undo_confirmed||e.selection_confirmed||
       e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    return *e.district_after==district_title_after(*e.district_before,r.district_title);
}
