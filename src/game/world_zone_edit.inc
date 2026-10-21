namespace {
bool zone_edit_bound(const world_zone_runtime::Edit& edit,q::World& world,const q::SavedPool*& saved,std::string& error){
    if(!edit.prepared||edit.published||edit.snapshot!=uuid||!monster_target_key::uuid(edit.operation)||
       edit.binding.native_epoch!=origin.world_epoch||edit.binding.native_generation!=origin.generation||
       !current(edit.world.saved_region,world,saved,error)||!npc_zone_append::same_world(world,edit.world)||
       !npc_zone_append::same_pool(*saved,edit.monsters))return q::fail(error,error.empty()?"Zone edit retained binding or current canonical pool differs":error);
    uintptr_t manager=0;if(!q::at(memory,world.region,0x968,manager,error))return false;
    const auto retained=world_zone_pools.find(manager);std::map<uint64_t,world_zone_runtime::Current> others;
    for(const auto& row:edit.before)if(row.type!=1)others.emplace(row.uid,row);
    if(retained==world_zone_pools.end()||retained->second.rows!=others||retained->second.scene!="root"||
       retained->second.root!=world.root||retained->second.region!=world.region)
        return q::fail(error,"Zone edit retained non-Monster current canonical pool differs");
    return true;
}
bool same_zone_edit_roster(const dungeon_zone_roster::Census& left,const dungeon_zone_roster::Census& right){
    if(left.manager!=right.manager||left.shard!=right.shard||left.region!=right.region||left.members!=right.members)return false;
    for(size_t index=0;index<left.arrays.size();++index){const auto& a=left.arrays[index];const auto& b=right.arrays[index];
        if(a.data!=b.data||a.count!=b.count||a.capacity!=b.capacity||a.members!=b.members)return false;}
    return true;
}
bool zone_edit_raw(const q::World& world,dungeon_zone_roster::Census& out,std::string& error){
    uintptr_t manager=0;if(!q::at(memory,world.region,0x968,manager,error))return false;
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return q::at(memory,object,offset,value,cause);};
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& cause){const auto exception=exact_uid(uid,object);
        return !exception||q::fail(cause,"Zone edit exact FindUID exception="+std::to_string(exception));};
    return dungeon_zone_roster::read(at,find,image,manager,world.root,world.region,out,error);
}
bool zone_edit_monsters(const q::World& world,const q::SavedPool& prior,const std::vector<world_zone_runtime::Current>& rows,
    q::SavedPool& out,std::string& error){
    const auto link=[&](uintptr_t pointer,int32_t& id,std::string& cause){
        const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.object==pointer&&row.type==1;});
        if(found==rows.end())return q::fail(cause,"Zone edit Monster current array lacks its complete canonical preview");
        id=0;for(const auto& old:prior.array.zones)if(old.pointer==pointer&&old.uid==found->uid){id=old.link_id;break;}return true;
    };
    q::Array actual;if(!q::observe_array(memory,image,prior.array.pointer,budget,link,actual,error))return false;
    out=prior;out.array=std::move(actual);out.action_keys.clear();
    for(const auto& row:out.array.zones){
        const auto known=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value.object==row.pointer&&value.uid==row.uid&&value.type==1;});
        if(known==rows.end()||!out.action_keys.emplace(row.pointer,known->key).second)return q::fail(error,"Zone edit Monster alias/full UID preview differs");
    }
    return q::validate_saved(memory,world,out,budget,error);
}
}
bool prepare_world_zone_edit(const action_session::Binding& binding,const std::string& snapshot,int32_t region,
    const std::string& operation,world_zone_runtime::Edit& out,std::string& error){
    out={};std::lock_guard lock(gate);q::World world;const q::SavedPool* saved=nullptr;
    if(snapshot!=uuid||binding.native_epoch!=origin.world_epoch||binding.native_generation!=origin.generation||
       !monster_target_key::uuid(operation)||!current(region,world,saved,error)||!q::validate_saved(memory,world,*saved,budget,error))
        return q::fail(error,error.empty()?"Zone edit preparation snapshot/binding/current content differs":error);
    dungeon_zone_roster::Census raw;if(!zone_edit_raw(world,raw,error))return false;
    const auto other=world_zone_pools.find(raw.manager);
    if(other==world_zone_pools.end()||other->second.scene!="root"||other->second.root!=world.root||other->second.region!=world.region)
        return q::fail(error,"Zone edit root manager has no complete admitted four-array ownership");
    std::vector<world_zone_runtime::Current> rows;std::set<std::string> keys;size_t monsters=0,others=0;
    for(const auto& member:raw.members){world_zone_runtime::Current row;
        if(member.type==1){++monsters;const auto known=std::find_if(saved->array.zones.begin(),saved->array.zones.end(),[&](const auto& value){return value.pointer==member.object&&value.uid==member.uid;});
            if(known==saved->array.zones.end())return q::fail(error,"Zone edit current Monster lacks its admitted full UID alias");
            row={q::zone_key(*saved,*known),"root",member.object,world.root,world.region,raw.manager,member.uid,member.type};
        }else{++others;const auto known=other->second.rows.find(member.uid);
            if(known==other->second.rows.end()||known->second.object!=member.object||known->second.type!=member.type)
                return q::fail(error,"Zone edit current non-Monster identity differs from its admitted owner");row=known->second;}
        if(!keys.insert(row.key).second)return q::fail(error,"Zone edit before canonical key duplicated");rows.push_back(std::move(row));
    }
    if(monsters!=saved->array.zones.size()||others!=other->second.rows.size())return q::fail(error,"Zone edit before roster omitted a confirmed member");
    out={binding,snapshot,operation,world,*saved,std::move(rows),true,false};error.clear();return true;
}
bool preview_world_zone_edit(const world_zone_runtime::Edit& edit,uintptr_t born,uint64_t born_uid,
    const std::map<uint64_t,world_zone_runtime::Current>& restored,world_zone_runtime::Preview& out,std::string& error){
    out={};std::lock_guard lock(gate);q::World world;const q::SavedPool* saved=nullptr;
    if(!zone_edit_bound(edit,world,saved,error)||!zone_edit_raw(world,out.raw,error))return false;
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& cause){const auto exception=exact_uid(uid,object);
        return !exception||q::fail(cause,"Zone edit original lifetime FindUID exception="+std::to_string(exception));};
    if(!zone_edit_identity::reconcile(edit.before,out.raw,edit.operation,born,born_uid,restored,find,out.rows,error)||
       !zone_edit_monsters(world,*saved,out.rows,out.monsters,error))return false;
    out.born=born;out.born_uid=born_uid;out.restored=restored;
    q::World after;const q::SavedPool* same=nullptr;return zone_edit_bound(edit,after,same,error);
}
bool publish_world_zone_edit(world_zone_runtime::Edit& edit,const world_zone_runtime::Preview& preview,std::string& error){
    std::lock_guard lock(gate);q::World world;const q::SavedPool* saved=nullptr;
    if(!zone_edit_bound(edit,world,saved,error))return false;
    dungeon_zone_roster::Census actual;if(!zone_edit_raw(world,actual,error)||!same_zone_edit_roster(actual,preview.raw))
        return q::fail(error,error.empty()?"Zone edit original current roster changed before publication":error);
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& cause){const auto exception=exact_uid(uid,object);
        return !exception||q::fail(cause,"Zone edit publication FindUID exception="+std::to_string(exception));};
    std::vector<world_zone_runtime::Current> rows;
    if(!zone_edit_identity::reconcile(edit.before,actual,edit.operation,preview.born,preview.born_uid,preview.restored,find,rows,error)||rows!=preview.rows)
        return q::fail(error,error.empty()?"Zone edit canonical preview changed before publication":error);
    q::SavedPool monsters;if(!zone_edit_monsters(world,*saved,preview.rows,monsters,error)||!npc_zone_append::same_pool(monsters,preview.monsters))
        return q::fail(error,error.empty()?"Zone edit current Monster content changed before publication":error);
    const auto retained=world_zone_pools.find(actual.manager);
    if(retained==world_zone_pools.end())return q::fail(error,"Zone edit admitted non-Monster pool disappeared before publication");
    WorldZonePool others=retained->second;others.rows.clear();
    for(const auto& row:preview.rows)if(row.type!=1&&!others.rows.emplace(row.uid,row).second)return q::fail(error,"Zone edit non-Monster publication repeats a full UID");
    pools.at(world.saved_region)=std::move(monsters);world_zone_pools.at(actual.manager)=std::move(others);edit.published=true;error.clear();return true;
}
