namespace {
bool vip_client_active(){
    const auto* runtime=published_runtime.load(std::memory_order_acquire);
    return runtime&&runtime->controller.status().started&&!runtime->lobby_retiring&&
        runtime->controller.bound_context().role==action::Role::Client;
}
bool vip_owner(uintptr_t receiver,scenario_vip_authority::Receipt& receipt,std::string& error){
    receipt.receiver=receiver;
    if(!vip_client_active())return true;
    receipt.owned=true;
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    const auto reject=[&](const char* cause){error=cause;return false;};
    if(runtime->thread!=GetCurrentThreadId())return reject("VIP Client update is not on bound Mainthread");
    if(runtime->controller.status().locked||(runtime->live_state&&runtime->live_state->locked())||
       live_host_state::economic_fence().locked()||!runtime->live_error.empty()){
        error="VIP Client authority locked: "+runtime->live_error+" fence="+live_host_state::economic_fence().cause();return false;
    }
    action::Context context;
    if(!runtime->session.current(context,error))return false;
    if(context!=runtime->controller.bound_context())return reject("VIP current Session differs from Controller");
    const auto binding=runtime->controller.bound_binding();
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,error))return false;
    if(world.world_epoch!=binding.native_epoch||world.generation!=binding.native_generation)
        return reject("VIP NativeOpen lifetime differs from Controller");
    receipt.native_epoch=binding.native_epoch;receipt.native_generation=binding.native_generation;
    receipt.snapshot=context.snapshot_uuid;receipt.origin=world.uuid;
    native::ClockState clock;
    if(!native::read_clock(clock,error))return false;
    if(clock.time_scale!=0)return reject("VIP Client actual simulation Clock is not zero");
    const auto read=[&](uintptr_t base,size_t offset,auto& value){
        return toon_weapon_visual::visual_read(weapon_memory,base,offset,value,error);
    };
    uintptr_t ap=0,method=0,singleton=0,data=0;
    int32_t count=0,capacity=0;
    if(!read(receiver,0,ap))return false;
    if(ap!=weapon_image+0x1730e90)return reject("VIP actual receiver class differs");
    if(!read(ap,0xa0,method))return false;
    if(method!=weapon_image+0x4f85b0)return reject("VIP virtual Update entry differs");
    if(!read(uintptr_t(world.state),0x1e8,receipt.log)||!read(weapon_image,0x13df330,singleton)||
       !read(receipt.log,0,ap))return false;
    if(receipt.log!=singleton||ap!=weapon_image+0x16fd7b0)return reject("VIP current ScenarioLog owner differs");
    if(!read(receipt.log,0x20,data)||!read(receipt.log,0x28,count)||!read(receipt.log,0x2c,capacity))return false;
    // Use the existing ScenarioLog reader bound; no Subscriber census is needed.
    if(count<0||count>64||capacity<count||(count&&!data))return reject("VIP current ScenarioLog array bounds differ");
    unsigned matches=0;
    for(int32_t i=0;i<count;++i){
        uintptr_t item=0;if(!read(data,size_t(i)*sizeof item,item))return false;
        if(item==receiver)++matches;
    }
    if(matches!=1)return reject("VIP receiver is not a unique current ScenarioLog member");
    return true;
}
void vip_publish(const scenario_vip_authority::Receipt& receipt){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||!receipt.owned)return;
    if(receipt.suppressed){++runtime->vip_suppressed;runtime->vip_last=receipt;return;}
    if(runtime->vip_failure.cause.empty()){
        runtime->vip_failure=receipt;
        if(runtime->vip_failure.cause.empty())runtime->vip_failure.cause="VIP Client suppression failed without cause";
    }
    if(runtime->live_error.empty()){
        runtime->live_error=runtime->vip_failure.cause;
        LOG_ERROR("[VIPAuthority] {}",runtime->live_error);
    }
    if(!live_host_state::economic_fence().locked())live_host_state::economic_fence().journal_failed(runtime->live_error);
}
nlohmann::json vip_status(const Runtime& runtime){
    const auto encode=[](const scenario_vip_authority::Receipt& receipt){return nlohmann::json{
        {"receiver_address",receipt.receiver},{"scenario_log_address",receipt.log},{"owned",receipt.owned},
        {"suppressed",receipt.suppressed},{"snapshot_uuid",receipt.snapshot},{"native_origin_uuid",receipt.origin},
        {"native_epoch",receipt.native_epoch},{"native_generation",receipt.native_generation},{"cause",receipt.cause}};};
    return {{"suppressed_updates",runtime.vip_suppressed},{"last",encode(runtime.vip_last)},
        {"failure",runtime.vip_failure.cause.empty()?nlohmann::json(nullptr):encode(runtime.vip_failure)}};
}
}
