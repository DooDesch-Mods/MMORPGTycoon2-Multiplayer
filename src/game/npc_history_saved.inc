bool restore_history(const action_session::Binding& binding,const action_session::Request& request,const std::string& key,
                     int32_t id,uint64_t uid,uintptr_t previous,uintptr_t restored,std::string& error){
    std::lock_guard lock(gate);opened::CurrentWorld world;
    const auto controller=typed_action_runtime::controller_status();
    if(!action_session::npc_history(request)||request.snapshot_uuid!=uuid||!controller.value("pending",false)||
       controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid||
       !current(world,error)||world.world_epoch!=binding.native_epoch||world.generation!=binding.native_generation)
        return q::fail(error,error.empty()?"NPC saved restore requires the exact pending current History binding":error);
    auto pool=pools.find(id);
    if(pool==pools.end())return q::fail(error,"NPC history restored Region has no original complete saved pool");
    building_child_detail::Lookup find;uintptr_t region=0;npc_census::State rows;
    if(!find.initialize(error)||!building_child_detail::census(world,id,region,rows,error))return false;
    auto next=pool->second;bool saved=false;
    for(const auto& record:next.pool.records)if(q::key(sha,id,record)==key)saved=true;
    if(saved&&!npc_history_identity::restore(sha,key,next,rows,previous,uid,restored,find,error))return false;
    ChildBinding* child=nullptr;const npc_census::Row* row=nullptr;
    for(const auto& item:rows.rows)if(item.local_pointer==restored&&item.local_uid==uid){if(row)return q::fail(error,"NPC history restored complete census repeats its receiver");row=&item;}
    if(!row)return q::fail(error,"NPC history restored receiver is absent from complete Region census");
    for(auto& [parent,bound]:building_children){(void)parent;
        if(bound.receipt.child.key!=key)continue;
        if(child||!bound.receipt.present||bound.receipt.parent.saved_region!=id||bound.receipt.child.row.local_uid!=uid||
           bound.receipt.child.row.local_pointer!=previous||row->type!=bound.receipt.child.row.type)
            return q::fail(error,"NPC history retained Building child source alias differs");
        child=&bound;
        uintptr_t registered=0,ap=0,owner=0,parent_region=0,parent_root=0;uint64_t parent_uid=0,child_uid=0;
        const auto& original=bound.receipt.parent;
        if(!find(original.uid,registered,error)||registered!=original.object||
           !q::at(memory,original.object,0,ap,error)||ap!=image+0x16d93c0||
           !q::at(memory,original.object,0x18,parent_uid,error)||parent_uid!=original.uid||
           !q::at(memory,original.object,0x170,parent_root,error)||parent_root!=reinterpret_cast<uintptr_t>(world.map)+0x1c0||
           !q::at(memory,original.object,0x178,parent_region,error)||parent_region!=region||
           !q::at(memory,original.object,0xa30,child_uid,error)||
           !find(uid,registered,error)||registered!=restored||!q::at(memory,restored,0x268,owner,error))
            return q::fail(error,error.empty()?"NPC history restored retained Building source is no longer current":error);
        if(owner!=0||(child_uid!=0&&child_uid!=uid))return q::fail(error,"Original MakeNPC restore has an unexpected reciprocal Building link");
        for(const auto& other:rows.rows){uintptr_t other_owner=0;
            if(other.local_pointer==restored)continue;
            if(!q::at(memory,other.local_pointer,0x268,other_owner,error))return false;
            if(other_owner==original.object)return q::fail(error,"NPC history detached retained alias conflicts with another current Building child");}
    }
    if(!saved&&!child)return q::fail(error,"NPC history restore has no original saved or Building child canonical alias");
    opened::CurrentWorld final_world;
    if(!current(final_world,error)||!same_world(world,final_world))return q::fail(error,error.empty()?"NPC history saved restore changed current World":error);
    if(saved)pool->second=std::move(next);
    if(child){child->receipt.child.row=*row;child->retired=false;child->history_detached=true;}
    error.clear();return true;
}
