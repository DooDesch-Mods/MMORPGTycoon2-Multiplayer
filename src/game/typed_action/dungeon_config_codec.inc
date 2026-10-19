bool valid_dungeon_config_intent(const DungeonConfigIntent& v){return key(v.key)&&v.before>=0&&v.desired>=1&&v.desired<=10;}
json dungeon_config_intent_json(const DungeonConfigIntent& v){return {{"key",v.key},{"before",v.before},{"desired",v.desired}};}
DungeonConfigIntent dungeon_config_intent_read(const json& j){exact(j,{"key","before","desired"});return {j.at("key").get<std::string>(),i32(j.at("before")),i32(j.at("desired"))};}
bool valid_dungeon_config_state(const DungeonConfigState& v){if(v==DungeonConfigState{})return true;if(v.desired<0||v.price<0||v.instances.size()>10000)return false;std::set<int32_t> indices;for(const auto& row:v.instances)if(row.index<0||!indices.insert(row.index).second)return false;return true;}
json dungeon_config_state_json(const DungeonConfigState& v){if(v==DungeonConfigState{})return nullptr;json instances=json::array();for(const auto& row:v.instances)instances.push_back({{"index",row.index},{"started",row.started}});return {{"desired",v.desired},{"price",v.price},{"sandbox",v.sandbox},{"instances",instances},{"global_random",v.global_random},{"undo_count",v.undo_count}};}
DungeonConfigState dungeon_config_state_read(const json& j){if(j.is_null())return {};exact(j,{"desired","price","sandbox","instances","global_random","undo_count"});DungeonConfigState v;v.desired=i32(j.at("desired"));v.price=i32(j.at("price"));v.sandbox=ability_bool(j.at("sandbox"));
    if(!j.at("instances").is_array()||j.at("instances").size()>10000)throw std::runtime_error("Dungeon config instance census exceeds bound");for(const auto& row:j.at("instances")){exact(row,{"index","started"});v.instances.push_back({i32(row.at("index")),ability_bool(row.at("started"))});}
    ability_array(j.at("global_random"),v.global_random,u64);const auto count=u64(j.at("undo_count"));if(count>UINT32_MAX)throw std::runtime_error("Dungeon Undo count exceeds uint32");v.undo_count=uint32_t(count);return v;}
bool dungeon_config_ledger(const DungeonConfigIntent& intent,const DungeonConfigState& before,const Ledger& money,Ledger& after){
    const int64_t delta=int64_t(intent.desired)-before.desired;const int64_t cost=delta>0?delta*before.price:0;
    if(cost>INT32_MAX||money.record_present!=1||(!before.sandbox&&money.cash<cost))return false;after=money;
    const auto add=[](int32_t& target,int64_t amount){const auto value=int64_t(target)+amount;if(value<INT32_MIN||value>INT32_MAX)return false;target=int32_t(value);return true;};
    return add(after.cash,-cost)&&add(after.record_cash,-cost)&&add(after.record_expense,cost)&&add(after.line_counts[0],1)&&add(after.line_income[0],-cost);
}
