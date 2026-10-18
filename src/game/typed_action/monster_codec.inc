json monster_move_json(const MonsterMoveIntent& v){return {{"region",v.region},{"position",v.position},{"rotation",v.rotation},{"scale",v.scale}};}
json monster_intent_json(const MonsterIntent& v){return {{"region",v.region},{"tool",v.tool},{"character_id",{v.character_slot,v.character_type}},
    {"rank",v.rank},{"loose",uint64_t(v.loose)},{"level",v.level},{"position",v.position},{"rotation",v.rotation},{"scale",v.scale}};}
template<size_t N>std::array<float,N> monster_vector(const json& j){
    if(!j.is_array()||j.size()!=N)throw std::runtime_error("Monster vector dimension differs");
    std::array<float,N> out{};for(size_t i=0;i<N;++i)out[i]=scalar(j[i]);return out;
}
MonsterMoveIntent monster_move_read(const json& j){
    exact(j,{"region","position","rotation","scale"});MonsterMoveIntent v;v.region=i32(j.at("region"));
    v.position=monster_vector<3>(j.at("position"));v.rotation=monster_vector<4>(j.at("rotation"));v.scale=monster_vector<3>(j.at("scale"));
    if(!valid_monster_move(v))throw std::runtime_error("Monster Move intent outside original domain");return v;
}
MonsterIntent monster_intent_read(const json& j){
    exact(j,{"region","tool","character_id","rank","loose","level","position","rotation","scale"});MonsterIntent v;
    v.region=i32(j.at("region"));v.tool=j.at("tool").get<std::string>();const auto& id=j.at("character_id");
    if(!id.is_array()||id.size()!=2)throw std::runtime_error("Monster CharacterTypeID dimension differs");
    v.character_slot=i32(id[0]);v.character_type=i32(id[1]);v.rank=i32(j.at("rank"));v.level=i32(j.at("level"));
    const auto loose=u64(j.at("loose"));if(loose>1)throw std::runtime_error("Monster loose flag exceeds boolean domain");v.loose=uint8_t(loose);
    v.position=monster_vector<3>(j.at("position"));v.rotation=monster_vector<4>(j.at("rotation"));v.scale=monster_vector<3>(j.at("scale"));
    if(!valid_monster_intent(v))throw std::runtime_error("Monster intent outside original direct tool domain");return v;
}
json monster_actor_json(const MonsterActor& v){return {{"tag",v.tag},{"status",v.status},{"rank",v.rank},
    {"character_id",{v.character_slot,v.character_type}},{"level",v.level},{"name",v.name},
    {"position",v.position},{"rotation",v.rotation},{"scale",v.scale}};}
MonsterActor monster_actor_read(const json& j){
    exact(j,{"tag","status","rank","character_id","level","name","position","rotation","scale"});MonsterActor v;
    v.tag=i32(j.at("tag"));v.status=i32(j.at("status"));v.rank=i32(j.at("rank"));v.level=i32(j.at("level"));v.name=j.at("name").get<std::string>();
    const auto& id=j.at("character_id");if(!id.is_array()||id.size()!=2)throw std::runtime_error("Monster actor CharacterTypeID dimension differs");
    v.character_slot=i32(id[0]);v.character_type=i32(id[1]);v.position=monster_vector<3>(j.at("position"));
    v.rotation=monster_vector<4>(j.at("rotation"));v.scale=monster_vector<3>(j.at("scale"));
    if(!valid_monster_actor(v))throw std::runtime_error("Monster actor outside readback domain");return v;
}
json monster_state_json(const MonsterState& v){return {{"roster",zone_state_json(v.roster)},{"actor",monster_actor_json(v.actor)}};}
MonsterState monster_state_read(const json& j){exact(j,{"roster","actor"});return {zone_state_read(j.at("roster")),monster_actor_read(j.at("actor"))};}
