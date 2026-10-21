bool duplicate_intent_valid(const DuplicateIntent& v){return v.rows.size()<=max_payload&&game::duplicate_transition::valid(v,[](const auto& s){return key(s);});}
bool duplicate_state_valid(const DuplicateState& v){return v.sources.size()<=max_payload&&v.created.size()<=max_payload&&game::duplicate_transition::valid(v,[](const auto& s){return key(s);});}
json duplicate_intent_json(const DuplicateIntent& v){json rows=json::array();for(const auto& r:v.rows){
    auto row=json::array({r.key,r.record_sha,uint8_t(r.family),r.region,r.cost,r.before,r.after,r.members,r.created_keys});
    if(r.captured_uptime)row.push_back(*r.captured_uptime);rows.push_back(std::move(row));}
    return json::array({std::move(rows),v.cash_position});}
template<size_t N>std::array<uint32_t,N> duplicate_bits(const json& j){
    if(!j.is_array()||j.size()!=N)throw std::runtime_error("Duplicate native bit array width differs");std::array<uint32_t,N> out{};
    for(size_t i=0;i<N;++i){const auto n=u64(j[i]);if(n>UINT32_MAX)throw std::runtime_error("Duplicate native bits exceed uint32");out[i]=uint32_t(n);}return out;
}
DuplicateFamily duplicate_family(const json& j){const auto n=u64(j);if(n<1||n>3)throw std::runtime_error("Duplicate family invalid");return DuplicateFamily(n);}
DuplicateIntent duplicate_intent_read(const json& j){
    if(!j.is_array()||j.size()!=2||!j[0].is_array()||j[0].size()>max_payload)throw std::runtime_error("Duplicate intent width/count differs");
    DuplicateIntent v;v.cash_position=duplicate_bits<3>(j[1]);
    for(const auto& r:j[0]){if(!r.is_array()||(r.size()!=9&&r.size()!=10)||!r[7].is_array()||!r[8].is_array()||r[7].size()>max_payload||r[8].size()>max_payload)
            throw std::runtime_error("Duplicate complete target width/count differs");
        v.rows.push_back({r[0].get<std::string>(),r[1].get<std::string>(),duplicate_family(r[2]),i32(r[3]),i32(r[4]),duplicate_bits<10>(r[5]),duplicate_bits<10>(r[6]),r[7].get<std::vector<std::string>>(),r[8].get<std::vector<std::string>>()});
        if(r.size()==10){if(!r[9].is_array()||r[9].size()>10)throw std::runtime_error("Duplicate captured Building uptime width/count differs");
            std::vector<int32_t> values;for(const auto& value:r[9])values.push_back(i32(value));v.rows.back().captured_uptime=std::move(values);}}
    if(!duplicate_intent_valid(v))throw std::runtime_error("Duplicate intent canonical identity/pose/cost invalid");return v;
}
json duplicate_objects(const std::vector<DuplicateObject>& rows){json out=json::array();for(const auto& r:rows)
    out.push_back(json::array({r.key,r.parent,r.record_sha,uint8_t(r.family),r.region,r.pose,r.members}));return out;}
json duplicate_state_json(const std::optional<DuplicateState>& v){if(!v)return nullptr;
    auto out=json::array({duplicate_objects(v->sources),duplicate_objects(v->created),v->untouched_sha,v->random,v->release});
    if(v->history){const auto& h=*v->history;out.push_back(json::array({h.services_sha,h.quests_sha,h.related_sha,h.related_keys}));}return out;}
std::optional<DuplicateState> duplicate_state_read(const json& j){if(j.is_null())return {};
    if(!j.is_array()||(j.size()!=5&&j.size()!=6)||!j[0].is_array()||!j[1].is_array()||j[0].size()>max_payload||j[1].size()>max_payload||!j[3].is_array()||j[3].size()!=4||!j[4].is_array()||j[4].size()!=8)
        throw std::runtime_error("Duplicate complete evidence width/count differs");
    DuplicateState v;auto rows=[&](const json& values,auto& out){for(const auto& r:values){
        if(!r.is_array()||r.size()!=7||!r[6].is_array()||r[6].size()>max_payload)throw std::runtime_error("Duplicate complete object width/count differs");
        out.push_back(DuplicateObject{r[0].get<std::string>(),r[1].get<std::string>(),r[2].get<std::string>(),duplicate_family(r[3]),i32(r[4]),duplicate_bits<10>(r[5]),r[6].get<std::vector<std::string>>()});}};
    rows(j[0],v.sources);rows(j[1],v.created);v.untouched_sha=j[2].get<std::string>();
    for(size_t i=0;i<4;++i)v.random[i]=u64(j[3][i]);for(size_t i=0;i<8;++i)v.release[i]=i32(j[4][i]);
    if(j.size()==6){const auto& h=j[5];
        if(!h.is_array()||h.size()!=4||!h[3].is_array()||h[3].size()>max_payload)throw std::runtime_error("Duplicate Building history witness width/count differs");
        v.history=DuplicateHistoryEffects{h[0].get<std::string>(),h[1].get<std::string>(),h[2].get<std::string>(),h[3].get<std::vector<std::string>>()};}
    if(!duplicate_state_valid(v))throw std::runtime_error("Duplicate complete evidence identity/pose/record invalid");return v;
}
bool duplicate_effect(const Request& r,const Evidence& e){
    if(!duplicate_intent_valid(r.duplicate)||!e.duplicate_before||!e.duplicate_after||!duplicate_state_valid(*e.duplicate_before)||!duplicate_state_valid(*e.duplicate_after)||
       !e.undo_confirmed||!e.selection_confirmed||e.target_retired||e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    std::string cause;if(!game::duplicate_transition::effect(r.duplicate,*e.duplicate_before,*e.duplicate_after,cause))return false;
    int64_t total=0;for(const auto& row:r.duplicate.rows)total+=row.cost;auto expected=e.before;
    auto add=[](int32_t& n,int64_t delta){const auto next=int64_t(n)+delta;if(next<INT32_MIN||next>INT32_MAX)return false;n=int32_t(next);return true;};
    if(total&&(!expected.record_present||!add(expected.cash,-total)||!add(expected.record_cash,-total)||!add(expected.record_expense,total)||
               !add(expected.line_counts[6],1)||!add(expected.line_income[6],-total)))return false;
    return expected==e.after;
}
