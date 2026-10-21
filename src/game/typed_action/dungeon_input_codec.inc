template<size_t N>std::array<uint32_t,N> dungeon_gizmo_bits(const json& value){
    if(!value.is_array()||value.size()!=N)throw std::runtime_error("Gizmo control bit vector dimensions differ");
    std::array<uint32_t,N> out{};for(size_t i=0;i<N;++i){const auto bits=u64(value[i]);
        if(bits>UINT32_MAX)throw std::runtime_error("Gizmo control bit vector exceeds uint32");out[i]=uint32_t(bits);}return out;
}
json dungeon_scenery_row(const game::dungeon_gizmo_control::SceneryRow& row){
    return {{"key",row.key},{"type",row.value.type},{"variant",row.value.variant},{"pose",row.value.pose},{"height",row.value.height},{"automatic",row.value.automatic}};
}
game::dungeon_gizmo_control::SceneryRow dungeon_scenery_row_read(const json& row){
    exact(row,{"key","type","variant","pose","height","automatic"});game::dungeon_gizmo_control::SceneryRow decoded;
    decoded.key=row.at("key").get<std::string>();decoded.value.type=row.at("type").get<std::string>();decoded.value.variant=row.at("variant").get<std::string>();
    decoded.value.pose=dungeon_gizmo_bits<10>(row.at("pose"));const auto height=u64(row.at("height"));
    if(height>UINT32_MAX)throw std::runtime_error("Scene Scenery height bits exceed uint32");decoded.value.height=uint32_t(height);decoded.value.automatic=ability_bool(row.at("automatic"));
    if(!game::scenery_scene_key::valid(decoded.key)||!game::dungeon_gizmo_control::valid(decoded.value))throw std::runtime_error("Gizmo control Scene Scenery row differs");return decoded;
}
json dungeon_input_intent_json(const DungeonInputIntent& value){
    json shapes=json::array();for(const auto& shape:value.shapes){json paths=json::array();
        for(const auto& path:shape){json points=json::array();for(const auto& point:path){points.push_back(point.x);points.push_back(point.z);}paths.push_back(std::move(points));}
        shapes.push_back(std::move(paths));}
    json document={{"key",value.key},{"scene",value.scene},{"before_sha256",value.before_sha256},{"tool",uint8_t(value.tool)},
        {"type",value.type},{"variant",value.variant},{"position",value.position},{"rotation",value.rotation},{"scale",value.scale},
        {"character_type",value.character_type},{"rank",value.rank},{"level",value.level},{"cost",value.cost},
        {"erase",value.erase},{"shapes",std::move(shapes)},{"fill",value.fill}};
    if(value.controls){json rows=json::array();for(const auto& row:value.controls->rows){
        if(row.scenery)rows.push_back({{"scenery",dungeon_scenery_row({row.key,*row.scenery})},{"after",row.after},{"cost",row.cost}});
        else rows.push_back({{"before",game::dungeon_gizmo_portable::row({row.key,row.before})},{"after",row.after},{"cost",row.cost}});}
        document["controls"]={{"command",uint8_t(value.controls->command)},{"rows",std::move(rows)},{"cash_position",value.controls->cash_position}};}
    return document;
}
DungeonInputIntent dungeon_input_intent_read(const json& source){
    auto base=source;const auto controls=base.find("controls");json control_value;
    const bool has_controls=controls!=base.end();if(has_controls){control_value=*controls;base.erase(controls);}
    exact(base,{"key","scene","before_sha256","tool","type","variant","position","rotation","scale","character_type","rank","level","cost","erase","shapes","fill"});
    DungeonInputIntent value;value.key=source.at("key").get<std::string>();value.scene=source.at("scene").get<std::string>();
    value.before_sha256=source.at("before_sha256").get<std::string>();const auto tool=u64(source.at("tool"));
    if(tool>5)throw std::runtime_error("Dungeon input tool is outside original Door/Key/Monster/Paint/Treasure");value.tool=DungeonInputTool(tool);
    value.type=source.at("type").get<std::string>();value.variant=source.at("variant").get<std::string>();
    value.position=monster_vector<3>(source.at("position"));value.rotation=monster_vector<4>(source.at("rotation"));value.scale=monster_vector<3>(source.at("scale"));
    value.character_type=i32(source.at("character_type"));value.rank=i32(source.at("rank"));value.level=i32(source.at("level"));value.cost=i32(source.at("cost"));
    value.erase=ability_bool(source.at("erase"));value.fill=source.at("fill");
    if(!source.at("shapes").is_array())throw std::runtime_error("Dungeon input clipped shapes require an array");
    size_t remaining=1000000;
    for(const auto& shape:source.at("shapes")){
        if(!shape.is_array()||shape.empty())throw std::runtime_error("Dungeon input clipped shape has no paths");TerrainShape decoded;
        for(const auto& path:shape){if(!path.is_array()||path.size()<6||path.size()%2||path.size()/2>remaining)
                throw std::runtime_error("Dungeon input clipped path exceeds the signed-coordinate/point budget");
            remaining-=path.size()/2;TerrainPath points;for(size_t i=0;i<path.size();i+=2)points.push_back({terrain_i64(path[i]),terrain_i64(path[i+1])});decoded.push_back(std::move(points));}
        value.shapes.push_back(std::move(decoded));
    }
    if(has_controls){exact(control_value,{"command","rows","cash_position"});game::dungeon_gizmo_control::Intent decoded;
        const auto command=u64(control_value.at("command"));if(command<1||command>3)throw std::runtime_error("Gizmo control command differs");
        decoded.command=game::dungeon_gizmo_control::Command(command);decoded.cash_position=dungeon_gizmo_bits<3>(control_value.at("cash_position"));
        if(!control_value.at("rows").is_array())throw std::runtime_error("Gizmo control rows require array");
        for(const auto& row:control_value.at("rows")){
            if(row.contains("scenery")){exact(row,{"scenery","after","cost"});const auto scenery=dungeon_scenery_row_read(row.at("scenery"));
                game::dungeon_gizmo_control::Row decoded_row;decoded_row.key=scenery.key;decoded_row.scenery=scenery.value;
                decoded_row.after=dungeon_gizmo_bits<10>(row.at("after"));decoded_row.cost=i32(row.at("cost"));decoded.rows.push_back(std::move(decoded_row));continue;}
            exact(row,{"before","after","cost"});const auto& record=row.at("before");
            const auto wrapped=dungeon_gizmo_state_read({{"protected_sha256",std::string(64,'0')},{"global_random",std::array<uint64_t,4>{}},
                {"scenes",json::array({{{"key",value.scene},{"rows",json::array({record})}}})}});
            if(wrapped.scenes.size()!=1||wrapped.scenes[0].rows.size()!=1)throw std::runtime_error("Gizmo control original row differs");
            const auto& original=wrapped.scenes[0].rows[0];decoded.rows.push_back({original.key,original.value,dungeon_gizmo_bits<10>(row.at("after")),i32(row.at("cost"))});}
        value.controls=std::move(decoded);
    }
    if(!valid_dungeon_input(value))throw std::runtime_error("Dungeon input intent differs from its fixed original tool contract");return value;
}
json dungeon_input_state_json(const DungeonInputState& value){
    if(value==DungeonInputState{})return nullptr;
    json born=nullptr;
    if(value.birth){const auto& row=*value.birth;json payload;
        if(row.tool==DungeonInputTool::Monster)payload=monster_actor_json(row.monster);
        else payload=game::dungeon_gizmo_portable::row({row.key,row.gizmo});
        born={{"key",row.key},{"scene",row.scene},{"tool",uint8_t(row.tool)},{"value",std::move(payload)}};
    }
    json document={{"protected",dungeon_sound_state_json(value.protected_state)},
        {"dungeon_sha256",value.dungeon_sha256},{"scenes_sha256",value.scenes_sha256},
        {"quests_sha256",value.quests_sha256},{"layout_sha256",value.layout_sha256},{"birth",std::move(born)}};
    if(value.edits){json rows=json::array();for(const auto& row:*value.edits)rows.push_back(game::dungeon_gizmo_portable::row(row));document["edits"]=std::move(rows);}
    if(value.scenery_edits){json rows=json::array();for(const auto& row:*value.scenery_edits)rows.push_back(dungeon_scenery_row(row));document["scenery_edits"]=std::move(rows);document["scenery_release"]=*value.scenery_release;}
    return document;
}
DungeonInputState dungeon_input_state_read(const json& source){
    if(source.is_null())return {};
    auto base=source;json edits;const bool has_edits=base.contains("edits");if(has_edits){edits=base.at("edits");base.erase("edits");}
    json scenery_edits,scenery_release;const bool has_scenery=base.contains("scenery_edits");if(has_scenery){
        scenery_edits=base.at("scenery_edits");scenery_release=base.at("scenery_release");base.erase("scenery_edits");base.erase("scenery_release");}
    exact(base,{"protected","dungeon_sha256","scenes_sha256","quests_sha256","layout_sha256","birth"});DungeonInputState value;
    value.protected_state=dungeon_sound_state_read(source.at("protected"));
    if(!valid_dungeon_sound_state(value.protected_state))throw std::runtime_error("Dungeon input protected native Dungeon state invalid");
    value.dungeon_sha256=source.at("dungeon_sha256").get<std::string>();value.scenes_sha256=source.at("scenes_sha256").get<std::string>();
    value.quests_sha256=source.at("quests_sha256").get<std::string>();value.layout_sha256=source.at("layout_sha256").get<std::string>();
    const auto& born=source.at("birth");
    if(!born.is_null()){
        exact(born,{"key","scene","tool","value"});DungeonInputBirth row;
        row.key=born.at("key").get<std::string>();row.scene=born.at("scene").get<std::string>();const auto tool=u64(born.at("tool"));
        if(tool>2&&tool!=uint8_t(DungeonInputTool::Treasure))throw std::runtime_error("Dungeon input birth is not Door, Key, Monster or Treasure");row.tool=DungeonInputTool(tool);
        const auto& payload=born.at("value");
        if(row.tool==DungeonInputTool::Monster){
            exact(payload,{"tag","status","rank","character_id","level","name","position","rotation","scale"});
            auto& target=row.monster;target.tag=i32(payload.at("tag"));target.status=i32(payload.at("status"));target.rank=i32(payload.at("rank"));
            target.level=i32(payload.at("level"));target.name=payload.at("name").get<std::string>();const auto& id=payload.at("character_id");
            if(!id.is_array()||id.size()!=2)throw std::runtime_error("Dungeon input Monster CharacterTypeID dimensions differ");
            target.character_slot=i32(id[0]);target.character_type=i32(id[1]);target.position=monster_vector<3>(payload.at("position"));
            target.rotation=monster_vector<4>(payload.at("rotation"));target.scale=monster_vector<3>(payload.at("scale"));
        }else{
            const auto wrapped=dungeon_gizmo_state_read({{"protected_sha256",std::string(64,'0')},{"global_random",std::array<uint64_t,4>{}},
                {"scenes",json::array({{{"key",row.scene},{"rows",json::array({payload})}}})}});
            if(!valid_dungeon_gizmo_state(wrapped)||wrapped.scenes.size()!=1||wrapped.scenes[0].rows.size()!=1||wrapped.scenes[0].rows[0].key!=row.key)
                throw std::runtime_error("Dungeon input single native Gizmo birth differs");
            row.gizmo=wrapped.scenes[0].rows[0].value;
        }
        value.birth=std::move(row);
    }
    if(has_edits){
        if(!edits.is_array())throw std::runtime_error("Gizmo control state edits require array");value.edits.emplace();
        for(const auto& row:edits){
            const auto wrapped=dungeon_gizmo_state_read({{"protected_sha256",std::string(64,'0')},{"global_random",std::array<uint64_t,4>{}},
                {"scenes",json::array({{{"key","building:0/dungeon/template"},{"rows",json::array({row})}}})}});
            if(wrapped.scenes.size()!=1||wrapped.scenes[0].rows.size()!=1)throw std::runtime_error("Gizmo control state row differs");
            value.edits->push_back(wrapped.scenes[0].rows[0]);}
    }
    if(has_scenery){if(!scenery_release.is_array()||scenery_release.size()!=8)throw std::runtime_error("Scene Scenery Release counter dimensions differ");value.scenery_release.emplace();
        for(size_t i=0;i<8;++i)value.scenery_release->at(i)=i32(scenery_release[i]);
        if(!scenery_edits.is_array())throw std::runtime_error("Scene Scenery edits require array");value.scenery_edits.emplace();
        for(const auto& row:scenery_edits)value.scenery_edits->push_back(dungeon_scenery_row_read(row));}
    if(!valid_dungeon_input_state(value))throw std::runtime_error("Dungeon input native scene/actor proof invalid");return value;
}
json dungeon_input_message_json(const Message& message){
    const auto& request=message.request;const auto& evidence=message.evidence;
    json document={{"schema",uint64_t(36)},{"kind",uint64_t(message.kind)},{"session",message.session},{"revision",message.revision},
        {"ack_peer",message.ack_peer},{"required_mask",message.required_mask},{"outcome",uint64_t(message.outcome)},{"cause",message.cause},
        {"request",{{"action",uint64_t(request.action)},{"peer",request.peer},{"sequence",request.sequence},{"host_revision",request.host_revision},
            {"action_uuid",request.action_uuid},{"snapshot_uuid",request.snapshot_uuid},{"canonical_record_key",request.canonical_record_key}}},
        {"evidence",{{"before",ledger_json(evidence.before)},{"after",ledger_json(evidence.after)},{"target_retired",evidence.target_retired},
            {"undo_confirmed",evidence.undo_confirmed},{"selection_confirmed",evidence.selection_confirmed},{"retirement_confirmed",evidence.retirement_confirmed},
            {"dungeon_input_before",dungeon_input_state_json(evidence.dungeon_input_before)},
            {"dungeon_input_after",dungeon_input_state_json(evidence.dungeon_input_after)},
            {"dungeon_input_origin",evidence.dungeon_input_origin?dungeon_input_intent_json(*evidence.dungeon_input_origin):json(nullptr)}}}};
    if(request.action==Action::DungeonInput)document["request"]["dungeon_input"]=dungeon_input_intent_json(request.dungeon_input);
    else document["request"]["target_action_uuid"]=request.target_action_uuid;
    return document;
}
bool dungeon_input_packet(const json& source){
    const auto& request=source.at("request");const auto action=u64(request.at("action"));
    return action==66||((action==10||action==11)&&request.at("canonical_record_key").get<std::string>().starts_with("dungeon-input:"));
}
bool dungeon_input_message_read(const json& source,Message& out,std::string& error){
    const auto& request=source.at("request");const auto action=u64(request.at("action"));
    if(action!=66&&action!=10&&action!=11){error="Dungeon input schema36 contains another action";return false;}
    if(action==66)exact(request,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","dungeon_input"});
    else exact(request,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid"});
    const auto kind=u64(source.at("kind")),outcome=u64(source.at("outcome"));
    if(kind<1||kind>5||outcome>3){error="Dungeon input schema36 message enum differs";return false;}
    Message next;next.kind=Kind(kind);next.outcome=Outcome(outcome);next.session=u64(source.at("session"));next.revision=u64(source.at("revision"));
    next.ack_peer=u64(source.at("ack_peer"));next.required_mask=u64(source.at("required_mask"));next.cause=source.at("cause").get<std::string>();
    auto& r=next.request;r.action=Action(action);r.peer=u64(request.at("peer"));r.sequence=u64(request.at("sequence"));r.host_revision=u64(request.at("host_revision"));
    r.action_uuid=request.at("action_uuid").get<std::string>();r.snapshot_uuid=request.at("snapshot_uuid").get<std::string>();r.canonical_record_key=request.at("canonical_record_key").get<std::string>();
    if(action==66)r.dungeon_input=dungeon_input_intent_read(request.at("dungeon_input"));
    else r.target_action_uuid=request.at("target_action_uuid").get<std::string>();
    const auto& evidence=source.at("evidence");
    exact(evidence,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","dungeon_input_before","dungeon_input_after","dungeon_input_origin"});
    auto& e=next.evidence;e.before=ledger_read(evidence.at("before"));e.after=ledger_read(evidence.at("after"));
    e.target_retired=ability_bool(evidence.at("target_retired"));e.undo_confirmed=ability_bool(evidence.at("undo_confirmed"));
    e.selection_confirmed=ability_bool(evidence.at("selection_confirmed"));e.retirement_confirmed=ability_bool(evidence.at("retirement_confirmed"));
    e.dungeon_input_before=dungeon_input_state_read(evidence.at("dungeon_input_before"));e.dungeon_input_after=dungeon_input_state_read(evidence.at("dungeon_input_after"));
    if(!evidence.at("dungeon_input_origin").is_null())e.dungeon_input_origin=dungeon_input_intent_read(evidence.at("dungeon_input_origin"));
    out=std::move(next);error.clear();return true;
}
