using MakeScenery=void(*)(void*,void*);
MakeScenery original_make_scenery=nullptr;
bool make_observer_installed=false,make_observer_attempted=false;
std::string make_observer_error;
thread_local Local* observing_clearance=nullptr;
thread_local const host::WorldState* observing_boundary=nullptr;
bool raw_original_world(Runtime& r,const Local& value,host::WorldState& out,std::string& error){
    host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(r.memory.process,r.image,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",api,error))return false;
    host::Reader reader(r.memory.process,origin,r.image,r.thread,api);
    if(!reader.current(out,false,error))return false;
    const auto& before=value.sample.world;
    if(!host::same_world(out.origin,before.origin)||out.root!=before.root||out.selection!=before.selection||
       before.saves||before.edits||before.active_graph||out.saves||out.edits<0)
        return q::fail(error,"Ordered clearance original World/save/edit boundary differs");
    return true;
}
bool clearance_world(Runtime& r,const Local& value,host::WorldState& out,std::string& error){
    if(!observing_boundary)return world(r,out,error);
    if(observing_clearance!=&value||!value.original_started||value.original_completed||!value.ordered_local)
        return q::fail(error,"Ordered clearance original read window differs");
    if(!raw_original_world(r,value,out,error))return false;
    if(out.saves!=observing_boundary->saves||out.edits!=observing_boundary->edits||out.active_graph!=observing_boundary->active_graph)
        return q::fail(error,"Ordered clearance original save/edit/graph changed during observation");
    return true;
}


bool affected_region(Runtime& r,const Local& value,uintptr_t pointer,int32_t& id,std::string& error){
    uintptr_t actual=0;
    if(!q::at(r.memory,pointer,0x834,id,error))return false;
    auto read=[&](uintptr_t p,size_t offset,auto& out,std::string& why){return q::at(r.memory,p,offset,out,why);};
    return region_saved_identity::resolve(r.image,uintptr_t(value.sample.world.origin.map),value.sample.world.root,id,pointer,actual,read,error);
}
bool affected_key(Runtime& r,const Local& value,uintptr_t pointer,uint64_t uid,std::string& key,int32_t& region,std::string& error){
    uintptr_t owner=0;
    return remove_diagnostics::resolve_clearing_scenery_key(value.request.snapshot_uuid,pointer,uid,key,error)&&
        q::at(r.memory,pointer,0x178,owner,error)&&affected_region(r,value,owner,region,error);
}
bool affected_group(Runtime& r,const Local& value,uintptr_t parent,ordered_clearance_group::State& out,std::string& error){
    auto bind=[&](uintptr_t p,uint64_t uid,std::string& key,int32_t& region,std::string& why){return affected_key(r,value,p,uid,key,region,why);};
    auto region=[&](uintptr_t p,int32_t& id,std::string& why){return affected_region(r,value,p,id,why);};
    auto find=[&](uint64_t uid,uintptr_t& p,std::string& why){return group_edit::find(r,uid,p,why);};
    auto key=[&](uint64_t uid,const std::string& derived,std::string& canonical,std::string& why){return group_edit::canonical_group(value.binding,value.sample.world,uid,derived,canonical,why);};
    return ordered_clearance_group::read(r.memory,r.image,value.sample.world.root,parent,bind,region,find,key,out,error);
}
bool observe_make(Runtime& r,Local& value,uintptr_t node,uintptr_t object,std::string& error){
    if(!ordered_owner(r,value,error))return false;
    uint64_t uid=0;uintptr_t region=0,parent=0;
    if(!q::at(r.memory,object,0x18,uid,error)||!q::at(r.memory,object,0x178,region,error)||!q::at(r.memory,object,0x180,parent,error))return false;
    if(value.removed.contains(uid)||value.observed_nodes.contains(node))return q::fail(error,"Ordered clearance original Make repeats an affected object or node");
    Saved saved;auto find=[&](uint64_t id,uintptr_t& p,std::string& why){return group_edit::find(r,id,p,why);};
    if(!scenery_make_history::live(r.memory,r.image,object,uid,value.sample.world.root,region,find,saved,error)||
       !scenery_make_history::make(r.memory,r.image,node,saved,error))return false;
    action::DuplicateObject row;row.family=Family::Scenery;
    if(!affected_key(r,value,object,uid,row.key,row.region,error)||!group_reader::pose(r.memory,object,0x100,row.pose,error))return false;
    if(parent){ordered_clearance_group::State current;
        if(!affected_group(r,value,parent,current,error))return false;
        const auto prior=value.parents.find(parent);
        if(prior!=value.parents.end()){
            std::set<uint64_t> removed;for(const auto& [id,unused]:value.removed){(void)unused;removed.insert(id);}
            if(!ordered_clearance_group::removed_members(prior->second,current,removed,error))return false;
        }else value.parents.emplace(parent,current);
        bool member=false;
        for(const auto& child:current.members){
            value.sample.handles.emplace(child.prop.key,duplicate_native::Handle{child.handle.object,child.handle.uid});
            if(child.handle==group_reader::Handle{object,uid}&&child.prop.key==row.key)member=true;
        }
        if(!member)return q::fail(error,"Ordered clearance original affected parent lacks its exact member");
        row.parent=current.row.key;
        value.sample.handles.emplace(row.parent,duplicate_native::Handle{parent,current.handle.uid});
        if(std::find(value.sample.census.regions.begin(),value.sample.census.regions.end(),current.region)==value.sample.census.regions.end())value.sample.census.regions.push_back(current.region);
    }
    value.sample.handles.emplace(row.key,duplicate_native::Handle{object,uid});
    if(std::find(value.sample.census.regions.begin(),value.sample.census.regions.end(),region)==value.sample.census.regions.end())value.sample.census.regions.push_back(region);
    // Only this actually cleared live object's original Record is retained.
    // Existing Undo/Redo uses it after the native inverse restores the object.
    if(!duplicate_edit::record_object(r,value.sample,{object,uid},row,error))return false;
    if(!value.original.emplace(row.key,row).second||!value.aliases.emplace(uid,row.key).second)
        return q::fail(error,"Ordered clearance affected canonical identity repeats");
    value.removed.emplace(uid,saved);value.observed_nodes.emplace(node,uid);return true;
}
void make_scenery_entry(void* node,void* object){
    const auto returned_to=uintptr_t(_ReturnAddress());
    original_make_scenery(node,object);
    auto* value=observing_clearance;auto* r=published.load(std::memory_order_acquire);
    if(!value||!r||returned_to!=r->image+0x2cd390||!value->observation_error.empty())return;
    try{
        host::WorldState boundary;
        if(observing_boundary||!raw_original_world(*r,*value,boundary,value->observation_error)){
            if(value->observation_error.empty())value->observation_error="Ordered clearance original observer is nested";
            return;
        }
        observing_boundary=&boundary;
        struct ClearBoundary{~ClearBoundary(){observing_boundary=nullptr;}}clear_boundary;
        host::WorldState after;
        if((!observe_make(*r,*value,uintptr_t(node),uintptr_t(object),value->observation_error)||!clearance_world(*r,*value,after,value->observation_error))&&value->observation_error.empty())
            value->observation_error="Ordered clearance observer failed without original cause";
    }catch(const std::exception& failure){value->observation_error=std::string("Ordered clearance observer exception: ")+failure.what();}
    catch(...){value->observation_error="Ordered clearance observer nonstandard exception";}
}
bool enable_make_observer(Runtime& r,std::string& error){
    struct Code{uintptr_t rva;size_t size;const char* sha;};
    static const Code code[]{
        {0x2cd1a0,848,"ad7f57957ab2e5101f6a517532a36da40d3825934ed8df00a08d30c7bab7d962"},
        {0x559ee0,1248,"df811c53bfc8aad352a904f30d0053bc05906c5e21c0999b8cee485102b3e8b4"},
        {0x4d05c0,2064,"98f0b2ee3d0dfdb58784835eba2297451cac87d0794284d34c38a40e8b4cdff1"},
        {0x4ccd00,240,"8c165e9288e1e5d2ba7797cc10dac132f1c7a3fb8df095d061f31748e782b89b"}};
    for(const auto& body:code){std::vector<uint8_t> actual(body.size);
        if(!r.memory.read(r.image+body.rva,actual.data(),actual.size(),error)||
           !mt2mp::hooks::framework::verify_original_hash(r.image+body.rva,actual.data(),actual.size(),body.sha,error))return false;}
    if(make_observer_installed)return true;
    if(make_observer_attempted)return q::fail(error,"Ordered clearance observer installation previously failed: "+make_observer_error);
    make_observer_attempted=true;
    struct Hook{uintptr_t rva;void* entry;void** original;};
    const Hook hooks[]{{0x559ee0,reinterpret_cast<void*>(&make_scenery_entry),reinterpret_cast<void**>(&original_make_scenery)}};
    const auto created=MH_CreateHook(reinterpret_cast<void*>(r.image+hooks[0].rva),hooks[0].entry,hooks[0].original);
    if(created!=MH_OK||!original_make_scenery){error=make_observer_error="Ordered clearance MakeScenery hook create failed status="+std::to_string(created);return false;}
    if(!mt2mp::hooks::enable_prepared_routes(r.image,hooks,"Ordered clearance MakeScenery observer",error)){make_observer_error=error;return false;}
    make_observer_installed=true;return true;
}
bool read_ordered_graph(Runtime& r,const Local& value,ordered_empty_clearance::Receipt& out,std::string& error){
    if(!value.original_completed||!value.observation_error.empty())return q::fail(error,value.observation_error.empty()?"Ordered clearance original window did not complete":value.observation_error);
    if(value.removed.empty())return ordered_empty_clearance::read(r.memory,r.image,value.graph,value.primary_uid,out,error);
    q::Bounds span;std::vector<action::DuplicateCash> cash;std::map<uintptr_t,uint64_t> made;
    if(!q::typed(r.memory,r.image,value.graph+0x18,0x16e2dc0,error)||!q::bounds(r.memory,value.graph+0x18,100000,span,error))return false;
    for(int32_t i=0;i<span.count;++i){uintptr_t node=0,ap=0;
        if(!q::at(r.memory,span.data,size_t(i)*8,node,error)||!q::at(r.memory,node,0,ap,error))return false;
        if(ap==r.image+0x1706590){uint64_t uid=0;if(!q::at(r.memory,node,0xb8,uid,error)||!made.emplace(node,uid).second)return q::fail(error,error.empty()?"Ordered clearance graph repeats Make node":error);}
        else if(ap==r.image+0x1786fb0){action::DuplicateCash row;if(!q::at(r.memory,node,0xb8,row.amount,error)||!q::at(r.memory,node,0xbc,row.position,error))return false;cash.push_back(row);}
    }
    if(made!=value.observed_nodes)return q::fail(error,"Ordered clearance graph does not contain exactly the original observed MakeScenery nodes");
    duplicate_history::Graph proof;
    if(!q::unchanged(r.memory,value.graph+0x18,100000,span,error)||
       !scenery_stroke_graph::read(r.memory,r.image,value.graph,value.removed,{value.primary_uid},cash,proof,error))return false;
    std::set<uint64_t> removed;
    for(const auto& [uid,saved]:value.removed){(void)saved;uintptr_t actual=0;
        if(!group_edit::find(r,uid,actual,error)||actual)return q::fail(error,error.empty()?"Ordered clearance affected native UID still exists":error);
        removed.insert(uid);}
    for(const auto& [parent,before]:value.parents){ordered_clearance_group::State after;
        if(!affected_group(r,value,parent,after,error)||!ordered_clearance_group::removed_members(before,after,removed,error))return false;}
    out.cash=std::move(proof.cash);out.operations=std::move(proof.operations);return true;
}
