bool valid_dungeon_gizmo_state(const DungeonGizmoState&);
json dungeon_gizmo_state_json(const DungeonGizmoState&);
DungeonGizmoState dungeon_gizmo_state_read(const json&);
bool prop_text_valid(const std::string& text){
    if(text.size()>4096||text.find('\0')!=std::string::npos)return false;
    try{(void)json(text).dump();return true;}catch(const json::type_error&){return false;}
}
bool prop_text_intent_valid(const PropTextIntent& v){
    return (v.kind==PropTextKind::Name||v.kind==PropTextKind::Description)&&
        prop_text_valid(v.text)&&
        (v.kind!=PropTextKind::Name||region_name(v.text))&&
        (v.gizmo_scene.empty()||(v.kind==PropTextKind::Name&&game::dungeon_gizmo_identity::scene(v.gizmo_scene)));
}
bool prop_text_state_valid(const PropTextState& v){
    if(v.gizmos)return v.object==BuildingState{}&&!v.scenery&&v.name.empty()&&v.description.empty()&&!v.dungeon&&v.dungeon_name.empty()&&v.dungeon_region_name.empty()&&
        *v.gizmos!=DungeonGizmoState{}&&!v.gizmos->live&&!v.gizmos->network&&valid_dungeon_gizmo_state(*v.gizmos);
    return valid_building(v.object)&&v.object.alive&&prop_text_valid(v.name)&&prop_text_valid(v.description)&&
        (!v.scenery||(v.description.empty()&&!v.dungeon))&&prop_text_valid(v.dungeon_name)&&prop_text_valid(v.dungeon_region_name)&&
        (v.dungeon||(v.dungeon_name.empty()&&v.dungeon_region_name.empty()));
}
json prop_text_intent_json(const PropTextIntent& v){json out={{"kind",uint8_t(v.kind)},{"text",v.text}};if(!v.gizmo_scene.empty())out["gizmo_scene"]=v.gizmo_scene;return out;}
PropTextIntent prop_text_intent_read(const json& j,bool gizmo=false){
    if(gizmo)exact(j,{"kind","text","gizmo_scene"});else exact(j,{"kind","text"});const auto kind=u64(j.at("kind"));
    if(kind<1||kind>2)throw std::runtime_error("Prop text kind invalid");
    PropTextIntent v;v.kind=PropTextKind(kind);v.text=j.at("text").get<std::string>();if(gizmo){v.gizmo_scene=j.at("gizmo_scene").get<std::string>();if(v.gizmo_scene.empty())throw std::runtime_error("Gizmo Rename schema36 requires its canonical scene");}
    if(!prop_text_intent_valid(v))throw std::runtime_error("Prop text value invalid");return v;
}
json prop_text_state_json(const std::optional<PropTextState>& v){
    if(v&&v->gizmos)return {{"gizmos",dungeon_gizmo_state_json(*v->gizmos)}};
    return v?json{{"object",building_json(v->object)},{"scenery",v->scenery},{"name",v->name},{"description",v->description},
        {"dungeon",v->dungeon},{"dungeon_name",v->dungeon_name},{"dungeon_region_name",v->dungeon_region_name}}:json(nullptr);
}
std::optional<PropTextState> prop_text_state_read(const json& j,bool gizmo=false){
    if(j.is_null())return {};
    if(gizmo){exact(j,{"gizmos"});PropTextState value;value.gizmos=dungeon_gizmo_state_read(j.at("gizmos"));
        if(!prop_text_state_valid(value))throw std::runtime_error("Gizmo Rename complete authored state invalid");return value;}
    exact(j,{"object","scenery","name","description","dungeon","dungeon_name","dungeon_region_name"});PropTextState v;
    if(!j.at("scenery").is_boolean()||!j.at("dungeon").is_boolean())throw std::runtime_error("Prop text family flags invalid");
    v.object=building_read(j.at("object"),false);v.scenery=j.at("scenery").get<bool>();
    v.name=j.at("name").get<std::string>();v.description=j.at("description").get<std::string>();
    v.dungeon=j.at("dungeon").get<bool>();v.dungeon_name=j.at("dungeon_name").get<std::string>();v.dungeon_region_name=j.at("dungeon_region_name").get<std::string>();
    if(!prop_text_state_valid(v))throw std::runtime_error("Prop text state invalid");return v;
}
bool prop_text_effect(const Request& r,const Evidence& e){
    if(!prop_text_intent_valid(r.prop_text)||!e.prop_text_before||!e.prop_text_after||
       !prop_text_state_valid(*e.prop_text_before)||!prop_text_state_valid(*e.prop_text_after)||
       e.before!=e.after||e.target_retired||e.undo_confirmed||e.selection_confirmed||
       e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    if(!r.prop_text.gizmo_scene.empty()){
        if(!e.prop_text_before->gizmos||!e.prop_text_after->gizmos)return false;
        auto expected=*e.prop_text_before;unsigned changed=0;
        for(auto& scene:expected.gizmos->scenes)for(auto& row:scene.rows)if(row.key==r.canonical_record_key){
            if(scene.key!=r.prop_text.gizmo_scene||row.value.name==r.prop_text.text)return false;
            row.value.name=r.prop_text.text;++changed;}
        return changed==1&&expected==*e.prop_text_after;
    }
    if(e.prop_text_before->gizmos||e.prop_text_after->gizmos)return false;
    auto expected=*e.prop_text_before;
    if(r.prop_text.kind==PropTextKind::Name){
        if(expected.name==r.prop_text.text)return false;expected.name=r.prop_text.text;
        if(expected.dungeon){expected.dungeon_name=r.prop_text.text;expected.dungeon_region_name=r.prop_text.text;}
    }else{
        if(expected.scenery||expected.description==r.prop_text.text)return false;expected.description=r.prop_text.text;
    }
    return *e.prop_text_after==expected;
}
