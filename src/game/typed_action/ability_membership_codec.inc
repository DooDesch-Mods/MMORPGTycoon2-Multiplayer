bool valid_ability_membership_intent(const AbilityMembershipIntent& v){
    if(v.kind<0||v.kind>4||v.id<0||v.id>=1000000||!digest(v.before_sha256))return false;
    switch(v.command){
    case AbilityMembershipCommand::Create:return v.slot==-1&&v.level>=0&&v.display_slot==-1&&v.copy_name.empty()&&v.prefab==AbilityRecord{};
    case AbilityMembershipCommand::Remove:return v.slot>=0&&v.slot<10000&&v.level==0&&v.display_slot==-1&&v.copy_name.empty()&&v.prefab==AbilityRecord{};
    case AbilityMembershipCommand::Dock:return v.slot>=0&&v.slot<10000&&v.level>=0&&v.display_slot>=0&&v.copy_name.empty()&&v.prefab==AbilityRecord{};
    case AbilityMembershipCommand::Copy:return v.slot==-1&&v.level>=0&&v.display_slot>=0&&quest_text(v.copy_name)&&!v.copy_name.empty()&&v.copy_name.size()<=4096&&valid_ability_record(v.prefab);
    }
    return false;
}
json ability_membership_intent_json(const AbilityMembershipIntent& v){return {{"command",uint64_t(v.command)},
    {"kind",v.kind},{"id",v.id},{"slot",v.slot},{"level",v.level},{"display_slot",v.display_slot},
    {"before_sha256",v.before_sha256},{"copy_name",v.copy_name},{"prefab",ability_record_json(v.prefab)}};}
AbilityMembershipIntent ability_membership_intent_read(const json& j){
    exact(j,{"command","kind","id","slot","level","display_slot","before_sha256","copy_name","prefab"});
    const auto command=u64(j.at("command"));if(command>uint64_t(AbilityMembershipCommand::Copy))throw std::runtime_error("Ability membership command outside domain");
    return {AbilityMembershipCommand(command),i32(j.at("kind")),i32(j.at("id")),i32(j.at("slot")),i32(j.at("level")),
        i32(j.at("display_slot")),j.at("before_sha256").get<std::string>(),j.at("copy_name").get<std::string>(),ability_record_read(j.at("prefab"))};
}
bool valid_ability_membership_state(const AbilityMembershipState& v){
    if(v==AbilityMembershipState{})return true;
    if(!valid_character_membership_state(v.complete)||v.complete==CharacterMembershipState{}||v.complete.selected.deleted||
       v.complete.created||!v.complete.retired_definitions.empty()||v.slots.size()>10000)return false;
    for(const auto& slot:v.slots)if(!valid_ability_record(slot.record)||slot.display_slot< -1)return false;
    return true;
}
json ability_membership_state_json(const AbilityMembershipState& v){if(v==AbilityMembershipState{})return nullptr;
    json slots=json::array();for(const auto& slot:v.slots)slots.push_back({{"record",ability_record_json(slot.record)},{"display_slot",slot.display_slot}});
    return {{"complete",character_membership_state_json(v.complete)},{"slots",slots}};
}
AbilityMembershipState ability_membership_state_read(const json& j){if(j.is_null())return {};
    exact(j,{"complete","slots"});if(!j.at("slots").is_array()||j.at("slots").size()>10000)throw std::runtime_error("Ability membership owner slots exceed bound");
    AbilityMembershipState out;out.complete=character_membership_state_read(j.at("complete"));
    for(const auto& slot:j.at("slots")){exact(slot,{"record","display_slot"});out.slots.push_back({ability_record_read(slot.at("record")),i32(slot.at("display_slot"))});}
    return out;
}
