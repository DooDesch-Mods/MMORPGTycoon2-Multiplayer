bool reconcile_world_zone_cells(const WorldZonePool& before,const dungeon_zone_roster::Census& actual,
    const std::map<uint64_t,world_zone_runtime::Current>* original,const std::set<uint64_t>* restored,
    WorldZonePool& out,std::string& error){
    if(before.manager!=actual.manager||before.root!=actual.shard||before.region!=actual.region)
        return q::fail(error,"Dungeon cell non-Monster Zone current owner differs");
    WorldZonePool next=before;next.rows.clear();std::set<std::string> keys;
    for(const auto& member:actual.members){if(member.type==1)continue;
        const world_zone_runtime::Current* saved=nullptr;const auto current=before.rows.find(member.uid);
        if(current!=before.rows.end())saved=&current->second;
        else if(original&&restored&&restored->contains(member.uid)){const auto old=original->find(member.uid);if(old!=original->end())saved=&old->second;}
        if(!saved||saved->type!=member.type||saved->root!=actual.shard||saved->region!=actual.region||saved->manager!=actual.manager||saved->scene_key!=before.scene||
           (current!=before.rows.end()&&saved->object!=member.object)||!keys.insert(saved->key).second)
            return q::fail(error,"Dungeon cell non-Monster Zone lacks current identity or exact original Make receipt");
        uintptr_t found=0;const auto exception=exact_uid(member.uid,found);
        if(exception||found!=member.object)return q::fail(error,"Dungeon cell non-Monster Zone full UID registry differs exception="+std::to_string(exception));
        auto row=*saved;row.object=member.object;
        if(!next.rows.emplace(member.uid,std::move(row)).second)return q::fail(error,"Dungeon cell non-Monster Zone full UID duplicated");
    }
    for(const auto& [uid,row]:before.rows){(void)row;if(next.rows.contains(uid))continue;uintptr_t found=0;const auto exception=exact_uid(uid,found);
        if(exception||found)return q::fail(error,"Dungeon cell non-Monster Zone original retirement incomplete exception="+std::to_string(exception));}
    if(original&&restored)for(const auto& [uid,row]:*original){(void)row;if(restored->contains(uid)&&!next.rows.contains(uid))
        return q::fail(error,"Dungeon cell original Make non-Monster Zone absent after Undo/Redo");}
    out=std::move(next);return true;
}
bool prepare_world_zone_cells(DungeonCellCascade& receipt,std::string& error){
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return q::at(memory,p,offset,value,cause);};
    auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& cause){const auto exception=exact_uid(uid,pointer);return !exception||q::fail(cause,"Dungeon cell Zone before FindUID exception="+std::to_string(exception));};
    for(const auto& [manager,scene]:scene_pools){(void)scene;const auto retained=world_zone_pools.find(manager);
        if(retained==world_zone_pools.end())return q::fail(error,"Dungeon cell non-Monster Zone owner lacks current confirmed pool");
        dungeon_zone_roster::Census current;WorldZonePool checked;
        if(!dungeon_zone_roster::read(at,find,image,manager,retained->second.root,retained->second.region,current,error)||
           !reconcile_world_zone_cells(retained->second,current,nullptr,nullptr,checked,error)||checked.rows!=retained->second.rows)
            return q::fail(error,error.empty()?"Dungeon cell non-Monster Zone before roster differs":error);
        receipt.other_before.emplace(manager,retained->second.rows);
    }
    return true;
}
bool confirm_world_zone_cells(const DungeonCellCascade& receipt,const dungeon_scene_identity_runtime::Census& scenes,
    const DungeonCellCascade* original,const std::set<uint64_t>* restored,std::map<uintptr_t,WorldZonePool>& out,std::string& error){
    if(receipt.other_before.size()!=scene_pools.size())return q::fail(error,"Dungeon cell non-Monster Zone receipt omitted scene pool");
    auto next=world_zone_pools;
    for(const auto& [manager,rows]:receipt.other_before){const auto held=world_zone_pools.find(manager);
        const auto actual=std::find_if(scenes.pools.begin(),scenes.pools.end(),[&](const auto& pool){return pool.managers[1]==manager;});
        if(held==world_zone_pools.end()||held->second.rows!=rows||actual==scenes.pools.end()||actual->scene.owner.key!=held->second.scene)
            return q::fail(error,"Dungeon cell non-Monster Zone retained identity/scene changed before confirmation");
        const std::map<uint64_t,world_zone_runtime::Current>* history=nullptr;
        if(original){const auto old=original->other_before.find(manager);if(old==original->other_before.end())return q::fail(error,"Dungeon cell non-Monster Zone original history owner absent");history=&old->second;}
        WorldZonePool changed;if(!reconcile_world_zone_cells(held->second,actual->zones,history,restored,changed,error))return false;
        next[manager]=std::move(changed);
    }
    out=std::move(next);return true;
}
