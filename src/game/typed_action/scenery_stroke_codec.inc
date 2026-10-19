bool scenery_stroke_action(Action action){return action==Action::SceneryBrushStroke||action==Action::SceneryEraseStroke;}
bool scenery_stroke_intent_valid(const SceneryStrokeIntent& v){return game::scenery_stroke_transition::valid(v,max_payload);}
bool scenery_stroke_state_valid(const SceneryStrokeState& v){return game::scenery_stroke_transition::valid(v);}
bool scenery_stroke_result_valid(const SceneryStrokeEffect& v,bool history=false){return game::scenery_stroke_transition::valid(v,[](const auto& value){return key(value);},max_payload,history);}
json scenery_stroke_json(const SceneryStrokeIntent& v){
    json stamps=json::array();for(const auto& stamp:v.stamps){json shape=json::array();
        for(const auto& path:stamp.shape){json points=json::array();for(const auto& p:path){points.push_back(p.x);points.push_back(p.z);}shape.push_back(std::move(points));}
        stamps.push_back(json::array({std::move(shape),stamp.dots}));}
    return json::array({uint8_t(v.kind),v.region,v.mode,v.type,v.variant,v.replace,v.random_variant,std::move(stamps),v.bounds,v.finish_position});
}
SceneryStrokeIntent scenery_stroke_read(const json& j){
    if(!j.is_array()||j.size()!=10||!j[7].is_array()||j[7].size()>max_payload||!j[5].is_boolean()||!j[6].is_boolean())
        throw std::runtime_error("Scenery stroke intent width/type/count differs");
    const auto kind=u64(j[0]);if(kind<1||kind>2)throw std::runtime_error("Scenery stroke kind invalid");
    SceneryStrokeIntent v;v.kind=SceneryStrokeKind(kind);v.region=i32(j[1]);v.mode=i32(j[2]);v.type=j[3].get<std::string>();v.variant=j[4].get<std::string>();
    v.replace=j[5].get<bool>();v.random_variant=j[6].get<bool>();v.bounds=duplicate_bits<6>(j[8]);v.finish_position=duplicate_bits<3>(j[9]);
    size_t points=0,dots=0;
    for(const auto& stamp:j[7]){
        if(!stamp.is_array()||stamp.size()!=2||!stamp[0].is_array()||!stamp[1].is_array()||stamp[0].size()>max_payload||stamp[1].size()>max_payload-dots)
            throw std::runtime_error("Scenery stroke stamp width/count differs");
        SceneryStrokeStamp value;dots+=stamp[1].size();
        for(const auto& path:stamp[0]){
            if(!path.is_array()||path.size()%2||path.size()/2>max_payload-points)throw std::runtime_error("Scenery stroke polygon coordinate width/count differs");
            TerrainPath row;points+=path.size()/2;for(size_t i=0;i<path.size();i+=2)row.push_back({terrain_i64(path[i]),terrain_i64(path[i+1])});value.shape.push_back(std::move(row));
        }
        for(const auto& pose:stamp[1])value.dots.push_back(duplicate_bits<10>(pose));v.stamps.push_back(std::move(value));
    }
    if(!scenery_stroke_intent_valid(v))throw std::runtime_error("Scenery stroke intent native domain differs");return v;
}
json scenery_stroke_state_json(const std::optional<SceneryStrokeState>& v){
    if(!v)return nullptr;return json::array({v->props_sha,v->bugs_sha,v->bug_count,v->random,v->region_random,v->release});
}
std::optional<SceneryStrokeState> scenery_stroke_state_read(const json& j){
    if(j.is_null())return {};if(!j.is_array()||j.size()!=6||!j[3].is_array()||j[3].size()!=4||!j[4].is_array()||j[4].size()!=4||!j[5].is_array()||j[5].size()!=8)
        throw std::runtime_error("Scenery stroke complete state width differs");
    SceneryStrokeState v;v.props_sha=j[0].get<std::string>();v.bugs_sha=j[1].get<std::string>();v.bug_count=u64(j[2]);
    for(size_t i=0;i<4;++i){v.random[i]=u64(j[3][i]);v.region_random[i]=u64(j[4][i]);}for(size_t i=0;i<8;++i)v.release[i]=i32(j[5][i]);
    if(!scenery_stroke_state_valid(v))throw std::runtime_error("Scenery stroke complete state domain differs");return v;
}
DuplicateObject scenery_stroke_object(const json& j){
    if(!j.is_array()||j.size()!=7||!j[6].is_array()||j[6].size()>max_payload)throw std::runtime_error("Scenery stroke object width/count differs");
    return {j[0].get<std::string>(),j[1].get<std::string>(),j[2].get<std::string>(),duplicate_family(j[3]),i32(j[4]),duplicate_bits<10>(j[5]),j[6].get<std::vector<std::string>>()};
}
json scenery_stroke_result_json(const std::optional<SceneryStrokeEffect>& v){
    if(!v)return nullptr;json changed=json::array();for(const auto& pair:v->changed)
        changed.push_back(json::array({duplicate_objects({pair.before})[0],duplicate_objects({pair.after})[0]}));
    return json::array({duplicate_objects(v->removed),duplicate_objects(v->created),std::move(changed),v->untouched_sha});
}
std::optional<SceneryStrokeEffect> scenery_stroke_result_read(const json& j,bool history=false){
    if(j.is_null())return {};if(!j.is_array()||j.size()!=4||!j[0].is_array()||!j[1].is_array()||!j[2].is_array()||
       j[0].size()>max_payload||j[1].size()>max_payload||j[2].size()>max_payload)throw std::runtime_error("Scenery stroke effect width/count differs");
    SceneryStrokeEffect v;v.untouched_sha=j[3].get<std::string>();
    for(const auto& row:j[0])v.removed.push_back(scenery_stroke_object(row));for(const auto& row:j[1])v.created.push_back(scenery_stroke_object(row));
    for(const auto& pair:j[2]){if(!pair.is_array()||pair.size()!=2)throw std::runtime_error("Scenery stroke changed pair width differs");
        v.changed.push_back({scenery_stroke_object(pair[0]),scenery_stroke_object(pair[1])});}
    if(!scenery_stroke_result_valid(v,history))throw std::runtime_error("Scenery stroke complete effect domain differs");return v;
}
bool scenery_stroke_effect(const Request& r,const Evidence& e){
    if(!scenery_stroke_action(r.action)||!e.scenery_stroke_before||!e.scenery_stroke_after||!e.scenery_stroke_result||
       e.target_retired||!e.undo_confirmed||!e.selection_confirmed||e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    const auto& b=e.before;const auto& a=e.after;
    if(b.record_present!=1||a.record_present!=1||b.flat_day!=a.flat_day||b.record_day!=a.record_day)return false;
    const int64_t income=int64_t(a.record_income)-b.record_income,expense=int64_t(a.record_expense)-b.record_expense,cash=int64_t(a.cash)-b.cash;
    if(income<0||expense<0||cash!=income-expense||int64_t(a.record_cash)-b.record_cash!=cash||
       int64_t(a.line_income[6])-b.line_income[6]!=cash||a.line_counts[6]<b.line_counts[6])return false;
    for(size_t i=0;i<b.line_counts.size();++i)if(i!=6&&(a.line_counts[i]!=b.line_counts[i]||a.line_income[i]!=b.line_income[i]))return false;
    if(r.scenery_stroke.kind==SceneryStrokeKind::Erase&&expense)return false;
    std::string cause;return game::scenery_stroke_transition::effect(r.scenery_stroke,*e.scenery_stroke_before,*e.scenery_stroke_after,*e.scenery_stroke_result,
        [](const auto& value){return key(value);},max_payload,cause);
}
