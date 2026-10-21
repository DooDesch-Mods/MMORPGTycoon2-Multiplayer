namespace dungeon_building {
struct Local {
    a::Binding binding;std::string snapshot;
    host::WorldState world;uintptr_t random=0;
    dungeon_cells::Sample cells;
    std::vector<dungeon_input_authored::Scene> authored;
    std::vector<BuildingOwner> buildings;
    std::map<std::string,dungeon_palette::Sample> palettes;
    std::vector<zone_edit_read::Row> zones;
    duplicate_service_history::World services;
    std::map<uint64_t,std::string> service_keys;
    std::map<std::string,a::ServiceMembershipState> service_states;
    std::map<std::string,service_membership_reader::Local> service_owners;
    std::map<std::string,building_npc_lifecycle::Receipt> children;
    qr::Witness quests;quest_list::References references;
};
namespace {
bool current(Memory& memory,const a::Binding& binding,const std::string& snapshot,host::WorldState& world,std::string& error){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"Dungeon Building sample requires owned Character Mainthread");
    a::Context context;if(!a::selected_session().current(context,error))return false;
    if(context.snapshot_uuid!=snapshot)return fail(error,"Dungeon Building sample session snapshot differs");
    host::LookupApi api;host::NativeOrigin origin;
    if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
    host::Reader reader(memory.process,origin,state.image,state.thread,api);
    return reader.current(world,true,error)&&action_runtime::bound(world,binding,snapshot,error);
}
bool find(Memory& memory,uint64_t uid,uintptr_t& pointer,std::string& error){
    host::LookupApi api;if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
    const auto exception=lookup(api.find,uid,pointer);
    return !exception||fail(error,"Dungeon Building original full UID lookup SEH="+std::to_string(exception));
}
bool text(Memory& memory,uintptr_t pointer,std::string& out,std::string& error){
    return npc_quest_records::read_string(memory,pointer,0,"Dungeon Building original string",out,error);
}
struct ValueMemory {
    Memory& memory;
    bool read(uintptr_t pointer,void* out,size_t bytes,std::string& error){return memory.copy(pointer,out,bytes,error);}
    bool copy(uintptr_t pointer,void* out,size_t bytes,std::string& error){return memory.copy(pointer,out,bytes,error);}
};
struct Localized {
    Memory& memory;alignas(8) std::array<unsigned char,0x38> value{};
    bool get(uintptr_t object,uintptr_t& pointer,std::string& error){
        const auto exception=duplicate_history_value_calls::get_fluff(state.image,object,value.data());
        if(exception)return fail(error,"Dungeon Building original GetFluff SEH="+std::to_string(exception));
        pointer=reinterpret_cast<uintptr_t>(value.data());return true;
    }
    bool read(uintptr_t pointer,duplicate_localized_history::Value& out,std::string& error){
        const auto read_text=[&](uintptr_t p,std::string& result,std::string& cause){return text(memory,p,result,cause);};
        ValueMemory view{memory};return duplicate_localized_history::read(view,pointer,{100000,100000,32*1024*1024},read_text,out,error);
    }
    bool destroy(uintptr_t pointer,bool arguments,std::string& error){
        const auto exception=duplicate_history_value_calls::destroy_fluff(state.image,reinterpret_cast<void*>(pointer),arguments);
        return !exception||fail(error,"Dungeon Building original localized cleanup SEH="+std::to_string(exception));
    }
};
bool owned_value(Memory& memory,uintptr_t object,duplicate_building_values::OwnedKind kind,std::string& sha,std::string& error){
    const auto capture=[&](uintptr_t receiver,uintptr_t prepare,uintptr_t save,subscriber_lifecycle::Graph& graph,std::string& cause){
        dungeon_cells::ValueMemory view(memory);dungeon_cells::ValueCapture captured{view};subscriber_lifecycle::CaptureReceipt receipt;
        const subscriber_lifecycle::CaptureApi api{
            reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x9667c0),reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x9676a0),
            reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x89e580),reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x89e5f0),
            reinterpret_cast<subscriber_lifecycle::PrepareSave>(state.image+prepare),reinterpret_cast<subscriber_lifecycle::SaveRecord>(state.image+save)};
        live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);
        if(!subscriber_lifecycle::capture_original(api,reinterpret_cast<const void*>(receiver),dungeon_cells::inspect_value,&captured,receipt,cause))return false;
        graph=std::move(captured.graph);return true;
    };
    ValueMemory view{memory};return duplicate_building_values::live(view,state.image,object,kind,capture,sha,error);
}
bool building_value(Memory& memory,uintptr_t object,duplicate_building_history::Saved& out,std::string& error){
    ValueMemory view{memory};if(!duplicate_history_value_calls::code(view,state.image,error))return false;
    const auto read_text=[&](uintptr_t p,std::string& value,std::string& cause){return text(memory,p,value,cause);};
    const auto fluff=[&](uintptr_t p,std::string& sha,std::string& cause){Localized port{memory};duplicate_building_values::LocalizedReceipt receipt;
        return duplicate_building_values::localized(port,p,sha,receipt,cause);};
    const auto record=[&](uintptr_t p,duplicate_building_values::OwnedKind kind,std::string& sha,std::string& cause){return owned_value(memory,p,kind,sha,cause);};
    const auto theme=[&](uintptr_t p,std::string& value,std::string& cause){uintptr_t selected=0;
        const auto exception=duplicate_history_value_calls::theme(state.image,p,selected);
        if(exception||!selected)return fail(cause,"Dungeon Building original Theme absent/SEH="+std::to_string(exception));
        return text(memory,selected+0x20,value,cause);};
    const auto model=[&](uintptr_t p,uintptr_t& selected,std::string& cause){const auto exception=duplicate_history_value_calls::model(state.image,p,selected);
        return !exception||fail(cause,"Dungeon Building original Art model SEH="+std::to_string(exception));};
    const auto registered=[&](uint64_t uid,uintptr_t& pointer,std::string& cause){return find(memory,uid,pointer,cause);};
    return duplicate_building_history::live(view,state.image,object,read_text,fluff,record,theme,model,registered,out,error);
}
json shape(const a::TerrainShape& value){json paths=json::array();for(const auto& path:value){json points=json::array();
    for(const auto& point:path)points.push_back({point.x,point.z});paths.push_back(std::move(points));}return paths;}
bool same_census(const dungeon_scene_identity_runtime::Census& before,const dungeon_scene_identity_runtime::Census& after,std::string& error){
    const auto& x=before.world;const auto& y=after.world;
    if(x.game!=y.game||x.mode!=y.mode||x.state!=y.state||x.world!=y.world||x.map!=y.map||x.uuid!=y.uuid||x.sha256!=y.sha256||
       x.world_epoch!=y.world_epoch||x.generation!=y.generation||before.keys!=after.keys||before.pools.size()!=after.pools.size())
        return fail(error,"Dungeon Building current NativeOpen/scene/Gizmo ownership changed during sample");
    for(size_t i=0;i<before.pools.size();++i){const auto& left=before.pools[i];const auto& right=after.pools[i];
        if(left.scene.owner.key!=right.scene.owner.key||left.scene.owner.exterior_key!=right.scene.owner.exterior_key||
           left.scene.owner.exterior!=right.scene.owner.exterior||left.scene.owner.exterior_uid!=right.scene.owner.exterior_uid||
           left.scene.owner.dungeon!=right.scene.owner.dungeon||left.scene.dungeon_uid!=right.scene.dungeon_uid||
           left.scene.owner.shard!=right.scene.owner.shard||left.scene.owner.region!=right.scene.owner.region||
           left.scene.owner.instance!=right.scene.owner.instance||left.managers!=right.managers||left.local!=right.local||
           left.zones.members!=right.zones.members||left.scenery!=right.scenery)
            return fail(error,"Dungeon Building original scene/member reciprocal owner changed during sample");
        for(size_t type=0;type<4;++type)if(left.zones.arrays[type].members!=right.zones.arrays[type].members||
           left.zones.arrays[type].data!=right.zones.arrays[type].data||left.zones.arrays[type].count!=right.zones.arrays[type].count||
           left.zones.arrays[type].capacity!=right.zones.arrays[type].capacity)return fail(error,"Dungeon Building Zone owner arrays changed during sample");
    }
    return true;
}
bool buildings(Memory& memory,Local& local,std::map<std::string,json>& values,std::string& error){
    values.clear();std::map<uint64_t,std::string> names;std::map<uintptr_t,std::string> dungeons;
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return field(memory,p,offset,value,cause);};
    for(const auto& pool:local.cells.scenes.pools){const auto& owner=pool.scene.owner;
        const auto [exterior,inserted]=names.emplace(owner.exterior_uid,owner.exterior_key);
        if(exterior->second!=owner.exterior_key)return fail(error,"Dungeon Building exterior full UID aliases another key");
        dungeons.emplace(owner.dungeon,owner.exterior_key);
        if(inserted){BuildingOwner row;row.key=owner.exterior_key;row.scene="world";row.object=owner.exterior;row.uid=owner.exterior_uid;
            if(!at(row.object,0x170,row.shard,error)||!at(row.object,0x178,row.region,error)||!at(row.region,0x950,row.manager,error)||
               !building_value(memory,row.object,row.saved,error))return false;
            if(row.shard!=local.world.root||row.saved.uid!=row.uid||row.saved.root!=row.shard||row.saved.dungeon!=owner.dungeon)
                return fail(error,"Dungeon Building original exterior full saved owner differs");
            local.buildings.push_back(std::move(row));}
        uintptr_t manager=0,ap=0,shard=0,region=0;
        if(!at(owner.region,0x950,manager,error)||!at(manager,0,ap,error)||!at(manager,0x450,shard,error)||!at(manager,0x458,region,error))return false;
        if(ap!=state.image+0x17183c0||shard!=owner.shard||region!=owner.region)return fail(error,"Dungeon Building contained manager reciprocal scene differs");
        dungeon_scene_roster::Array members;if(!dungeon_scene_roster::array(at,manager+0x408,members,error))return false;
        for(const auto object:members.members){BuildingOwner row;row.scene=owner.key;row.object=object;row.manager=manager;row.shard=shard;row.region=region;
            uintptr_t actual=0,object_shard=0,object_region=0;
            if(!at(object,0x18,row.uid,error)||!find(memory,row.uid,actual,error)||!at(object,0x170,object_shard,error)||!at(object,0x178,object_region,error))return false;
            if(actual!=object||object_shard!=shard||object_region!=region)return fail(error,"Dungeon Building contained full UID/reciprocal owner differs");
            if(!remove_diagnostics::resolve_current_building_key(local.snapshot,object,row.uid,row.key,error)||!building_value(memory,object,row.saved,error))return false;
            if(row.saved.uid!=row.uid||row.saved.root!=shard||!names.emplace(row.uid,row.key).second)return fail(error,"Dungeon Building contained canonical/full UID owner repeats or differs");
            local.buildings.push_back(std::move(row));
        }
        if(!dungeon_scene_roster::unchanged(at,manager+0x408,members,error))return false;
        values.emplace(owner.key,json::array());
    }
    for(auto& row:local.buildings){const auto& saved=row.saved;std::string parent,dungeon;
        if(saved.parent_uid){const auto found=names.find(saved.parent_uid);
            if(found==names.end())return fail(error,"Dungeon Building parent full UID has no current canonical owner");parent=found->second;}
        if(saved.dungeon){const auto found=dungeons.find(saved.dungeon);
            if(found==dungeons.end())return fail(error,"Dungeon Building contained Dungeon link has no current exterior owner");dungeon=found->second;}
        native::BuildingState prop;if(!scenery_kiss::read_action_prop(row.uid,prop,error))return false;
        if(prop.prop.uid!=row.uid||(row.scene=="world"?(prop.dungeon_membership||prop.dungeon_instance_membership):(!prop.dungeon_membership&&!prop.dungeon_instance_membership)))
            return fail(error,"Dungeon Building typed current scene membership differs");
        row.state.type=prop.type;row.state.variant=prop.variant;row.state.x=prop.prop.x;row.state.y=prop.prop.y;row.state.z=prop.prop.z;
        row.state.rotation=prop.rotation;row.state.scale=prop.scale;row.state.alive=true;
        uint8_t active=2;if(!field(memory,row.object,0x7e1,active,error))return false;
        if(active>1)return fail(error,"Dungeon Building original active property outside bool domain");row.active=active!=0;
        // Full original Make values remain local for inverse verification. The
        // shared pose is the existing typed transform, without native padding.
        auto value=json::array({row.key,uint8_t(saved.form),saved.state,saved.user_set,row.active,saved.name,
            prop.type,prop.variant,prop.prop.x,prop.prop.y,prop.prop.z,prop.rotation,prop.scale,
            saved.theme,saved.fluff_sha,saved.layout_sha,saved.plan_sha,saved.bugs_sha,parent,dungeon});
        if(row.scene=="world")values.emplace(row.key,std::move(value));
        else values.at(row.scene).push_back(std::move(value));
    }
    return true;
}
bool services(Memory& memory,Local& local,std::map<std::string,std::string>& hashes,std::string& error){
    ValueMemory view{memory};hashes.clear();
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return field(memory,p,offset,value,cause);};
    const auto read_text=[&](uintptr_t p,std::string& value,std::string& cause){return text(memory,p,value,cause);};
    const auto registered=[&](uint64_t uid,uintptr_t& p,std::string& cause){return find(memory,uid,p,cause);};
    const auto destination=[&](uintptr_t p,std::string& key,std::string& cause){uint64_t uid=0;
        if(!field(memory,p,0x18,uid,cause))return false;
        return remove_diagnostics::resolve_current_building_key(local.snapshot,p,uid,key,cause);};
    std::vector<BuildingOwner> owners=local.buildings;std::set<uint64_t> seen;
    for(const auto& row:owners)seen.insert(row.uid);
    json bindings;if(!remove_diagnostics::bindings(bindings,error))return false;
    for(const auto& binding:bindings.at("bindings")){
        if(binding.at("revoked").get<bool>())continue;
        if(binding.at("unknown").get<bool>()||binding.at("snapshot_id")!=local.snapshot)return fail(error,"Dungeon Building complete Service registry unknown or foreign");
        const auto uid=binding.at("local_uid").get<uint64_t>();if(seen.contains(uid))continue;
        uintptr_t object=0,ap=0;if(!registered(uid,object,error)||!field(memory,object,0,ap,error))return false;
        if(ap!=state.image+0x16d93c0)continue;
        BuildingOwner row;row.uid=uid;row.object=object;row.key=binding.at("record_key").get<std::string>();
        seen.insert(uid);owners.push_back(std::move(row));
    }
    for(const auto& building:owners){auto& state_value=local.service_states[building.key];auto& owner=local.service_owners[building.key];
        if(!local.service_keys.emplace(building.uid,building.key).second)return fail(error,"Dungeon Building complete Service owner UID repeats");
        if(!service_membership_reader::sample(at,read_text,destination,state.image,building.object,state_value,owner,error))return false;
        auto& saved=local.services[building.uid];
        for(size_t i=0;i<owner.rows.size();++i){std::array<int32_t,2> price{};duplicate_service_history::Saved row;
            if(!duplicate_service_history::effective_price(state_value,owner,i,price,error)||
               !duplicate_service_history::capture(view,state.image,owner.rows[i].object,building.object,price,registered,row,error))return false;
            saved.push_back(std::move(row));}
        duplicate_service_history::World single{{building.uid,saved}};std::map<uint64_t,a::ServiceMembershipState> values{{building.uid,state_value}};
        const std::map<uint64_t,std::string> keys{{building.uid,building.key}};
        if(!duplicate_history_effects::services(single,values,keys,hashes[building.key],error))return false;
    }
    return true;
}
bool children(Memory& memory,Local& local,std::map<std::string,json>& values,std::string& error){
    values.clear();
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return field(memory,p,offset,value,cause);};
    const auto registered=[&](uint64_t uid,uintptr_t& p,std::string& cause){return find(memory,uid,p,cause);};
    uintptr_t manager=0;character_definition_identity::Census definitions;
    if(!at(uintptr_t(local.world.origin.state),0x168,manager,error)||
       !character_definition_identity::read(at,state.image,uintptr_t(local.world.origin.state),manager,definitions,error))return false;
    std::vector<npc_saved_identity::Current> aliases;if(!npc_saved_identity_runtime::read_current(aliases,error))return false;
    std::map<uintptr_t,std::string> external;
    external.emplace(local.world.root,"root_shard");
    for(const auto& owner:local.buildings)external.emplace(owner.object,owner.key);
    for(const auto& pool:local.cells.scenes.pools){const auto& owner=pool.scene.owner;
        external.emplace(owner.shard,owner.key+"/shard");external.emplace(owner.region,owner.key+"/region");
        external.emplace(owner.dungeon,owner.exterior_key+"/dungeon");if(owner.instance)external.emplace(owner.instance,owner.key);}
    for(const auto& definition:definitions.definitions)if(!definition.deleted)external.emplace(definition.object,definition.key);
    for(const auto& alias:aliases)external.emplace(alias.row.local_pointer,alias.key);
    for(const auto& [pointer,owner]:local.quests.owners){external.emplace(pointer,owner.key);
        for(size_t i=0;i<owner.state.local.size();++i)external.emplace(owner.state.local[i].quest,owner.key+"/quest/"+std::to_string(i));}
    const dungeon_input_codes::Pin npc_functions[]{
        {0x10ecae0,87,"52f1b850ad131c46f7d249b5b6365e5c289a571e83c0a6d4996c36647dd36bc2"},
        {0x10ecb40,409,"4a5445004eb0a955a6b8fb7a84c77341f2001af6fa5fa20489039ebe472c621e"}};
    for(const auto& pin:npc_functions){std::vector<unsigned char> bytes(pin.bytes);
        if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;
        if(!hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))
            return fail(error,"Dungeon Building original child Record body differs: "+error);}
    for(const auto& building:local.buildings){building_npc_lifecycle::Receipt receipt;receipt.parent.key=building.key;
        receipt.parent.object=building.object;receipt.parent.uid=building.uid;
        receipt.parent.scene=building.scene;
        if(!at(building.region,0x834,receipt.parent.saved_region,error))return false;
        receipt.parent.make_form=building.saved.form==duplicate_building_history::Form::Functional?building_npc_lifecycle::MakeForm::Functional:
            building.saved.form==duplicate_building_history::Form::Modular?building_npc_lifecycle::MakeForm::Modular:building_npc_lifecycle::MakeForm::Art;
        npc_census::State census;npc_census::Row row;
        if(!npc_census::read(memory,state.image,building.region,census,error)||
           !building_npc_owner::read(at,registered,{state.image,building.shard,building.region},receipt.parent,census,row,receipt.present,error))return false;
        if(receipt.present){receipt.child.row=row;receipt.child.saved_region=receipt.parent.saved_region;
            receipt.child.key=building_npc_key::make(building.key);
            for(const auto& alias:aliases)if(alias.row.local_pointer==row.local_pointer||alias.row.local_uid==row.local_uid){
                if(alias.row.local_pointer!=row.local_pointer||alias.row.local_uid!=row.local_uid)
                    return fail(error,"Dungeon Building child current alias full UID differs");receipt.child.key=alias.key;}
            uintptr_t definition=0;if(!at(row.local_pointer,0x498,definition,error))return false;
            const character_definition_identity::Definition* selected=nullptr;
            for(const auto& entry:definitions.definitions)if(entry.object==definition)selected=&entry;
            if(!selected||selected->deleted||selected->kind!=row.character_type||selected->id!=row.character_slot)
                return fail(error,"Dungeon Building child actual Character definition differs");
            uintptr_t actual=0;if(!registered(selected->uid,actual,error))return false;
            if(actual!=selected->object)return fail(error,"Dungeon Building child Character full UID registry differs");
            receipt.original_character_uid=selected->uid;receipt.original_display_name=row.name;
            if(!at(row.local_pointer,0x170,receipt.original_shard,error)||!at(row.local_pointer,0x178,receipt.original_region,error)||
               !at(row.local_pointer,0x100,receipt.original_transform,error))return false;
            // The existing full Quest witness owns all authored records. The
            // actual child transform stays local for original Make verification.
            const auto quest_owner=local.quests.owners.find(row.local_pointer);
            if(row.type==0){if(quest_owner==local.quests.owners.end()&&row.quest_count)
                    return fail(error,"Dungeon Building child Quest owner absent from complete global witness");
                if(quest_owner!=local.quests.owners.end()){receipt.child.key=quest_owner->second.key;receipt.original_quests=quest_owner->second.state;
                    receipt.original_destination_uids=quest_owner->second.destination_uids;}}
            const auto data=json::array({row.type,row.character_type,row.character_slot,row.level,row.name});
            dungeon_cells::ValueMemory capture_memory(memory);dungeon_cells::ValueCapture captured{capture_memory};subscriber_lifecycle::CaptureReceipt cleanup;
            const subscriber_lifecycle::CaptureApi api{
                reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x9667c0),reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x9676a0),
                reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x89e580),reinterpret_cast<subscriber_lifecycle::Construct>(state.image+0x89e5f0),
                reinterpret_cast<subscriber_lifecycle::PrepareSave>(state.image+0x10ecae0),reinterpret_cast<subscriber_lifecycle::SaveRecord>(state.image+0x10ecb40)};
            {live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);
                if(!subscriber_lifecycle::capture_original(api,reinterpret_cast<const void*>(row.local_pointer),dungeon_cells::inspect_value,&captured,cleanup,error))return false;}
            const auto canonical=[&](uintptr_t p,std::string& key,std::string& cause){
                if(p==row.local_pointer){key=receipt.child.key;return true;}
                const auto found=external.find(p);if(found!=external.end()){key=found->second;return true;}
                bool dungeon=false;return npc_quest_target_runtime::bind_current_destination(p,key,dungeon,cause);};
            duplicate_records::Witness complete;if(!duplicate_records::summarize(captured.graph,row.local_pointer,canonical,complete,error))return false;
            receipt.original_record_sha=complete.sha;
            values.emplace(building.key,json::array({true,receipt.child.key,data}));
        }else values.emplace(building.key,json::array({false}));
        building_npc_lifecycle::Receipt retained;bool found=false,retired=false;
        if(!building_npc_lifecycle::retained_receipt(building.key,retained,found,retired,error))return false;
        if(found){
            if(retired||retained.parent.object!=receipt.parent.object||retained.parent.uid!=receipt.parent.uid||
               retained.parent.scene!=receipt.parent.scene||retained.present!=receipt.present||
               (receipt.present&&(retained.child.key!=receipt.child.key||retained.child.row.local_pointer!=receipt.child.row.local_pointer||
                                 retained.child.row.local_uid!=receipt.child.row.local_uid)))
                return fail(error,"Dungeon Building sampled child differs from its confirmed original lifecycle");
            receipt.lease=retained.lease;receipt.inverse_child_uid=retained.inverse_child_uid;
        }
        local.children.emplace(building.key,std::move(receipt));
    }
    return true;
}
bool once(Memory& memory,const a::Binding& binding,const std::string& snapshot,Snapshot& output,std::string& error,const Snapshot* previous){
    output={};auto next=std::make_shared<Local>();next->binding=binding;next->snapshot=snapshot;
    if(!current(memory,binding,snapshot,next->world,error)||!field(memory,state.image,0x164b250,next->random,error)||
       !field(memory,next->random,0,output.value.random,error)||!dungeon_scene_identity_runtime::read_current(next->cells.scenes,error)||
       !dungeon_cells::monster_members(memory,next->cells.scenes,next->cells.monsters,false,error)||
       !dungeon_cells::authored(memory,binding,snapshot,next->cells,error,&next->authored))return false;
    if(next->cells.scenes.world.world_epoch!=binding.native_epoch||next->cells.scenes.world.generation!=binding.native_generation||
       next->cells.scenes.world.world!=next->world.origin.world||next->cells.scenes.world.map!=next->world.origin.map)
        return fail(error,"Dungeon Building complete scene census differs from current native binding");
    const qr::Witness* old=nullptr;if(previous){if(!previous->local||previous->local->binding!=binding||previous->local->snapshot!=snapshot)
        return fail(error,"Dungeon Building previous Quest witness belongs to another snapshot/binding");old=&previous->local->quests;}
    if(!read_remove_quests(memory,binding,snapshot,0,{},next->quests,error,old)||
       !quest_list::references(memory,binding,snapshot,next->quests,next->references,error)||
       !dungeon_building_sample::quests(next->quests,output.value.quests,error)||
       !quest_bridge::sha256(next->references.rows.dump(),output.value.references,error))return false;
    std::vector<world_zone_runtime::Current> zone_owners;if(!world_zone_runtime::read_current(binding,snapshot,zone_owners,error))return false;
    std::erase_if(zone_owners,[](const auto& row){return row.scene_key=="world";});
    const auto registered=[&](uint64_t uid,uintptr_t& object,std::string& cause){return find(memory,uid,object,cause);};
    ValueMemory view{memory};if(!zone_edit_read::read(view,state.image,zone_owners,registered,next->zones,error))return false;
    std::map<std::string,json> building_values;if(!buildings(memory,*next,building_values,error))return false;
    std::map<std::string,std::string> service_values;std::map<std::string,json> child_values;
    if(!services(memory,*next,service_values,error)||!children(memory,*next,child_values,error))return false;
    std::map<uint64_t,a::ServiceMembershipState> all_service_values;
    for(const auto& [uid,key]:next->service_keys)all_service_values.emplace(uid,next->service_states.at(key));
    std::string complete_services;if(!duplicate_history_effects::services(next->services,all_service_values,next->service_keys,complete_services,error))return false;
    for(const auto& owner:next->buildings){auto& list=building_values.at(owner.scene=="world"?owner.key:owner.scene);
        if(owner.scene=="world")list.push_back(json::array({service_values.at(owner.key),child_values.at(owner.key)}));
        else for(auto& row:list)if(row.at(0)==owner.key)row.push_back(json::array({service_values.at(owner.key),child_values.at(owner.key)}));}
    std::map<std::string,std::string> configs;std::vector<dungeon_building_sample::Scene> rows;
    for(const auto& authored:next->authored){const auto& key=authored.dungeon;
        if(!configs.contains(key)){a::DungeonPaletteIntent target;target.key=key;dungeon_palette::Sample palette;
            if(!dungeon_palette::sample(memory,binding,snapshot,target,palette,error))return false;
            const auto& sound=palette.value.protected_state;json instances=json::array();
            for(const auto& instance:sound.dungeon.instances)instances.push_back({instance.index,instance.started});
            std::string layout,sha;if(!dungeon_palette::digest(palette.value,layout,error)||
               !quest_bridge::sha256(json::array({layout,sound.dungeon.desired,instances,sound.light,sound.atmosphere,building_values.at(key),complete_services}).dump(),sha,error))return false;
            configs.emplace(key,std::move(sha));next->palettes.emplace(key,std::move(palette));}
        json zones=json::array();for(const auto& zone:next->zones)if(zone.owner.scene_key==authored.key){const auto& value=zone.content;
            zones.push_back(json::array({zone.owner.key,value.type,value.density,value.level,value.monster_type,value.constructed_day,value.sections,shape(zone.shape)}));}
        dungeon_building_sample::Scene row;row.authored=authored;
        const auto found=building_values.find(authored.key);if(found==building_values.end())return fail(error,"Dungeon Building complete scene Building proof absent");
        if(!quest_bridge::sha256(zones.dump(),row.zones,error)||!quest_bridge::sha256(found->second.dump(),row.buildings,error))return false;
        rows.push_back(std::move(row));
    }
    if(!dungeon_building_sample::compose(rows,configs,output.value.authored,error)||!a::valid_dungeon_building_state(output.value))
        return fail(error,error.empty()?"Dungeon Building complete original authored state invalid":error);
    host::WorldState after;std::array<uint64_t,4> random{};uintptr_t source=0;
    if(!current(memory,binding,snapshot,after,error)||!field(memory,state.image,0x164b250,source,error)||!field(memory,source,0,random,error))return false;
    if(!action_runtime::same_world(next->world,after)||source!=next->random||random!=output.value.random)
        return fail(error,"Dungeon Building current World/RNG changed during complete sample");
    output.local=std::move(next);return true;
}
struct CapturePort {
    Memory memory;const a::Binding& binding;const std::string& snapshot;
    bool read(Snapshot& out,std::string& error,const Snapshot* previous){return once(memory,binding,snapshot,out,error,previous);}
    bool same(const Snapshot& before,const Snapshot& after,std::string& error){
        if(!before.local||!after.local)return fail(error,"Dungeon Building complete sample omitted local ownership witness");
        if(!same_census(before.local->cells.scenes,after.local->cells.scenes,error))return false;
        if(before.value!=after.value||before.local->buildings!=after.local->buildings||before.local->zones!=after.local->zones||
           before.local->services!=after.local->services||before.local->service_owners!=after.local->service_owners||
           before.local->cells.monsters!=after.local->cells.monsters||before.local->references.local!=after.local->references.local||
           before.local->references.manager!=after.local->references.manager||before.local->references.data!=after.local->references.data)
            return fail(error,"Dungeon Building authored values/Quest references/current owners changed during complete sample");
        return true;
    }
};
struct SeedPort {
    Memory memory;const a::Binding& binding;const std::string& snapshot;host::WorldState world;uintptr_t source=0;
    bool before(std::string& error){const auto controller=typed_action_runtime::controller_status();
        if(!controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false))
            return fail(error,"Dungeon Building Host RNG adoption requires exact pending ordered Controller");
        return current(memory,binding,snapshot,world,error)&&field(memory,state.image,0x164b250,source,error)&&
            (source!=0||fail(error,"Dungeon Building global RNG receiver absent"));
    }
    bool write(const std::array<uint64_t,4>& desired,std::string& error){const auto exception=action_runtime::seed(state.image,desired);
        return !exception||fail(error,"Dungeon Building Host RNG adoption SEH="+std::to_string(exception));}
    bool after(const std::array<uint64_t,4>& desired,std::string& error){host::WorldState now;uintptr_t pointer=0;std::array<uint64_t,4> actual{};
        if(!current(memory,binding,snapshot,now,error)||!field(memory,state.image,0x164b250,pointer,error)||!field(memory,pointer,0,actual,error))return false;
        return (action_runtime::same_world(world,now)&&source==pointer&&actual==desired)||fail(error,"Dungeon Building original RNG full words/owner readback differs");}
};
}
bool capture(const a::Binding& binding,const std::string& snapshot,Snapshot& out,std::string& error,const Snapshot* previous)try{
    CapturePort port{{},binding,snapshot};return dungeon_building_sample::capture(port,out,error,previous);
}catch(const std::exception& exception){out={};return fail(error,std::string("Dungeon Building sample boundary: ")+exception.what());}
bool seed(const a::Binding& binding,const std::string& snapshot,const std::array<uint64_t,4>& desired,std::string& error){
    SeedPort port{{},binding,snapshot};return dungeon_building_sample::seed(port,desired,error);
}
bool read_quests(const Snapshot& snapshot,qr::Witness& out,json& references,std::string& error){
    if(!snapshot.local)return fail(error,"Dungeon Building retained Quest sample unavailable");
    out=snapshot.local->quests;references=snapshot.local->references.rows;error.clear();return true;
}
bool read_owners(const Snapshot& snapshot,dungeon_scene_identity_runtime::Census& census,std::vector<BuildingOwner>& out,std::string& error){
    if(!snapshot.local)return fail(error,"Dungeon Building retained native owner sample unavailable");
    census=snapshot.local->cells.scenes;out=snapshot.local->buildings;error.clear();return true;
}
bool building_inverse(const Snapshot& snapshot,uintptr_t node,const std::string& key,std::string& error){
    if(!snapshot.local)return fail(error,"Dungeon Building original Make validation lacks retained sample");
    const auto& local=*snapshot.local;
    const auto found=std::find_if(local.buildings.begin(),local.buildings.end(),[&](const auto& row){return row.key==key;});
    if(found==local.buildings.end())return fail(error,"Dungeon Building original Make target is absent from complete captured owners");
    Memory memory;host::WorldState world;if(!current(memory,local.binding,local.snapshot,world,error))return false;
    if(!action_runtime::same_world(local.world,world))return fail(error,"Dungeon Building original Make sample world changed");
    ValueMemory view{memory};if(!duplicate_history_value_calls::code(view,state.image,error))return false;
    const auto read_text=[&](uintptr_t p,std::string& value,std::string& cause){return text(memory,p,value,cause);};
    const auto localized=[&](uintptr_t p,std::string& sha,std::string& cause){duplicate_localized_history::Value value;
        return duplicate_localized_history::read(view,p,{100000,100000,32*1024*1024},read_text,value,cause)&&
            duplicate_localized_history::summarize(value,sha,cause);};
    const auto record=[&](uintptr_t p,bool stored,std::string& sha,std::string& cause){
        if(stored){dungeon_cells::ValueMemory saved(memory);return duplicate_building_values::stored(saved,p,{100000,1000000,32*1024*1024,100000},sha,cause);}
        uintptr_t ap=0;if(!field(memory,p,0,ap,cause))return false;
        using Kind=duplicate_building_values::OwnedKind;
        if(ap==state.image+0x1712a60)return owned_value(memory,p,Kind::Layout,sha,cause);
        if(ap==state.image+0x16fea40)return owned_value(memory,p,Kind::Plan,sha,cause);
        return fail(cause,"Dungeon Building original Make owned value is not Layout or Plan");};
    const auto registered=[&](uint64_t uid,uintptr_t& object,std::string& cause){return find(memory,uid,object,cause);};
    return duplicate_building_history::inverse(view,state.image,node,found->saved,read_text,localized,record,registered,error);
}
bool related_inverse(const Snapshot& snapshot,uintptr_t node,uintptr_t addresspoint,std::string& error){
    if(!snapshot.local)return fail(error,"Dungeon Building related inverse lacks retained sample");
    const auto& local=*snapshot.local;Memory memory;host::WorldState world;
    if(!current(memory,local.binding,local.snapshot,world,error))return false;
    if(!action_runtime::same_world(local.world,world))return fail(error,"Dungeon Building related inverse current world differs");
    uintptr_t ap=0,method=0;uint64_t uid=0;
    if(!field(memory,node,0,ap,error)||!field(memory,node,0xb8,uid,error)||!field(memory,ap,0x10,method,error))return false;
    if(ap!=addresspoint)return fail(error,"Dungeon Building related inverse actual class differs");
    const auto registered=[&](uint64_t value,uintptr_t& object,std::string& cause){return find(memory,value,object,cause);};
    if(ap==state.image+0x1737830){ValueMemory view{memory};const duplicate_service_history::Saved* saved=nullptr;
        for(const auto& [owner,rows]:local.services){(void)owner;for(const auto& row:rows)if(row.uid==uid){
            if(saved)return fail(error,"Dungeon Building related Service inverse full UID repeats");saved=&row;}}
        return saved?duplicate_service_history::inverse(view,state.image,node,*saved,registered,error):
            fail(error,"Dungeon Building related Service inverse has no exact current captured owner");}
    if(ap==state.image+0x1713330){const qr::Owner* saved=nullptr;
        for(const auto& [pointer,owner]:local.quests.owners){(void)pointer;if(owner.uid==uid){
            if(saved)return fail(error,"Dungeon Building Quest inverse full owner UID repeats");saved=&owner;}}
        if(method!=state.image+0x563f50)return fail(error,"Dungeon Building Quest inverse original method differs");
        return saved?qr::inverse(memory,state.image,node,*saved,error):fail(error,"Dungeon Building Quest inverse owner absent from complete current witness");}
    if(ap==state.image+0x16de8a0){const building_npc_lifecycle::Receipt* saved=nullptr;
        for(const auto& [key,child]:local.children){(void)key;if(child.present&&child.child.row.local_uid==uid){
            if(saved)return fail(error,"Dungeon Building child inverse full UID repeats");saved=&child;}}
        if(!saved)return fail(error,"Dungeon Building child inverse has no complete captured original owner");
        if(method!=state.image+0x556ff0)return fail(error,"Dungeon Building child inverse original method differs");
        auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return field(memory,p,offset,value,cause);};
        const auto read_text=[&](uintptr_t p,size_t offset,std::string& value,std::string& cause){return npc_quest_records::read_string(memory,p,offset,"Dungeon child MakeNPC name",value,cause);};
        building_npc_lifecycle::MakeNPCInverse receipt;
        if(!building_npc_owner::make_npc_inverse(at,read_text,state.image,node,*saved,receipt,error)||
           !building_npc_owner::inverse_uid_set(at,node+8,receipt.produced_uid,error)||
           !building_npc_owner::inverse_uid_set(at,node+0x30,receipt.required_character_uid,error))return false;
        uintptr_t object=0;if(!registered(uid,object,error))return false;
        return !object||fail(error,"Dungeon Building child inverse target full UID remains live");}
    return fail(error,"Dungeon Building related inverse is not an original supported Service/Quest/NPC payload");
}
bool read_children(const Snapshot& snapshot,std::map<std::string,building_npc_lifecycle::Receipt>& out,std::string& error){
    if(!snapshot.local)return fail(error,"Dungeon Building child original sample unavailable");
    out=snapshot.local->children;error.clear();return true;
}
bool read_services(const Snapshot& snapshot,duplicate_service_history::World& out,std::map<uint64_t,std::string>& keys,std::string& error){
    if(!snapshot.local)return fail(error,"Dungeon Building retained Service sample unavailable");
    out=snapshot.local->services;keys=snapshot.local->service_keys;error.clear();return true;
}
bool quest_seed(const Snapshot& current_snapshot,const Snapshot& original,const std::vector<duplicate_quest_history::OwnerRebind>& restored,
                const std::set<uintptr_t>& retired,Snapshot& out,std::string& error){
    out={};if(!current_snapshot.local||!original.local||current_snapshot.local->binding!=original.local->binding||
       current_snapshot.local->snapshot!=original.local->snapshot)return fail(error,"Dungeon Building Quest previous-owner seed lacks exact same-world original samples");
    auto local=std::make_shared<Local>(*current_snapshot.local);
    if(!duplicate_quest_history::previous_owners(current_snapshot.local->quests,original.local->quests,restored,retired,local->quests,error))return false;
    out.value=current_snapshot.value;out.local=std::move(local);return true;
}
}

#include "dungeon_building_children_runtime.inc"
