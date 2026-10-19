namespace qr=monster_quest_retirement;
bool read_remove_quests(Memory& m,const a::Binding& binding,const std::string& snapshot,uintptr_t object,const std::string& target,qr::Witness& out,std::string& e,
                        const qr::Witness* previous=nullptr){
    const auto owner=[&](uintptr_t pointer,uint64_t& uid,std::string& key,std::string& cause){
        if(!field(m,pointer,0x18,uid,cause)||!uid||!action_runtime::local_key(pointer,uid,key,cause))return false;
        action_runtime::ui::LocalBinding local;uintptr_t region=0;npc_census::State census;
        if(!action_runtime::target_binding(npc_target_key::argument(key),binding,snapshot,local,region,census,cause))return false;
        return (local.object==pointer&&local.uid==uid)||fail(cause,"Monster Quest canonical owner full UID or pointer differs");
    };
    const auto destination=[&](uintptr_t pointer,std::string& key,std::string& cause){
        uintptr_t ap=0;if(!field(m,pointer,0,ap,cause))return false;
        if(ap==state.image+0x16c0770)return quest_authoring::direct_destination(pointer,key,cause);
        if(ap==state.image+0x1742980)return quest_authoring::npc_destination(pointer,key,cause);
        bool in_dungeon=false;
        return npc_quest_target_runtime::bind_current_destination(pointer,key,in_dungeon,cause);
    };
    return qr::read(m,state.image,object,target,owner,destination,out,e,previous);
}
bool quest_digest(const qr::Witness& value,std::string& out,std::string& e){
    json rows=json::array();
    for(const auto& row:value.rows)rows.push_back({{"owner_key",row.owner_key},{"record",quest_bridge::record_json(row.record)}});
    return quest_bridge::sha256(rows.dump(),out,e);
}
bool quest_before(const qr::Witness& before,a::MonsterQuestRetirement& out,std::string& e){
    out={};out.before_count=uint32_t(before.quests.size());out.completed_before=before.completed;
    if(!quest_digest(before,out.before_sha256,e))return false;
    for(size_t i=0;i<before.quests.size();++i)
        if(std::find(before.removed.begin(),before.removed.end(),before.quests[i])!=before.removed.end())out.removed.push_back(before.rows[i]);
    return true;
}
bool quest_before_matches(const a::MonsterQuestRetirement& local,const a::MonsterQuestRetirement& host){
    return local.before_count==host.before_count&&local.completed_before==host.completed_before&&
        local.before_sha256==host.before_sha256&&local.removed==host.removed;
}
std::string quest_before_difference(const a::MonsterQuestRetirement& local,const a::MonsterQuestRetirement& host){
    std::string cause;
    const auto add=[&](const std::string& value){if(!cause.empty())cause+="; ";cause+=value;};
    if(local.before_count!=host.before_count)add("global_count local="+std::to_string(local.before_count)+" host="+std::to_string(host.before_count));
    if(local.completed_before!=host.completed_before)add("completed_count local="+std::to_string(local.completed_before)+" host="+std::to_string(host.completed_before));
    if(local.before_sha256!=host.before_sha256)add("global_records_or_order_sha256 local="+local.before_sha256+" host="+host.before_sha256);
    if(local.removed!=host.removed)add("matching_records differ local_count="+std::to_string(local.removed.size())+" host_count="+std::to_string(host.removed.size()));
    return cause;
}
json quest_retirement_json(const a::MonsterQuestRetirement& value){
    json rows=json::array();for(const auto& row:value.removed)rows.push_back({{"owner_key",row.owner_key},{"record",quest_bridge::record_json(row.record)}});
    return {{"before_sha256",value.before_sha256},{"after_sha256",value.after_sha256},
        {"before_count",value.before_count},{"after_count",value.after_count},
        {"completed_before",value.completed_before},{"completed_after",value.completed_after},{"removed",rows}};
}
bool quest_receipt_fits(const a::MonsterQuestRetirement& value){
    return quest_retirement_json(value).dump().size()<=a::max_payload-4096;
}
