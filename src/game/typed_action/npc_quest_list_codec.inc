bool valid_npc_quest_list_records(const std::vector<npc_quest_records::Record>& rows){
    if(rows.size()>5)return false;
    for(size_t i=0;i<rows.size();++i)if(!valid_quest(rows[i])||rows[i].sequence_id!=int32_t(i))return false;
    return true;
}
json npc_quest_list_records_json(const std::vector<npc_quest_records::Record>& rows){
    json out=json::array();for(const auto& row:rows)out.push_back(quest_json(row));return out;
}
std::vector<npc_quest_records::Record> npc_quest_list_records_read(const json& j){
    if(!j.is_array()||j.size()>5)throw std::runtime_error("Quest list requires at most5 native slots");
    std::vector<npc_quest_records::Record> out;for(const auto& row:j)out.push_back(quest_read(row));return out;
}
bool valid_npc_quest_list_intent(const NpcQuestListIntent& v){
    if(v.edit!=QuestListEdit::List){
        if(v.region<0||!valid_npc_quest_list_records(v.before)||v.remove_slot<0||size_t(v.remove_slot)>=v.before.size())return false;
        if(v.edit==QuestListEdit::Title)return !v.title.empty()&&v.title.size()<=1024&&quest_text(v.title)&&v.kills==0&&v.xp==0&&v.gold==0&&!v.defaults;
        if(v.edit==QuestListEdit::Kills)return v.kills>=0&&v.kills<=1000000&&v.title.empty()&&v.xp==0&&v.gold==0&&!v.defaults;
        return v.edit==QuestListEdit::Rewards&&v.title.empty()&&v.kills==0&&v.xp>=0&&v.gold>=0&&(!v.defaults||(v.xp==0&&v.gold==0));
    }
    if(!v.title.empty()||v.kills||v.xp||v.gold||v.defaults)return false;
    return v.region>=0&&valid_npc_quest_list_records(v.before)&&
        (v.remove_slot==-1?v.before.size()<5:v.remove_slot>=0&&size_t(v.remove_slot)<v.before.size());
}
json npc_quest_list_intent_json(const NpcQuestListIntent& v){
    json out={{"region",v.region},{"remove_slot",v.remove_slot},{"before",npc_quest_list_records_json(v.before)}};
    if(v.edit!=QuestListEdit::List)out["edit"]={{"kind",int(v.edit)},{"title",v.title},{"kills",v.kills},{"xp",v.xp},{"gold",v.gold},{"defaults",v.defaults}};
    return out;
}
NpcQuestListIntent npc_quest_list_intent_read(const json& j){
    const bool editing=j.contains("edit");
    if(editing)exact(j,{"region","remove_slot","before","edit"});else exact(j,{"region","remove_slot","before"});NpcQuestListIntent v;
    if(editing){const auto& x=j.at("edit");exact(x,{"kind","title","kills","xp","gold","defaults"});const auto kind=u64(x.at("kind"));
        if(kind<1||kind>3)throw std::runtime_error("Quest editor command outside domain");v.edit=QuestListEdit(kind);v.title=x.at("title").get<std::string>();
        v.kills=i32(x.at("kills"));v.xp=i32(x.at("xp"));v.gold=i32(x.at("gold"));if(!x.at("defaults").is_boolean())throw std::runtime_error("Quest reward defaults requires bool");v.defaults=x.at("defaults").get<bool>();}
    v.region=i32(j.at("region"));v.remove_slot=i32(j.at("remove_slot"));v.before=npc_quest_list_records_read(j.at("before"));return v;
}
bool valid_npc_quest_list_state(const NpcQuestListState& v){
    if(v==NpcQuestListState{})return true;
    return valid_npc_quest_list_records(v.records)&&v.count>=v.records.size()&&v.count<=4096&&v.completed>=0&&
        uint32_t(v.completed)<=v.count&&v.undo_count<=20&&digest(v.all_sha256)&&digest(v.targets_sha256)&&digest(v.references_sha256);
}
json npc_quest_list_state_json(const NpcQuestListState& v){
    if(v==NpcQuestListState{})return nullptr;
    return {{"records",npc_quest_list_records_json(v.records)},{"all_sha256",v.all_sha256},{"targets_sha256",v.targets_sha256},
        {"references_sha256",v.references_sha256},{"count",v.count},{"completed",v.completed},{"undo_count",v.undo_count},{"global_random",v.global_random}};
}
NpcQuestListState npc_quest_list_state_read(const json& j){
    if(j.is_null())return {};
    exact(j,{"records","all_sha256","targets_sha256","references_sha256","count","completed","undo_count","global_random"});
    NpcQuestListState v;v.records=npc_quest_list_records_read(j.at("records"));v.all_sha256=j.at("all_sha256").get<std::string>();
    v.targets_sha256=j.at("targets_sha256").get<std::string>();v.references_sha256=j.at("references_sha256").get<std::string>();
    const auto count=u64(j.at("count"));if(count>4096)throw std::runtime_error("Quest global count exceeds bound");v.count=uint32_t(count);
    v.completed=i32(j.at("completed"));v.undo_count=u64(j.at("undo_count"));
    const auto& rng=j.at("global_random");if(!rng.is_array()||rng.size()!=4)throw std::runtime_error("Quest list RNG shape differs");
    for(size_t i=0;i<4;++i)v.global_random[i]=u64(rng[i]);return v;
}
