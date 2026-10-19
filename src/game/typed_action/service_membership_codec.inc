bool service_state_valid(const ServiceMembershipState& v){
    if(!prop_text_valid(v.building_definition)||!prop_text_valid(v.spawn_type)||v.spawn_type.empty()||v.rows.size()>1024||v.characters.size()>10000||v.configs.size()>4096||(!v.config_present&&!v.configs.empty()))return false;
    for(size_t i=0;i<v.characters.size();++i)if(v.characters[i].id!=int32_t(i)||!prop_text_valid(v.characters[i].name))return false;
    for(const auto& config:v.configs)if(!prop_text_valid(config.key))return false;
    for(const auto& row:v.rows){
        if(row.type<0||row.type>16||row.character<-1||(row.character>=0&&size_t(row.character)>=v.characters.size())||
           (!row.destination.empty()&&!key(row.destination))||!prop_text_valid(row.config_key))return false;
        for(const auto& count:row.counts)if(count.history.size()>10)return false;
        if(row.config&&!std::any_of(v.configs.begin(),v.configs.end(),[&](const auto& config){return config.key==row.config_key;}))return false;
    }return true;
}
json service_intent_json(const ServiceMembershipIntent& v){return {{"kind",uint8_t(v.kind)},{"character",v.character},{"ordinal",v.ordinal}};}
ServiceMembershipIntent service_intent_read(const json& j){exact(j,{"kind","character","ordinal"});const auto kind=i32(j.at("kind"));
    if(kind<1||kind>2)throw std::runtime_error("Service membership kind invalid");
    ServiceMembershipIntent v{ServiceMembershipKind(kind),i32(j.at("character")),i32(j.at("ordinal"))};
    if(!service_membership_intent_valid(v))throw std::runtime_error("Service membership intent invalid");return v;
}
json service_state_json(const std::optional<ServiceMembershipState>& state){
    if(!state)return nullptr;const auto& v=*state;json rows=json::array(),characters=json::array(),configs=json::array();
    for(const auto& row:v.rows){json counts=json::array();for(const auto& d:row.counts)counts.push_back({{"today",d.today},{"total",d.total},{"highest",d.highest},{"history",d.history},{"warning",d.warning}});
        rows.push_back({{"type",row.type},{"character",row.character},{"index",row.index},{"destination",row.destination},{"config_key",row.config_key},{"price",row.price},{"flags",row.flags},{"prototype",row.prototype},{"config",row.config},{"counts",std::move(counts)}});}
    for(const auto& c:v.characters)characters.push_back({{"id",c.id},{"name",c.name},{"deleted",c.deleted}});
    for(const auto& c:v.configs)configs.push_back({{"key",c.key},{"price",c.price}});
    return {{"building_definition",v.building_definition},{"spawn_type",v.spawn_type},{"rows",std::move(rows)},{"characters",std::move(characters)},{"config_present",v.config_present},{"configs",std::move(configs)}};
}
std::optional<ServiceMembershipState> service_state_read(const json& j){
    if(j.is_null())return {};exact(j,{"building_definition","spawn_type","rows","characters","config_present","configs"});ServiceMembershipState v;
    auto boolean=[](const json& value){if(!value.is_boolean())throw std::runtime_error("Service membership boolean invalid");return value.get<bool>();};
    auto span=[](const json& value,size_t maximum){if(!value.is_array()||value.size()>maximum)throw std::runtime_error("Service membership array invalid");};
    auto price=[&](const json& value){span(value,2);if(value.size()!=2)throw std::runtime_error("Service price pair invalid");return std::array<int32_t,2>{i32(value.at(0)),i32(value.at(1))};};
    v.building_definition=j.at("building_definition").get<std::string>();v.spawn_type=j.at("spawn_type").get<std::string>();v.config_present=boolean(j.at("config_present"));
    span(j.at("rows"),1024);span(j.at("characters"),10000);span(j.at("configs"),4096);
    for(const auto& c:j.at("characters")){exact(c,{"id","name","deleted"});v.characters.push_back({i32(c.at("id")),c.at("name").get<std::string>(),boolean(c.at("deleted"))});}
    for(const auto& c:j.at("configs")){exact(c,{"key","price"});v.configs.push_back({c.at("key").get<std::string>(),price(c.at("price"))});}
    for(const auto& r:j.at("rows")){exact(r,{"type","character","index","destination","config_key","price","flags","prototype","config","counts"});ServiceMember row;
        row.type=i32(r.at("type"));row.character=i32(r.at("character"));row.index=i32(r.at("index"));row.destination=r.at("destination").get<std::string>();row.config_key=r.at("config_key").get<std::string>();
        row.price=price(r.at("price"));row.prototype=boolean(r.at("prototype"));row.config=boolean(r.at("config"));
        span(r.at("flags"),3);span(r.at("counts"),2);if(r.at("flags").size()!=3||r.at("counts").size()!=2)throw std::runtime_error("Service flags/count pair invalid");
        for(size_t i=0;i<3;++i)row.flags[i]=boolean(r.at("flags").at(i));
        for(size_t i=0;i<2;++i){const auto& d=r.at("counts").at(i);exact(d,{"today","total","highest","history","warning"});auto& value=row.counts[i];
            value.today=i32(d.at("today"));value.total=i32(d.at("total"));value.highest=i32(d.at("highest"));value.warning=boolean(d.at("warning"));span(d.at("history"),10);
            for(const auto& h:d.at("history"))value.history.push_back(i32(h));}
        v.rows.push_back(std::move(row));
    }
    if(!service_state_valid(v))throw std::runtime_error("Service membership complete state invalid");return v;
}
bool service_effect(const Request& r,const Evidence& e){
    if(!e.service_membership_before||!e.service_membership_after||!service_state_valid(*e.service_membership_before)||!service_state_valid(*e.service_membership_after)||
       e.before!=e.after||e.target_retired||e.undo_confirmed||e.selection_confirmed||e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    ServiceMembershipState expected;return service_membership_expected(r.service_membership,*e.service_membership_before,expected)&&*e.service_membership_after==expected;
}
