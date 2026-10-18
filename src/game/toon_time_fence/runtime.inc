namespace {
bool toon_time_client_active(){
    const auto* runtime=published_runtime.load(std::memory_order_acquire);
    return runtime&&runtime->controller.status_view().started&&!runtime->lobby_retiring&&
        runtime->controller.bound_context_view().role==action::Role::Client&&
        runtime->live_state&&runtime->live_state->committed_economy();
}
bool toon_time_owner(uintptr_t toon,toon_time_fence::Owner& owner,bool& owned,std::string& error){
    owner={};owned=false;auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||!runtime->controller.status_view().started||runtime->lobby_retiring||
       runtime->controller.bound_context_view().role!=action::Role::Client)return true;
    // No authoritative pose has been adopted before the first committed frame.
    if(!runtime->live_state||!runtime->live_state->committed_economy())return true;
    owned=true;
    const auto rejected=[&](const char* cause){if(error.empty())error=cause;else error=std::string(cause)+" original="+error;return false;};
    if(runtime->thread!=GetCurrentThreadId())return rejected("Toon time Client entry is not on bound Mainthread");
    uintptr_t ap=0,reverse=0,shard=0;
    const auto read=[&](uintptr_t base,size_t offset,auto& out){return toon_weapon_visual::visual_read(weapon_memory,base,offset,out,error);};
    if(!read(toon,0,ap))return false;
    if(ap!=weapon_image+0x1744e90){owned=false;return true;}
    if(runtime->controller.status_view().locked||runtime->live_state->locked()||
       live_host_state::economic_fence().locked()||!runtime->live_error.empty()){
        error="Toon time Client authority locked: "+runtime->live_error+" fence="+live_host_state::economic_fence().cause();return false;
    }
    {live_host_state::apply_timing::Scope measured(live_host_state::apply_timing::Part::ToonOwnerSession);
        if(!runtime->session.current(owner.context,error))return false;}
    owner.binding=runtime->controller.bound_binding();
    if(owner.context!=runtime->controller.bound_context_view())return rejected("Toon time current Session differs from Controller");
    building_mixed_identity::opened_file::CurrentWorld world;
    {live_host_state::apply_timing::Scope measured(live_host_state::apply_timing::Part::ToonOwnerNativeOpen);
        if(!native_open_runtime::read_current_world(world,error))return false;}
    if(world.world_epoch!=owner.binding.native_epoch||world.generation!=owner.binding.native_generation)
        return rejected("Toon time NativeOpen lifetime differs from Controller");
    owner.origin={uintptr_t(world.game),uintptr_t(world.mode),uintptr_t(world.state),uintptr_t(world.world),uintptr_t(world.map)};
    owner.origin_uuid=std::move(world.uuid);owner.origin_sha=std::move(world.sha256);owner.origin_bytes=world.bytes;
    const auto& frame=runtime->live_state->committed_economy()->day.frame;
    if(frame.session!=owner.context.session||frame.snapshot!=owner.context.snapshot_uuid||frame.epoch!=net::current_world_epoch())
        return rejected("Toon time committed Economy Context/common epoch differs");
    native::ClockState clock;
    {live_host_state::apply_timing::Scope measured(live_host_state::apply_timing::Part::ToonOwnerClock);
        if(!native::read_clock(clock,error))return false;}
    if(clock.time_scale!=0)return rejected("Toon time Client actual simulation Clock is not zero");
    if(!read(toon,0x940,owner.subscriber)||!read(owner.subscriber,0,ap)||ap!=weapon_image+0x16e75f0||
       !read(owner.subscriber,0x80,reverse)||reverse!=toon||!read(owner.subscriber,0x20,owner.subscriber_id)||owner.subscriber_id<0)
        return rejected("Toon time reciprocal Subscriber/Toon identity differs");
    if(!read(toon,0x170,shard)||shard!=uintptr_t(world.map)+0x1c0)
        return rejected("Toon time actual Toon is outside the bound root shard");
    // EntityUpdate is entered from the original current ToonManager iteration.
    // Its reciprocal Subscriber and root-shard owner are checked directly; no
    // full census, Economy sample or JSON is constructed per Toon update.
    owner.toon=toon;return true;
}
void toon_time_publish(uintptr_t toon,const toon_time_fence::Receipt& receipt){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||!receipt.owned)return;
    runtime->toon_time_receipt=receipt;runtime->toon_time_receiver=toon;
    if(receipt.confirmed){
        ++runtime->toon_time_confirmed;
        if(receipt.backlog_bits){runtime->toon_time_discarded=receipt;runtime->toon_time_discarded_receiver=toon;}
        return;
    }
    const auto cause=receipt.cause.empty()?std::string("Toon time owned normalization failed without cause"):receipt.cause;
    if(runtime->live_error.empty())runtime->live_error=cause;
    if(!live_host_state::economic_fence().locked())live_host_state::economic_fence().journal_failed(runtime->live_error);
}
nlohmann::json toon_time_status(const Runtime& runtime){
    const auto encode=[](uintptr_t toon,const toon_time_fence::Receipt& receipt){return nlohmann::json{
        {"toon_address",toon},{"subscriber_id",receipt.subscriber_id},{"owned",receipt.owned},
        {"cache_attempted",receipt.cache_attempted},{"cache_cleared",receipt.cache_cleared},
        {"original_attempted",receipt.original_attempted},{"original_returned",receipt.original_returned},
        {"confirmed",receipt.confirmed},{"unknown",receipt.unknown},{"cause",receipt.cause},
        {"input_dt_bits",receipt.input_bits},{"backlog_bits",receipt.backlog_bits},{"after_bits",receipt.after_bits}};};
    return {{"confirmed_updates",runtime.toon_time_confirmed},
        {"last",encode(runtime.toon_time_receiver,runtime.toon_time_receipt)},
        {"last_discarded",runtime.toon_time_discarded.owned?encode(runtime.toon_time_discarded_receiver,runtime.toon_time_discarded):nlohmann::json(nullptr)}};
}
}
