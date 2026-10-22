namespace building_child_detail {
namespace child=building_npc_lifecycle;
namespace owner=building_npc_owner;
namespace host=build_actions::private_native::host;
bool has_region(int32_t id){for(const auto& [key,bound]:building_children){(void)key;if(!bound.retired&&bound.receipt.present&&bound.receipt.parent.saved_region==id)return true;}return false;}
bool lease(const child::Lease& expected,opened::CurrentWorld& world,std::string& error){
    const auto status=typed_action_runtime::controller_status();
    if(!building_npc_key::action("action:"+expected.request_uuid)||
       !building_npc_key::action("action:"+expected.duplicate_uuid)||
       !status.value("initialized",false)||!status.value("started",false)||status.value("locked",true)||
       !status.value("pending",false)||status.value("lobby_retiring",true)||
       status.value("action_uuid",std::string{})!=expected.request_uuid||!status.value("cause",std::string{}).empty())
        return q::fail(error,"Building child NPC requires the exact pending unlocked Controller request");
    if(!current(world,error))return false;
    return (expected.snapshot==uuid&&expected.epoch==world.world_epoch&&expected.generation==world.generation)||
        q::fail(error,"Building child NPC pending snapshot/native epoch/generation differs");
}
struct Lookup {
    host::LookupApi api;
    bool initialize(std::string& error){host::ProcessMemory process;
        return host::resolve_lookup(process,image,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",api,error);}
    bool operator()(uint64_t uid,uintptr_t& pointer,std::string& error){pointer=0;const auto code=find_call(api.find,uid,pointer);
        return !code||q::fail(error,"Building child NPC full UID lookup exception="+std::to_string(code));}
};
bool census(const opened::CurrentWorld& world,int32_t id,uintptr_t& region,npc_census::State& out,std::string& error){
    const auto map=reinterpret_cast<uintptr_t>(world.map);
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return q::at(memory,p,offset,value,cause);};
    return region_saved_identity::resolve(image,map,map+0x1c0,id,0,region,at,error)&&npc_census::read(memory,image,region,out,error);
}
template<class Find>
bool scene_census(const opened::CurrentWorld& world,const child::Parent& parent,Find& find,
                  owner::Scope& scope,npc_census::State& out,std::string& error){
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return q::at(memory,p,offset,value,cause);};
    const auto root=[&](int32_t id,uintptr_t& region,std::string& cause){
        const auto map=reinterpret_cast<uintptr_t>(world.map);
        return region_saved_identity::resolve(image,map,map+0x1c0,id,0,region,at,cause);};
    const auto scenes=[&](std::vector<dungeon_scene_roster::Scene>& rows,std::string& cause){
        dungeon_scene_identity_runtime::World current_world;
        if(!dungeon_scene_identity_runtime::read_scene_roster(current_world,rows,cause))return false;
        return (current_world.uuid==world.uuid&&current_world.sha256==world.sha256&&current_world.generation==world.generation&&
            current_world.world_epoch==world.world_epoch&&current_world.world==world.world&&current_world.map==world.map)||
            q::fail(cause,"Building child NPC complete scene roster World differs");};
    const auto held=[&](uintptr_t object,std::string& key,bool& matched,std::string& cause,std::string* scene){
        return typed_action_runtime::resolve_held_dungeon_destination(object,key,matched,cause,scene);};
    if(!owner::scope(at,root,scenes,held,find,image,reinterpret_cast<uintptr_t>(world.map)+0x1c0,parent,scope,error))return false;
    return npc_census::read(memory,image,scope.region,out,error);
}
template<class Find>
bool retirement_census(const opened::CurrentWorld& world,const child::Receipt& before,Find& find,
                       owner::Scope& scope,npc_census::State& rows,std::string& error){
    if(before.parent.scene=="world")return scene_census(world,before.parent,find,scope,rows,error);
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return q::at(memory,p,offset,value,cause);};
    std::vector<dungeon_scene_roster::Scene> scenes;dungeon_scene_identity_runtime::World current_world;
    if(!dungeon_scene_identity_runtime::read_scene_roster(current_world,scenes,error))return false;
    if(current_world.uuid!=world.uuid||current_world.sha256!=world.sha256||current_world.generation!=world.generation||
       current_world.world_epoch!=world.world_epoch||current_world.world!=world.world||current_world.map!=world.map)
        return q::fail(error,"Building child NPC retirement scene roster World differs");
    if(!owner::retirement_scope(at,scenes,image,before,scope,error))return false;
    return npc_census::read(memory,image,scope.region,rows,error);
}
bool read(const child::Lease& expected,const child::Parent& parent,opened::CurrentWorld& world,
          npc_census::State& rows,npc_census::Row& row,bool& present,Lookup& find,std::string& error){
    owner::Scope scope;if(!lease(expected,world,error)||!find.initialize(error)||!scene_census(world,parent,find,scope,rows,error))return false;
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return q::at(memory,p,offset,value,cause);};
    return owner::read(at,find,scope,parent,rows,row,present,error);
}
bool same_lease(const child::Lease& a,const child::Lease& b){
    return a.snapshot==b.snapshot&&a.duplicate_uuid==b.duplicate_uuid&&a.epoch==b.epoch&&a.generation==b.generation;
}
bool same_before(const child::Receipt& a,const child::Receipt& b){
    if(a.parent.object!=b.parent.object||a.parent.uid!=b.parent.uid||a.parent.saved_region!=b.parent.saved_region||a.parent.scene!=b.parent.scene||
       a.original_record_sha!=b.original_record_sha||a.original_character_uid!=b.original_character_uid||
       a.original_shard!=b.original_shard||a.original_region!=b.original_region||a.original_transform!=b.original_transform||
       a.original_display_name!=b.original_display_name||a.original_quests.records!=b.original_quests.records||
       a.original_destination_uids!=b.original_destination_uids||a.original_quests.local.size()!=b.original_quests.local.size())return false;
    for(size_t i=0;i<a.original_quests.local.size();++i){const auto& x=a.original_quests.local[i];const auto& y=b.original_quests.local[i];
        if(x.quest!=y.quest||x.owner!=y.owner||x.destination!=y.destination||x.npc_list_member!=y.npc_list_member||
           x.owner_weak_confirmed!=y.owner_weak_confirmed||x.destination_weak_confirmed!=y.destination_weak_confirmed||
           x.manager_member!=y.manager_member||x.task!=y.task)return false;
    }return true;
}
bool binding(const child::Lease& expected,const child::Receipt& before,ChildBinding*& out,std::string& error){
    out=nullptr;const auto found=building_children.find(before.parent.key);
    if(found==building_children.end()||!same_lease(expected,before.lease)||!same_lease(expected,found->second.receipt.lease)||
       found->second.receipt.child.key!=before.child.key||found->second.receipt.present!=before.present||!same_before(found->second.receipt,before)||
       (before.present&&(found->second.receipt.child.row.local_pointer!=before.child.row.local_pointer||
                         found->second.receipt.child.row.local_uid!=before.child.row.local_uid)))
        return q::fail(error,"Building child NPC history lacks its exact retained canonical parent/child receipt");
    out=&found->second;return true;
}
bool code(std::string& error){
    struct Pin {uintptr_t rva;size_t size;const char* sha;};
    const Pin pins[]{
        {0x9667c0,181,"f061a1fe2ca42b75546e22e95981092d682869382e859ed5b10af7ecd80ba6c9"},
        {0x9676a0,148,"7f6e66c76e2be9e2d7f43ab04d8721cc060f16ddcd111a422605af135ab1bb4a"},
        {0x89e580,106,"fc9d17c2e62785b1cfd9e4bb59ae1568cb603f1e232cb30ce48d429581f73646"},
        {0x89e5f0,151,"0808e0234ca17c59c1fd755944f300a0145c52953160aa8231eef3006665e690"},
        {0x10ecae0,87,"52f1b850ad131c46f7d249b5b6365e5c289a571e83c0a6d4996c36647dd36bc2"},
        {0x10ecb40,409,"4a5445004eb0a955a6b8fb7a84c77341f2001af6fa5fa20489039ebe472c621e"}};
    for(const auto& pin:pins){std::vector<unsigned char> bytes(pin.size);
        if(!memory.read(image+pin.rva,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=pin.sha)return q::fail(error,"Building child NPC original capture helper full body differs rva="+std::to_string(pin.rva));
    }return true;
}
struct GraphMemory final:subscriber_lifecycle::Memory {
    bool read(uintptr_t p,void* out,size_t bytes,std::string& error)override{return memory.read(p,out,bytes,error);}
};
struct Capture {GraphMemory memory;subscriber_lifecycle::Graph graph;};
bool inspect(void* user,const void* record,const void* context,std::string& error){auto& value=*static_cast<Capture*>(user);
    return subscriber_lifecycle::read_graph(value.memory,reinterpret_cast<uintptr_t>(record),reinterpret_cast<uintptr_t>(context),
        {100000,1000000,32*1024*1024,100000},value.graph,error);
}
bool witness(const opened::CurrentWorld& world,const child::OwnerResolver& external,child::Receipt& receipt,std::string& error){
    if(!receipt.present){error.clear();return true;}
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return q::at(memory,p,offset,value,cause);};
    character_definition_identity::Census definitions;uintptr_t manager=0,type=0,ap=0,name_method=0;
    if(!at(reinterpret_cast<uintptr_t>(world.state),0x168,manager,error)||
       !character_definition_identity::read(at,image,reinterpret_cast<uintptr_t>(world.state),manager,definitions,error)||
       !at(receipt.child.row.local_pointer,0x498,type,error)||!at(receipt.child.row.local_pointer,0,ap,error)||
       !at(ap,0xa8,name_method,error))return false;
    const character_definition_identity::Definition* definition=nullptr;
    for(const auto& candidate:definitions.definitions)if(candidate.object==type)definition=&candidate;
    if(!definition||definition->deleted||!definition->uid||!character_definition_identity::matches_actor(*definition,receipt.child.row)||name_method!=image+0xe3f530)
        return q::fail(error,"Building child NPC current Character definition/virtual name owner differs");
    Lookup lookup;uintptr_t registered=0;
    if(!lookup.initialize(error)||!lookup(definition->uid,registered,error))return false;
    if(registered!=definition->object)return q::fail(error,"Building child NPC Character definition full UID registry differs");
    receipt.original_character_uid=definition->uid;receipt.original_display_name=receipt.child.row.name;
    if(!at(receipt.child.row.local_pointer,0x170,receipt.original_shard,error)||
       !at(receipt.child.row.local_pointer,0x178,receipt.original_region,error)||
       !at(receipt.child.row.local_pointer,0x100,receipt.original_transform,error)||!code(error))return false;
    const auto canonical=[&](uintptr_t p,std::string& key,std::string& cause){
        if(p==receipt.child.row.local_pointer){key=receipt.child.key;return true;}
        if(p==receipt.parent.object){key=receipt.parent.key;return true;}
        for(const auto& candidate:definitions.definitions)if(candidate.object==p){
            if(candidate.deleted||!candidate.uid)return q::fail(cause,"Building child NPC Record references a retired Character definition");
            key=candidate.key;return true;
        }
        return external?external(p,key,cause):q::fail(cause,"Building child NPC original external Record owner has no canonical resolver");
    };
    Capture captured;subscriber_lifecycle::CaptureReceipt cleanup;
    const subscriber_lifecycle::CaptureApi api{
        reinterpret_cast<subscriber_lifecycle::Construct>(image+0x9667c0),reinterpret_cast<subscriber_lifecycle::Construct>(image+0x9676a0),
        reinterpret_cast<subscriber_lifecycle::Construct>(image+0x89e580),reinterpret_cast<subscriber_lifecycle::Construct>(image+0x89e5f0),
        reinterpret_cast<subscriber_lifecycle::PrepareSave>(image+0x10ecae0),reinterpret_cast<subscriber_lifecycle::SaveRecord>(image+0x10ecb40)};
    {live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);
        if(!subscriber_lifecycle::capture_original(api,reinterpret_cast<const void*>(receipt.child.row.local_pointer),inspect,&captured,cleanup,error))return false;}
    duplicate_records::Witness complete;
    if(!duplicate_records::summarize(captured.graph,receipt.child.row.local_pointer,canonical,complete,error))return false;
    receipt.original_record_sha=complete.sha;
    if(receipt.child.row.type==0){
        if(!npc_quest_records::read(memory,image,receipt.child.row.local_pointer,canonical,receipt.original_quests,error))return false;
        for(const auto& local:receipt.original_quests.local){uint64_t uid=0;if(!at(local.destination,0x18,uid,error))return false;
            if(!uid)return q::fail(error,"Building child NPC own Quest destination full UID is absent");receipt.original_destination_uids.push_back(uid);}
    }else if(receipt.child.row.quest_count)return q::fail(error,"Building child NPC non-Quest type has an unexpected owned Quest count");
    character_definition_identity::Census after;
    if(!character_definition_identity::read(at,image,reinterpret_cast<uintptr_t>(world.state),manager,after,error))return false;
    const character_definition_identity::Definition* retained=nullptr;
    return character_definition_identity::find(after,definition->object,definition->uid,retained,error)&&
        (!retained->deleted||q::fail(error,"Building child NPC Character definition retired during capture"));
}
bool select_key(const child::Parent& parent,const npc_census::Row& row,std::string& key,std::string& error){
    key.clear();if(parent.scene!="world"){key=building_npc_key::make(parent.key);return building_npc_key::valid(key)||q::fail(error,"Building child NPC scene parent key invalid");}
    const auto saved=pools.find(parent.saved_region);
    if(saved==pools.end())return q::fail(error,"Building child NPC Region has no complete snapshot pool");
    for(const auto& record:saved->second.pool.records)if(record.pointer==row.local_pointer||record.uid==row.local_uid){
        if(record.pointer!=row.local_pointer||record.uid!=row.local_uid||record.type!=row.type)
            return q::fail(error,"Building child NPC conflicts with a retained serialized pointer/full UID");
        key=q::key(sha,parent.saved_region,record);break;
    }
    if(key.empty())key=building_npc_key::make(parent.key);
    return (snapshot_npc_key::valid(key)||building_npc_key::valid(key))||q::fail(error,"Building child NPC canonical key is invalid");
}
bool capture(const child::Lease& expected,const child::Parent& parent,const child::OwnerResolver& external,
             bool birth,child::Receipt& out,std::string& error){
    out={};opened::CurrentWorld world;npc_census::State census_rows;npc_census::Row row;bool present=false;Lookup find;
    if(!read(expected,parent,world,census_rows,row,present,find,error))return false;
    const auto old=building_children.find(parent.key);
    if(birth&&old!=building_children.end())return q::fail(error,"Building child NPC birth already has a retained parent receipt");
    child::Receipt next;next.lease=expected;next.parent=parent;next.present=present;
    if(present){next.child.saved_region=parent.saved_region;next.child.row=row;
        if(!select_key(parent,row,next.child.key,error))return false;}
    if(!birth&&old!=building_children.end()){
        if(old->second.retired||old->second.history_detached||!same_lease(expected,old->second.receipt.lease)||old->second.receipt.present!=present||
           (present&&(old->second.receipt.child.key!=next.child.key||old->second.receipt.child.row.local_pointer!=row.local_pointer||
                      old->second.receipt.child.row.local_uid!=row.local_uid)))return q::fail(error,"Building child NPC before receipt differs from retained current alias");
    }
    if(!witness(world,external,next,error))return false;
    opened::CurrentWorld after;npc_census::State final_census;npc_census::Row final_row;bool final_present=false;
    if(!read(expected,parent,after,final_census,final_row,final_present,find,error)||!same_world(world,after)||
       final_present!=present||(present&&row!=final_row))return q::fail(error,error.empty()?"Building child NPC identity/state changed during original capture":error);
    building_children[parent.key]={next,false};out=std::move(next);error.clear();return true;
}
template<class Find>
bool append(int32_t id,uintptr_t region,const npc_census::State& census_rows,Find& find,std::vector<Current>& rows,std::string& error){
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return q::at(memory,p,offset,value,cause);};
    for(const auto& [key,bound]:building_children){(void)key;const auto& receipt=bound.receipt;
        if(bound.retired||!receipt.present||receipt.parent.scene!="world"||receipt.parent.saved_region!=id)continue;
        uintptr_t registered=0;if(!find(receipt.child.row.local_uid,registered,error))return false;
        if(!registered){
            if(!owner::absent(find,census_rows,receipt.child.row,error))return false;
            continue;
        }
        if(registered!=receipt.child.row.local_pointer)return q::fail(error,"Building child NPC current alias requires an explicit original restoration receipt");
        npc_census::Row row;bool present=false;
        if(bound.history_detached){
            uintptr_t parent_registered=0,parent_ap=0,parent_region=0,parent_root=0;uint64_t parent_uid=0,parent_child=0;
            if(!find(receipt.parent.uid,parent_registered,error)||parent_registered!=receipt.parent.object||
               !at(parent_registered,0,parent_ap,error)||parent_ap!=image+0x16d93c0||
               !at(parent_registered,0x18,parent_uid,error)||parent_uid!=receipt.parent.uid||
               !at(parent_registered,0x170,parent_root,error)||parent_root!=reinterpret_cast<uintptr_t>(origin.map)+0x1c0||
               !at(parent_registered,0x178,parent_region,error)||parent_region!=region||
               !at(parent_registered,0xa30,parent_child,error)||(parent_child&&parent_child!=receipt.child.row.local_uid))
                return q::fail(error,error.empty()?"NPC history retained detached Building source changed":error);
            for(const auto& candidate:census_rows.rows){uintptr_t actual_owner=0;
                if(!at(candidate.local_pointer,0x268,actual_owner,error))return false;
                if(candidate.local_uid==receipt.child.row.local_uid){
                    if(present||candidate.local_pointer!=registered||actual_owner)return q::fail(error,"NPC history detached current owner/full UID differs");
                    row=candidate;present=true;
                }else if(actual_owner==receipt.parent.object)return q::fail(error,"NPC history detached alias has a conflicting Building child");
            }
        }else if(!owner::read(at,find,{image,reinterpret_cast<uintptr_t>(origin.map)+0x1c0,region},receipt.parent,census_rows,row,present,error))return false;
        if(!present||row.local_pointer!=receipt.child.row.local_pointer||row.local_uid!=receipt.child.row.local_uid)
            return q::fail(error,"Building child NPC retained active alias no longer matches its complete current owner");
        bool found=false;for(const auto& existing:rows)if(existing.key==receipt.child.key||existing.row.local_pointer==row.local_pointer||existing.row.local_uid==row.local_uid){
            if(existing.key!=receipt.child.key||existing.row.local_pointer!=row.local_pointer||existing.row.local_uid!=row.local_uid)
                return q::fail(error,"Building child NPC active alias collides with a current saved member");found=true;
        }
        if(!found)rows.push_back({receipt.child.key,id,row});
    }return true;
}
template<class Find>
bool append_scenes(const opened::CurrentWorld& world,Find& find,std::vector<Current>& output,std::string& error){
    for(const auto& [key,bound]:building_children){(void)key;const auto& receipt=bound.receipt;
        if(bound.retired||!receipt.present||receipt.parent.scene=="world")continue;
        owner::Scope scope;npc_census::State rows;
        uintptr_t registered=0;if(!find(receipt.child.row.local_uid,registered,error))return false;
        if(!registered){
            if(!retirement_census(world,receipt,find,scope,rows,error)||!owner::absent(find,rows,receipt.child.row,error))return false;
            continue;
        }
        if(!scene_census(world,receipt.parent,find,scope,rows,error))return false;
        if(registered!=receipt.child.row.local_pointer)return q::fail(error,"Building child NPC scene alias requires explicit original restoration");
        npc_census::Row row;bool present=false;
        auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return q::at(memory,p,offset,value,cause);};
        if(!owner::read(at,find,scope,receipt.parent,rows,row,present,error))return false;
        if(!present||row.local_pointer!=receipt.child.row.local_pointer||row.local_uid!=receipt.child.row.local_uid)
            return q::fail(error,"Building child NPC complete retained scene alias changed without original restoration");
        for(const auto& current:output)if(current.key==receipt.child.key||current.row.local_pointer==row.local_pointer||current.row.local_uid==row.local_uid)
            return q::fail(error,"Building child NPC scene alias collides with another current owner");
        output.push_back({receipt.child.key,receipt.parent.saved_region,row,scope.root,scope.region,receipt.parent.scene});
    }return true;
}
}
