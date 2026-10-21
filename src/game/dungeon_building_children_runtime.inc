namespace dungeon_building {
namespace {
namespace child=building_npc_lifecycle;
namespace children_flow=dungeon_building_children;
struct ChildrenPort {
    Memory memory;const a::Binding& binding;const std::string& snapshot;const a::Request& request;
    std::shared_ptr<Local> local;std::map<uintptr_t,std::string> external;bool birth=false;
    bool find(uint64_t uid,uintptr_t& object,std::string& error){return dungeon_building::find(memory,uid,object,error);}
    bool retained(const std::string& key,child::Receipt& receipt,bool& found,bool& retired,std::string& error){
        return child::retained_receipt(key,receipt,found,retired,error);
    }
    child::Lease lease(const child::Receipt* previous=nullptr)const{
        return {snapshot,request.action_uuid,previous?previous->lease.duplicate_uuid:request.action_uuid,
            binding.native_epoch,binding.native_generation};
    }
    bool owner(uintptr_t pointer,std::string& key,std::string& error){
        const auto row=external.find(pointer);if(row==external.end())return fail(error,"Dungeon Building child Record external owner was not bound before private capture");
        key=row->second;return true;
    }
    bool capture(const child::Parent& parent,const child::Receipt* old,child::Receipt& actual,std::string& error){
        const auto resolver=[&](uintptr_t p,std::string& key,std::string& cause){return owner(p,key,cause);};
        return birth&&!old?child::capture_birth(lease(),parent,resolver,actual,error):
            child::capture_before(lease(old),parent,resolver,actual,error);
    }
    bool inverse(const std::vector<uintptr_t>& operations,child::Receipt& receipt,std::string& error){
        uintptr_t selected=0;
        for(auto node:operations){uintptr_t ap=0;uint64_t uid=0;
            if(!field(memory,node,0,ap,error))return false;
            if(ap!=state.image+0x16de8a0)continue;
            if(!field(memory,node,0xb8,uid,error))return false;
            if(uid!=receipt.child.row.local_uid)continue;
            if(selected)return fail(error,"Dungeon Building original child inverse repeats its complete saved UID");selected=node;
        }
        if(!selected)return fail(error,"Dungeon Building removed child has no MakeNPC in the actual original inverse graph");
        child::MakeNPCInverse proof;
        return child::validate_make_npc(lease(&receipt),selected,receipt,proof,error);
    }
    bool retire(const child::Receipt& receipt,std::string& error){return child::confirm_retired(lease(&receipt),receipt,error);}
    bool restore(const child::Receipt& old,const child::Parent& parent,child::Receipt& actual,std::string& error){
        const auto resolver=[&](uintptr_t p,std::string& key,std::string& cause){return owner(p,key,cause);};
        return child::confirm_restored(lease(&old),old,parent,resolver,actual,error);
    }
};
bool child_current(ChildrenPort& port,std::vector<child::Parent>& parents,std::string& error){
    port.local=std::make_shared<Local>();auto& local=*port.local;local.binding=port.binding;local.snapshot=port.snapshot;
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("initialized",false)||!controller.value("started",false)||controller.value("locked",true)||
       !controller.value("pending",false)||controller.value("lobby_retiring",true)||
       controller.value("action_uuid",std::string{})!=port.request.action_uuid||!controller.value("cause",std::string{}).empty())
        return fail(error,"Dungeon Building child preparation requires the exact pending unlocked Controller request");
    if(port.request.snapshot_uuid!=port.snapshot||!current(port.memory,port.binding,port.snapshot,local.world,error)||
       !dungeon_scene_identity_runtime::read_current(local.cells.scenes,error))return false;
    if(local.cells.scenes.world.world_epoch!=port.binding.native_epoch||local.cells.scenes.world.generation!=port.binding.native_generation||
       local.cells.scenes.world.world!=local.world.origin.world||local.cells.scenes.world.map!=local.world.origin.map)
        return fail(error,"Dungeon Building child preparation complete Scene census belongs to another World");
    std::map<std::string,json> values;if(!buildings(port.memory,local,values,error))return false;
    parents.clear();for(const auto& building:local.buildings){child::Parent parent;parent.key=building.key;
        parent.object=building.object;parent.uid=building.uid;parent.scene=building.scene;
        if(!field(port.memory,building.region,0x834,parent.saved_region,error))return false;
        parent.make_form=building.saved.form==duplicate_building_history::Form::Functional?child::MakeForm::Functional:
            building.saved.form==duplicate_building_history::Form::Modular?child::MakeForm::Modular:child::MakeForm::Art;
        parents.push_back(std::move(parent));
    }
    return true;
}
// Resolve the actual original SaveContext links outside the saved-NPC gate.
// The callback used under that gate is then a read-only local lookup.
bool child_external(ChildrenPort& port,const Snapshot* previous,std::string& error){
    auto& local=*port.local;auto& names=port.external;
    names.emplace(local.world.root,"root_shard");
    for(const auto& row:local.buildings){names.emplace(row.object,row.key);
        if(row.scene=="world"){int32_t id=-1;if(!field(port.memory,row.region,0x834,id,error))return false;
            names.emplace(row.region,"region:"+std::to_string(id));}}
    for(const auto& pool:local.cells.scenes.pools){const auto& row=pool.scene.owner;
        names.emplace(row.shard,row.key+"/shard");names.emplace(row.region,row.key+"/region");
        names.emplace(row.dungeon,row.exterior_key+"/dungeon");if(row.instance)names.emplace(row.instance,row.key);
    }
    std::vector<npc_saved_identity::Current> aliases;if(!npc_saved_identity_runtime::read_current(aliases,error))return false;
    for(const auto& row:aliases){names.emplace(row.row.local_pointer,row.key);
        if(row.scene=="world"){uintptr_t region=0;if(!field(port.memory,row.row.local_pointer,0x178,region,error))return false;
            names.emplace(region,"region:"+std::to_string(row.saved_region));}}
    uintptr_t character_manager=0;character_definition_identity::Census definitions;
    auto definition_at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return field(port.memory,p,offset,value,cause);};
    if(!definition_at(uintptr_t(local.world.origin.state),0x168,character_manager,error)||
       !character_definition_identity::read(definition_at,state.image,uintptr_t(local.world.origin.state),character_manager,definitions,error))return false;
    for(const auto& row:definitions.definitions)if(!row.deleted)names.emplace(row.object,row.key);
    if(previous){if(!previous->local||previous->local->binding!=port.binding||previous->local->snapshot!=port.snapshot)
            return fail(error,"Dungeon Building child external seed belongs to another native lifetime");
        for(const auto& [pointer,row]:previous->local->quests.owners){uintptr_t actual=0;
            if(!port.find(row.uid,actual,error))return false;
            if(actual==pointer)names.emplace(pointer,row.key);
        }
    }
    // Prebind every current reciprocal child before inspecting any Record so a
    // cross-child destination does not depend on the parent iteration order.
    std::vector<std::pair<const BuildingOwner*,npc_census::Row>> current_children;
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return field(port.memory,p,offset,value,cause);};
    auto registered=[&](uint64_t uid,uintptr_t& pointer,std::string& cause){return port.find(uid,pointer,cause);};
    for(const auto& building:local.buildings){child::Parent parent;parent.key=building.key;parent.object=building.object;
        parent.uid=building.uid;parent.scene=building.scene;if(!at(building.region,0x834,parent.saved_region,error))return false;
        npc_census::State census;npc_census::Row row;bool present=false;
        if(!npc_census::read(port.memory,state.image,building.region,census,error)||
           !building_npc_owner::read(at,registered,{state.image,building.shard,building.region},parent,census,row,present,error))return false;
        if(!present)continue;
        std::string key=building_npc_key::make(building.key);
        child::Receipt retained;bool found=false,retired=false;if(!port.retained(building.key,retained,found,retired,error))return false;
        if(found&&retained.present&&retained.child.row.local_uid==row.local_uid)key=retained.child.key;
        const auto old=names.find(row.local_pointer);if(old!=names.end())key=old->second;
        names[row.local_pointer]=key;current_children.emplace_back(&building,row);
    }
    const dungeon_input_codes::Pin pins[]{
        {0x9667c0,181,"f061a1fe2ca42b75546e22e95981092d682869382e859ed5b10af7ecd80ba6c9"},
        {0x9676a0,148,"7f6e66c76e2be9e2d7f43ab04d8721cc060f16ddcd111a422605af135ab1bb4a"},
        {0x89e580,106,"fc9d17c2e62785b1cfd9e4bb59ae1568cb603f1e232cb30ce48d429581f73646"},
        {0x89e5f0,151,"0808e0234ca17c59c1fd755944f300a0145c52953160aa8231eef3006665e690"},
        {0x10ecae0,87,"52f1b850ad131c46f7d249b5b6365e5c289a571e83c0a6d4996c36647dd36bc2"},
        {0x10ecb40,409,"4a5445004eb0a955a6b8fb7a84c77341f2001af6fa5fa20489039ebe472c621e"}};
    for(const auto& pin:pins){std::vector<unsigned char> bytes(pin.bytes);
        if(!port.memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||
           !hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))return false;
    }
    for(const auto& [building,row]:current_children){(void)building;
        dungeon_cells::ValueMemory view(port.memory);dungeon_cells::ValueCapture captured{view};subscriber_lifecycle::CaptureReceipt cleanup;
        const subscriber_lifecycle::CaptureApi api{
            reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x9667c0),reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x9676a0),
            reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x89e580),reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x89e5f0),
            reinterpret_cast<subscriber_lifecycle::PrepareSave>(state.image+0x10ecae0),reinterpret_cast<subscriber_lifecycle::SaveRecord>(state.image+0x10ecb40)};
        {live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);
            if(!subscriber_lifecycle::capture_original(api,reinterpret_cast<const void*>(row.local_pointer),dungeon_cells::inspect_value,&captured,cleanup,error))return false;}
        for(const auto& link:captured.graph.private_links){if(link.has_inline_definition||names.contains(link.object))continue;
            std::string key;bool dungeon=false;
            if(!npc_quest_target_runtime::bind_current_destination(link.object,key,dungeon,error))return false;
            if(key.empty()||!names.emplace(link.object,std::move(key)).second)return fail(error,"Dungeon Building child original external canonical owner repeats or is empty");
        }
    }
    return true;
}
bool child_owner_seed(const children_flow::Children& children,qr::Witness& seed,std::string& error){
    for(const auto& [key,row]:children){(void)key;if(!row.present||row.child.row.type!=0)continue;
        qr::Owner owner;owner.pointer=row.child.row.local_pointer;owner.uid=row.child.row.local_uid;owner.key=row.child.key;
        owner.state=row.original_quests;owner.destination_uids=row.original_destination_uids;
        const auto found=seed.owners.find(owner.pointer);
        if(found!=seed.owners.end()&&(found->second.uid!=owner.uid||found->second.key!=owner.key))
            return fail(error,"Dungeon Building child Quest seed aliases an unrelated current owner");
        seed.owners[owner.pointer]=std::move(owner);
    }
    return true;
}
}
bool prepare_children(const a::Binding& binding,const std::string& snapshot,const a::Request& request,Snapshot& out,std::string& error,const Snapshot* previous)try{
    out={};ChildrenPort port{{},binding,snapshot,request};std::vector<child::Parent> parents;
    if(!child_current(port,parents,error)||!child_external(port,previous,error))return false;
    for(const auto& parent:parents){child::Receipt old,actual;bool found=false,retired=false;
        if(!port.retained(parent.key,old,found,retired,error))return false;
        if(retired)return fail(error,"Dungeon Building current child requires original restoration before sample preparation");
        if(!port.capture(parent,found?&old:nullptr,actual,error))return false;
        port.local->children.emplace(parent.key,std::move(actual));
    }
    if(previous)port.local->quests=previous->local->quests;
    if(!child_owner_seed(port.local->children,port.local->quests,error))return false;
    out.local=std::move(port.local);return true;
}catch(const std::exception& ex){return fail(error,std::string("Dungeon Building child preparation exception: ")+ex.what());}
bool prepare_history_seed(const a::Binding& binding,const std::string& snapshot,const Snapshot& consumed,Snapshot& out,std::string& error)try{
    out={};if(!consumed.local||consumed.local->binding!=binding||consumed.local->snapshot!=snapshot)
        return fail(error,"Dungeon Building history child seed lacks the exact consumed native lifetime");
    Memory memory;host::WorldState world;if(!current(memory,binding,snapshot,world,error))return false;
    if(!action_runtime::same_world(world,consumed.local->world))return fail(error,"Dungeon Building history child seed current World differs");
    struct Port {Memory& memory;
        bool find(uint64_t uid,uintptr_t& pointer,std::string& cause){return dungeon_building::find(memory,uid,pointer,cause);}
        bool retained(const std::string& key,child::Receipt& value,bool& found,bool& retired,std::string& cause){return child::retained_receipt(key,value,found,retired,cause);}
    }port{memory};
    auto local=std::make_shared<Local>(*consumed.local);
    if(!children_flow::history_seed(consumed.local->quests,consumed.local->children,port,local->quests,error))return false;
    out.value=consumed.value;out.local=std::move(local);return true;
}catch(const std::exception& ex){return fail(error,std::string("Dungeon Building history child seed exception: ")+ex.what());}
bool publish_children(const a::Binding& binding,const std::string& snapshot,const a::Request& request,const Snapshot& before,
                      const Snapshot* consumed,const std::vector<uintptr_t>& operations,Snapshot& out,std::string& error)try{
    out={};if(!before.local||before.local->binding!=binding||before.local->snapshot!=snapshot||
       (consumed&&(!consumed->local||consumed->local->binding!=binding||consumed->local->snapshot!=snapshot)))
        return fail(error,"Dungeon Building child publication lacks exact original samples");
    ChildrenPort port{{},binding,snapshot,request};port.birth=true;std::vector<child::Parent> parents;
    if(!child_current(port,parents,error))return false;
    // Confirm vanished children first. The normal saved-NPC reader can then
    // inspect current aliases without following a deleted parent.
    children_flow::Children retired_before;std::set<uintptr_t> actually_retired;
    if(!children_flow::retire_children(before.local->children,operations,port,retired_before,actually_retired,error))return false;
    if(!child_external(port,&before,error))return false;
    // Retire was already admitted above so the pure flow sees only surviving
    // before rows. Its restore branch still requires the consumed exact receipt.
    auto survivors=before.local->children;for(const auto& [key,row]:retired_before){(void)row;survivors.erase(key);}
    std::set<uintptr_t> retired;std::vector<duplicate_quest_history::OwnerRebind> restored;
    if(!children_flow::publish(survivors,consumed?&consumed->local->children:nullptr,parents,operations,port,
        port.local->children,retired,restored,error))return false;
    for(const auto& [key,row]:retired_before){(void)key;if(before.local->quests.owners.contains(row.child.row.local_pointer))retired.insert(row.child.row.local_pointer);}
    // Non-Quest children never enter the global previous-owner set.
    std::erase_if(restored,[&](const auto& row){return !consumed||!consumed->local->quests.owners.contains(row.before);});
    const auto& original=consumed?consumed->local->quests:before.local->quests;
    if(!duplicate_quest_history::previous_owners(before.local->quests,original,restored,retired,port.local->quests,error)||
       !child_owner_seed(port.local->children,port.local->quests,error))return false;
    out.local=std::move(port.local);return true;
}catch(const std::exception& ex){return fail(error,std::string("Dungeon Building child publication exception: ")+ex.what());}
}
