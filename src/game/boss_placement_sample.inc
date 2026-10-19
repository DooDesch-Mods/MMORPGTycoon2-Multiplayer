namespace boss_placement {
namespace {
namespace q=npc_quest_targets;
namespace actors=interventions::actors;
namespace definitions=character_definition_identity;
using State=action::BossPlacementState;
struct Sample {
    host::WorldState world;boss_current::State bosses;actors::State actors;actors::Locals locals;
    State value;action::Ledger money;witness::UndoWitness undo;
    uintptr_t random=0,release=0,business=0;
};
bool code(Runtime&,bool,std::string&);
bool owner_add(std::map<uintptr_t,std::string>& owners,uintptr_t object,const std::string& key,std::string& error){
    if(!object||key.empty())return q::fail(error,"Boss complete original Record owner is empty");
    const auto [found,inserted]=owners.emplace(object,key);
    return inserted||found->second==key||q::fail(error,"Boss original Record pointer aliases two canonical owners");
}
bool character_owners(Runtime& r,const definitions::Census& census,std::map<uintptr_t,std::string>& owners,std::string& error){
    for(const auto& definition:census.definitions){
        if(!owner_add(owners,definition.object,definition.key,error))return false;
        if(definition.variants&&!owner_add(owners,definition.variants,"character-variants:"+definition.key,error))return false;
    }
    for(const auto& definition:census.definitions){if(definition.deleted)continue;
        if(definition.owning_collection&&!owners.contains(definition.owning_collection))return q::fail(error,"Boss selected Character private Collection has no current canonical owner");
        uintptr_t data=0;int32_t count=0,capacity=0;
        if(!q::at(r.memory,definition.object,0x130,data,error)||!q::at(r.memory,definition.object,0x138,count,error)||
           !q::at(r.memory,definition.object,0x13c,capacity,error))return false;
        if(count<0||count>10000||capacity<count||(count&&!data))return q::fail(error,"Boss Character Ability prototype owner-array bounds differ");
        for(int32_t slot=0;slot<count;++slot){uintptr_t prototype=0,found=0;uint64_t uid=0;
            if(!q::at(r.memory,data,size_t(slot)*8,prototype,error)||!q::typed(r.memory,r.image,prototype,0x171c930,error)||
               !q::at(r.memory,prototype,0x18,uid,error)||!uid||!group_edit::find(r,uid,found,error)||found!=prototype||
               !owner_add(owners,prototype,definition.key+"/ability/"+std::to_string(slot),error))
                return q::fail(error,error.empty()?"Boss Character Ability actual original full UID differs":error);
        }
        uintptr_t after=0;int32_t after_count=0,after_capacity=0;
        if(!q::at(r.memory,definition.object,0x130,after,error)||!q::at(r.memory,definition.object,0x138,after_count,error)||
           !q::at(r.memory,definition.object,0x13c,after_capacity,error)||data!=after||count!=after_count||capacity!=after_capacity)
            return q::fail(error,error.empty()?"Boss Character Ability owner-array changed during capture":error);
    }return true;
}
bool definition(Runtime& r,const action::Context& context,const action::Binding& binding,int32_t type,std::string& sha,std::string& error){
    (void)r;
    return npc_probe::character_actor_records::read_definition(context,binding,3,type,sha,error);
}
bool region_pool(Runtime& r,const boss_current::State& bosses,int32_t saved,const boss_current::Pool*& selected,std::string& error){
    selected=nullptr;
    for(const auto& pool:bosses.pools){if(pool.scope.scene!="world"||pool.scope.shard!=pool.scope.root)continue;int32_t id=-1;
        if(!q::at(r.memory,pool.scope.region,0x834,id,error))return false;
        if(id==saved){if(selected)return q::fail(error,"Boss release Region ID repeats in complete current Root census");selected=&pool;}}
    return selected||q::fail(error,"Boss release Region has no current original Root BossManager");
}
bool source_intent(Runtime& r,const action::Context& context,const action::Binding& binding,action::BossPlacementIntent& intent,std::string& error){
    auto semantic=intent;semantic.region=0;semantic.character_sha=std::string(64,'0');
    if(!boss_placement_reader::valid(semantic))return q::fail(error,"Boss source type/pose/release point violates the native finite domain");
    boss_current::State bosses;if(!code(r,true,error)||!boss_current::read(context,binding,bosses,error))return false;
    uintptr_t region=0;const auto code=boss_placement_calls::region(r.image,uintptr_t(bosses.world.map),intent.point,region);
    if(code||!region)return q::fail(error,"Boss source original RegionAt failed SEH="+std::to_string(code));
    int32_t id=-1;if(!q::at(r.memory,region,0x834,id,error))return false;
    const boss_current::Pool* pool=nullptr;
    if(!region_pool(r,bosses,id,pool,error)||pool->scope.region!=region)return q::fail(error,error.empty()?"Boss source original RegionAt differs from complete Root census":error);
    intent.region=id;return definition(r,context,binding,intent.type,intent.character_sha,error);
}
bool actor_record(Runtime& r,const action::Context& context,const action::Binding& binding,const Sample& sample,
                  const std::string& key,std::string& sha,std::string& error){
    const auto local=sample.locals.find(key);
    if(local==sample.locals.end()||local->second.target.family!=interventions::Family::Boss)return q::fail(error,"Boss new complete original Record lacks its confirmed native Birth alias");
    definitions::Census census;character_membership_reader::Users users;
    if(!npc_probe::character_actor_records::read_current(context,binding,census,users,error))return false;
    std::map<uintptr_t,std::string> owners;
    if(!character_owners(r,census,owners,error)||!owner_add(owners,sample.world.root,"root_shard",error)||
       !owner_add(owners,uintptr_t(sample.world.origin.map),"world-map",error)||!owner_add(owners,uintptr_t(sample.world.origin.state),"native-state",error))return false;
    for(const auto& pool:sample.bosses.pools){int32_t id=-1;
        if(!q::at(r.memory,pool.scope.region,0x834,id,error))return false;
        const auto name=pool.scope.scene=="world"?"region:"+std::to_string(id):pool.scope.scene+"/region";
        if(!owner_add(owners,pool.scope.region,name,error))return false;
    }
    for(const auto& [name,value]:sample.locals)if(!owner_add(owners,value.object,name,error))return false;
    auto external=[&](uintptr_t pointer,std::string& name,std::string& cause){const auto found=owners.find(pointer);
        if(found==owners.end())return q::fail(cause,"Boss complete original new Record has an unresolved current external owner");name=found->second;return true;};
    npc_probe::character_actor_records::Result record;
    if(!npc_probe::character_actor_records::read(binding,context.snapshot_uuid,character_membership_reader::Family::Boss,local->second.object,external,record,error))return false;
    sha=record.portable.sha;return true;
}
bool sample(Runtime& r,const action::Context& context,const action::Binding& binding,const std::string& born,Sample& out,std::string& error,
            bool capture_record=true,const std::string& retained_record={}){
    out={};
    if(!world(r,out.world,error)||context.snapshot_uuid!=net::current_snapshot_id()||out.world.origin.world_epoch!=binding.native_epoch||out.world.origin.generation!=binding.native_generation||
       !boss_current::read(context,binding,out.bosses,error)||out.bosses.world.map!=out.world.origin.map||out.bosses.world.state!=out.world.origin.state||
       !actors::read_current(context,binding,out.actors,out.locals,error)||!boss_placement_state::split(out.actors,born,out.value,error))
        return q::fail(error,error.empty()?"Boss current complete Actor/manager census or World binding differs":error);
    if(out.value.created&&capture_record){
        if(!actor_record(r,context,binding,out,born,out.value.record_sha,error)||!actors::read_current(context,binding,out.actors,out.locals,error)||
           !boss_placement_state::split(out.actors,born,out.value,error))return false;
    }else if(out.value.created)out.value.record_sha=retained_record;
    uintptr_t release_manager=0;
    if(!q::at(r.memory,r.image,0x164b250,out.random,error)||!q::at(r.memory,out.random,0,out.value.random,error)||
       !q::at(r.memory,r.image,0x13df4f0,release_manager,error)||!q::typed(r.memory,r.image,release_manager,0x1714ed0,error)||
       !q::at(r.memory,release_manager,0x38,out.release,error)||!q::typed(r.memory,r.image,out.release,0x16c12b0,error)||
       !q::at(r.memory,out.release,0x120,out.value.release,error)||!q::at(r.memory,r.image,0x13df220,out.business,error)||
       !ledger(out.money,error)||!witness::read_undo(r.memory.process,r.image,out.undo,error)||out.undo.active_graph)return q::fail(error,error.empty()?"Boss original Ledger/RNG/release or idle Undo owner differs":error);
    host::WorldState after;
    return (world(r,after,error)&&host::same_world(out.world.origin,after.origin)&&boss_placement_state::valid(out.value))||
        q::fail(error,error.empty()?"Boss complete sample final World or portable state differs":error);
}
bool same_binding(const Sample& before,const Sample& after){return host::same_world(before.world.origin,after.world.origin)&&before.world.root==after.world.root&&before.world.selection==after.world.selection&&
    before.random==after.random&&before.release==after.release&&before.business==after.business;}
void trim(Sample& value){value.actors.clear();value.locals.clear();value.bosses={};}
}
}
