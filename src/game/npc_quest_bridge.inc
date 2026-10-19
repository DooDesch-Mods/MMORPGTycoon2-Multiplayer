namespace quest_bridge {
namespace quest = mt2mp::npc_quest_records;
namespace targets = mt2mp::game::npc_quest_target_runtime;
int32_t tool_type(const std::string& name) {
    return name=="QuestGiver"?0:name=="Guard"?1:name=="ClassTrainer"?2:-1;
}

bool sha256(const std::string& input,std::string& output,std::string& error) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE handle=nullptr;
    auto status=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0);
    if(status)return fail(error,"NPC SHA256 provider NTSTATUS="+std::to_string(status));
    std::array<unsigned char,32> bytes{};
    status=BCryptCreateHash(algorithm,&handle,nullptr,0,nullptr,0,0);
    if(!status)status=BCryptHashData(handle,reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),ULONG(input.size()),0);
    if(!status)status=BCryptFinishHash(handle,bytes.data(),ULONG(bytes.size()),0);
    const auto hash_cleanup=handle?BCryptDestroyHash(handle):0;
    const auto algorithm_cleanup=BCryptCloseAlgorithmProvider(algorithm,0);
    if(status||hash_cleanup||algorithm_cleanup)return fail(error,"NPC SHA256 failure NTSTATUS="+std::to_string(status)+
        " hash_cleanup="+std::to_string(hash_cleanup)+" provider_cleanup="+std::to_string(algorithm_cleanup));
    constexpr char hex[]="0123456789abcdef";output.clear();output.reserve(64);
    for(const auto byte:bytes){output+=hex[byte>>4];output+=hex[byte&15];}return true;
}
void append(std::string& packed,const std::string& value) {
    packed+=std::to_string(value.size());packed+=':';packed+=value;
}
bool pool(int32_t saved_region,const targets::CurrentTargets& current,
          std::string& digest,std::string& error) {
    if(current.saved_region!=saved_region||current.snapshot_uuid!=net::current_snapshot_id())
        return fail(error,"NPC Quest target pool region or snapshot differs");
    std::string packed="npc-quest-targets-v1:";
    packed+=std::to_string(current.building_keys.size());packed+=':';
    for(const auto& key:current.building_keys){if(key.empty())return fail(error,"NPC Quest Building target key absent");append(packed,key);}
    packed+=std::to_string(current.monster_zone_keys.size());packed+=':';
    for(const auto& key:current.monster_zone_keys){if(key.empty())return fail(error,"NPC Quest Monster target key absent");append(packed,key);}
    return sha256(packed,digest,error);
}
bool pool(int32_t saved_region,std::string& digest,std::string& error) {
    targets::CurrentTargets current;
    return targets::read_current(saved_region,current,error)&&pool(saved_region,current,digest,error);
}
bool records(Memory& memory,uintptr_t image,int32_t saved_region,const npc_census::Row& row,
             quest::State& output,std::string& error) {
    output={};
    if(row.type!=0)return row.quest_count==0||fail(error,"NPC non-QuestGiver has Quest records");
    targets::CurrentTargets current;
    if(!targets::read_current(saved_region,current,error)||
       current.saved_region!=saved_region||current.snapshot_uuid!=net::current_snapshot_id())
        return fail(error,error.empty()?"NPC Quest destination pool snapshot differs":error);
    const auto destination=[&current,&memory,image](uintptr_t pointer,std::string& key,std::string& cause){
        uintptr_t ap=0;if(!field(memory,pointer,0,ap,cause))return false;
        if(ap==image+0x16c0770)return monster_action_runtime::quest_authoring::direct_destination(pointer,key,cause);
        if(ap==image+0x1742980)return monster_action_runtime::quest_authoring::npc_destination(pointer,key,cause);
        bool in_dungeon=false;
        if(!targets::bind_current_destination(pointer,key,in_dungeon,cause))return false;
        if(in_dungeon)return true;
        const auto building=std::find(current.building_keys.begin(),current.building_keys.end(),key);
        const auto zone=std::find(current.monster_zone_keys.begin(),current.monster_zone_keys.end(),key);
        return building!=current.building_keys.end()||zone!=current.monster_zone_keys.end()||
            fail(cause,"NPC Quest destination absent from original ordered target pools");
    };
    if(!quest::read(memory,image,row.local_pointer,destination,output,error))return false;
    if(output.records.size()!=size_t(row.quest_count))
        return fail(error,"NPC QuestGiver Quest count differs from reader");
    targets::CurrentTargets after;
    if(!targets::read_current(saved_region,after,error))return false;
    return (after.snapshot_uuid==current.snapshot_uuid&&
            after.saved_region==current.saved_region&&
            after.building_keys==current.building_keys&&
            after.monster_zone_keys==current.monster_zone_keys)||
        fail(error,"NPC Quest destination pools changed during record read");
}
json daily_json(const quest::DailyCount& value) {
    return {{"history",value.history},{"today",value.today},{"total",value.total}};
}
json record_json(const quest::Record& value) {
    return {{"sequence_id",value.sequence_id},{"type",value.type},{"name",value.name},
        {"description",value.description},{"action",value.action},
        {"monsters_to_kill",value.monsters_to_kill},{"monster_type",value.monster_type},
        {"xp_reward",value.xp_reward},{"gold_reward",value.gold_reward},
        {"destination_key",value.destination_key},{"accepted",daily_json(value.accepted)},
        {"completed",daily_json(value.completed)},
        {"rating",{{"beauty",value.rating.beauty},{"convenience",value.rating.convenience},
                   {"danger",value.rating.danger},{"set",value.rating.set}}}};
}
json records_json(const std::vector<quest::Record>& records) {
    json result=json::array();for(const auto& item:records)result.push_back(record_json(item));return result;
}
const char* task_name(quest::TaskStatus value) {
    switch(value){case quest::TaskStatus::Absent:return "absent";
        case quest::TaskStatus::Pending:return "pending";
        case quest::TaskStatus::FinishedWaitingTick:return "finished_waiting_tick";}
    return "invalid";
}
json tasks_json(const quest::State& value) {
    json result=json::array();for(size_t i=0;i<value.local.size();++i){const auto& item=value.local[i];
        result.push_back({{"sequence_id",value.records[i].sequence_id},{"task",task_name(item.task)},
            {"npc_list_member",item.npc_list_member},{"owner_weak_confirmed",item.owner_weak_confirmed},
            {"destination_weak_confirmed",item.destination_weak_confirmed},
            {"manager_member",item.manager_member}});}
    return result;
}
action_session::NpcActor actor(const npc_census::Row& row,const quest::State& quests) {
    action_session::NpcActor value;value.name=row.name;value.type=row.type;
    value.character_slot=row.character_slot;value.character_type=row.character_type;
    value.level=row.level;value.quest_count=row.quest_count;value.position=row.position;
    value.rotation=row.rotation;value.scale=row.scale;value.quests=quests.records;return value;
}
bool effect_rows(Memory& memory,uintptr_t image,int32_t saved_region,uint64_t created_uid,
                 const npc_census::State& census,uint64_t undo_count,bool quest_pool,
                 action_session::NpcState& output,json* task_receipt,std::string& error) {
    output={};output.count=census.rows.size();output.undo_count=undo_count;
    output.global_random=census.global_random;
    if(quest_pool&&!pool(saved_region,output.quest_targets_sha256,error))return false;
    if(task_receipt)*task_receipt=json::array();
    std::vector<std::string> canonical;canonical.reserve(census.rows.size());bool found=!created_uid;
    for(const auto& row:census.rows) {
        quest::State quests;
        if(!records(memory,image,saved_region,row,quests,error))return false;
        const auto value=actor(row,quests);
        canonical.push_back(json{{"name",value.name},{"type",value.type},{"character_slot",value.character_slot},
            {"character_type",value.character_type},{"level",value.level},{"quest_count",value.quest_count},
            {"position",value.position},{"rotation",value.rotation},{"scale",value.scale},
            {"quests",records_json(value.quests)}}.dump());
        if(task_receipt&&row.type==0)task_receipt->push_back({{"local_uid",row.local_uid},
            {"quest_tasks",tasks_json(quests)}});
        if(created_uid==row.local_uid){if(found)return fail(error,"NPC action created UID duplicated");
            found=true;output.created=value;}
    }
    if(!found)return fail(error,"NPC action created UID absent from actual manager");
    std::sort(canonical.begin(),canonical.end());
    std::string packed;for(const auto& row:canonical)append(packed,row);
    return sha256(packed,output.roster_sha256,error);
}
bool move_quest_stable(const action_session::NpcState& before,
                       const action_session::NpcState& after,
                       const json& tasks_before,const json& tasks_after,
                       std::string& error) {
    if(before.created.type!=0||after.created.type!=0||
       before.quest_targets_sha256.empty()||
       before.quest_targets_sha256!=after.quest_targets_sha256)
        return fail(error,"NPC Move Quest target pool or actor type changed");
    if(before.created.quests!=after.created.quests)
        return fail(error,"NPC Move Quest records or ratings changed");
    if(!tasks_before.is_array()||tasks_before!=tasks_after)
        return fail(error,"NPC Move Quest Task status or membership changed");
    return true;
}
bool removed_quests(Memory& memory,uintptr_t image,
                    const std::vector<uintptr_t>& saved_pointers,
                    const std::vector<int32_t>& sequence_ids,
                    json& receipt,std::string& error) {
    receipt=json::object();
    if(saved_pointers.size()!=sequence_ids.size())
        return fail(error,"NPC Remove Quest pointer/sequence counts differ");
    std::set<uintptr_t> pointers;std::set<int32_t> ids;
    for(size_t index=0;index<saved_pointers.size();++index)
        if(!saved_pointers[index]||!pointers.insert(saved_pointers[index]).second||
           sequence_ids[index]<0||!ids.insert(sequence_ids[index]).second)
            return fail(error,"NPC Remove saved Quest pointer or sequence duplicated/invalid");
    mt2mp::npc_quest_retirement::State current;
    if(!mt2mp::npc_quest_retirement::read(memory,image,saved_pointers,current,error))return false;
    if(current.quests.size()!=saved_pointers.size())
        return fail(error,"NPC Remove Quest retirement result count differs");
    size_t hash_absent=0,array_absent=0;
    std::vector<int32_t> confirmed_ids;
    for(size_t index=0;index<current.quests.size();++index){
        const auto& item=current.quests[index];
        if(!item.hash_present)++hash_absent;
        if(!item.array_present)++array_absent;
        if(!item.hash_present&&!item.array_present)confirmed_ids.push_back(sequence_ids[index]);
    }
    const bool confirmed=current.all_absent()&&confirmed_ids.size()==saved_pointers.size();
    receipt={{"prior_count",saved_pointers.size()},
             {"manager_count",current.manager_count},
             {"hash_absent_count",hash_absent},
             {"array_absent_count",array_absent},
             {"confirmed_sequence_ids",confirmed_ids},{"confirmed",confirmed}};
    return confirmed||fail(error,"NPC Remove QuestManager hash or active array retains original Quest hash_absent="+
        std::to_string(hash_absent)+" array_absent="+std::to_string(array_absent)+
        " required="+std::to_string(saved_pointers.size()));
}
}
