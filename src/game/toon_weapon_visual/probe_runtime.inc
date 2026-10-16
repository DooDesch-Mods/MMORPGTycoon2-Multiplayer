namespace {
bool weapon_probe_pin(int32_t id,toon_weapon_visual::Lease& lease,std::string& error){
    lease={};auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(id<0||!runtime||runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring||!runtime->controller.status().started)
        return toon_weapon_visual::visual_fail(error,"Weapon probe requires nonnegative ID and current started Mainthread Controller");
    if(!runtime->session.current(lease.context,error))return false;
    lease.binding=runtime->controller.bound_binding();
    if(lease.context!=runtime->controller.bound_context())return toon_weapon_visual::visual_fail(error,"Weapon probe current Session differs from Controller");
    if(!runtime->engine.current(lease.binding,lease.context.snapshot_uuid,error))return false;
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,error))return false;
    if(world.world_epoch!=lease.binding.native_epoch||world.generation!=lease.binding.native_generation)
        return toon_weapon_visual::visual_fail(error,"Weapon probe NativeOpen lifetime differs from Controller");
    lease.origin={uintptr_t(world.game),uintptr_t(world.mode),uintptr_t(world.state),uintptr_t(world.world),uintptr_t(world.map)};
    lease.origin_uuid=world.uuid;lease.origin_sha=world.sha256;lease.origin_bytes=world.bytes;
    std::vector<toon_weapon_visual::ToonOwner> census;
    if(!toon_weapon_visual::toon_census(weapon_memory,weapon_image,census,error))return false;
    for(const auto& row:census)if(row.id==id){
        lease.subscriber=row.subscriber;lease.toon=row.toon;lease.subscriber_id=id;return true;
    }
    return toon_weapon_visual::visual_fail(error,"Weapon probe Subscriber ID absent from current census");
}
}
bool read_subscriber_weapon(int32_t id,nlohmann::json& out,std::string& error){
    out={{"read_only",true},{"attempted",false},{"read_confirmed",false},{"serialized_id",id}};
    try{
        toon_weapon_visual::Lease before,after;
        if(!weapon_probe_pin(id,before,error))return false;
        toon_weapon_visual::Observation observed;
        if(before.toon&&!toon_weapon_visual::inspect(before.toon,observed,error))return false;
        if(!weapon_probe_pin(id,after,error))return false;
        if(before!=after)return toon_weapon_visual::visual_fail(error,"Weapon probe full Origin/Controller/Subscriber changed during read");
        if(before.toon)out=toon_weapon_visual::encode_observation(observed);
        else out["read_confirmed"]=true;
        out["serialized_id"]=id;out["toon_absent"]=before.toon==0;
        out["role"]=before.context.role==action::Role::Host?"Host":"Client";
        out["session"]=before.context.session;out["snapshot_uuid"]=before.context.snapshot_uuid;
        out["native_epoch"]=before.binding.native_epoch;out["native_generation"]=before.binding.native_generation;
        out["origin_uuid"]=before.origin_uuid;out["origin_sha256"]=before.origin_sha;out["origin_bytes"]=before.origin_bytes;
        return true;
    }catch(const std::exception& e){error=std::string("Weapon read-only probe exception: ")+e.what();return false;}
    catch(...){error="Weapon read-only probe nonstandard exception";return false;}
}
