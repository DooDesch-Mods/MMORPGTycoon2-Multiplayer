bool valid_npc_quest_intent(const NpcQuestIntent& v){
    if(v.region<0||v.destination_region<0||!valid_quest(v.before)||v.destination_key.empty()||v.action<0||v.action>7||v.kills<0||v.kills>1000000||v.monster_type < -1)return false;
    if(v.family==QuestDestinationFamily::NPC)return game::npc_target_key::valid(v.destination_key)&&v.action==7&&v.kills==0&&v.monster_type==0;
    if(v.family==QuestDestinationFamily::DirectMonster)return game::monster_target_key::valid(v.destination_key)&&v.action==6&&v.kills==1;
    if(v.family==QuestDestinationFamily::Zone)return (key(v.destination_key)||game::snapshot_zone_key::valid(v.destination_key))&&v.action==1&&v.kills==(v.before.monsters_to_kill?v.before.monsters_to_kill:8);
    return v.family==QuestDestinationFamily::Building&&key(v.destination_key)&&v.kills==0&&v.action!=1&&v.action!=6&&v.action!=7;
}
json npc_quest_intent_json(const NpcQuestIntent& v){
    return {{"region",v.region},{"destination_region",v.destination_region},{"family",uint64_t(v.family)},
        {"destination_key",v.destination_key},{"action",v.action},{"kills",v.kills},{"monster_type",v.monster_type},{"before",quest_json(v.before)}};
}
NpcQuestIntent npc_quest_intent_read(const json& j){
    exact(j,{"region","destination_region","family","destination_key","action","kills","monster_type","before"});
    NpcQuestIntent v;v.region=i32(j.at("region"));v.destination_region=i32(j.at("destination_region"));
    const auto family=u64(j.at("family"));if(family<1||family>4)throw std::runtime_error("Quest destination family invalid");
    v.family=QuestDestinationFamily(family);v.destination_key=j.at("destination_key").get<std::string>();
    v.action=i32(j.at("action"));v.kills=i32(j.at("kills"));v.monster_type=i32(j.at("monster_type"));v.before=quest_read(j.at("before"));return v;
}
bool valid_npc_quest_state(const NpcQuestState& v){
    if(v==NpcQuestState{})return true;
    return valid_quest(v.record)&&v.count>0&&v.count<=4096&&v.completed>=0&&uint32_t(v.completed)<=v.count&&
        v.all_sha256.size()==64&&v.all_sha256.find_first_not_of("0123456789abcdef")==std::string::npos;
}
json npc_quest_state_json(const NpcQuestState& v){
    if(v==NpcQuestState{})return nullptr;
    return {{"record",quest_json(v.record)},{"all_sha256",v.all_sha256},{"count",v.count},{"completed",v.completed},
        {"undo_count",v.undo_count},{"global_random",v.global_random}};
}
NpcQuestState npc_quest_state_read(const json& j){
    if(j.is_null())return {};
    exact(j,{"record","all_sha256","count","completed","undo_count","global_random"});
    NpcQuestState v;v.record=quest_read(j.at("record"));v.all_sha256=j.at("all_sha256").get<std::string>();
    const auto count=u64(j.at("count"));if(count>4096)throw std::runtime_error("Quest state count exceeds bound");v.count=uint32_t(count);
    v.completed=i32(j.at("completed"));v.undo_count=u64(j.at("undo_count"));
    const auto& rng=j.at("global_random");if(!rng.is_array()||rng.size()!=4)throw std::runtime_error("Quest RNG shape differs");
    for(size_t i=0;i<4;++i)v.global_random[i]=u64(rng[i]);return v;
}
