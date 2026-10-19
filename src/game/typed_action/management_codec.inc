json management_intent_json(const ManagementIntent& v){return {{"op",uint64_t(v.op)},{"value",v.value},{"statistics",v.statistics},{"key",v.key},{"service_ordinal",v.service_ordinal},{"currency",v.currency}};}
ManagementIntent management_intent_read(const json& j){
    exact(j,{"op","value","statistics","key","service_ordinal","currency"});ManagementIntent v;const auto op=u64(j.at("op"));
    if(op<1||op>11)throw std::runtime_error("Management operation outside domain");v.op=ManagementOp(op);v.value=i32(j.at("value"));v.key=j.at("key").get<std::string>();v.service_ordinal=i32(j.at("service_ordinal"));v.currency=i32(j.at("currency"));
    const auto& a=j.at("statistics");if(!a.is_array()||a.size()!=10)throw std::runtime_error("Management statistics requires ten values");
    for(size_t n=0;n<10;++n)v.statistics[n]=i32(a[n]);if(!management_valid(v))throw std::runtime_error("Management intent invalid");return v;
}
json management_state_json(const ManagementState& v){
    json loans=json::array(),offers=json::array();for(const auto& r:v.loans)loans.push_back({{"text",r.text},{"amounts",r.amounts}});
    for(const auto& r:v.offers)offers.push_back({{"text",r.text},{"amounts",r.amounts},{"eligibility",r.eligibility},{"available",r.available}});
    json services=nullptr;if(v.services){json rows=json::array(),configs=json::array();for(const auto& row:v.services->rows)rows.push_back({{"type",row.type},{"config_key",row.config_key},{"price",row.price},{"synced",row.synced},{"allow_sync",row.allow_sync}});for(const auto& row:v.services->configs)configs.push_back({{"key",row.key},{"price",row.price}});
        services={{"building_key",v.services->building_key},{"rows",rows},{"config_present",v.services->config_present},{"configs",configs}};}
    return {{"game_masters",v.game_masters},{"release_funds",v.release_funds},{"statistics_points",v.statistics_points},{"purchased_points",v.purchased_points},
        {"statistics",v.statistics},{"spent",v.spent},{"purchased",v.purchased},{"loans",loans},{"offers",offers},{"campaign_costs",v.campaign_costs},{"marketing",v.marketing},
        {"release_present",v.release_present},{"release_type",v.release_type},{"software_version",v.software_version},{"planned_version",v.planned_version},{"character_slots",v.character_slots},{"services",services},{"lounge_employed",v.lounge_employed},{"gm_employment",v.gm_employment}};
}
ManagementState management_state_read(const json& j){
    exact(j,{"game_masters","release_funds","statistics_points","purchased_points","statistics","spent","purchased","loans","offers","campaign_costs","marketing","release_present","release_type","software_version","planned_version","character_slots","services","lounge_employed","gm_employment"});ManagementState v;
    v.lounge_employed=i32(j.at("lounge_employed"));const auto& pool=j.at("gm_employment");if(!pool.is_array()||pool.size()>50)throw std::runtime_error("Management Lounge pool invalid");for(const auto& row:pool){if(!row.is_array()||row.size()!=2)throw std::runtime_error("Management employment pair required");auto x=i32(row[0]),y=i32(row[1]);if(x<0||x>1||y<0||y>1)throw std::runtime_error("Management employment flags invalid");v.gm_employment.push_back({uint8_t(x),uint8_t(y)});}
    const auto& slots=j.at("character_slots");if(!slots.is_array()||slots.size()!=3)throw std::runtime_error("Management requires3 Character slot counts");for(size_t n=0;n<3;++n)v.character_slots[n]=i32(slots[n]);
    if(!j.at("services").is_null()){
        const auto& group=j.at("services");exact(group,{"building_key","rows","config_present","configs"});ManagementServices s;s.building_key=group.at("building_key").get<std::string>();
        if(!group.at("config_present").is_boolean()||!group.at("rows").is_array()||!group.at("configs").is_array()||group.at("rows").size()>1024||group.at("configs").size()>4096)throw std::runtime_error("Management service group bounds invalid");s.config_present=group.at("config_present").get<bool>();
        auto price=[](const json& value){if(!value.is_array()||value.size()!=2)throw std::runtime_error("Management Price requires Currency/amount pair");return std::array<int32_t,2>{i32(value[0]),i32(value[1])};};
        for(const auto& row:group.at("rows")){exact(row,{"type","config_key","price","synced","allow_sync"});if(!row.at("synced").is_boolean()||!row.at("allow_sync").is_boolean())throw std::runtime_error("Management service sync requires bool");s.rows.push_back({i32(row.at("type")),row.at("config_key").get<std::string>(),price(row.at("price")),row.at("synced").get<bool>(),row.at("allow_sync").get<bool>()});}
        for(const auto& row:group.at("configs")){exact(row,{"key","price"});s.configs.push_back({row.at("key").get<std::string>(),price(row.at("price"))});}v.services=std::move(s);
    }
    if(!j.at("release_present").is_boolean())throw std::runtime_error("Management release presence requires bool");v.release_present=j.at("release_present").get<bool>();v.release_type=i32(j.at("release_type"));
    for(const auto* key:{"software_version","planned_version"}){const auto& row=j.at(key);if(!row.is_array()||row.size()!=3)throw std::runtime_error("Management version requires3 fields");auto& out=std::string(key)=="software_version"?v.software_version:v.planned_version;for(size_t n=0;n<3;++n)out[n]=i32(row[n]);}
    const auto& costs=j.at("campaign_costs");if(!costs.is_array()||costs.size()!=9)throw std::runtime_error("Management campaign catalogue requires9 costs");for(size_t n=0;n<9;++n)v.campaign_costs[n]=i32(costs[n]);v.marketing=j.at("marketing");
    v.game_masters=i32(j.at("game_masters"));v.release_funds=i32(j.at("release_funds"));v.statistics_points=i32(j.at("statistics_points"));v.purchased_points=i32(j.at("purchased_points"));
    for(const char* key:{"statistics","spent","purchased"}){
        const auto& a=j.at(key);if(!a.is_array()||a.size()!=10)throw std::runtime_error("Management statistics state requires ten values");
        for(size_t n=0;n<10;++n){auto x=i32(a[n]);if(std::string(key)=="statistics")v.statistics[n]=x;else if(std::string(key)=="spent")v.spent[n]=x;else {if(x<0||x>1)throw std::runtime_error("Management purchase flag requires0/1");v.purchased[n]=uint8_t(x);}}
    }
    for(const char* key:{"loans","offers"}){
        const auto& a=j.at(key);if(!a.is_array()||a.size()>1024)throw std::runtime_error("Management Loan array invalid");
        for(const auto& r:a){const bool loan=std::string(key)=="loans";
            if(loan)exact(r,{"text","amounts"});else exact(r,{"text","amounts","eligibility","available"});
            if(!r.at("text").is_array()||r.at("text").size()!=3||!r.at("amounts").is_array()||r.at("amounts").size()!=(loan?4:3))throw std::runtime_error("Management Loan row extent invalid");
            if(loan){ManagementLoan row;for(size_t n=0;n<3;++n)row.text[n]=r.at("text")[n].get<std::string>();for(size_t n=0;n<4;++n)row.amounts[n]=i32(r.at("amounts")[n]);v.loans.push_back(std::move(row));}
            else {ManagementOffer row;for(size_t n=0;n<3;++n){row.text[n]=r.at("text")[n].get<std::string>();row.amounts[n]=i32(r.at("amounts")[n]);}row.eligibility=i32(r.at("eligibility"));if(!r.at("available").is_boolean())throw std::runtime_error("Management Offer available requires bool");row.available=r.at("available").get<bool>();v.offers.push_back(std::move(row));}
        }
    }
    if(!management_valid(v))throw std::runtime_error("Management state outside native domain");return v;
}
