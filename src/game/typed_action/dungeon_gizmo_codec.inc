bool valid_dungeon_gizmo_intent(const DungeonGizmoIntent& v){return game::dungeon_gizmo_identity::key(v.key)&&game::dungeon_gizmo_identity::scene(v.scene)&&digest(v.before_sha256)&&uint8_t(v.edit)<=4&&v.value>=0&&v.value<=5;}
json dungeon_gizmo_intent_json(const DungeonGizmoIntent& v){return {{"key",v.key},{"scene",v.scene},{"before_sha256",v.before_sha256},{"edit",uint8_t(v.edit)},{"value",v.value}};}
DungeonGizmoIntent dungeon_gizmo_intent_read(const json& j){
    exact(j,{"key","scene","before_sha256","edit","value"});DungeonGizmoIntent v;v.key=j.at("key").get<std::string>();v.scene=j.at("scene").get<std::string>();v.before_sha256=j.at("before_sha256").get<std::string>();
    const auto edit=u64(j.at("edit"));if(edit>4)throw std::runtime_error("Dungeon Gizmo edit outside original numeric commands");v.edit=DungeonGizmoEdit(edit);v.value=i32(j.at("value"));return v;
}
bool valid_dungeon_gizmo_state(const DungeonGizmoState& v){
    if(v==DungeonGizmoState{})return true;if(!digest(v.protected_sha256))return false;
    std::set<std::string> scenes,keys;
    for(const auto& scene:v.scenes){if(!game::dungeon_gizmo_identity::scene(scene.key)||!scenes.insert(scene.key).second)return false;
        for(const auto& row:scene.rows){const auto& r=row.value;if(!game::dungeon_gizmo_identity::key(row.key)||!keys.insert(row.key).second||
            (!r.parent.empty()&&!game::dungeon_gizmo_identity::key(r.parent)))return false;
            for(const auto* text:{&r.name,&r.type,&r.variant})if(text->find('\0')!=std::string::npos)return false;
            for(float x:r.rotation)if(!std::isfinite(x))return false;for(float x:r.position)if(!std::isfinite(x))return false;for(float x:r.scale)if(!std::isfinite(x))return false;
        }
    }
    for(const auto& scene:v.scenes)for(const auto& row:scene.rows)if(!row.value.parent.empty()&&(!keys.contains(row.value.parent)||row.value.parent==row.key))return false;
    return true;
}
json dungeon_gizmo_state_json(const DungeonGizmoState& v){return v==DungeonGizmoState{}?json(nullptr):game::dungeon_gizmo_portable::state(v);}
DungeonGizmoState dungeon_gizmo_state_read(const json& j){
    if(j.is_null())return {};exact(j,{"protected_sha256","global_random","scenes"});DungeonGizmoState v;v.protected_sha256=j.at("protected_sha256").get<std::string>();
    ability_array(j.at("global_random"),v.global_random,u64);if(!j.at("scenes").is_array())throw std::runtime_error("Dungeon Gizmo scene roster requires array");
    for(const auto& item:j.at("scenes")){exact(item,{"key","rows"});DungeonGizmoScene scene;scene.key=item.at("key").get<std::string>();if(!item.at("rows").is_array())throw std::runtime_error("Dungeon Gizmo native roster requires array");
        for(const auto& source:item.at("rows")){exact(source,{"key","name","type","variant","parent","rotation","position","scale","triggered","state","gold","loot","vendor_trash","keys","boss_keys","lock_id","edge_a","edge_b"});
            DungeonGizmoRow row;auto& r=row.value;row.key=source.at("key").get<std::string>();r.name=source.at("name").get<std::string>();r.type=source.at("type").get<std::string>();r.variant=source.at("variant").get<std::string>();r.parent=source.at("parent").get<std::string>();
            ability_array(source.at("rotation"),r.rotation,scalar);ability_array(source.at("position"),r.position,scalar);ability_array(source.at("scale"),r.scale,scalar);
            if(!source.at("triggered").is_boolean())throw std::runtime_error("Dungeon Gizmo native trigger requires boolean");r.triggered=source.at("triggered").get<bool>();
            r.state=i32(source.at("state"));r.gold=i32(source.at("gold"));r.loot=i32(source.at("loot"));r.vendor_trash=i32(source.at("vendor_trash"));r.keys=i32(source.at("keys"));r.boss_keys=i32(source.at("boss_keys"));
            r.lock_id=i32(source.at("lock_id"));r.edge_a=i32(source.at("edge_a"));r.edge_b=i32(source.at("edge_b"));scene.rows.push_back(std::move(row));
        }v.scenes.push_back(std::move(scene));
    }return v;
}
