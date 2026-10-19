bool group_selector(const std::string& v){return v.starts_with("group:")?key(v.substr(6)):key(v);}
bool group_intent_valid(const GroupIntent& v){
    if((v.kind!=GroupKind::Group&&v.kind!=GroupKind::Ungroup)||v.selected.empty()||v.selected.size()>max_payload||
       (v.kind==GroupKind::Group&&v.selected.size()<2))return false;
    std::set<std::string> keys;for(const auto& k:v.selected)if(!group_selector(k)||!keys.insert(k).second)return false;return true;
}
bool group_state_valid(const GroupState& v){return game::group_transition::valid(v,[](const auto& k){return key(k);});}
json group_intent_json(const GroupIntent& v){return {{"kind",uint8_t(v.kind)},{"selected",v.selected}};}
GroupIntent group_intent_read(const json& j){exact(j,{"kind","selected"});const auto kind=u64(j.at("kind"));
    if(kind<1||kind>2||!j.at("selected").is_array()||j.at("selected").size()>max_payload)throw std::runtime_error("Group intent kind/count invalid");
    GroupIntent v{GroupKind(kind),j.at("selected").get<std::vector<std::string>>()};if(!group_intent_valid(v))throw std::runtime_error("Group intent key/order invalid");return v;
}
json group_state_json(const std::optional<GroupState>& v){
    if(!v)return nullptr;json groups=json::array(),props=json::array();
    for(const auto& g:v->groups){json members=json::array();for(const auto& m:g.members){json row=json::array({m.key});for(auto n:m.relative)row.push_back(n);members.push_back(std::move(row));}
        groups.push_back(json::array({g.key,g.region,g.pose,std::move(members)}));}
    for(const auto& p:v->props)props.push_back(json::array({p.key,p.parent,p.region,p.pose}));
    return json::array({std::move(groups),std::move(props),v->random});
}
std::optional<GroupState> group_state_read(const json& j){
    if(j.is_null())return {};if(!j.is_array()||j.size()!=3||!j[0].is_array()||!j[1].is_array()||j[0].size()>max_payload||j[1].size()>max_payload)
        throw std::runtime_error("Group complete state span invalid");
    auto pose=[](const json& values,std::array<uint32_t,10>& out){if(!values.is_array()||values.size()!=10)throw std::runtime_error("Group pose width differs");
        for(size_t i=0;i<10;++i){const auto n=u64(values[i]);if(n>UINT32_MAX)throw std::runtime_error("Group pose bits exceed uint32");out[i]=uint32_t(n);}};
    GroupState v;
    for(const auto& row:j[0]){if(!row.is_array()||row.size()!=4||!row[3].is_array()||row[3].size()>max_payload)throw std::runtime_error("Group row/member width invalid");
        GroupRow g;g.key=row[0].get<std::string>();g.region=i32(row[1]);pose(row[2],g.pose);
        for(const auto& member:row[3]){if(!member.is_array()||member.size()!=11)throw std::runtime_error("Group member row width invalid");
            GroupMember m;m.key=member[0].get<std::string>();for(size_t i=0;i<10;++i){const auto n=u64(member[i+1]);if(n>UINT32_MAX)throw std::runtime_error("Group relative bits exceed uint32");m.relative[i]=uint32_t(n);}g.members.push_back(std::move(m));}
        v.groups.push_back(std::move(g));}
    for(const auto& row:j[1]){if(!row.is_array()||row.size()!=4)throw std::runtime_error("Group Scenery row width invalid");
        GroupProp p;p.key=row[0].get<std::string>();p.parent=row[1].get<std::string>();p.region=i32(row[2]);pose(row[3],p.pose);v.props.push_back(std::move(p));}
    if(!j[2].is_array()||j[2].size()!=4)throw std::runtime_error("Group RNG width invalid");for(size_t i=0;i<4;++i)v.random[i]=u64(j[2][i]);
    if(!group_state_valid(v))throw std::runtime_error("Group complete canonical ownership/transform invalid");return v;
}
bool group_effect(const Request& r,const Evidence& e){
    if(!group_intent_valid(r.group)||!e.group_before||!e.group_after||!group_state_valid(*e.group_before)||!group_state_valid(*e.group_after)||
       e.before!=e.after||e.target_retired||!e.undo_confirmed||!e.selection_confirmed||e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    std::string error;return game::group_transition::effect(*e.group_before,*e.group_after,r.group,error,"group:action:"+r.action_uuid);
}
GroupState group_budget_after(const GroupState& before,const GroupIntent& intent,const std::string& created_key){
    game::group_transition::Plan plan;std::string error;if(!game::group_transition::plan(before,intent,plan,error,created_key))throw std::runtime_error(error);
    auto after=before;std::erase_if(after.groups,[&](const auto& g){return std::find(plan.removed.begin(),plan.removed.end(),g.key)!=plan.removed.end();});
    for(auto& p:after.props)if(std::find(plan.added.begin(),plan.added.end(),p.key)!=plan.added.end())p.parent=intent.kind==GroupKind::Group?plan.target:"";
    if(intent.kind==GroupKind::Group){
        if(plan.create){GroupRow g;g.key=plan.target;g.region=std::find_if(before.props.begin(),before.props.end(),[&](const auto& p){return p.key==plan.added.front();})->region;after.groups.push_back(std::move(g));}
        auto& g=*std::find_if(after.groups.begin(),after.groups.end(),[&](const auto& row){return row.key==plan.target;});
        for(const auto& key_value:plan.added)g.members.push_back({key_value,{}});
        g.pose.fill(0xff7fffff);for(auto& member:g.members)member.relative.fill(0xff7fffff);
    }
    return after;
}
