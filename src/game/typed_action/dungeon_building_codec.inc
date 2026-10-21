bool dungeon_building_action(Action action){return action==Action::PlaceBuilding||action==Action::RemoveSnapshotBuilding||
    action==Action::MoveBuilding||action==Action::UndoBuilding||action==Action::RedoBuilding;}
bool dungeon_building_type(const std::string& scene,const BuildingState& object){
    return object==BuildingState{}||((object.type=="DungeonExterior")== (scene=="world"));
}
bool dungeon_building_request(const Request& request){
    if(!request.dungeon_building||!dungeon_building_action(request.action)||!valid_dungeon_building_scene(request.dungeon_scene))return false;
    if(request.action==Action::PlaceBuilding)
        return request.building.alive&&valid_building(request.building)&&dungeon_building_type(request.dungeon_scene,request.building);
    return dungeon_building_type(request.dungeon_scene,request.building);
}
bool dungeon_building_execution(const Request& request,const Evidence& evidence){
    if(!dungeon_building_request(request)||!evidence.dungeon_building_effect||
       !valid_dungeon_building_effect(*evidence.dungeon_building_effect)||
       evidence.dungeon_building_effect->mapping.operation!=request.action_uuid||
       !dungeon_building_type(request.dungeon_scene,evidence.object_before)||
       !dungeon_building_type(request.dungeon_scene,evidence.object_after))return false;
    const auto& proof=*evidence.dungeon_building_effect;
    if(request.dungeon_scene=="world")return proof.before.authored.contains(request.canonical_record_key)==evidence.object_before.alive&&
        proof.after.authored.contains(request.canonical_record_key)==evidence.object_after.alive;
    const auto exterior=request.dungeon_scene.substr(0,request.dungeon_scene.size()-9);
    return proof.before.authored.contains(exterior)&&proof.after.authored.contains(exterior)&&
        std::any_of(proof.mapping.scenes.begin(),proof.mapping.scenes.end(),[&](const auto& row){return row.scene==request.dungeon_scene;});
}
json dungeon_building_state_json(const DungeonBuildingState& value){
    return {{"authored",value.authored},{"quests",value.quests},{"references",value.references},{"random",value.random}};
}
DungeonBuildingState dungeon_building_state_read(const json& value){
    exact(value,{"authored","quests","references","random"});DungeonBuildingState out;
    const auto& authored=value.at("authored");const auto& random=value.at("random");
    if(!authored.is_object()||authored.size()>4096||!random.is_array()||random.size()!=4)
        throw std::runtime_error("Dungeon Building complete authored/RNG extent differs");
    for(auto row=authored.begin();row!=authored.end();++row)out.authored.emplace(row.key(),row.value().get<std::string>());
    out.quests=value.at("quests").get<std::string>();out.references=value.at("references").get<std::string>();
    for(size_t i=0;i<4;++i)out.random[i]=u64(random[i]);
    if(!valid_dungeon_building_state(out))throw std::runtime_error("Dungeon Building authored keys/digests/RNG are invalid");return out;
}
json dungeon_building_effect_json(const DungeonBuildingEffect& value){return {{"before",dungeon_building_state_json(value.before)},
    {"after",dungeon_building_state_json(value.after)},{"mapping",game::scene_clone::encode(value.mapping)},{"inverse",value.inverse}};}
DungeonBuildingEffect dungeon_building_effect_read(const json& value){
    exact(value,{"before","after","mapping","inverse"});DungeonBuildingEffect out;
    out.before=dungeon_building_state_read(value.at("before"));out.after=dungeon_building_state_read(value.at("after"));
    std::string error;if(!game::scene_clone::decode(value.at("mapping"),out.mapping,error))throw std::runtime_error("Dungeon Building original scene mapping: "+error);
    out.inverse=value.at("inverse").get<std::string>();
    if(!valid_dungeon_building_effect(out))throw std::runtime_error("Dungeon Building original complete effect is invalid");return out;
}
bool dungeon_building_packet(const json& document){
    const auto& request=document.at("request");const auto& evidence=document.at("evidence");
    return request.contains("dungeon_building")||request.contains("dungeon_scene")||evidence.contains("dungeon_building_effect");
}
json dungeon_building_message_json(const Message& message){const auto& request=message.request;const auto& evidence=message.evidence;
    return {{"schema",uint64_t(37)},{"kind",uint64_t(message.kind)},{"session",message.session},{"revision",message.revision},
        {"ack_peer",message.ack_peer},{"required_mask",message.required_mask},{"outcome",uint64_t(message.outcome)},{"cause",message.cause},
        {"request",{{"action",uint64_t(request.action)},{"peer",request.peer},{"sequence",request.sequence},{"host_revision",request.host_revision},
            {"action_uuid",request.action_uuid},{"snapshot_uuid",request.snapshot_uuid},{"canonical_record_key",request.canonical_record_key},
            {"target_action_uuid",request.target_action_uuid},{"building",building_json(request.building)},
            {"dungeon_building",request.dungeon_building},{"dungeon_scene",request.dungeon_scene}}},
        {"evidence",{{"before",ledger_json(evidence.before)},{"after",ledger_json(evidence.after)},
            {"target_retired",evidence.target_retired},{"undo_confirmed",evidence.undo_confirmed},{"selection_confirmed",evidence.selection_confirmed},
            {"retirement_confirmed",evidence.retirement_confirmed},{"object_before",building_json(evidence.object_before)},{"object_after",building_json(evidence.object_after)},
            {"dungeon_building_effect",evidence.dungeon_building_effect?dungeon_building_effect_json(*evidence.dungeon_building_effect):json(nullptr)}}}};
}
bool dungeon_building_message_read(const json& document,Message& out,std::string& error)try{
    exact(document,{"schema","kind","session","revision","ack_peer","required_mask","outcome","cause","request","evidence"});
    if(u64(document.at("schema"))!=37)throw std::runtime_error("Dungeon Building schema requires37");
    const auto kind=u64(document.at("kind")),outcome=u64(document.at("outcome"));
    if(kind<1||kind>5||outcome>3)throw std::runtime_error("Dungeon Building message kind/outcome outside domain");
    auto storage=std::make_unique<Message>();auto& value=*storage;value.kind=Kind(kind);value.outcome=Outcome(outcome);value.session=u64(document.at("session"));value.revision=u64(document.at("revision"));
    value.ack_peer=u64(document.at("ack_peer"));value.required_mask=u64(document.at("required_mask"));value.cause=document.at("cause").get<std::string>();
    const auto& request=document.at("request");const auto& evidence=document.at("evidence");
    exact(request,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid","building","dungeon_building","dungeon_scene"});
    const auto action=u64(request.at("action"));if(action>255||!dungeon_building_action(Action(action)))throw std::runtime_error("Dungeon Building original action outside domain");
    if(!request.at("dungeon_building").is_boolean()||!request.at("dungeon_building").get<bool>())throw std::runtime_error("Dungeon Building typed owner flag must be true");
    auto& r=value.request;r.action=Action(action);r.peer=u64(request.at("peer"));r.sequence=u64(request.at("sequence"));r.host_revision=u64(request.at("host_revision"));
    r.action_uuid=request.at("action_uuid").get<std::string>();r.snapshot_uuid=request.at("snapshot_uuid").get<std::string>();
    r.canonical_record_key=request.at("canonical_record_key").get<std::string>();r.target_action_uuid=request.at("target_action_uuid").get<std::string>();
    r.building=building_read(request.at("building"));r.dungeon_building=true;r.dungeon_scene=request.at("dungeon_scene").get<std::string>();
    exact(evidence,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","object_before","object_after","dungeon_building_effect"});
    auto& e=value.evidence;e.before=ledger_read(evidence.at("before"));e.after=ledger_read(evidence.at("after"));
    for(const auto* name:{"target_retired","undo_confirmed","selection_confirmed","retirement_confirmed"})if(!evidence.at(name).is_boolean())throw std::runtime_error("Dungeon Building original receipt Boolean differs");
    e.target_retired=evidence.at("target_retired").get<bool>();e.undo_confirmed=evidence.at("undo_confirmed").get<bool>();
    e.selection_confirmed=evidence.at("selection_confirmed").get<bool>();e.retirement_confirmed=evidence.at("retirement_confirmed").get<bool>();
    e.object_before=building_read(evidence.at("object_before"));e.object_after=building_read(evidence.at("object_after"));
    if(!evidence.at("dungeon_building_effect").is_null())e.dungeon_building_effect=dungeon_building_effect_read(evidence.at("dungeon_building_effect"));
    out=std::move(value);return true;
}catch(const std::exception& exception){error=std::string("Dungeon Building action decode: ")+exception.what();return false;}
