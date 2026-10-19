bool valid_character_rename_intent(const CharacterRenameIntent& v){
    if(v.kind<0||v.kind>=5||v.id<0||v.id>=1000000||!quest_text(v.before)||v.property>4||!std::isfinite(v.value))return false;
    if(v.property)return v.name.empty()&&v.value>=0&&v.value<=1000000.f;
    return v.value==0&&v.name.size()<=1024&&quest_text(v.name)&&v.before!=v.name;
}
json character_rename_intent_json(const CharacterRenameIntent& v){json out={{"kind",v.kind},{"id",v.id},{"before",v.before},{"name",v.name}};
    if(v.property){out["property"]=uint64_t(v.property);out["value"]=v.value;}return out;}
CharacterRenameIntent character_rename_intent_read(const json& j){const bool property=j.contains("property");
    if(property)exact(j,{"kind","id","before","name","property","value"});else exact(j,{"kind","id","before","name"});
    CharacterRenameIntent v{i32(j.at("kind")),i32(j.at("id")),j.at("before").get<std::string>(),j.at("name").get<std::string>()};
    if(property){const auto field=u64(j.at("property"));if(field<1||field>4)throw std::runtime_error("Character stat property outside domain");v.property=uint8_t(field);v.value=scalar(j.at("value"));}return v;}
bool valid_character_rename_state(const CharacterRenameState& v){
    for(const auto& set:v.attributes)for(float value:set)if(!std::isfinite(value))return false;
    if(!std::isfinite(v.speed)||v.boss_phases<0||v.boss_phases>10000)return false;
    return v==CharacterRenameState{}||(v.count>0&&v.count<=1000000&&v.undo_count<=20&&quest_text(v.name)&&quest_text(v.definition_name)&&digest(v.all_sha256)&&digest(v.others_sha256));
}
json character_rename_state_json(const CharacterRenameState& v){if(v==CharacterRenameState{})return nullptr;
    return {{"day",v.day},{"rename_day",v.rename_day},{"count",v.count},{"name",v.name},{"definition_name",v.definition_name},
        {"all_sha256",v.all_sha256},{"others_sha256",v.others_sha256},{"edited",v.edited},{"released",v.released},
        {"undo_count",v.undo_count},{"release_edits",v.release_edits},{"global_random",v.global_random},{"attributes",v.attributes},{"speed",v.speed},{"boss_health",v.boss_health},{"boss_phases",v.boss_phases}};
}
CharacterRenameState character_rename_state_read(const json& j){if(j.is_null())return {};
    exact(j,{"day","rename_day","count","name","definition_name","all_sha256","others_sha256","edited","released","undo_count","release_edits","global_random","attributes","speed","boss_health","boss_phases"});CharacterRenameState v;
    v.day=i32(j.at("day"));v.rename_day=i32(j.at("rename_day"));const auto count=u64(j.at("count"));if(count>1000000)throw std::runtime_error("Character census exceeds bound");v.count=uint32_t(count);
    v.name=j.at("name").get<std::string>();v.definition_name=j.at("definition_name").get<std::string>();v.all_sha256=j.at("all_sha256").get<std::string>();v.others_sha256=j.at("others_sha256").get<std::string>();
    if(!j.at("edited").is_boolean()||!j.at("released").is_boolean())throw std::runtime_error("Character flags require bool");v.edited=j.at("edited").get<bool>();v.released=j.at("released").get<bool>();v.undo_count=u64(j.at("undo_count"));
    const auto& edits=j.at("release_edits");const auto& rng=j.at("global_random");if(!edits.is_array()||edits.size()!=8||!rng.is_array()||rng.size()!=4)throw std::runtime_error("Character release/RNG dimension differs");
    const auto& stats=j.at("attributes");if(!stats.is_array()||stats.size()!=2)throw std::runtime_error("Character attribute-set dimension differs");
    for(size_t i=0;i<2;++i){if(!stats[i].is_array()||stats[i].size()!=3)throw std::runtime_error("Character attribute dimension differs");for(size_t n=0;n<3;++n)v.attributes[i][n]=scalar(stats[i][n]);}
    v.speed=scalar(j.at("speed"));v.boss_health=i32(j.at("boss_health"));v.boss_phases=i32(j.at("boss_phases"));
    for(size_t i=0;i<8;++i)v.release_edits[i]=i32(edits[i]);for(size_t i=0;i<4;++i)v.global_random[i]=u64(rng[i]);return v;
}
