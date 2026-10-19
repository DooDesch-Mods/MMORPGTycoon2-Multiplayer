namespace complete_record {
struct Memory final:subscriber_lifecycle::Memory {
    host::ProcessMemory source;
    bool read(uintptr_t p,void* out,size_t bytes,std::string& error)override{return source.copy(p,out,bytes,error);}
};
struct Capture {Memory& memory;subscriber_lifecycle::Graph graph;};
bool inspect(void* user,const void* record,const void* context,std::string& error){auto& value=*static_cast<Capture*>(user);
    return subscriber_lifecycle::read_graph(value.memory,reinterpret_cast<uintptr_t>(record),reinterpret_cast<uintptr_t>(context),
        {100000,1000000,32*1024*1024,100000},value.graph,error);
}
bool capture(Memory& memory,uintptr_t image,uintptr_t object,bool dungeon,subscriber_lifecycle::Graph& out,std::string& error){
    Capture value{memory};subscriber_lifecycle::CaptureReceipt cleanup;
    const subscriber_lifecycle::CaptureApi api{
        reinterpret_cast<subscriber_lifecycle::Construct>(image+0x9667c0),reinterpret_cast<subscriber_lifecycle::Construct>(image+0x9676a0),
        reinterpret_cast<subscriber_lifecycle::Construct>(image+0x89e580),reinterpret_cast<subscriber_lifecycle::Construct>(image+0x89e5f0),
        reinterpret_cast<subscriber_lifecycle::PrepareSave>(image+(dungeon?0xe4a3a0:0x110ef70)),
        reinterpret_cast<subscriber_lifecycle::SaveRecord>(image+(dungeon?0xe4a400:0x110efd0))};
    {live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);
        if(!subscriber_lifecycle::capture_original(api,reinterpret_cast<const void*>(object),inspect,&value,cleanup,error))return false;}
    out=std::move(value.graph);return true;
}
bool same(const Census& a,const Census& b,std::string& error){
    if(a.world.game!=b.world.game||a.world.mode!=b.world.mode||a.world.state!=b.world.state||a.world.world!=b.world.world||a.world.map!=b.world.map||
       a.world.generation!=b.world.generation||a.world.world_epoch!=b.world.world_epoch||a.world.uuid!=b.world.uuid||a.world.sha256!=b.world.sha256||a.pools.size()!=b.pools.size())
        return fail(error,"Dungeon original complete Record capture changed NativeOpen lifetime or scene roster");
    for(size_t i=0;i<a.pools.size();++i){const auto& x=a.pools[i];const auto& y=b.pools[i];
        if(x.scene.owner.key!=y.scene.owner.key||x.scene.owner.exterior_key!=y.scene.owner.exterior_key||x.scene.owner.shard!=y.scene.owner.shard||
           x.scene.owner.region!=y.scene.owner.region||x.scene.owner.dungeon!=y.scene.owner.dungeon||x.scene.owner.exterior!=y.scene.owner.exterior||
           x.scene.owner.exterior_uid!=y.scene.owner.exterior_uid||x.scene.dungeon_uid!=y.scene.dungeon_uid||x.managers!=y.managers||
           x.gizmos!=y.gizmos||x.local!=y.local||x.zones.members!=y.zones.members||x.scenery!=y.scenery)
            return fail(error,"Dungeon original complete Record capture changed a current scene owner or actor roster");
        for(size_t type=0;type<4;++type)if(x.zones.arrays[type].data!=y.zones.arrays[type].data||
            x.zones.arrays[type].count!=y.zones.arrays[type].count||x.zones.arrays[type].capacity!=y.zones.arrays[type].capacity||
            x.zones.arrays[type].members!=y.zones.arrays[type].members)
            return fail(error,"Dungeon original complete Record capture changed an all-four-type Zone owner array");
    }return true;
}
}
bool read_records(const dungeon_scene_records::External& external,Census& current,dungeon_scene_records::Witness& out,std::string& error)try{
    current={};out={};Census before;if(!read_current(before,error))return false;
    uintptr_t image=0;uint32_t thread=0;std::string digest;
    if(!native::read_resolver_receipt(image,thread,digest,error))return false;
    if(digest!=executable||image!=uintptr_t(GetModuleHandleW(nullptr))||thread!=GetCurrentThreadId())
        return fail(error,"Dungeon complete Record capture requires exact native image and Mainthread");
    complete_record::Memory memory;
    for(const auto& pin:dungeon_scene_record_codes::helpers){std::vector<unsigned char> bytes(pin.bytes);
        if(!memory.read(image+pin.rva,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=pin.sha)return fail(error,"Dungeon complete Record original capture helper differs RVA="+std::to_string(pin.rva));
    }
    std::vector<dungeon_scene_records::Scene> scenes;std::vector<dungeon_scene_records::Dungeon> dungeons;std::map<uintptr_t,std::string> owners;
    for(const auto& pool:before.pools){const auto& owner=pool.scene.owner;dungeon_scene_records::Scene scene;
        scene.key=owner.key;scene.dungeon_key=owner.exterior_key;scene.region=owner.region;scene.shard=owner.shard;scene.dungeon=owner.dungeon;
        if(!complete_record::capture(memory,image,scene.region,false,scene.graph,error))return false;
        scenes.push_back(std::move(scene));const auto known=owners.find(owner.dungeon);
        if(known!=owners.end()){if(known->second!=owner.exterior_key)return fail(error,"Dungeon complete Record actual Dungeon has two exterior keys");continue;}
        owners.emplace(owner.dungeon,owner.exterior_key);dungeon_scene_records::Dungeon dungeon;dungeon.key=owner.exterior_key;dungeon.object=owner.dungeon;
        if(!complete_record::capture(memory,image,dungeon.object,true,dungeon.graph,error))return false;
        dungeons.push_back(std::move(dungeon));
    }
    dungeon_scene_records::Witness result;if(!dungeon_scene_records::summarize(scenes,dungeons,external,result,error))return false;
    Census after;if(!read_current(after,error)||!complete_record::same(before,after,error))return false;
    current=std::move(after);out=std::move(result);error.clear();return true;
}catch(const std::exception& ex){current={};out={};return fail(error,std::string("Dungeon complete original Record boundary: ")+ex.what());}
