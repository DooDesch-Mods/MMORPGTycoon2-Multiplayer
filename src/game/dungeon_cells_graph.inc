namespace dungeon_cells {
namespace {
namespace cg=dungeon_cell_graph;
struct Actor {
    cg::Kind kind=cg::Kind::Scenery;uintptr_t object=0,manager=0,shard=0,region=0;uint64_t uid=0;
    std::string scene,key;identity::Entry monster;scenery_make_history::Saved scenery;
    dungeon_gizmo_identity::Member gizmo()const{return {scene,object,manager,shard,region,uid};}
};
struct BoundGraph {
    cg::Graph value;cg::Policy policy;std::map<uint64_t,Actor> actors;qr::Witness quests;
    uintptr_t dungeon=0;uint64_t dungeon_uid=0;
};
bool actor_models(const Sample& sample,std::map<uint64_t,Actor>& out,std::string& error){
    const auto add=[&](Actor actor){const auto found=out.find(actor.uid);
        if(!actor.uid||!actor.object||!actor.manager||!actor.shard||!actor.region)return fail(error,"Dungeon original graph actor witness lacks its full native owner");
        if(found!=out.end()&&(found->second.kind!=actor.kind||found->second.scene!=actor.scene||found->second.key!=actor.key))return fail(error,"Dungeon original graph full UID aliases another actor/scene");
        out[actor.uid]=std::move(actor);return true;};
    size_t gizmo=0;
    for(const auto& pool:sample.scenes.pools){const auto& scene=pool.scene.owner;
        for(const auto& row:pool.scenery.members){Actor actor;actor.kind=cg::Kind::Scenery;actor.object=row.object;actor.uid=row.uid;actor.manager=pool.managers[0];actor.shard=scene.shard;actor.region=scene.region;actor.scene=scene.key;
            const auto saved=sample.scenery.find(row.uid);if(saved==sample.scenery.end()||saved->second.second.object!=row.object)return fail(error,"Dungeon graph Scenery lacks its complete original inputs/alias");
            actor.key=saved->second.first;actor.scenery=saved->second.second;
            if(!add(std::move(actor)))return false;}
        for(const auto& row:pool.zones.members){Actor actor;actor.kind=cg::Kind::Zone;actor.object=row.object;actor.uid=row.uid;actor.manager=pool.managers[1];actor.shard=scene.shard;actor.region=scene.region;actor.scene=scene.key;
            if(!add(std::move(actor)))return false;}
        for(const auto& row:pool.local.members){if(gizmo>=sample.scenes.keys.size())return fail(error,"Dungeon original graph complete Gizmo canonical roster is truncated");
            Actor actor;actor.kind=cg::Kind::Gizmo;actor.object=row.object;actor.uid=row.uid;actor.manager=pool.managers[3];actor.shard=scene.shard;actor.region=scene.region;actor.scene=scene.key;actor.key=sample.scenes.keys[gizmo++];
            if(!add(std::move(actor)))return false;}
    }
    if(gizmo!=sample.scenes.keys.size())return fail(error,"Dungeon original graph Gizmo canonical roster has extra members");
    for(const auto& [key,entry]:sample.monsters){const auto pool=std::find_if(sample.scenes.pools.begin(),sample.scenes.pools.end(),[&](const auto& row){return row.managers[2]==entry.scope.manager;});
        if(pool==sample.scenes.pools.end())return fail(error,"Dungeon original graph Monster alias has no complete scene owner");
        Actor actor;actor.kind=cg::Kind::Monster;actor.object=entry.pointer;actor.uid=entry.uid;actor.manager=entry.scope.manager;actor.shard=pool->scene.owner.shard;actor.region=entry.scope.region;
        actor.scene=pool->scene.owner.key;actor.key=key;actor.monster=entry;if(!add(std::move(actor)))return false;
    }return true;
}
using ValueSource=Memory;
struct ValueMemory final:subscriber_lifecycle::Memory {ValueSource& source;explicit ValueMemory(ValueSource& value):source(value){}
    bool read(uintptr_t p,void* out,size_t bytes,std::string& error)override{return source.copy(p,out,bytes,error);}};
struct ValueCapture {ValueMemory& memory;subscriber_lifecycle::Graph graph;};
bool inspect_value(void* user,const void* record,const void* context,std::string& error){auto& capture=*static_cast<ValueCapture*>(user);
    return subscriber_lifecycle::read_graph(capture.memory,uintptr_t(record),uintptr_t(context),{100000,1000000,32*1024*1024,100000},capture.graph,error);}
bool value_record(Memory& memory,uintptr_t object,uintptr_t prepare,uintptr_t save,std::string& hash,std::string& error){
    ValueMemory view(memory);ValueCapture captured{view};subscriber_lifecycle::CaptureReceipt receipt;
    const subscriber_lifecycle::CaptureApi api{reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x9667c0),reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x9676a0),
        reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x89e580),reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x89e5f0),
        reinterpret_cast<subscriber_lifecycle::PrepareSave>(state.image+prepare),reinterpret_cast<subscriber_lifecycle::SaveRecord>(state.image+save)};
    {live_host_state::apply_timing::PageReadScope original(live_host_state::apply_timing::PageBoundary::OriginalCall);
        if(!subscriber_lifecycle::capture_original(api,reinterpret_cast<const void*>(object),inspect_value,&captured,receipt,error))return false;}
    return duplicate_owned_history::captured(captured.graph,hash,error);
}
bool graph_values(Memory& memory,const BoundGraph& bound,cg::Node& node,std::string& error){
    const auto record=[&](uintptr_t object,uintptr_t prepare,uintptr_t save){std::string hash;if(!value_record(memory,object,prepare,save,hash,error))return false;node.values.push_back(std::move(hash));return true;};
    if(node.kind==cg::Kind::Paint||node.kind==cg::Kind::Layout){if(node.uid!=bound.dungeon_uid)return fail(error,"Dungeon original Paint/Layout graph targets another full Dungeon UID");}
    else if(node.kind==cg::Kind::Quests){const auto owner=std::find_if(bound.quests.owners.begin(),bound.quests.owners.end(),[&](const auto& row){return row.second.uid==node.uid;});
        if(owner==bound.quests.owners.end()||!qr::inverse(memory,state.image,node.pointer,owner->second,error))return fail(error,error.empty()?"Dungeon original Quest inverse lacks its complete before owner":error);
        node.values.push_back(quest_bridge::records_json(owner->second.state.records).dump());
        node.values.push_back(json(owner->second.destination_uids).dump());return true;
    }else {const auto found=bound.actors.find(node.uid);if(found==bound.actors.end())return fail(error,"Dungeon original graph target lacks its complete before/after scene census UID");
        const auto family=node.kind==cg::Kind::Move?found->second.kind:node.kind==cg::Kind::Door?cg::Kind::Gizmo:node.kind==cg::Kind::EditZone?cg::Kind::Zone:node.kind;
        if(node.kind!=cg::Kind::Destroy&&family!=found->second.kind)return fail(error,"Dungeon original graph target class differs from complete scene owner");
        if(node.kind==cg::Kind::Move&&found->second.kind!=cg::Kind::Gizmo&&found->second.kind!=cg::Kind::Scenery)return fail(error,"Scene Move graph target has an unsupported concrete family");
        if(node.kind==cg::Kind::Scenery){MonsterMemory adapter{memory};if(!scenery_make_history::make(adapter,state.image,node.pointer,found->second.scenery,error))return false;}
        node.values.push_back(found->second.scene);node.values.push_back(found->second.key);
    }
    if(node.kind==cg::Kind::Layout)return record(node.pointer+0xc0,0xf67ba0,0xf67c00)&&record(node.pointer+0x108,0xf0c9b0,0xf0ca10);
    if(node.kind==cg::Kind::Paint){uintptr_t data=0;int32_t count=0,capacity=0;
        if(!field(memory,node.pointer,0xc8,data,error)||!field(memory,node.pointer,0xd0,count,error)||!field(memory,node.pointer,0xd4,capacity,error)||
           count<0||count>100000||capacity<count||capacity>1000000||(count&&(!data||data>UINTPTR_MAX-size_t(count)*0x120)))return fail(error,error.empty()?"Dungeon original Paint complete stroke array differs":error);
        for(int32_t i=0;i<count;++i){const auto stroke=data+size_t(i)*0x120;if(!record(stroke,0xe95580,0xe955e0)||!record(stroke+0x70,0x10ee6f0,0x10ee750))return false;}
    }else if(node.kind==cg::Kind::Zone||node.kind==cg::Kind::EditZone){if(!record(node.pointer+0xc8,0xe95580,0xe955e0))return false;}
    else if(node.kind==cg::Kind::Monster||node.kind==cg::Kind::Gizmo||node.kind==cg::Kind::Door){std::string text;
        if(!npc_quest_records::read_string(memory,node.pointer,0xc0,"Dungeon original Make stored name/type",text,error))return false;node.values.push_back(text);
        if(node.kind!=cg::Kind::Monster){if(!npc_quest_records::read_string(memory,node.pointer,0xe0,"Dungeon original Make stored variant",text,error))return false;node.values.push_back(text);}
    }return true;
}
bool graph_capture(Memory& memory,const Sample& before,const Sample& after,uintptr_t pointer,BoundGraph& out,std::string& error,cg::Policy policy={}){
    out={};out.policy=policy;out.quests=before.quests;out.dungeon=before.palette.dungeon.dungeon.dungeon.local.dungeon;out.dungeon_uid=before.palette.dungeon_uid;
    if(!actor_models(before,out.actors,error)||!actor_models(after,out.actors,error))return false;
    std::map<uintptr_t,std::string> owners;if(!external_owners(memory,{before.scenes.world.world_epoch,before.scenes.world.generation},net::current_snapshot_id(),before.palette,owners,error))return false;
    for(const auto& pool:before.scenes.pools)owners.emplace(pool.scene.owner.region,pool.scene.owner.key+"/region");
    MonsterMemory adapter{memory};auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& e){return field(memory,p,offset,value,e);};
    auto copy=[&](uintptr_t p,void* bytes,size_t count,std::string& e){return memory.copy(p,bytes,count,e);};
    auto values=[&](cg::Node& node,std::string& e){return graph_values(memory,out,node,e);};
    auto region=[&](const cg::Node&,uintptr_t p,std::string& e){const auto found=owners.find(p);
        return (found!=owners.end()&&(found->second.starts_with("region:")||found->second.ends_with("/region")))||fail(e,"Dungeon original graph cached Region has no current canonical owner");};
    undo_probe::GraphWitness graph;if(!undo_probe::read_graph(memory.process,state.image,pointer,graph,error)||!graph.locked||graph.nodes||graph.pending_delete)
        return fail(error,error.empty()?"Dungeon original history graph is not locked, complete and drained":error);
    return cg::read(adapter,at,copy,values,region,state.image,pointer,out.value,error,out.policy);
}
bool graph_unchanged(Memory& memory,const BoundGraph& bound,std::string& error){
    MonsterMemory adapter{memory};auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& e){return field(memory,p,offset,value,e);};
    auto copy=[&](uintptr_t p,void* bytes,size_t count,std::string& e){return memory.copy(p,bytes,count,e);};
    auto values=[&](cg::Node& node,std::string& e){return graph_values(memory,bound,node,e);};
    auto region=[&](const cg::Node& node,uintptr_t p,std::string& e){const auto known=std::find_if(bound.value.nodes.begin(),bound.value.nodes.end(),[&](const auto& row){return row.pointer==node.pointer;});
        return (known!=bound.value.nodes.end()&&std::find(known->regions.begin(),known->regions.end(),p)!=known->regions.end())||fail(e,"Dungeon original graph cached Region changed");};
    undo_probe::GraphWitness locked;if(!undo_probe::read_graph(memory.process,state.image,bound.value.pointer,locked,error)||!locked.locked||locked.nodes||locked.pending_delete)
        return fail(error,error.empty()?"Dungeon retained original graph is no longer locked and drained":error);
    cg::Graph actual;if(!cg::read(adapter,at,copy,values,region,state.image,bound.value.pointer,actual,error,bound.policy))return false;
    return actual==bound.value||fail(error,"Dungeon original complete stored graph payload or sorted dependencies changed before execution");
}
}
}
