namespace world_path_runtime {
bool current_remove(region::Runtime& r,uintptr_t object,Pending& out,uint64_t& uid,std::string& error){
    uintptr_t region_pointer=0,second=0;int32_t id=-1,end=-1;
    if(!native::ap(r.memory,object,r.image,0x1743f10,error)||
       !region::at(r,object,0x178,region_pointer,error)||!region_pointer||
       !region::at(r,region_pointer,0x834,id,error)||!region::at(r,object,0x2f8,second,error))return false;
    end=id;if(second&&!region::at(r,second,0x834,end,error))return false;
    region::LocalView view;Sample before;undo::UndoWitness history;
    const auto snapshot=net::current_snapshot_id();
    if(!region::identity(r,id,region_pointer,view,error)||
       !world_path_sample(r,snapshot,id,end,before,error)||!undo::read_undo(r.memory,r.image,history,error))return false;
    if(history.active_graph)return fail(error,"mixed Path Remove normal capture requires idle Undo");
    const auto found=std::find_if(before.mixed.native.paths.begin(),before.mixed.native.paths.end(),
        [&](const auto& row){return row.member.object==object;});
    if(found==before.mixed.native.paths.end()||found->owner!=region_pointer||found->second!=second||
       !owner.keys.keys().contains(found->member.uid))return fail(error,"mixed Path Remove current target lacks canonical full identity");
    action::WorldPathIntent intent;intent.kind=action::WorldPathKind::Remove;
    intent.start_region=id;intent.end_region=end;intent.type=found->member.ref.type;
    intent.target_path=owner.keys.keys().at(found->member.uid);intent.graph_before_sha256=before.wire.complete_sha256;
    if(!action::world_path_valid(intent))return fail(error,"mixed Path Remove current semantic intent invalid");
    uid=found->member.uid;out={std::move(intent),{view.world.origin.world_epoch,view.world.origin.generation},snapshot};
    return true;
}
}
bool remove_local_binding(uintptr_t manager,uintptr_t object,action::ui_capture::LocalBinding& out,std::string& error){
    auto* r=runtime(error);uintptr_t current_manager=0;
    if(!r||!path.ready||!world_path_runtime::capture.ready||!world_path_runtime::idle(*r,error)||
       !region::at(*r,r->image,0x13df630,current_manager,error)||current_manager!=manager||
       !native::ap(r->memory,manager,r->image,0x17205d0,error))
        return fail(error,error.empty()?"mixed Path Remove current SelectionManager differs":error);
    world_path_runtime::Pending current;uint64_t uid=0;
    if(!world_path_runtime::current_remove(*r,object,current,uid,error))return false;
    action::ui_capture::LocalBinding binding;
    binding.manager=manager;binding.object=object;binding.uid=uid;
    binding.native_epoch=current.binding.native_epoch;binding.native_generation=current.binding.native_generation;
    binding.snapshot_uuid=current.snapshot;binding.canonical_record_key=current.intent.target_path;binding.supported_path=true;
    out=std::move(binding);return true;
}
bool submit_remove(const action::ui_capture::Intent& intent,std::string& error){
    auto* r=runtime(error);
    if(!r||!world_path_runtime::idle(*r,error)||intent.snapshot_uuid!=net::current_snapshot_id())
        return fail(error,error.empty()?"mixed Path Remove pending snapshot or Controller changed":error);
    uintptr_t object=0;
    for(const auto& [uid,key]:world_path_runtime::owner.keys.keys())if(key==intent.canonical_record_key){
        // The complete current census below resolves the retained UID without dereferencing a retired pointer.
        uintptr_t map=0;if(!region::at(*r,r->image,0x13df8c0,map,error))return false;
        mixed_path_native::Census census;
        if(!mixed_path_native::read_world(r->memory,r->image,map,map+0x1c0,census,error))return false;
        for(const auto& row:census.paths)if(row.member.uid==uid){object=row.member.object;break;}
        if(object)break;
    }
    if(!object)return fail(error,"mixed Path Remove canonical UI target retired before submission");
    world_path_runtime::Pending current;uint64_t uid=0;
    if(!world_path_runtime::current_remove(*r,object,current,uid,error)||
       current.intent.target_path!=intent.canonical_record_key||current.snapshot!=intent.snapshot_uuid)
        return fail(error,error.empty()?"mixed Path Remove current identity differs before submission":error);
    return typed_action_runtime::submit_world_path(intent.action_uuid,current.intent,error);
}
