namespace dungeon_cells {
namespace {
bool scene_monsters(Memory& memory,const dungeon_scene_identity_runtime::Census& scenes,
                    std::vector<monster_zone_read::State>& out,std::string& error){
    out.clear();dungeon_cells::MonsterMemory reader{memory};
    for(const auto& pool:scenes.pools){
        npc_quest_targets::World world;world.image=state.image;world.map=uintptr_t(scenes.world.map);
        world.root=pool.scene.owner.shard;world.region=pool.scene.owner.region;
        world.origin={scenes.world.uuid,scenes.world.sha256,scenes.world.world_epoch,scenes.world.generation};
        const auto owners=[&](auto&,const auto& current,uintptr_t& buildings,uintptr_t& zones,std::string& cause){
            uintptr_t ap=0,shard=0,region=0;
            if(!field(memory,current.region,0x950,buildings,cause)||!field(memory,buildings,0,ap,cause))return false;
            if(ap!=state.image+0x17183c0)return fail(cause,"Dungeon input original BuildingManager class differs");
            if(!field(memory,buildings,0x450,shard,cause)||!field(memory,buildings,0x458,region,cause)||
               !field(memory,current.region,0x968,zones,cause)||!field(memory,zones,0,ap,cause))return false;
            if(shard!=current.root||region!=current.region||ap!=state.image+0x16fe170)
                return fail(cause,"Dungeon input current scene manager reciprocal owner differs");
            if(!field(memory,zones,0xf0,region,cause))return false;
            return region==current.region||fail(cause,"Dungeon input ZoneManager current Region differs");
        };
        monster_zone_read::State current;if(!monster_zone_read::read_owned(reader,world,owners,current,error))return false;
        if(current.monster_manager!=pool.managers[2]||current.manager!=pool.zones.manager)
            return fail(error,"Dungeon input complete original scene/Monster census manager differs");
        out.push_back(std::move(current));
    }
    return true;
}
bool authored(Memory& memory,const a::Binding& binding,const std::string& snapshot,Sample& sample,std::string& error,std::vector<dungeon_input_authored::Scene>* typed){
    constexpr dungeon_input_codes::Pin canvas_functions[]{
        {0x11094a0,96,"f95eac43c00aaaee16c2f9b4eadcc679a5a63e7ef8f84539a47fdf2985e1c04a"},
        {0x1109500,464,"bd05e6e9ef28cd38d980dc6b32045595431ca90ffaef2ca7aa4ffd161f7304a0"},
        {0xf0c180,96,"722ac015cfb299889dce092a79602c0356b25411406953b20315736b94b8b61c"},
        {0xf0c1e0,96,"ee1abd962af7eda6e44be722863582b241ef0bd4ed2369baf6bf5f40687ef7a4"},
        {0xf67ba0,96,"7d447e612a99d503f192d2f61864e6923b64f12bafdc63bb7ecae37078c59db2"},
        {0xf67c00,96,"3ad2b08d99af40f775c67aab943b4cb7f24d33da30788b82f66b4aec287af597"},
        {0xf0c9b0,96,"35867545c11252814219741620272a7d1ca80c4594743fbe629f94f8a99b8001"},
        {0xf0ca10,416,"34c4b60079f1555d6c2482462a93509587b797776e88c27ba7b9d3961c9f4aaf"}};
    for(const auto& pin:canvas_functions){std::vector<unsigned char> bytes(pin.bytes);
        if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;
        if(!hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))
            return fail(error,"Dungeon authored original Canvas body differs RVA="+std::to_string(pin.rva)+" original="+error);}
    std::vector<monster_zone_read::State> monster_pools;
    if(!scene_monsters(memory,sample.scenes,monster_pools,error))return false;
    size_t gizmo_index=0;
    std::vector<world_zone_runtime::Current> zones;
    if(!world_zone_runtime::read_current(binding,snapshot,zones,error))return false;
    std::map<uintptr_t,std::pair<std::string,std::string>> canvases;
    std::map<uintptr_t,std::string> designs,scenery_owners;
    for(const auto& pool:sample.scenes.pools)for(size_t i=0;i<pool.scenery.members.size();++i){
        const auto& member=pool.scenery.members[i];
        if(!member.object||!member.uid||!scenery_owners.emplace(member.object,pool.scene.owner.key+"/scenery/"+std::to_string(i)).second)
            return fail(error,"Dungeon authored Plan has an absent or repeated current Scenery owner");
    }
    std::vector<dungeon_input_authored::Scene> scenes;
    for(size_t index=0;index<sample.scenes.pools.size();++index){
        const auto& pool=sample.scenes.pools[index];const auto& owner=pool.scene.owner;
        if(!owner.dungeon||owner.dungeon>UINTPTR_MAX-0x500)return fail(error,"Dungeon input original Canvas owner address overflows");
        const auto canvas=owner.dungeon+0x500;uintptr_t ap=0;
        if(!field(memory,canvas,0,ap,error))return false;
        if(ap!=state.image+0x1786c50)return fail(error,"Dungeon input original Canvas class/current Dungeon owner differs");
        auto saved=canvases.find(canvas);
        if(saved==canvases.end()){
            dungeon_cells::ValueMemory view(memory);dungeon_cells::ValueCapture captured{view};subscriber_lifecycle::CaptureReceipt receipt;
            const subscriber_lifecycle::CaptureApi api{
                reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x9667c0),reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x9676a0),
                reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x89e580),reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x89e5f0),
                reinterpret_cast<subscriber_lifecycle::PrepareSave>(state.image+0x11094a0),reinterpret_cast<subscriber_lifecycle::SaveRecord>(state.image+0x1109500)};
            {live_host_state::apply_timing::PageReadScope original(live_host_state::apply_timing::PageBoundary::OriginalCall);
                if(!subscriber_lifecycle::capture_original(api,reinterpret_cast<const void*>(canvas),dungeon_cells::inspect_value,&captured,receipt,error))return false;}
            std::string sha;if(!dungeon_input_authored::canvas(captured.graph,sha,error))return false;
            saved=canvases.emplace(canvas,std::pair{owner.exterior_key,std::move(sha)}).first;
        }
        if(saved->second.first!=owner.exterior_key)return fail(error,"Dungeon input original Canvas aliases two current Dungeon owners");
        auto design=designs.find(owner.dungeon);
        if(design==designs.end()){
            json parts=json::array();
            for(const auto& part:std::array<std::array<uintptr_t,4>,3>{{
                {0x58,0x16fba10,0xf0c180,0xf0c1e0},{0x108,0x1706690,0xf67ba0,0xf67c00},{0x150,0x16fbad0,0xf0c9b0,0xf0ca10}}}){
                const auto object=owner.dungeon+part[0];uintptr_t addresspoint=0;
                if(!field(memory,object,0,addresspoint,error))return false;
                if(addresspoint!=state.image+part[1])return fail(error,"Dungeon authored Grid/Layout/Plan current class differs");
                ValueMemory view(memory);ValueCapture captured{view};subscriber_lifecycle::CaptureReceipt receipt;
                const subscriber_lifecycle::CaptureApi api{
                    reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x9667c0),reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x9676a0),
                    reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x89e580),reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x89e5f0),
                    reinterpret_cast<subscriber_lifecycle::PrepareSave>(state.image+part[2]),reinterpret_cast<subscriber_lifecycle::SaveRecord>(state.image+part[3])};
                {live_host_state::apply_timing::PageReadScope original(live_host_state::apply_timing::PageBoundary::OriginalCall);
                    if(!subscriber_lifecycle::capture_original(api,reinterpret_cast<const void*>(object),inspect_value,&captured,receipt,error))return false;}
                const auto external=[&](uintptr_t pointer,std::string& key,std::string& e){
                    const auto found=scenery_owners.find(pointer);
                    if(found==scenery_owners.end())return fail(e,"Dungeon authored Plan external link has no current complete Scenery owner");
                    key=found->second;return true;};
                duplicate_records::Witness witness;
                if(!duplicate_records::summarize(captured.graph,object,external,witness,error))return false;
                parts.push_back(json::array({part[0],witness.sha}));
            }
            std::string sha;if(!quest_bridge::sha256(parts.dump(),sha,error))return false;
            design=designs.emplace(owner.dungeon,std::move(sha)).first;
        }
        dungeon_input_authored::Scene scene;scene.key=owner.key;scene.dungeon=owner.exterior_key;
        scene.canvas_sha=saved->second.second;scene.design_sha=design->second;
        if(pool.gizmos.size()!=pool.local.members.size())return fail(error,"Dungeon authored Gizmo record/member count differs");
        for(const auto& row:pool.gizmos){
            if(gizmo_index>=sample.scenes.keys.size())return fail(error,"Dungeon authored Gizmo canonical roster truncated");
            scene.gizmos.push_back({sample.scenes.keys[gizmo_index++],row});}
        scene.scenery_counts=pool.scenery.counts;
        for(const auto& row:pool.scenery.members){dungeon_input_authored::Scenery value;value.category=row.category;value.bounds_valid=row.bounds.valid!=0;
            const auto vec=[](const auto& v){return std::array<float,3>{v.x,v.y,v.z};};
            value.minimum=vec(row.bounds.min);value.maximum=vec(row.bounds.max);value.position=vec(row.transform.position);value.scale=vec(row.transform.scale);
            value.rotation=row.transform.rotation;scene.scenery.push_back(value);}
        for(const auto& row:monster_pools[index].monsters){
            const auto alias=std::find_if(sample.monsters.begin(),sample.monsters.end(),[&](const auto& item){
                return item.second.scope.manager==pool.managers[2]&&item.second.pointer==row.pointer&&item.second.uid==row.uid;});
            if(alias==sample.monsters.end())return fail(error,"Dungeon authored Monster lacks its confirmed scene/full-UID owner");
            scene.monsters.push_back({monster_target_key::canonical(alias->first),row.rank,row.slot,row.type});}
        for(const auto& member:pool.zones.members){const auto found=std::find_if(zones.begin(),zones.end(),[&](const auto& row){
                return row.scene_key==owner.key&&row.object==member.object&&row.uid==member.uid&&row.root==owner.shard&&row.region==owner.region&&row.manager==pool.zones.manager;});
            if(found==zones.end())return fail(error,"Dungeon input original Zone has no exact current canonical scene/full-UID owner");
            scene.zones.emplace_back(found->key,found->type);}
        const auto count=std::count_if(zones.begin(),zones.end(),[&](const auto& row){return row.scene_key==owner.key;});
        if(size_t(count)!=pool.zones.members.size())return fail(error,"Dungeon input current canonical Zone roster differs from complete native scene");
        scenes.push_back(std::move(scene));
    }
    if(gizmo_index!=sample.scenes.keys.size())return fail(error,"Dungeon authored Gizmo canonical roster has extra members");
    if(!dungeon_input_authored::digest(scenes,sample.canvas_sha,sample.value.stroke.scenes_sha,error))return false;
    if(typed)*typed=std::move(scenes);return true;
}
}
}
