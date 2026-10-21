namespace zone_edit_runtime {
namespace {
namespace a=action_session;
namespace q=npc_quest_targets;
namespace identity=world_zone_runtime;
namespace qr=monster_quest_retirement;
namespace za=zone_action_runtime;
namespace d=zone_probe_detail;
namespace poly=ui_terrain_runtime::poly;
using Kind=a::ZoneEditKind;
struct Sample {
    identity::Edit edit;
    host::WorldState world;
    std::vector<zone_edit_read::Row> rows;
    std::map<uint64_t,std::string> keys;
    std::map<uint64_t,std::string> monster_keys;
    std::map<uintptr_t,std::string> objects;
    d::Sample native;
    qr::Witness quests;
    json references,authored;
    a::ZoneEditState value;
    a::Ledger money;
};
bool find_uid(Memory& memory,uint64_t uid,uintptr_t& object,std::string& error){
    host::LookupApi api;
    if(!host::resolve_lookup(memory.process,za::state.image,exe_sha,api,error))return false;
    const auto exception=lookup(api.find,uid,object);
    return !exception||fail(error,"Zone edit original FindUID exception="+std::to_string(exception));
}
bool current_world(Memory& memory,const identity::Edit& edit,host::WorldState& world,std::string& error){
    host::LookupApi api;host::NativeOrigin origin;
    if(!host::resolve_lookup(memory.process,za::state.image,exe_sha,api,error))return false;
    host::Reader reader(memory.process,origin,za::state.image,za::state.thread,api);
    return reader.current(world,true,error)&&action_runtime::bound(world,edit.binding,edit.snapshot,error)&&
        ((world.root==edit.world.root&&uintptr_t(world.origin.map)==edit.world.map&&world.selection==edit.world.selection)||
         fail(error,"Zone edit complete current World/Selection differs from prepared ownership"));
}
json shape_json(const a::TerrainShape& shape){
    json paths=json::array();
    for(const auto& path:shape){json points=json::array();for(const auto& point:path)points.push_back({point.x,point.z});paths.push_back(std::move(points));}
    return paths;
}
bool read_quests(Memory& memory,const identity::Edit& edit,const std::vector<identity::Current>& rows,
                 qr::Witness& out,std::string& error,const qr::Witness* previous){
    const auto owner=[&](uintptr_t pointer,uint64_t& uid,std::string& key,std::string& cause){
        if(!field(memory,pointer,0x18,uid,cause)||!uid||!action_runtime::local_key(pointer,uid,key,cause))return false;
        action_runtime::ui::LocalBinding local;uintptr_t region=0;npc_census::State census;
        return action_runtime::target_binding(npc_target_key::argument(key),edit.binding,edit.snapshot,local,region,census,cause)&&
            ((local.object==pointer&&local.uid==uid)||fail(cause,"Zone Quest owner current full UID differs"));
    };
    const auto destination=[&](uintptr_t pointer,std::string& key,std::string& cause){
        uintptr_t ap=0;if(!field(memory,pointer,0,ap,cause))return false;
        if(ap==za::state.image+0x1745ce0){
            const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value.object==pointer;});
            if(row!=rows.end()){
                uint64_t uid=0;uintptr_t actual=0,root=0,region=0;
                if(!field(memory,pointer,0x18,uid,cause)||!find_uid(memory,uid,actual,cause)||
                   !field(memory,pointer,0x170,root,cause)||!field(memory,pointer,0x178,region,cause))return false;
                if(uid!=row->uid||actual!=pointer||root!=row->root||region!=row->region)
                    return fail(cause,"Zone Quest preview destination full UID/owner differs");
                key=row->key;return true;
            }
        }
        if(ap==za::state.image+0x16c0770)return monster_action_runtime::quest_authoring::direct_destination(pointer,key,cause);
        if(ap==za::state.image+0x1742980)return monster_action_runtime::quest_authoring::npc_destination(pointer,key,cause);
        // A changed Zone pool must not invalidate an unrelated Building target.
        // The existing Building binder still verifies its full live ownership.
        if(ap!=za::state.image+0x1745ce0)return q::building_key(edit.world,pointer,key,cause);
        bool dungeon=false;return npc_quest_target_runtime::bind_current_destination(pointer,key,dungeon,cause);
    };
    return qr::read(memory,za::state.image,0,"",owner,destination,out,error,previous);
}
bool sample(Memory& memory,const a::Binding& binding,const std::string& snapshot,int32_t region,const std::string& operation,
            Sample& out,std::string& error,const identity::Edit* prepared=nullptr,const identity::Preview* preview=nullptr,
            const qr::Witness* previous=nullptr){
    Sample next;
    if(prepared)next.edit=*prepared;
    else if(!identity::prepare_edit(binding,snapshot,region,operation,next.edit,error))return false;
    if(next.edit.binding!=binding||next.edit.snapshot!=snapshot||next.edit.world.saved_region!=region||
       !current_world(memory,next.edit,next.world,error))return fail(error,error.empty()?"Zone sample prepared binding differs":error);
    const auto& owners=preview?preview->rows:next.edit.before;
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& cause){return find_uid(memory,uid,object,cause);};
    if(!zone_edit_read::read(memory,za::state.image,owners,find,next.rows,error)||
       !d::sample(memory,next.edit.world,next.native,error))return false;
    next.authored=json::array();
    for(const auto& row:next.rows){
        if(!next.keys.emplace(row.owner.uid,row.owner.key).second||!next.objects.emplace(row.owner.object,row.owner.key).second)
            return fail(error,"Zone sample canonical owner repeats");
        const auto& content=row.content;
        ++next.value.counts[size_t(row.owner.type)];
        next.authored.push_back({{"key",row.owner.key},{"type",content.type},{"density",content.density},
            {"level",content.level},{"monster_type",content.monster_type},{"constructed_day",content.constructed_day},
            {"sections",content.sections},{"shape",shape_json(row.shape)}});
    }
    json generated=json::array(),members=json::array();
    std::map<uint64_t,std::string> generated_keys;
    for(const auto& zone:next.native.state.zones){
        const auto canonical=next.keys.find(zone.value.uid);
        if(canonical==next.keys.end()||next.objects.find(zone.value.pointer)==next.objects.end())
            return fail(error,"Zone sample Monster pool is absent from complete four-array ownership");
        json ordered=json::array();
        for(size_t index=0;index<zone.monsters.size();++index){
            const auto key=canonical->second+":monster:"+std::to_string(index);
            if(!generated_keys.emplace(zone.monsters[index],key).second)return fail(error,"Zone sample generated full UID repeats");
            ordered.push_back(key);
        }
        members.push_back({{"zone",canonical->second},{"members",std::move(ordered)}});
    }
    for(const auto& monster:next.native.state.monsters){
        uintptr_t actual=0;if(!find(monster.uid,actual,error)||actual!=monster.pointer)
            return fail(error,error.empty()?"Zone sample Monster full UID lookup differs":error);
        std::string key;
        if(monster.zone){
            const auto found=generated_keys.find(monster.uid);
            if(found==generated_keys.end()||!next.objects.contains(monster.zone))
                return fail(error,"Zone sample generated Monster reciprocal membership differs");
            key=found->second;
        }else{
            const direct_monster_identity::Scope scope{next.world.origin.uuid,next.world.origin.world_epoch,next.world.origin.generation,
                next.world.root,next.edit.world.region,next.native.state.monster_manager,"world"};
            if(!direct_monster_identity::registry.resolve(scope,monster.pointer,monster.uid,key,error))return false;
        }
        if(!next.monster_keys.emplace(monster.uid,key).second)return fail(error,"Zone sample Monster canonical UID repeats");
        // Moving state is published by Actor24. These are the authored identity
        // and template fields affected by the original Zone operation.
        generated.push_back({{"key",key},{"rank",monster.rank},{"slot",monster.slot},{"type",monster.type},{"level",monster.level}});
    }
    next.value.monsters=uint32_t(next.native.state.monsters.size());
    next.value.global_random=next.native.state.global_random;next.value.region_random=next.native.state.region_random;
    next.value.next_tag=next.native.state.next_monster_tag;next.value.created_monsters=next.native.state.created_monsters;
    next.value.release_edits=next.native.release_edits;next.money=za::ledger(next.native.money);
    if(!read_quests(memory,next.edit,owners,next.quests,error,previous)||
       !world_quest_history::references(binding,snapshot,next.quests,next.references,error)||
       !monster_action_runtime::quest_digest(next.quests,next.value.quests_sha,error)||
       !quest_bridge::sha256(next.references.dump(),next.value.references_sha,error)||
       !quest_bridge::sha256(json{{"zones",next.authored},{"members",members},{"monsters",generated}}.dump(),next.value.values_sha,error))return false;
    host::WorldState after;
    if(!current_world(memory,next.edit,after,error)||!action_runtime::same_world(next.world,after)||next.native.undo.active_graph)
        return fail(error,error.empty()?"Zone complete sample World/Undo changed":error);
    out=std::move(next);return true;
}
bool same(const Sample& first,const Sample& second){
    return action_runtime::same_world(first.world,second.world)&&first.rows==second.rows&&first.value==second.value&&
        first.money==second.money&&first.native.undo==second.native.undo&&first.references==second.references;
}
}
}
