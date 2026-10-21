namespace zone_edit_runtime {
namespace {
namespace cg=dungeon_cell_graph;
struct Graph {
    cg::Graph native;
    cg::Policy policy;
    std::map<uint64_t,std::string> keys;
    std::map<uint64_t,identity::Current> zones;
    std::map<uint64_t,zone_edit_read::Row> zone_values;
    std::map<uint64_t,monster_zone_read::Monster> monsters;
    qr::Witness quests;
    uintptr_t root=0,region=0;
    int32_t region_id=-1;
    json proof;
    std::string sha;
};
bool key_merge(std::map<uint64_t,std::string>& keys,uint64_t uid,const std::string& key,std::string& error){
    const auto [found,inserted]=keys.emplace(uid,key);
    return (uid&&!key.empty()&&(inserted||found->second==key))||fail(error,"Zone graph full UID has conflicting canonical ownership");
}
bool graph_models(const Sample& sample,Graph& graph,std::string& error){
    for(const auto& row:sample.rows){
        if(!key_merge(graph.keys,row.owner.uid,row.owner.key,error))return false;
        const auto found=graph.zones.find(row.owner.uid);
        if(found!=graph.zones.end()&&(found->second.key!=row.owner.key||found->second.type!=row.owner.type))
            return fail(error,"Zone graph same full UID changed class or canonical key");
        graph.zones[row.owner.uid]=row.owner;
        graph.zone_values.try_emplace(row.owner.uid,row);
    }
    for(const auto& row:sample.native.state.monsters){
        if(!key_merge(graph.keys,row.uid,sample.monster_keys.at(row.uid),error))return false;
        graph.monsters.try_emplace(row.uid,row);
    }
    for(const auto& [pointer,owner]:sample.quests.owners){
        (void)pointer;if(!key_merge(graph.keys,owner.uid,owner.key,error))return false;
        for(size_t index=0;index<owner.destination_uids.size();++index)
            if(!key_merge(graph.keys,owner.destination_uids[index],owner.state.records[index].destination_key,error))return false;
    }
    return true;
}
bool graph_values(Memory& memory,const Graph& graph,cg::Node& node,std::string& error){
    if(!graph.keys.contains(node.uid))return fail(error,"Zone graph original target full UID is outside captured ownership");
    if(node.kind==cg::Kind::Quests){
        const auto owner=std::find_if(graph.quests.owners.begin(),graph.quests.owners.end(),[&](const auto& row){return row.second.uid==node.uid;});
        if(owner==graph.quests.owners.end()||!qr::inverse(memory,za::state.image,node.pointer,owner->second,error))
            return fail(error,error.empty()?"Zone graph Quest inverse lacks its complete original owner":error);
        node.values.push_back(quest_bridge::records_json(owner->second.state.records).dump());return true;
    }
    if(node.kind==cg::Kind::Zone||node.kind==cg::Kind::EditZone){
        zone_edit_stored::Stored stored;
        if(!zone_edit_stored::read(memory,za::state.image,node.pointer,stored,error))return false;
        const auto owner=graph.zones.find(stored.uid);
        if(owner==graph.zones.end()||stored.root!=graph.root||stored.region!=graph.region||stored.type!=owner->second.type)
            return fail(error,"Zone graph stored full UID/class/root/Region differs");
        const auto& prior=graph.zone_values.at(stored.uid);
        if(stored.type!=prior.content.type||stored.density!=prior.content.density||stored.monster_type!=prior.content.monster_type||
           stored.level!=prior.content.level||stored.shape!=prior.shape)
            return fail(error,"Zone graph inverse does not retain the complete prior authored values");
        node.values.push_back(json{{"type",stored.type},{"density",stored.density},{"monster_type",stored.monster_type},
            {"level",stored.level},{"shape",shape_json(stored.shape)}}.dump());return true;
    }
    if(node.kind==cg::Kind::Monster){
        const auto found=graph.monsters.find(node.uid);
        if(found==graph.monsters.end())return fail(error,"Zone graph MakeMonster is outside complete current Monster ownership");
        uintptr_t root=0,region=0;uint64_t zone=0,id=0;int32_t rank=-1,level=0;
        std::array<float,4> rotation{};std::array<float,3> position{},scale{};std::string name;
        if(!field(memory,node.pointer,0xe0,root,error)||!field(memory,node.pointer,0xe8,region,error)||
           !field(memory,node.pointer,0xf0,rank,error)||!field(memory,node.pointer,0xf8,zone,error)||
           !field(memory,node.pointer,0x100,rotation,error)||!field(memory,node.pointer,0x110,position,error)||
           !field(memory,node.pointer,0x11c,scale,error)||!field(memory,node.pointer,0x16c,level,error)||
           !field(memory,node.pointer,0x170,id,error)||
           !npc_quest_records::read_string(memory,node.pointer,0xc0,"Zone graph MakeMonster name",name,error))return false;
        const auto& actor=found->second;uint64_t expected_zone=0;
        for(const auto& [uid,owner]:graph.zones)if(owner.object==actor.zone)expected_zone=uid;
        const uint64_t expected_id=uint64_t(uint32_t(actor.slot))|(uint64_t(uint32_t(actor.type))<<32);
        if(root!=graph.root||region!=graph.region||rank!=actor.rank||level!=actor.level||id!=expected_id||zone!=expected_zone||
           position!=actor.position||rotation!=actor.rotation||scale!=actor.scale)
            return fail(error,"Zone graph MakeMonster full owner/template/local captured pose differs");
        const auto canonical_zone=zone?graph.keys.find(zone):graph.keys.end();
        if(zone&&canonical_zone==graph.keys.end())return fail(error,"Zone graph stored Monster Zone UID has no canonical owner");
        node.values.push_back(json{{"rank",rank},{"level",level},{"character",id},{"name",name},
            {"zone",zone?canonical_zone->second:std::string{}}}.dump());return true;
    }
    return node.kind==cg::Kind::Destroy||fail(error,"Zone graph node is outside original Zone history");
}
bool read_graph(Memory& memory,Graph& graph,uintptr_t pointer,std::string& error){
    d::ReadMemory adapter{memory};
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return field(memory,object,offset,value,cause);};
    auto copy=[&](uintptr_t object,void* value,size_t bytes,std::string& cause){return memory.copy(object,value,bytes,cause);};
    auto values=[&](cg::Node& node,std::string& cause){return graph_values(memory,graph,node,cause);};
    auto owner=[&](const cg::Node&,uintptr_t region,std::string& cause){
        uintptr_t root=0;int32_t id=-1;
        return field(memory,region,0x70,root,cause)&&field(memory,region,0x834,id,cause)&&
            ((region==graph.region&&root==graph.root&&id==graph.region_id)||fail(cause,"Zone graph cached Region owner differs"));
    };
    undo_probe::GraphWitness witness;
    if(!undo_probe::read_graph(memory.process,za::state.image,pointer,witness,error)||!witness.locked||witness.nodes||witness.pending_delete)
        return fail(error,error.empty()?"Zone graph is not locked, complete and drained":error);
    cg::Graph actual;
    if(!cg::read(adapter,at,copy,values,owner,za::state.image,pointer,actual,error,graph.policy))return false;
    graph.native=std::move(actual);return true;
}
bool graph_capture(Memory& memory,const Sample& before,const Sample& after,uintptr_t pointer,uint32_t cash,Graph& out,std::string& error){
    Graph graph;graph.policy={cg::Shape::Zones,cash};graph.quests=before.quests;
    graph.root=before.world.root;graph.region=before.edit.world.region;graph.region_id=before.edit.world.saved_region;
    if(!graph_models(before,graph,error)||!graph_models(after,graph,error)||!read_graph(memory,graph,pointer,error))return false;
    json nodes=json::array();std::map<uintptr_t,size_t> indices;
    for(const auto& node:graph.native.nodes){
        json dependencies=json::array();
        for(const auto& set:node.dependencies){json values=json::array();for(auto uid:set){
            const auto key=graph.keys.find(uid);
            if(key==graph.keys.end())return fail(error,"Zone original dependency full UID lacks canonical ownership");
            values.push_back(key->second);
        }dependencies.push_back(std::move(values));}
        json regions=json::array();for(auto region:node.regions){(void)region;regions.push_back(graph.region_id);}
        indices.emplace(node.pointer,nodes.size());
        nodes.push_back({{"kind",uint8_t(node.kind)},{"key",node.kind==cg::Kind::Cash?std::string{}:graph.keys.at(node.uid)},
            {"dependencies",std::move(dependencies)},{"regions",std::move(regions)},{"values",node.values}});
    }
    json order=json::array();for(auto pointer_value:graph.native.execution){
        const auto index=indices.find(pointer_value);if(index==indices.end())return fail(error,"Zone original execution order has an unknown node");
        order.push_back(index->second);
    }
    graph.proof={{"nodes",std::move(nodes)},{"execution",std::move(order)}};
    if(!quest_bridge::sha256(graph.proof.dump(),graph.sha,error))return false;
    out=std::move(graph);return true;
}
bool graph_unchanged(Memory& memory,const Graph& before,std::string& error){
    auto after=before;
    return read_graph(memory,after,before.native.pointer,error)&&
        ((after.native==before.native)||fail(error,"Zone retained complete inverse payload/dependencies/order changed"));
}
}
}
