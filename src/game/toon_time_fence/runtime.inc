namespace {
bool toon_camera_owner(uintptr_t toon,uintptr_t caller,uintptr_t root,bool& owned,std::string& error){
    owned=false;
    if(caller!=weapon_image+0x2ffea1&&caller!=weapon_image+0x300064){error.clear();return true;}
    uintptr_t ap=0,shard=0,subscriber=0,reverse=0,region=0,manager=0;
    const auto read=[&](uintptr_t base,size_t offset,auto& out){return toon_weapon_visual::visual_read(weapon_memory,base,offset,out,error);};
    if(!read(toon,0,ap))return false;
    if(ap!=weapon_image+0x1744e90){error.clear();return true;}
    if(!read(toon,0x170,shard))return false;
    if(shard!=root){error.clear();return true;}
    int32_t id=-1;
    if(!read(toon,0x940,subscriber)||!subscriber||!read(subscriber,0,ap)||ap!=weapon_image+0x16e75f0||
       !read(subscriber,0x80,reverse)||reverse!=toon||!read(subscriber,0x20,id)||id<0||
       !read(toon,0x178,region)||!region||!read(region,0,ap)||ap!=weapon_image+0x1787930||
       !read(region,0x9c0,manager)||!manager||!read(manager,0,ap)||ap!=weapon_image+0x16fde30){
        if(error.empty())error="Toon camera reciprocal Subscriber or Region manager differs";return false;
    }
    owned=true;error.clear();return true;
}
struct ToonMotionSample {
    uint32_t backlog=0,deadline=0,scale=0;
    uint8_t branch=0,distance=0,nearby=0,force=0,flag269=0,flag26a=0;
    std::array<uint32_t,10> pose{};
    std::array<uint32_t,8> attributes{};
};
struct ToonMotionRow {
    uint64_t tick=0,completed=0,lease=0,session=0,generation=0,world_epoch=0;
    uint32_t phase=0,dt=0;
    ToonMotionSample before{},after{};
    bool original_returned=false,after_read=false;
};
std::array<ToonMotionRow,8192> toon_motion_rows{};
size_t toon_motion_count=0;
bool toon_motion_overflow=false;
std::string toon_motion_error;
thread_local uintptr_t toon_motion_pending=0;
thread_local size_t toon_motion_index=0;
bool toon_motion_enabled(){
    static const bool enabled=[] {char value[4]{};return Config::instance().local_test&&
        GetEnvironmentVariableA("MT2MP_TOON46_TRACE",value,sizeof(value))==1&&value[0]=='1';}();
    return enabled;
}
bool toon_motion_sample(uintptr_t toon,ToonMotionSample& out,std::string& error){
    const auto read=[&](size_t offset,auto& value){return toon_weapon_visual::visual_read(weapon_memory,toon,offset,value,error);};
    if(!read(0xad4,out.backlog)||!read(0x2b0,out.deadline)||!read(0x270,out.scale)||
       !read(0x1c0,out.branch)||!read(0x808,out.distance)||!read(0x809,out.nearby)||!read(0x269,out.flag269)||
       !read(0x26a,out.flag26a)||!toon_weapon_visual::visual_read(weapon_memory,weapon_image,0x1a323a0,out.force,error))return false;
    for(size_t i=0;i<out.pose.size();++i)if(!read(0x100+i*4,out.pose[i]))return false;
    constexpr std::array<size_t,8> offsets{0x750,0x778,0x7a0,0x754,0x77c,0x7a4,0x7ec,0x7f0};
    for(size_t i=0;i<offsets.size();++i)if(!read(offsets[i],out.attributes[i]))return false;
    return true;
}
void toon_motion_observe(uintptr_t toon,float dt,uintptr_t caller,bool returned) noexcept{
    if(!toon_motion_enabled())return;
    try{
        auto* runtime=published_runtime.load(std::memory_order_acquire);
        if(!runtime||!runtime->ordered_steps||!runtime->controller.status_view().started||
           runtime->thread!=GetCurrentThreadId()||runtime->lobby_retiring)return;
        if(returned){
            if(toon_motion_pending!=toon)return;
            auto& row=toon_motion_rows[toon_motion_index];row.original_returned=true;
            std::string error;row.after_read=toon_motion_sample(toon,row.after,error);
            if(!row.after_read&&toon_motion_error.empty())toon_motion_error="Toon motion after read: "+error;
            toon_motion_pending=0;return;
        }
        uintptr_t ap=0,subscriber=0,reverse=0;int32_t id=-1;std::string error;
        const auto read=[&](uintptr_t base,size_t offset,auto& value){return toon_weapon_visual::visual_read(weapon_memory,base,offset,value,error);};
        if(!read(toon,0,ap)||ap!=weapon_image+0x1744e90||!read(toon,0x940,subscriber)||!subscriber||
           !read(subscriber,0x20,id)||id!=46)return;
        if(toon_motion_pending){if(toon_motion_error.empty())toon_motion_error="Toon motion nested target entry";return;}
        if(caller!=weapon_image+0x2ffea1&&caller!=weapon_image+0x300064){
            if(toon_motion_error.empty())toon_motion_error="Toon motion original manager caller differs";return;
        }
        MT2FW_MT2_StepReceipt step{};
        if(!read(subscriber,0,ap)||ap!=weapon_image+0x16e75f0||!read(subscriber,0x80,reverse)||reverse!=toon||
           !runtime->ordered_steps->diagnostic_read(step,error)){
            if(toon_motion_error.empty())toon_motion_error="Toon motion owner or step read: "+error;return;
        }
        if(step.tick<256||step.tick>4096)return;
        building_mixed_identity::opened_file::CurrentWorld world;uintptr_t shard=0,region=0,manager=0;
        const auto binding=runtime->controller.bound_binding();
        if(!native_open_runtime::read_current_world(world,error)||world.uuid!=runtime->controller.bound_context_view().snapshot_uuid||
           step.session!=runtime->controller.bound_context_view().session||world.world_epoch!=binding.native_epoch||world.generation!=binding.native_generation||
           !read(toon,0x170,shard)||shard!=uintptr_t(world.map)+0x1c0||!read(toon,0x178,region)||
           !read(region,0,ap)||ap!=weapon_image+0x1787930||!read(region,0x9c0,manager)||
           !read(manager,0,ap)||ap!=weapon_image+0x16fde30){
            if(toon_motion_error.empty())toon_motion_error="Toon motion world or manager guard: "+error;return;
        }
        if(toon_motion_count==toon_motion_rows.size()){toon_motion_overflow=true;return;}
        auto& row=toon_motion_rows[toon_motion_count];
        row.tick=step.tick;row.completed=step.completed_tick;row.lease=step.lease;row.generation=step.generation;
        row.session=step.session;row.world_epoch=step.world_epoch;row.phase=step.phase;row.dt=std::bit_cast<uint32_t>(dt);
        if(!toon_motion_sample(toon,row.before,error)){
            if(toon_motion_error.empty())toon_motion_error="Toon motion before read: "+error;return;
        }
        toon_motion_index=toon_motion_count++;toon_motion_pending=toon;
    }catch(const std::exception& error){if(toon_motion_error.empty())toon_motion_error=std::string("Toon motion diagnostic exception: ")+error.what();}
    catch(...){if(toon_motion_error.empty())toon_motion_error="Toon motion diagnostic nonstandard exception";}
}
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
nlohmann::json toon_motion_status(){
    const auto sample=[](const ToonMotionSample& s){return nlohmann::json{{"backlog_bits",s.backlog},{"deadline_2b0_bits",s.deadline},
        {"scale_270_bits",s.scale},{"flag_1c0",s.branch},{"flag_269",s.flag269},{"flag_26a",s.flag26a},
        {"flag_808",s.distance},{"flag_809",s.nearby},{"force_update",s.force},{"pose_bits",s.pose},
        {"attribute_bits_750_778_7a0_754_77c_7a4_7ec_7f0",s.attributes}};};
    nlohmann::json rows=nlohmann::json::array();
    for(size_t i=0;i<toon_motion_count;++i){const auto& r=toon_motion_rows[i];rows.push_back({
        {"tick",r.tick},{"completed_tick",r.completed},{"lease",r.lease},{"session",r.session},{"generation",r.generation},{"world_epoch",r.world_epoch},
        {"phase",r.phase},{"input_dt_bits",r.dt},{"before",sample(r.before)},{"original_returned",r.original_returned},
        {"after_read",r.after_read},{"after",r.after_read?sample(r.after):nlohmann::json(nullptr)}});}
    return {{"enabled",toon_motion_enabled()},{"saved_id",46},{"count",toon_motion_count},{"capacity",toon_motion_rows.size()},
        {"overflow",toon_motion_overflow},{"complete",toon_motion_count>0&&!toon_motion_pending&&!toon_motion_overflow&&toon_motion_error.empty()},
        {"error",toon_motion_error},{"rows",std::move(rows)}};
}
}
