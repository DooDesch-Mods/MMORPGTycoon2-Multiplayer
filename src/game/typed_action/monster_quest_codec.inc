bool valid_monster_quests(const MonsterQuestRetirement& v,const std::string& target,bool complete){
    if(!complete)return v==MonsterQuestRetirement{};
    const auto digest=[](const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;};
    if(!digest(v.before_sha256)||!digest(v.after_sha256)||v.before_count>4096||v.after_count>v.before_count||
       v.before_count-v.after_count!=v.removed.size()||v.completed_before<0||v.completed_after<0||
       uint32_t(v.completed_before)>v.before_count||uint32_t(v.completed_after)>v.after_count)return false;
    int64_t expected=v.completed_before;std::set<std::pair<std::string,int32_t>> unique;
    for(const auto& row:v.removed){
        if(!game::npc_target_key::valid(row.owner_key)||!valid_quest(row.record)||row.record.destination_key!=target||
           !unique.emplace(row.owner_key,row.record.sequence_id).second)return false;
        if(row.record.completed.total>0)--expected;
    }
    return expected==v.completed_after&&(v.removed.empty()?v.before_sha256==v.after_sha256:v.before_sha256!=v.after_sha256);
}
json monster_quests_json(const MonsterQuestRetirement& v){
    json rows=json::array();for(const auto& row:v.removed)rows.push_back({{"owner_key",row.owner_key},{"record",quest_json(row.record)}});
    return {{"before_sha256",v.before_sha256},{"after_sha256",v.after_sha256},{"before_count",v.before_count},
        {"after_count",v.after_count},{"completed_before",v.completed_before},{"completed_after",v.completed_after},{"removed",rows}};
}
MonsterQuestRetirement monster_quests_read(const json& j){
    exact(j,{"before_sha256","after_sha256","before_count","after_count","completed_before","completed_after","removed"});
    MonsterQuestRetirement v;v.before_sha256=j.at("before_sha256").get<std::string>();v.after_sha256=j.at("after_sha256").get<std::string>();
    const auto before=u64(j.at("before_count")),after=u64(j.at("after_count"));
    if(before>4096||after>4096)throw std::runtime_error("Monster Quest count exceeds bound");
    v.before_count=uint32_t(before);v.after_count=uint32_t(after);
    v.completed_before=i32(j.at("completed_before"));v.completed_after=i32(j.at("completed_after"));
    const auto& rows=j.at("removed");if(!rows.is_array()||rows.size()>4096)throw std::runtime_error("Monster Quest removed list exceeds bound");
    for(const auto& row:rows){exact(row,{"owner_key","record"});v.removed.push_back({row.at("owner_key").get<std::string>(),quest_read(row.at("record"))});}
    return v;
}
