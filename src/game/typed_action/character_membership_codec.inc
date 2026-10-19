bool valid_character_membership_intent(const CharacterMembershipIntent& v){
    return v.kind>=0&&v.kind<5&&v.id>=0&&v.id<1000000&&digest(v.before_sha)&&
        ((v.command==CharacterMembershipCommand::CreateVariant&&quest_text(v.name)&&v.name.size()<=1024)||
         (v.command==CharacterMembershipCommand::Destroy&&v.name.empty()));
}
json character_membership_intent_json(const CharacterMembershipIntent& v){return {{"command",uint64_t(v.command)},
    {"kind",v.kind},{"id",v.id},{"name",v.name},{"before_sha",v.before_sha}};}
CharacterMembershipIntent character_membership_intent_read(const json& j){
    exact(j,{"command","kind","id","name","before_sha"});const auto command=u64(j.at("command"));
    if(command>uint64_t(CharacterMembershipCommand::Destroy))throw std::runtime_error("Character membership command outside domain");
    return {CharacterMembershipCommand(command),i32(j.at("kind")),i32(j.at("id")),j.at("name").get<std::string>(),j.at("before_sha").get<std::string>()};
}
json character_membership_row_json(const CharacterMembershipRow& v){return {{"kind",v.kind},{"id",v.id},
    {"parent",v.parent},{"deleted",v.deleted},{"name",v.name},{"variants",v.variants}};}
CharacterMembershipRow character_membership_row_read(const json& j){
    exact(j,{"kind","id","parent","deleted","name","variants"});
    if(!j.at("deleted").is_boolean()||!j.at("variants").is_array()||j.at("variants").size()>1000000)
        throw std::runtime_error("Character membership row flags/variants differ");
    CharacterMembershipRow out;out.kind=i32(j.at("kind"));out.id=i32(j.at("id"));out.parent=i32(j.at("parent"));
    out.deleted=j.at("deleted").get<bool>();out.name=j.at("name").get<std::string>();
    for(const auto& id:j.at("variants"))out.variants.push_back(i32(id));return out;
}
bool valid_character_membership_state(const CharacterMembershipState& v){
    if(v==CharacterMembershipState{})return true;
    if(!character_membership_row(v.selected)||!quest_text(v.selected.name)||v.selected.name.size()>1024||v.undo_count>20||
       (v.created&&(!character_membership_row(*v.created)||!quest_text(v.created->name)||v.created->name.size()>1024)))return false;
    for(size_t kind=0;kind<5;++kind)if(v.counts[kind]<0||v.counts[kind]>1000000||v.first_deleted[kind]< -1||v.first_deleted[kind]>=v.counts[kind])return false;
    if(v.selected.id>=v.counts[size_t(v.selected.kind)])return false;
    if(v.created&&v.created->id>=v.counts[size_t(v.created->kind)])return false;
    for(const auto* sha:{&v.manager_sha,&v.family_sha,&v.others_sha,&v.users_sha,&v.quests_sha,&v.economy_sha,&v.monsters_sha,&v.bosses_sha})if(!digest(*sha))return false;
    int32_t last=-1;for(const auto id:v.retired_definitions){if(id<=last||id>=v.counts[size_t(v.selected.kind)])return false;last=id;}
    return true;
}
json character_membership_state_json(const CharacterMembershipState& v){if(v==CharacterMembershipState{})return nullptr;
    return {{"counts",v.counts},{"first_deleted",v.first_deleted},{"selected",character_membership_row_json(v.selected)},
        {"created",v.created?character_membership_row_json(*v.created):json(nullptr)},{"retired_definitions",v.retired_definitions},
        {"manager_sha",v.manager_sha},{"family_sha",v.family_sha},{"others_sha",v.others_sha},{"users_sha",v.users_sha},
        {"quests_sha",v.quests_sha},{"economy_sha",v.economy_sha},{"monsters_sha",v.monsters_sha},{"bosses_sha",v.bosses_sha},
        {"global_random",v.global_random},{"undo_count",v.undo_count},{"release_edits",v.release_edits}};
}
CharacterMembershipState character_membership_state_read(const json& j){if(j.is_null())return {};
    exact(j,{"counts","first_deleted","selected","created","retired_definitions","manager_sha","family_sha","others_sha","users_sha",
        "quests_sha","economy_sha","monsters_sha","bosses_sha","global_random","undo_count","release_edits"});CharacterMembershipState out;
    for(const auto* name:{"counts","first_deleted"})if(!j.at(name).is_array()||j.at(name).size()!=5)throw std::runtime_error("Character membership census dimension differs");
    if(!j.at("global_random").is_array()||j.at("global_random").size()!=4||!j.at("release_edits").is_array()||j.at("release_edits").size()!=8||
       !j.at("retired_definitions").is_array()||j.at("retired_definitions").size()>1000000)throw std::runtime_error("Character membership consequence dimension differs");
    for(size_t kind=0;kind<5;++kind){out.counts[kind]=i32(j.at("counts")[kind]);out.first_deleted[kind]=i32(j.at("first_deleted")[kind]);}
    out.selected=character_membership_row_read(j.at("selected"));if(!j.at("created").is_null())out.created=character_membership_row_read(j.at("created"));
    for(const auto& id:j.at("retired_definitions"))out.retired_definitions.push_back(i32(id));
    out.manager_sha=j.at("manager_sha").get<std::string>();out.family_sha=j.at("family_sha").get<std::string>();out.others_sha=j.at("others_sha").get<std::string>();
    out.users_sha=j.at("users_sha").get<std::string>();out.quests_sha=j.at("quests_sha").get<std::string>();out.economy_sha=j.at("economy_sha").get<std::string>();
    out.monsters_sha=j.at("monsters_sha").get<std::string>();out.bosses_sha=j.at("bosses_sha").get<std::string>();out.undo_count=u64(j.at("undo_count"));
    for(size_t i=0;i<4;++i)out.global_random[i]=u64(j.at("global_random")[i]);for(size_t i=0;i<8;++i)out.release_edits[i]=i32(j.at("release_edits")[i]);return out;
}
