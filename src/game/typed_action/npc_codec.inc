bool quest_text(const std::string& s) {
    if(s.size()>65536||s.find('\0')!=std::string::npos)return false;
    for(size_t i=0;i<s.size();) {
        const auto a=uint8_t(s[i]);size_t n=0;uint32_t cp=0;
        if(a<0x80){n=1;cp=a;}
        else if(a>=0xc2&&a<=0xdf){n=2;cp=a&0x1f;}
        else if(a>=0xe0&&a<=0xef){n=3;cp=a&0x0f;}
        else if(a>=0xf0&&a<=0xf4){n=4;cp=a&0x07;}
        else return false;
        if(i+n>s.size())return false;
        for(size_t j=1;j<n;++j){const auto b=uint8_t(s[i+j]);if((b&0xc0)!=0x80)return false;cp=(cp<<6)|(b&0x3f);}
        if((n==2&&cp<0x80)||(n==3&&cp<0x800)||(n==4&&cp<0x10000)||(cp>=0xd800&&cp<=0xdfff)||cp>0x10ffff)return false;
        i+=n;
    }
    return true;
}
bool valid_quest(const npc_quest_records::Record& v) {
    return v.sequence_id>=0&&quest_text(v.name)&&quest_text(v.description)&&
        (key(v.destination_key)||game::snapshot_zone_key::valid(v.destination_key))&&
        v.accepted.history.size()<=100000&&v.completed.history.size()<=100000&&
        std::isfinite(v.rating.beauty)&&std::isfinite(v.rating.convenience)&&std::isfinite(v.rating.danger);
}
json quest_daily_json(const npc_quest_records::DailyCount& v) {
    return {{"history",v.history},{"today",v.today},{"total",v.total}};
}
npc_quest_records::DailyCount quest_daily_read(const json& j) {
    exact(j,{"history","today","total"});npc_quest_records::DailyCount v;
    v.today=i32(j.at("today"));v.total=i32(j.at("total"));const auto& a=j.at("history");
    if(!a.is_array()||a.size()>100000)throw std::runtime_error("Quest daily history array differs");
    for(const auto& n:a)v.history.push_back(i32(n));return v;
}
json quest_json(const npc_quest_records::Record& v) {
    return {{"sequence_id",v.sequence_id},{"type",v.type},{"name",v.name},{"description",v.description},
        {"action",v.action},{"monsters_to_kill",v.monsters_to_kill},{"monster_type",v.monster_type},
        {"xp_reward",v.xp_reward},{"gold_reward",v.gold_reward},{"destination_key",v.destination_key},
        {"accepted",quest_daily_json(v.accepted)},{"completed",quest_daily_json(v.completed)},
        {"rating",{{"beauty",v.rating.beauty},{"convenience",v.rating.convenience},{"danger",v.rating.danger},{"set",v.rating.set}}}};
}
npc_quest_records::Record quest_read(const json& j) {
    exact(j,{"sequence_id","type","name","description","action","monsters_to_kill","monster_type",
        "xp_reward","gold_reward","destination_key","accepted","completed","rating"});npc_quest_records::Record v;
    v.sequence_id=i32(j.at("sequence_id"));v.type=i32(j.at("type"));v.name=j.at("name").get<std::string>();
    v.description=j.at("description").get<std::string>();v.action=i32(j.at("action"));
    v.monsters_to_kill=i32(j.at("monsters_to_kill"));v.monster_type=i32(j.at("monster_type"));
    v.xp_reward=i32(j.at("xp_reward"));v.gold_reward=i32(j.at("gold_reward"));
    v.destination_key=j.at("destination_key").get<std::string>();
    v.accepted=quest_daily_read(j.at("accepted"));v.completed=quest_daily_read(j.at("completed"));
    const auto& rating=j.at("rating");exact(rating,{"beauty","convenience","danger","set"});
    v.rating.beauty=scalar(rating.at("beauty"));v.rating.convenience=scalar(rating.at("convenience"));
    v.rating.danger=scalar(rating.at("danger"));
    if(!rating.at("set").is_boolean())throw std::runtime_error("Quest rating set requires boolean");
    v.rating.set=rating.at("set").get<bool>();
    if(!valid_quest(v))throw std::runtime_error("Quest record outside domain");return v;
}
bool npc_quest_schema(const Message& m) {
    if(m.request.action!=Action::PlaceNPC&&m.request.action!=Action::RemoveNPC&&m.request.action!=Action::MoveNPC)return false;
    const auto normal=[](const NpcState& s){return !s.quest_targets_sha256.empty()||(s.created!=NpcActor{}&&s.created.type==0);};
    return (m.request.action==Action::PlaceNPC&&m.request.npc.type==0)||normal(m.evidence.npc_before)||normal(m.evidence.npc_after);
}
bool npc_quest_pool_effect(const Evidence& e,bool normal) {
    const auto& a=e.npc_before.quest_targets_sha256;const auto& b=e.npc_after.quest_targets_sha256;
    return normal?(digest(a)&&a==b):(a.empty()&&b.empty());
}

bool valid_npc_intent(const NpcIntent& v) {
    if(v.region<0||v.type>2||v.character_slot<0||v.character_type<0)return false;
    for(float x:v.position)if(!std::isfinite(x)||std::abs(x)>1000000.0f)return false;
    for(float x:v.rotation)if(!std::isfinite(x))return false;
    for(float x:v.scale)if(!std::isfinite(x)||x<=0)return false;
    return true;
}
bool valid_npc_actor(const NpcActor& v) {
    if(v==NpcActor{})return true;
    if(!region_name(v.name)||v.type<0||v.type>2||v.character_slot<0||v.character_type<0||
       v.level<0||v.quest_count<0||size_t(v.quest_count)!=v.quests.size()||v.quests.size()>100000||
       (v.type!=0&&v.quest_count!=0))return false;
    std::set<int32_t> sequences;
    for(const auto& q:v.quests)if(!valid_quest(q)||!sequences.insert(q.sequence_id).second)return false;
    for(const float x:v.position)if(!std::isfinite(x)||std::abs(x)>1000000.0f)return false;
    for(const float x:v.rotation)if(!std::isfinite(x))return false;
    for(const float x:v.scale)if(!std::isfinite(x))return false;
    return true;
}
bool valid_npc_state(const NpcState& v) {
    return v==NpcState{}||(v.count<=100000&&v.undo_count<=100000&&digest(v.roster_sha256)&&valid_npc_actor(v.created)&&
        (v.quest_targets_sha256.empty()||digest(v.quest_targets_sha256))&&
        (v.created==NpcActor{}||v.created.type==0||v.quest_targets_sha256.empty()));
}
json npc_actor_json(const NpcActor& v,bool quests=false) {
    json result={{"name",v.name},{"type",v.type},{"character_slot",v.character_slot},
        {"character_type",v.character_type},{"level",v.level},{"quest_count",v.quest_count},
        {"position",v.position},{"rotation",v.rotation},{"scale",v.scale}};
    if(quests){result["quests"]=json::array();for(const auto& q:v.quests)result["quests"].push_back(quest_json(q));}
    return result;
}
NpcActor npc_actor_read(const json& j,bool quests=false) {
    if(quests)exact(j,{"name","type","character_slot","character_type","level","quest_count","position","rotation","scale","quests"});
    else exact(j,{"name","type","character_slot","character_type","level","quest_count","position","rotation","scale"});
    NpcActor v;v.name=j.at("name").get<std::string>();v.type=i32(j.at("type"));
    v.character_slot=i32(j.at("character_slot"));v.character_type=i32(j.at("character_type"));
    v.level=i32(j.at("level"));v.quest_count=i32(j.at("quest_count"));
    const auto read_floats=[&](const char* key,auto& target){const auto& a=j.at(key);
        if(!a.is_array()||a.size()!=target.size())throw std::runtime_error(std::string("NPC ")+key+" array size differs");
        for(size_t i=0;i<target.size();++i)target[i]=scalar(a[i]);};
    read_floats("position",v.position);read_floats("rotation",v.rotation);read_floats("scale",v.scale);
    if(quests){const auto& rows=j.at("quests");if(!rows.is_array()||rows.size()>100000)throw std::runtime_error("NPC Quest array differs");
        for(const auto& q:rows)v.quests.push_back(quest_read(q));}
    if(!valid_npc_actor(v))throw std::runtime_error("NPC actor outside domain");return v;
}
json npc_state_json(const NpcState& v,bool quests=false) {
    json result={{"count",v.count},{"undo_count",v.undo_count},{"roster_sha256",v.roster_sha256},
        {"global_random",v.global_random},{"created",npc_actor_json(v.created,quests)}};
    if(quests)result["quest_targets_sha256"]=v.quest_targets_sha256;return result;
}
NpcState npc_state_read(const json& j,bool quests=false) {
    if(quests)exact(j,{"count","undo_count","roster_sha256","global_random","created","quest_targets_sha256"});
    else exact(j,{"count","undo_count","roster_sha256","global_random","created"});
    NpcState v;v.count=u64(j.at("count"));v.undo_count=u64(j.at("undo_count"));
    v.roster_sha256=j.at("roster_sha256").get<std::string>();
    const auto& random=j.at("global_random");if(!random.is_array()||random.size()!=4)
        throw std::runtime_error("NPC global random array size differs");
    for(size_t i=0;i<4;++i)v.global_random[i]=u64(random[i]);
    v.created=npc_actor_read(j.at("created"),quests);
    if(quests)v.quest_targets_sha256=j.at("quest_targets_sha256").get<std::string>();
    if(!valid_npc_state(v))throw std::runtime_error("NPC state outside domain");return v;
}
json npc_intent_json(const NpcIntent& v) {
    return {{"region",v.region},{"type",v.type},{"character_slot",v.character_slot},
        {"character_type",v.character_type},{"position",v.position},{"rotation",v.rotation},{"scale",v.scale}};
}
NpcIntent npc_intent_read(const json& j) {
    exact(j,{"region","type","character_slot","character_type","position","rotation","scale"});
    NpcIntent v;v.region=i32(j.at("region"));const auto type=u64(j.at("type"));
    if(type>UINT8_MAX)throw std::runtime_error("NPC type overflow");v.type=uint8_t(type);
    v.character_slot=i32(j.at("character_slot"));v.character_type=i32(j.at("character_type"));
    const auto read_floats=[&](const char* key,auto& target){const auto& a=j.at(key);
        if(!a.is_array()||a.size()!=target.size())throw std::runtime_error(std::string("NPC ")+key+" array size differs");
        for(size_t i=0;i<target.size();++i)target[i]=scalar(a[i]);};
    read_floats("position",v.position);read_floats("rotation",v.rotation);read_floats("scale",v.scale);
    if(!valid_npc_intent(v))throw std::runtime_error("NPC intent outside domain");return v;
}
bool npc_effect(const Request& r,const Evidence& e) {
    const auto& a=e.npc_before;const auto& b=e.npc_after;
    if(!valid_npc_state(a)||!valid_npc_state(b)||a==NpcState{}||b==NpcState{}||
       a.created!=NpcActor{}||b.created==NpcActor{}||a.count>=100000||
       b.count!=a.count+1||a.undo_count>=100000||b.undo_count!=a.undo_count+1||
       b.created.type!=r.npc.type||b.created.character_slot!=r.npc.character_slot||
       b.created.character_type!=r.npc.character_type||b.created.position!=r.npc.position||
       b.created.rotation!=r.npc.rotation||b.created.scale!=r.npc.scale)return false;
    return npc_quest_pool_effect(e,r.npc.type==0)&&e.before==e.after&&e.undo_confirmed&&!e.target_retired&&!e.selection_confirmed&&
        e.object_before==BuildingState{}&&e.object_after==BuildingState{};
}

bool npc_remove_effect(const Evidence& e) {
    const auto& a=e.npc_before;const auto& b=e.npc_after;
    if(!valid_npc_state(a)||!valid_npc_state(b)||a==NpcState{}||b==NpcState{}||
       a.created==NpcActor{}||b.created!=NpcActor{}||a.count==0||b.count!=a.count-1||
       a.undo_count>=100000||b.undo_count!=a.undo_count+1)return false;
    return npc_quest_pool_effect(e,a.created.type==0)&&e.before==e.after&&e.undo_confirmed&&e.target_retired&&e.selection_confirmed&&
        e.object_before==BuildingState{}&&e.object_after==BuildingState{}&&
        e.region_before==RegionState{}&&e.region_after==RegionState{}&&
        e.property_before==RegionPropertyState{}&&e.property_after==RegionPropertyState{}&&
        e.price_before==PriceState{}&&e.price_after==PriceState{}&&
        e.terrain_before==TerrainState{}&&e.terrain_after==TerrainState{}&&
        e.path_before==PathState{}&&e.path_after==PathState{}&&
        e.network_before==NetworkState{}&&e.network_after==NetworkState{}&&e.region_retired_sha256.empty();
}

bool valid_npc_transform(const BuildingState& v) {
    if(!v.type.empty()||!v.variant.empty()||!v.alive)return false;
    for(float x:{v.x,v.y,v.z})if(!std::isfinite(x)||std::abs(x)>1000000.0f)return false;
    for(float x:v.rotation)if(!std::isfinite(x))return false;
    for(float x:v.scale)if(!std::isfinite(x)||x<=0)return false;
    return true;
}
bool npc_move_effect(const Request& r,const Evidence& e) {
    const auto& a=e.npc_before;const auto& b=e.npc_after;
    if(!valid_npc_transform(r.building)||!valid_npc_state(a)||!valid_npc_state(b)||
       a.created==NpcActor{}||b.created==NpcActor{}||a.count==0||a.count!=b.count||
       a.undo_count>=100000||b.undo_count!=a.undo_count+1)return false;
    auto expected=a.created;expected.position={r.building.x,r.building.y,r.building.z};
    expected.rotation=r.building.rotation;expected.scale=r.building.scale;
    return npc_quest_pool_effect(e,a.created.type==0)&&b.created==expected&&e.before==e.after&&e.undo_confirmed&&!e.target_retired&&e.selection_confirmed&&
        e.object_before==BuildingState{}&&e.object_after==BuildingState{}&&
        e.region_before==RegionState{}&&e.region_after==RegionState{}&&
        e.property_before==RegionPropertyState{}&&e.property_after==RegionPropertyState{}&&
        e.price_before==PriceState{}&&e.price_after==PriceState{}&&
        e.terrain_before==TerrainState{}&&e.terrain_after==TerrainState{}&&
        e.path_before==PathState{}&&e.path_after==PathState{}&&
        e.network_before==NetworkState{}&&e.network_after==NetworkState{}&&e.region_retired_sha256.empty();
}
