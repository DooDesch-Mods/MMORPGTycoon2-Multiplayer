namespace quest_state=mt2mp::subscriber_quest_state;
bool boolean(const Json& value){if(!value.is_boolean())throw std::runtime_error("Quest bool required");return value.get<bool>();}
std::string string(const Json& value){if(!value.is_string())throw std::runtime_error("Quest string required");return value.get<std::string>();}
template<size_t N>void bit_patterns(const Json& values,std::array<uint32_t,N>& out){
    if(!values.is_array()||values.size()!=N)throw std::runtime_error("Quest exact bits array required");
    for(size_t i=0;i<N;++i)out[i]=u32(values.at(i));
}
Json daily_json(const npc_quest_records::DailyCount& value){return {{"history",value.history},{"today",value.today},{"total",value.total}};}
npc_quest_records::DailyCount daily_read(const Json& row){
    exact(row,{"history","today","total"});npc_quest_records::DailyCount value;
    if(!row.at("history").is_array())throw std::runtime_error("Quest DailyCount history array required");
    for(const auto& n:row.at("history"))value.history.push_back(i32(n));
    value.today=i32(row.at("today"));value.total=i32(row.at("total"));return value;
}
Json pose_json(const subscriber_motion::Pose& pose){return {{"region",pose.region},{"orientation",pose.orientation},{"position",pose.position},{"scale",pose.scale}};}
subscriber_motion::Pose pose_read(const Json& row){
    exact(row,{"region","orientation","position","scale"});subscriber_motion::Pose pose;pose.region=i32(row.at("region"));
    auto floats=[](const Json& values,auto& out){
        if(!values.is_array()||values.size()!=out.size())throw std::runtime_error("Quest pose exact float array required");
        for(size_t i=0;i<out.size();++i){if(!values.at(i).is_number())throw std::runtime_error("Quest pose number required");out[i]=values.at(i).template get<float>();}
    };
    floats(row.at("orientation"),pose.orientation);floats(row.at("position"),pose.position);floats(row.at("scale"),pose.scale);return pose;
}
Json link_json(const quest_state::Link& link){
    const auto state=link.state==quest_state::LinkState::Empty?"empty":link.state==quest_state::LinkState::Expired?"expired":"live";
    return {{"state",state},{"npc_key",link.npc_key},{"sequence_id",link.sequence_id}};
}
quest_state::Link link_read(const Json& row){
    exact(row,{"state","npc_key","sequence_id"});quest_state::Link value;const auto state=string(row.at("state"));
    if(state=="empty")value.state=quest_state::LinkState::Empty;
    else if(state=="expired")value.state=quest_state::LinkState::Expired;
    else if(state=="live")value.state=quest_state::LinkState::Live;
    else throw std::runtime_error("Quest exact link state required");
    value.npc_key=string(row.at("npc_key"));value.sequence_id=i32(row.at("sequence_id"));return value;
}
Json toon_json(const std::optional<quest_state::Toon>& value){
    if(!value)return nullptr;
    Json progress=Json::array();
    for(const auto& p:value->progress)progress.push_back({{"npc",link_json(p.npc)},{"quest",link_json(p.quest)},{"completed",p.completed},
        {"monsters_defeated",p.monsters_defeated},{"visited",p.visited},{"action_performed",p.action_performed},{"complete",p.complete}});
    return {{"gold",value->gold},{"xp_bits",value->xp_bits},{"level",value->level},{"quests_completed",value->quests_completed},
        {"attributes",value->attributes},{"seconds_to_level_bits",value->seconds_to_level_bits},{"progress",progress}};
}
std::optional<quest_state::Toon> toon_read(const Json& row){
    if(row.is_null())return std::nullopt;
    exact(row,{"gold","xp_bits","level","quests_completed","attributes","seconds_to_level_bits","progress"});quest_state::Toon value;
    value.gold=i32(row.at("gold"));value.xp_bits=u32(row.at("xp_bits"));value.level=i32(row.at("level"));value.quests_completed=i32(row.at("quests_completed"));
    bit_patterns(row.at("attributes"),value.attributes);value.seconds_to_level_bits=u32(row.at("seconds_to_level_bits"));
    if(!row.at("progress").is_array())throw std::runtime_error("Quest progress array required");
    for(const auto& p:row.at("progress")){
        exact(p,{"npc","quest","completed","monsters_defeated","visited","action_performed","complete"});quest_state::Progress item;
        item.npc=link_read(p.at("npc"));item.quest=link_read(p.at("quest"));item.completed=i32(p.at("completed"));item.monsters_defeated=i32(p.at("monsters_defeated"));
        item.visited=boolean(p.at("visited"));item.action_performed=boolean(p.at("action_performed"));item.complete=boolean(p.at("complete"));value.progress.push_back(std::move(item));
    }
    return value;
}
Json npcs_json(const std::vector<quest_state::Npc>& values){
    Json rows=Json::array();
    for(const auto& value:values){
        Json quests=Json::array();for(const auto& quest:value.quests)quests.push_back({{"sequence_id",quest.sequence_id},{"accepted",daily_json(quest.accepted)},{"completed",daily_json(quest.completed)},{"highest_cache",quest.highest_cache}});
        rows.push_back({{"key",value.key},{"type",value.type},{"level",value.level},{"pose",pose_json(value.pose)},
            {"accepted",daily_json(value.accepted)},{"completed",daily_json(value.completed)},{"quests",quests},{"highest_cache",value.highest_cache}});
    }
    return rows;
}
void npcs_read(const Json& rows,std::vector<quest_state::Npc>& values){
    if(!rows.is_array())throw std::runtime_error("Quest NPC array required");
    for(const auto& row:rows){
        exact(row,{"key","type","level","pose","accepted","completed","quests","highest_cache"});quest_state::Npc value;
        value.key=string(row.at("key"));value.type=i32(row.at("type"));value.level=i32(row.at("level"));value.pose=pose_read(row.at("pose"));
        value.accepted=daily_read(row.at("accepted"));value.completed=daily_read(row.at("completed"));
        integers(row.at("highest_cache"),value.highest_cache);
        if(!row.at("quests").is_array())throw std::runtime_error("Quest counters array required");
        for(const auto& quest:row.at("quests")){exact(quest,{"sequence_id","accepted","completed","highest_cache"});quest_state::Counters counter;
            counter.sequence_id=i32(quest.at("sequence_id"));counter.accepted=daily_read(quest.at("accepted"));counter.completed=daily_read(quest.at("completed"));
            integers(quest.at("highest_cache"),counter.highest_cache);value.quests.push_back(std::move(counter));}
        values.push_back(std::move(value));
    }
}
