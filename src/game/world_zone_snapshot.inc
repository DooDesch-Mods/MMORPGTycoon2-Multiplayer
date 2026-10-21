bool world_zone_retain(world_zone_serialization::Pool value,std::string& error){
    if(world_zone_observed.size()>=4096||!world_zone_observed.emplace(value.manager,std::move(value)).second)
        return q::fail(error,"World Zone serializer repeated a manager or exceeded its budget");
    return true;
}
bool world_zone_seal(const opened::CurrentWorld& world,const std::map<int32_t,q::SavedPool>& roots,
                     const std::map<uintptr_t,ScenePool>& scenes,std::string& error){
    std::map<uintptr_t,WorldZonePool> next;std::set<std::string> keys;
    auto at=[&](uintptr_t p,size_t offset,auto& out,std::string& cause){return q::at(memory,p,offset,out,cause);};
    auto find=[&](uint64_t uid,uintptr_t& out,std::string& cause){const auto exception=exact_uid(uid,out);
        return !exception||q::fail(cause,"World Zone original FindUID failed exception="+std::to_string(exception));};
    const auto add=[&](uintptr_t manager,uintptr_t root,uintptr_t region,const std::string& scene,const std::string& owner)->bool{
        const auto saved=world_zone_observed.find(manager);
        if(saved==world_zone_observed.end())return q::fail(error,"World Zone manager lacks original four-array serialization receipt");
        dungeon_zone_roster::Census actual;if(!dungeon_zone_roster::read(at,find,image,manager,root,region,actual,error))return false;
        WorldZonePool pool{scene,root,region,manager,{}};size_t count=0;
        for(size_t type=0;type<4;++type){const auto& rows=saved->second.arrays[type];
            if(actual.arrays[type].members.size()!=rows.size())return q::fail(error,"World Zone serialized/current array count differs");
            for(size_t child=0;child<rows.size();++child){const auto& row=rows[child];const auto current=std::find_if(actual.members.begin(),actual.members.end(),[&](const auto& member){return member.object==row.object;});
                if(actual.arrays[type].members[child]!=row.object||current==actual.members.end()||current->uid!=row.uid||current->type!=row.type)
                    return q::fail(error,"World Zone serialized/current order/full UID/type differs");
                ++count;if(type==1)continue;
                const auto suffix=row.link>0?"record:"+std::to_string(row.link):owner+":child:"+std::to_string(child);
                const auto key=sha+":world:zone-type:"+std::to_string(type)+":"+suffix;
                if(!keys.insert(key).second||!pool.rows.emplace(row.uid,world_zone_runtime::Current{key,scene,row.object,root,region,manager,row.uid,row.type}).second)
                    return q::fail(error,"World Zone original canonical identity duplicated");
            }
        }
        if(count!=actual.members.size()||!next.emplace(manager,std::move(pool)).second)return q::fail(error,"World Zone serialized owner set differs or repeats");
        return true;
    };
    const auto map=reinterpret_cast<uintptr_t>(world.map),root=map+0x1c0;
    for(const auto& [id,saved]:roots){(void)saved;uintptr_t region=0,manager=0;
        if(!region_saved_identity::resolve(image,map,root,id,0,region,at,error)||!at(region,0x968,manager,error)||
           !add(manager,root,region,"root","root:"+std::to_string(id)))return false;}
    for(const auto& [manager,pool]:scenes){const auto& scene=pool.scene;
        const auto owner="scene:"+std::to_string(scene.exterior_region)+":"+std::to_string(scene.exterior_slot)+":"+std::to_string(scene.owner.instance_index);
        if(!add(manager,scene.owner.shard,scene.owner.region,scene.owner.key,owner))return false;}
    if(next.size()!=world_zone_observed.size())return q::fail(error,"World Zone serialization contains an unclassified manager");
    world_zone_pools=std::move(next);world_zone_observed.clear();return true;
}
