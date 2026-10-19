namespace {
namespace cb=character_boss_retirement_read;
struct CharacterBossPrepared {
    uint64_t ticket=0,revision=0;
    action_session::Binding binding;
    action_session::Request request;
    prop_lease::World world;
    uintptr_t root=0;
    std::map<uintptr_t,cb::Pool> pools;
    std::map<uintptr_t,std::vector<cb::Member>> removed;
    std::map<std::string,Bound> aliases;
    bool confirmed=false;
};
std::map<std::string,CharacterBossPrepared> character_boss_prepared;
uint64_t next_character_boss_ticket=0;
bool character_boss_lease(const action_session::Binding& binding,const action_session::Request& request,
                        host::opened::CurrentWorld& opened,uint64_t& revision,std::string& error){
    action_session::Binding actual;
    if(request.action!=action_session::Action::EditCharacterMembership||
       request.character_membership.command!=action_session::CharacterMembershipCommand::Destroy){
        error="Character Boss retirement requires original Character membership Destroy";return false;}
    if(!current(opened,error))return false;
    const prop_lease::World world{opened.world_epoch,opened.generation,opened.world,opened.map};
    if(unknown_locked||admitted_world!=world||
       request.snapshot_uuid!=net::current_snapshot_id()||
       binding.native_epoch!=opened.world_epoch||binding.native_generation!=opened.generation){
        error="Character Boss retirement current NativeOpen/snapshot binding differs";return false;}
    if(!typed_action_runtime::read_binding(request.snapshot_uuid,actual,error)||actual!=binding){
        if(error.empty())error="Character Boss retirement current Controller binding differs";return false;}
    return ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,error);
}
bool character_boss_heap(native_free_runtime::Receipt& heap,host::ProcessMemory& memory,host::LookupApi& api,std::string& error){
    if(!native_free_runtime::Runtime::instance().read_actual_receipt(heap,error))return false;
    if(heap.main_thread!=GetCurrentThreadId()){
        error="Character Boss retirement heap receipt belongs to another thread";return false;}
    return host::resolve_lookup(memory,heap.image,heap.executable.sha256.data(),api,error);
}
bool same_character_boss_alias(const Bound& old,const Bound& now){
    return old.snapshot==now.snapshot&&old.key==now.key&&old.sha==now.sha&&old.uid==now.uid&&
        old.entry==now.entry&&old.generation==now.generation&&old.sequence==now.sequence&&old.world==now.world&&
        old.revoked==now.revoked&&old.unknown==now.unknown&&old.metadata==now.metadata&&
        old.object==now.object&&old.class_addresspoint==now.class_addresspoint&&old.revocation_key==now.revocation_key;
}
}
bool prepare_character_boss_retirement(const action_session::Binding& binding,const action_session::Request& request,
    const std::vector<std::pair<uintptr_t,uint64_t>>& users,CharacterBossRetirement& out,std::string& error){
    out={};CharacterBossPrepared prepared;host::opened::CurrentWorld opened;
    json baseline;if(!collect_bindings(baseline,error,BindingDetails::Identity))return false;
    if(!character_boss_lease(binding,request,opened,prepared.revision,error))return false;
    if(users.size()>100000||character_boss_prepared.contains(request.action_uuid)||next_character_boss_ticket==UINT64_MAX){
        error="Character Boss retirement user budget, previous preparation or ticket exhaustion";return false;}
    native_free_runtime::Receipt heap;host::ProcessMemory memory;host::LookupApi api;
    if(!character_boss_heap(heap,memory,api,error))return false;
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){
        if(!object||object>UINTPTR_MAX-offset||object+offset>UINTPTR_MAX-sizeof(value)){
            cause="Character Boss retirement address null/overflow";return false;}
        return memory.copy(object+offset,&value,sizeof(value),cause);};
    auto find=[&](uint64_t uid,uintptr_t& found,std::string& cause){void* value=nullptr;uint32_t seh=0;
        if(!uid||!lookup(api,uid,value,seh)){cause="Character Boss full UID lookup failed SEH="+std::to_string(seh);return false;}
        found=uintptr_t(value);return true;};
    const auto root=uintptr_t(opened.map)+0x1c0;
    dungeon_scene_identity_runtime::Census scenes;bool scenes_read=false;
    std::set<uint64_t> ids;std::set<uintptr_t> pointers;
    for(const auto& [object,uid]:users){uintptr_t ap=0,shard=0,region=0,found=0,notify=0;uint64_t actual_uid=0;
        if(!object||!uid||!pointers.insert(object).second||!ids.insert(uid).second){
            error="Character Boss retirement selected identity is absent or duplicated";return false;}
        if(!read(object,0,ap,error)||!read(object,0x18,actual_uid,error)||!read(object,0x170,shard,error)||
           !read(object,0x178,region,error)||!read(ap,0x438,notify,error)||!find(uid,found,error))return false;
        if(ap!=heap.image+0x1743040||notify!=heap.image+0x3976f0||actual_uid!=uid||found!=object){
            error="Character Boss retirement selected native class/UID/Notify differs";return false;}
        if(!prepared.pools.contains(region)){
            if(shard==root){int32_t saved=-1;uintptr_t selected=0;
                if(!read(region,0x834,saved,error)||!region_saved_identity::resolve(heap.image,uintptr_t(opened.map),root,saved,region,selected,read,error))return false;
            }else{
                if(!scenes_read){if(!dungeon_scene_identity_runtime::read_current(scenes,error))return false;scenes_read=true;}
                size_t matches=0;for(const auto& pool:scenes.pools)if(pool.scene.owner.shard==shard&&pool.scene.owner.region==region)++matches;
                if(matches!=1){error="Character Boss retirement Region is absent or ambiguous in current saved Dungeon scenes";return false;}
                if(scenes.world.world!=opened.world||scenes.world.map!=opened.map||scenes.world.world_epoch!=opened.world_epoch||
                   scenes.world.generation!=opened.generation){error="Character Boss retirement Dungeon census NativeOpen differs";return false;}
            }
            cb::Pool pool;if(!cb::census(heap.image,shard,region,read,find,pool,error))return false;
            prepared.pools.emplace(region,std::move(pool));
        }
        const auto& pool=prepared.pools.at(region);const cb::Member member{object,uid};
        if(std::find(pool.members.begin(),pool.members.end(),member)==pool.members.end()){
            error="Character Boss retirement selected user absent from complete manager";return false;}
        prepared.removed[region].push_back(member);
        size_t matches=0;
        for(const auto& [id,alias]:registry){
            if(alias.generation!=registry_generation||alias.world!=admitted_world||alias.snapshot!=request.snapshot_uuid||alias.uid!=uid)continue;
            if(++matches>1||alias.revoked||alias.unknown||(alias.object&&alias.object!=object)||
               (alias.class_addresspoint&&alias.class_addresspoint!=ap)||!prepared.aliases.emplace(id,alias).second){
                error="Character Boss retirement existing alias is retired, unknown or binds another object";return false;}
        }
    }
    host::opened::CurrentWorld after;uint64_t revision=0;
    if(!character_boss_lease(binding,request,after,revision,error))return false;
    if(!host::same_world(opened,after)||revision!=prepared.revision){error="Character Boss retirement lease changed during preparation";return false;}
    prepared.ticket=++next_character_boss_ticket;prepared.binding=binding;prepared.request=request;prepared.root=root;
    prepared.world={opened.world_epoch,opened.generation,opened.world,opened.map};out.ticket=prepared.ticket;
    character_boss_prepared.emplace(request.action_uuid,std::move(prepared));return true;
}
bool confirm_character_boss_retirement(const action_session::Binding& binding,const action_session::Request& request,
    CharacterBossRetirement& out,std::string& error){
    const auto it=character_boss_prepared.find(request.action_uuid);
    if(it==character_boss_prepared.end()||!out.ticket||out.ticket!=it->second.ticket||!out.retired_keys.empty()||
       it->second.confirmed||it->second.binding!=binding||it->second.request!=request){
        error="Character Boss retirement exact prepared receipt is missing, changed or already confirmed";return false;}
    auto& saved=it->second;host::opened::CurrentWorld opened;uint64_t revision=0;
    if(!character_boss_lease(binding,request,opened,revision,error))return false;
    if(revision!=saved.revision||saved.world!=prop_lease::World{opened.world_epoch,opened.generation,opened.world,opened.map}){
        error="Character Boss retirement actual World/revision changed after original destruction";return false;}
    native_free_runtime::Receipt heap;host::ProcessMemory memory;host::LookupApi api;
    if(!character_boss_heap(heap,memory,api,error))return false;
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){
        if(!object||object>UINTPTR_MAX-offset||object+offset>UINTPTR_MAX-sizeof(value)){
            cause="Character Boss retirement post-read address null/overflow";return false;}
        return memory.copy(object+offset,&value,sizeof(value),cause);};
    auto find=[&](uint64_t uid,uintptr_t& found,std::string& cause){void* value=nullptr;uint32_t seh=0;
        if(!uid||!lookup(api,uid,value,seh)){cause="Character Boss post-destruction UID lookup failed SEH="+std::to_string(seh);return false;}
        found=uintptr_t(value);return true;};
    for(const auto& [region,before]:saved.pools){cb::Pool after;
        if(!cb::census(heap.image,before.root,region,read,find,after,error)||
           !cb::removed(before,after,saved.removed.at(region),error))return false;
        for(const auto& member:saved.removed.at(region)){uintptr_t found=0;
            if(!find(member.uid,found,error))return false;
            if(found){error="Character Boss original destruction full UID still registered";return false;}}
    }
    for(const auto& [id,before]:saved.aliases){const auto found=registry.find(id);
        if(found==registry.end()||!same_character_boss_alias(before,found->second)){
            error="Character Boss prepared alias changed before atomic retirement";return false;}}
    host::opened::CurrentWorld after;uint64_t after_revision=0;
    if(!character_boss_lease(binding,request,after,after_revision,error))return false;
    if(!host::same_world(opened,after)||after_revision!=revision){error="Character Boss retirement lease changed during confirmation";return false;}
    for(const auto& [id,unused]:saved.aliases){auto& alias=registry.at(id);(void)unused;
        alias.revoked=true;alias.revocation_key="native-character:"+request.action_uuid;out.retired_keys.push_back(alias.key);}
    std::sort(out.retired_keys.begin(),out.retired_keys.end());saved.confirmed=true;error.clear();return true;
}
