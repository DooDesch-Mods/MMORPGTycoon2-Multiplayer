namespace character_membership {
namespace {
namespace member=character_membership_reader;
namespace definitions=character_definition_identity;
struct Sample {
    host::WorldState world;host::LookupApi api;uintptr_t manager=0,selected=0,collection_record=0;uint64_t uid=0;
    definitions::Census definitions;member::Users users;std::vector<member::User> affected;
    a::CharacterMembershipState value;a::Ledger money;undo_probe::UndoWitness undo;
    qr::Witness quests;quest_list::References references;
    std::map<uintptr_t,std::string> owners;
    std::vector<action_runtime::CharacterNpcRetirement> npc_retirements;
    std::vector<character_membership_npc_completion::Pool> npc_pools;
    std::vector<std::pair<std::string,identity::Entry>> monster_aliases;
};
using NativeMemory=Memory;
struct ValueMemory final:subscriber_lifecycle::Memory {
    NativeMemory& source;explicit ValueMemory(NativeMemory& value):source(value){}
    bool read(uintptr_t p,void* out,size_t bytes,std::string& error)override{return source.copy(p,out,bytes,error);}
};
struct Captured {ValueMemory& memory;subscriber_lifecycle::Graph graph;};
bool inspect(void* user,const void* record,const void* context,std::string& error){auto& value=*static_cast<Captured*>(user);
    return subscriber_lifecycle::read_graph(value.memory,uintptr_t(record),uintptr_t(context),{100000,1000000,32*1024*1024,100000},value.graph,error);
}
bool owner_add(std::map<uintptr_t,std::string>& owners,uintptr_t object,const std::string& key,std::string& error){
    if(!object||key.empty())return fail(error,"Character original Record owner is empty");
    const auto [found,inserted]=owners.emplace(object,key);return inserted||found->second==key||fail(error,"Character original Record owner aliases two canonical keys");
}
bool complete_record(Memory& memory,uintptr_t object,uintptr_t prepare,uintptr_t save,const std::map<uintptr_t,std::string>& owners,std::string& hash,std::string& error){
    ValueMemory graph_memory(memory);Captured value{graph_memory};subscriber_lifecycle::CaptureReceipt cleanup;
    const subscriber_lifecycle::CaptureApi calls{
        reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x9667c0),reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x9676a0),
        reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x89e580),reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x89e5f0),
        reinterpret_cast<subscriber_lifecycle::PrepareSave>(state.image+prepare),reinterpret_cast<subscriber_lifecycle::SaveRecord>(state.image+save)};
    {live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);
        if(!subscriber_lifecycle::capture_original(calls,reinterpret_cast<const void*>(object),inspect,&value,cleanup,error))return false;}
    const auto external=[&](uintptr_t pointer,std::string& key,std::string& cause){const auto found=owners.find(pointer);
        if(found==owners.end())return fail(cause,"Character complete original definition Record contains an unresolved external reference");key=found->second;return true;};
    duplicate_records::Witness result;if(!duplicate_records::summarize(value.graph,object,external,result,error))return false;
    hash=std::move(result.sha);return true;
}
bool row(Memory& memory,const definitions::Census& census,const definitions::Definition& definition,a::CharacterMembershipRow& out,std::string& error){
    out={};out.kind=definition.kind;out.id=definition.id;out.deleted=definition.deleted;
    if(!npc_quest_records::read_string(memory,definition.object,0xb0,"Character membership name",out.name,error))return false;
    if(definition.parent&&!definition.deleted){const definitions::Definition* parent=nullptr;
        if(!definitions::find(census,definition.parent,definition.parent_uid,parent,error))return false;out.parent=parent->id;}
    for(const auto& collection:census.collections)if(collection.owner==definition.object)
        for(const auto& id:collection.ids)out.variants.push_back(id[1]);
    return a::character_membership_row(out)||fail(error,"Character original family metadata violates its native ID/variant contract");
}
bool external_owners(Memory& memory,const a::Binding& binding,const std::string& snapshot,Sample& out,std::string& error,const Sample* previous){
    auto& owners=out.owners;const auto map=uintptr_t(out.world.origin.map);
    if(!owner_add(owners,out.world.root,"root_shard",error)||!owner_add(owners,map,"world-map",error)||
       !owner_add(owners,uintptr_t(out.world.origin.state),"native-state",error)||!owner_add(owners,out.manager,"character-manager",error))return false;
    for(const auto& definition:out.definitions.definitions)if(!owner_add(owners,definition.object,definition.key,error))return false;
    for(const auto& definition:out.definitions.definitions)if(!definition.deleted){
        uintptr_t data=0;int32_t count=0,capacity=0;
        if(!field(memory,definition.object,0x130,data,error)||!field(memory,definition.object,0x138,count,error)||
           !field(memory,definition.object,0x13c,capacity,error))return false;
        if(count<0||count>10000||capacity<count||(count&&!data))return fail(error,"Character original Ability owner-array bounds differ");
        for(int32_t slot=0;slot<count;++slot){uintptr_t prototype=0,ap=0,found=0;uint64_t uid=0;
            if(!field(memory,data,size_t(slot)*8,prototype,error)||!field(memory,prototype,0,ap,error)||!field(memory,prototype,0x18,uid,error))return false;
            const auto seh=lookup(out.api.find,uid,found);
            if(seh||!uid||ap!=state.image+0x171c930||found!=prototype)
                return fail(error,"Character current Ability complete Record owner/full UID differs SEH="+std::to_string(seh));
            if(!owner_add(owners,prototype,definition.key+"/ability/"+std::to_string(slot),error))return false;}
        uintptr_t current_data=0;int32_t current_count=0,current_capacity=0;
        if(!field(memory,definition.object,0x130,current_data,error)||!field(memory,definition.object,0x138,current_count,error)||
           !field(memory,definition.object,0x13c,current_capacity,error))return false;
        if(current_data!=data||current_count!=count||current_capacity!=capacity)return fail(error,"Character original Ability owner-array changed during census");}
    for(const auto& definition:out.definitions.definitions)if(definition.variants&&!owner_add(owners,definition.variants,"character-variants:"+definition.key,error))return false;
    for(const auto& definition:out.definitions.definitions){
        if(!definition.deleted&&definition.owning_collection&&!owners.contains(definition.owning_collection)){
            const auto prior=previous?previous->owners.find(definition.owning_collection):owners.end();
            owners.emplace(definition.owning_collection,previous&&prior!=previous->owners.end()?prior->second:"character-collection:"+definition.key);}}
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return field(memory,base,offset,value,cause);};
    definitions::Array regions;if(!definitions::array(at,map+0x230,regions,error))return false;
    for(const auto region:regions.members){int32_t id=-1;uintptr_t selected=0;
        if(!at(region,0x834,id,error)||!region_saved_identity::resolve(state.image,map,out.world.root,id,region,selected,at,error)||
           !owner_add(owners,region,"region:"+std::to_string(id),error))return false;}
    if(!definitions::unchanged(at,regions,error))return false;
    dungeon_scene_identity_runtime::Census scenes;
    if(!dungeon_scene_identity_runtime::read_current(scenes,error))return false;
    if(scenes.world.map!=out.world.origin.map||scenes.world.state!=out.world.origin.state||
       scenes.world.uuid!=out.world.origin.uuid||scenes.world.world_epoch!=binding.native_epoch||scenes.world.generation!=binding.native_generation)
        return fail(error,"Character original Actor Record Dungeon census has a foreign current World");
    for(const auto& pool:scenes.pools){const auto& scene=pool.scene.owner;
        if(!owner_add(owners,scene.exterior,scene.exterior_key,error)||
           !owner_add(owners,scene.dungeon,scene.exterior_key+"/dungeon",error)||
           !owner_add(owners,scene.shard,scene.key+"/shard",error)||
           !owner_add(owners,scene.region,scene.key+"/region",error))return false;
        if(scene.instance&&!owner_add(owners,scene.instance,scene.key+"/instance-object",error))return false;
        for(size_t i=0;i<pool.managers.size();++i)
            if(!owner_add(owners,pool.managers[i],scene.key+"/manager/"+std::to_string(i),error))return false;
        for(const auto& gizmo:pool.local.members){std::string key;
            if(!dungeon_scene_identity_runtime::resolve(gizmo.object,gizmo.uid,key,error)||!owner_add(owners,gizmo.object,key,error))return false;}}
    json props;if(!remove_diagnostics::bindings(props,error))return false;
    for(const auto& prop:props.at("bindings")){if(prop.at("revoked").get<bool>())continue;
        if(prop.at("unknown").get<bool>()||prop.at("snapshot_id")!=snapshot)return fail(error,"Character complete original Record shared prop alias is unknown or foreign");
        uintptr_t pointer=0;const auto seh=lookup(out.api.find,prop.at("local_uid").get<uint64_t>(),pointer);
        if(seh||!pointer)return fail(error,"Character complete original Record prop FindUID failed SEH="+std::to_string(seh));
        if(!owner_add(owners,pointer,prop.at("record_key").get<std::string>(),error))return false;}
    a::Context context;std::vector<action_runtime::StateOwner> npcs;
    if(!a::selected_session().current(context,error)||!action_runtime::state_owners(context,binding,npcs,error))return false;
    for(const auto& npc:npcs){if(!owner_add(owners,npc.npc,npc.key,error))return false;
        for(const auto& [slot,quest]:npc.quests)if(!owner_add(owners,quest,npc.key+"/quest/"+std::to_string(slot),error))return false;}
    for(const auto& local:out.references.local){
        if(!owner_add(owners,local[0],"subscriber:"+std::to_string(local[2]),error))return false;
        if(local[1]&&!owner_add(owners,local[1],"toon:"+std::to_string(local[2]),error))return false;}
    std::vector<party_saved_identity_runtime::Current> parties;
    if(!party_saved_identity_runtime::read_current(parties,error)||
       !character_actor_party_owners::append({out.world.origin.uuid,binding.native_epoch,binding.native_generation,out.world.root},parties,owners,error))return false;
    for(const auto& [definition,users]:out.users.by_definition)for(size_t index=0;index<users.size();++index){const auto& user=users[index];
        if(!owners.contains(user.object))owners.emplace(user.object,definition+"/user/"+std::to_string(index));}
    return true;
}
bool sample(Memory& memory,const a::Binding& binding,const std::string& snapshot,const a::CharacterMembershipIntent& intent,
    Sample& out,std::string& error,const Sample* previous=nullptr,const a::Request* request=nullptr){
    out={};host::NativeOrigin origin;if(!host::resolve_lookup(memory.process,state.image,exe_sha,out.api,error))return false;
    host::Reader reader(memory.process,origin,state.image,state.thread,out.api);
    if(!reader.current(out.world,true,error)||!action_runtime::bound(out.world,binding,snapshot,error))return false;
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return field(memory,base,offset,value,cause);};
    auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& cause){const auto seh=lookup(out.api.find,uid,pointer);return !seh||fail(cause,"Character current user FindUID failed SEH="+std::to_string(seh));};
    a::Context context;if(!a::selected_session().current(context,error))return false;
    if(context.snapshot_uuid!=snapshot)return fail(error,"Character membership actual Session snapshot differs");
    if(!character_actor_records::read_current(context,binding,out.definitions,out.users,error))return false;
    out.manager=out.definitions.manager;
    const definitions::Definition* selected=nullptr;
    for(const auto& definition:out.definitions.definitions){const auto kind=size_t(definition.kind);++out.value.counts[kind];
        if(definition.deleted&&out.value.first_deleted[kind]<0)out.value.first_deleted[kind]=definition.id;
        if(definition.kind==intent.kind&&definition.id==intent.id)selected=&definition;}
    if(!selected)return fail(error,"Character membership selected global ID is absent");out.selected=selected->object;out.uid=selected->uid;
    if(!row(memory,out.definitions,*selected,out.value.selected,error))return false;
    if(!selected->deleted&&!member::collection_member(at,state.image,*selected,out.collection_record,error))return false;
    const auto family=member::affected(out.definitions,*selected);std::set<uint64_t> unique;
    for(const auto& user:out.users.rows)if(std::find(family.begin(),family.end(),user.definition)!=family.end()&&unique.insert(user.uid).second){
        out.affected.push_back(user);if(user.family==member::Family::Npc){uintptr_t region=0;uint8_t queued=2;
            if(!at(user.object,0x178,region,error)||!at(user.object,0x894,queued,error)||!region)return fail(error,error.empty()?"Character affected NPC lacks actual Region":error);
            if(queued)return fail(error,"Character membership pre-state contains an already queued native NPC destruction");
            out.npc_retirements.push_back({user.object,region,user.uid});
            if(std::none_of(out.npc_pools.begin(),out.npc_pools.end(),[&](const auto& pool){return pool.region==region;})){
                npc_census::State census;if(!npc_census::read(memory,state.image,region,census,error))return false;
                character_membership_npc_completion::Pool pool;pool.region=region;pool.manager=census.manager;pool.next_tag=census.next_local_tag;
                for(const auto& value:census.rows)pool.rows.push_back({value.local_pointer,value.local_uid,value.type});out.npc_pools.push_back(std::move(pool));}}}
    if(!identity::registry.enumerate(out.world.origin.uuid,binding.native_epoch,binding.native_generation,out.world.root,out.monster_aliases,error))return false;
    qr::Witness seed;if(previous){seed=previous->quests;
        for(const auto& user:previous->affected)if(user.family==member::Family::Npc){uintptr_t pointer=0;if(!find(user.uid,pointer,error))return false;if(!pointer)seed.owners.erase(user.object);}}
    for(const auto& user:out.users.rows)if(user.family==member::Family::Npc&&!seed.owners.contains(user.object)){
        std::string key;if(!action_runtime::local_key(user.object,user.uid,key,error))return false;
        qr::Owner current;current.pointer=user.object;current.uid=user.uid;current.key=key;seed.owners.emplace(user.object,std::move(current));}
    if(!read_remove_quests(memory,binding,snapshot,0,{},out.quests,error,&seed)||
       !quest_list::references(memory,binding,snapshot,out.quests,out.references,error)||!external_owners(memory,binding,snapshot,out,error,previous))return false;
    json metadata=json::array(),family_rows=json::array(),other_rows=json::array(),user_rows=json::array(),boss_rows=json::array();
    const int32_t new_id=previous?(previous->value.first_deleted[size_t(intent.kind)]<0?previous->value.counts[size_t(intent.kind)]:previous->value.first_deleted[size_t(intent.kind)]):
        (out.value.first_deleted[size_t(intent.kind)]<0?out.value.counts[size_t(intent.kind)]:out.value.first_deleted[size_t(intent.kind)]);
    for(const auto& definition:out.definitions.definitions){a::CharacterMembershipRow value;if(!row(memory,out.definitions,definition,value,error))return false;
        json portable={{"kind",value.kind},{"id",value.id},{"parent",value.parent},{"deleted",value.deleted},{"name",value.name},{"variants",value.variants}};
        metadata.push_back(portable);const bool related=definition.object==out.selected||(!definition.deleted&&definition.parent==out.selected);
        std::string complete;if(!definition.deleted&&!complete_record(memory,definition.object,0xf62880,0xf628e0,out.owners,complete,error))return false;
        portable["record_sha"]=complete;
        if(related)family_rows.push_back(portable);
        else if(!(intent.command==a::CharacterMembershipCommand::CreateVariant&&definition.kind==intent.kind&&definition.id==new_id))other_rows.push_back(portable);
        if(previous&&intent.command==a::CharacterMembershipCommand::CreateVariant&&definition.kind==intent.kind&&definition.id==new_id)out.value.created=value;
    }
    if(previous&&intent.command==a::CharacterMembershipCommand::Destroy){auto expected=previous->value.selected.variants;expected.push_back(intent.id);std::sort(expected.begin(),expected.end());
        for(const auto id:expected){const auto found=std::find_if(out.definitions.definitions.begin(),out.definitions.definitions.end(),[&](const auto& definition){return definition.kind==intent.kind&&definition.id==id;});
            if(found==out.definitions.definitions.end()||!found->deleted||found->uid)return fail(error,"Character original Destroy did not retire every selected/base-variant definition");
            out.value.retired_definitions.push_back(id);}}
    const auto external=[&](uintptr_t pointer,std::string& key,std::string& cause){const auto found=out.owners.find(pointer);
        if(found==out.owners.end())return fail(cause,"Character complete original Actor Record contains an unresolved external owner");key=found->second;return true;};
    std::map<uintptr_t,std::string> actor_records;
    for(const auto& user:out.users.rows)if(!actor_records.contains(user.object)){
        character_actor_records::Result captured;if(!character_actor_records::read(binding,snapshot,user.family,user.object,external,captured,error))return false;
        actor_records.emplace(user.object,std::move(captured.portable.sha));}
    for(const auto& [definition,users]:out.users.by_definition){if(users.empty())continue;json rows=json::array();
        for(const auto& user:users){json value={{"key",out.owners.at(user.object)},{"family",uint8_t(user.family)},{"record_sha",actor_records.at(user.object)}};
            rows.push_back(value);if(user.family==member::Family::Boss)boss_rows.push_back(value);}
        user_rows.push_back({{"definition",definition},{"users",rows}});}
    std::string manager,quest_rows;if(!complete_record(memory,out.manager,0x107fa10,0x107fa70,out.owners,manager,error)||
       !quest_digest(out.quests,quest_rows,error))return false;
    out.value.manager_sha=manager;
    if(intent.command==a::CharacterMembershipCommand::Import){uintptr_t store=0,ap=0;
        ValueMemory guarded(memory);if(!character_import_native::verify(state.image,guarded,error))return false;
        if(!field(memory,state.image,0x13df210,store,error)||!field(memory,store,0,ap,error)||ap!=state.image+0x16d9300)
            return fail(error,error.empty()?"Character import current original ArtStore singleton differs":error);
        a::Context current;std::map<uintptr_t,std::string> refs;
        if(!a::selected_session().current(current,error)||!general_project_runtime::release_reference_keys(current,binding,refs,error))return false;
        for(const auto& [pointer,key]:refs){const auto found=out.owners.find(pointer);
            if(found!=out.owners.end()&&found->second!=key)return fail(error,"Character import ArtStore canonical owner aliases another current object");
            out.owners[pointer]=key;}
        out.owners[store]="art-store";
        if(!complete_record(memory,store,0xe56320,0xe56380,out.owners,out.value.import_art_sha,error))return false;
    }
    if(!quest_bridge::sha256(family_rows.dump(),out.value.family_sha,error)||!quest_bridge::sha256(other_rows.dump(),out.value.others_sha,error)||
       !quest_bridge::sha256(user_rows.dump(),out.value.users_sha,error)||!quest_bridge::sha256(boss_rows.dump(),out.value.bosses_sha,error)||
       !quest_bridge::sha256(json{{"global",quest_rows},{"completed",out.quests.completed},{"active",out.references.rows}}.dump(),out.value.quests_sha,error))return false;
    if(request){json economy,monsters;if(!typed_action_runtime::read_character_membership_actors(binding,*request,economy,monsters,error)||
        !quest_bridge::sha256(economy.dump(),out.value.economy_sha,error)||!quest_bridge::sha256(monsters.dump(),out.value.monsters_sha,error))return false;}
    native::SimulationState money;uintptr_t random=0,release_manager=0,release=0,ap=0;
    if(!native::read_simulation_state(money,error)||!undo_probe::read_undo(memory.process,state.image,out.undo,error))return false;
    if(out.undo.active_graph)return fail(error,"Character membership current native Undo graph is still active");
    if(
       !at(state.image,0x164b250,random,error)||!at(random,0,out.value.global_random,error)||!at(state.image,0x13df4f0,release_manager,error)||
       !at(release_manager,0,ap,error))return false;
    if(ap!=state.image+0x1714ed0)return fail(error,"Character membership actual ReleaseManager class differs");
    if(!at(release_manager,0x38,release,error)||!at(release,0,ap,error))return false;
    if(ap!=state.image+0x16c12b0)return fail(error,"Character membership actual current Release class differs");
    if(!at(release,0x120,out.value.release_edits,error))return false;
    if(intent.command==a::CharacterMembershipCommand::Import){out.owners[release]="current-release";
        if(!complete_record(memory,release,0xe4f460,0xe4f4c0,out.owners,out.value.import_release_sha,error))return false;}
    out.money=ledger(money);out.value.undo_count=out.undo.undo.size();
    definitions::Census final_definitions;member::Users final_users;host::WorldState final_world;
    if(!definitions::read(at,state.image,uintptr_t(out.world.origin.state),out.manager,final_definitions,error)||
       !member::users(at,find,state.image,final_definitions,final_users,error)||final_users!=out.users||
       !reader.current(final_world,true,error)||!same_world(out.world,final_world))return fail(error,error.empty()?"Character complete original sampling changed current World/users":error);
    return true;
}
}
}
