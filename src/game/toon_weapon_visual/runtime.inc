namespace {
placement_ui_probe::ProcessMemory weapon_memory;
uintptr_t weapon_image=0;
bool weapon_initialized=false,weapon_initialize_attempted=false;
std::string weapon_initialize_error;
bool weapon_owner(uintptr_t toon,toon_weapon_visual::Lease& lease,bool& owned,std::string& error){
    owned=false;lease={};auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||!runtime->controller.status().started||runtime->lobby_retiring||
       runtime->controller.bound_context().role!=action::Role::Client)return true;
    uintptr_t ap=0;
    if(!toon_weapon_visual::visual_read(weapon_memory,toon,0,ap,error))return false;
    if(ap!=weapon_image+0x1744e90)return true;
    owned=true;
    if(runtime->thread!=GetCurrentThreadId())return toon_weapon_visual::visual_fail(error,"Weapon Client authority requires bound Mainthread");
    if(runtime->controller.status().locked||!runtime->live_state||runtime->live_state->locked()||
       live_host_state::economic_fence().locked()||!runtime->live_error.empty()){
        error="Weapon Client authority locked/unavailable: "+runtime->live_error+" fence="+live_host_state::economic_fence().cause();return false;
    }
    if(!runtime->session.current(lease.context,error))return false;
    lease.binding=runtime->controller.bound_binding();
    if(lease.context!=runtime->controller.bound_context())return toon_weapon_visual::visual_fail(error,"Weapon current Session differs from Controller");
    if(!runtime->engine.current(lease.binding,lease.context.snapshot_uuid,error))return false;
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,error))return false;
    if(world.world_epoch!=lease.binding.native_epoch||world.generation!=lease.binding.native_generation)
        return toon_weapon_visual::visual_fail(error,"Weapon NativeOpen lifetime differs from Controller");
    lease.origin={uintptr_t(world.game),uintptr_t(world.mode),uintptr_t(world.state),uintptr_t(world.world),uintptr_t(world.map)};
    lease.origin_uuid=world.uuid;lease.origin_sha=world.sha256;lease.origin_bytes=world.bytes;
    native::ClockState clock;if(!native::read_clock(clock,error))return false;
    if(clock.time_scale!=0)return toon_weapon_visual::visual_fail(error,"Weapon Client simulation Clock is not zero");
    const auto* authority=runtime->live_state->weapon_economy();
    if(!authority)return toon_weapon_visual::visual_fail(error,"Weapon admitted Client Economy unavailable");
    const auto& frame=authority->day.frame;
    if(frame.session!=lease.context.session||frame.snapshot!=lease.context.snapshot_uuid||frame.epoch!=net::current_world_epoch())
        return toon_weapon_visual::visual_fail(error,"Weapon Economy Context/common epoch differs");
    std::vector<toon_weapon_visual::ToonOwner> census;
    if(!toon_weapon_visual::toon_census(weapon_memory,weapon_image,census,error))return false;
    for(const auto& row:census)if(row.toon==toon){
        if(!toon_weapon_visual::authoritative_tuple(weapon_memory,row,*authority,error))return false;
        lease.subscriber=row.subscriber;lease.toon=toon;lease.subscriber_id=row.id;return true;
    }
    return toon_weapon_visual::visual_fail(error,"Weapon Toon absent from actual Subscriber census");
}
void weapon_publish(uintptr_t toon,const toon_weapon_visual::Receipt& receipt)noexcept{
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||!receipt.owned)return;
    runtime->weapon_receipt=receipt;runtime->weapon_toon=toon;
    if(receipt.hook){runtime->weapon_hook_receipt=receipt;runtime->weapon_hook_toon=toon;}
    if(receipt.confirmed)return;
    const auto cause=receipt.cause.empty()?std::string("Weapon Client visual failed without cause"):receipt.cause;
    if(runtime->live_error.empty())runtime->live_error=cause;
    live_host_state::economic_fence().journal_failed(runtime->live_error);
}
bool weapon_initialize(std::string& error){
    if(weapon_initialized)return toon_weapon_visual::verify_hook(error);
    if(weapon_initialize_attempted){error=weapon_initialize_error;return false;}
    weapon_initialize_attempted=true;weapon_image=uintptr_t(GetModuleHandleW(nullptr));
    weapon_initialized=toon_weapon_visual::initialize(weapon_image,{weapon_owner,weapon_publish},weapon_initialize_error);
    error=weapon_initialize_error;return weapon_initialized;
}
void weapon_bootstrap(Runtime& runtime){
    const auto& context=runtime.controller.bound_context();const auto binding=runtime.controller.bound_binding();
    if(context.role!=action::Role::Client||!runtime.live_state||runtime.live_state->locked()||
       live_host_state::economic_fence().locked()||!runtime.live_error.empty())return;
    if(runtime.weapon_bootstrap_context&&*runtime.weapon_bootstrap_context==context&&runtime.weapon_bootstrap_binding==binding)return;
    const auto* authority=runtime.live_state->committed_economy();if(!authority)return;
    std::string error;std::vector<toon_weapon_visual::ToonOwner> census;bool attempted=false;
    const auto failed=[&]{toon_weapon_visual::Receipt receipt;receipt.owned=true;receipt.attempted=receipt.unknown=attempted;receipt.cause=error;weapon_publish(0,receipt);};
    if(!toon_weapon_visual::toon_census(weapon_memory,weapon_image,census,error)){failed();return;}
    for(const auto& row:authority->subscribers){
        if(!row.quest_state)continue;
        const auto found=std::find_if(census.begin(),census.end(),[&](const auto& actual){return actual.id==row.serialized_id;});
        if(found==census.end()||!found->toon){error="Weapon initial refresh authoritative Toon absent from actual census";failed();return;}
        toon_weapon_visual::Receipt receipt;
        const bool refreshed=toon_weapon_visual::refresh(found->toon,receipt,error);attempted=attempted||receipt.attempted;
        if(!refreshed||!receipt.owned||!receipt.confirmed){
            if(error.empty())error="Weapon initial refresh did not confirm owned Client";failed();return;
        }
    }
    runtime.weapon_bootstrap_context=context;runtime.weapon_bootstrap_binding=binding;
}
nlohmann::json weapon_receipt_status(const toon_weapon_visual::Receipt& r,uintptr_t toon){
    const auto call=[](const toon_weapon_visual::Call& c){return nlohmann::json{{"attempted",c.attempted},{"returned",c.returned},{"exception",c.exception}};};
    auto attachments=nlohmann::json::array();for(const auto& c:r.attachments)attachments.push_back(call(c));
    return {{"toon_address",toon},{"subscriber_id",r.subscriber_id},
        {"owned",r.owned},{"attempted",r.attempted},{"confirmed",r.confirmed},{"unknown",r.unknown},{"deferred",r.deferred},
        {"hook",r.hook},{"forwarded",r.forwarded},{"ilvl",r.ilvl},{"category",r.category},{"variant",r.variant},
        {"model_id",r.model_id},{"bone",r.bone},{"attack",r.attack},{"cause",r.cause},
        {"random",{{"before_observed",r.random.before_observed},{"after_observed",r.random.after_observed},
            {"observed",r.random.observed},{"unchanged",r.random.unchanged},{"before",r.random.before},{"after",r.random.after},
            {"before_cause",r.random.before_cause},{"after_cause",r.random.after_cause},{"cause",r.random.cause}}},
        {"scene",call(r.scene)},{"matrix",call(r.matrix)},{"remove",call(r.remove)},{"cache",call(r.cache)},{"attachments",attachments}};
}
nlohmann::json weapon_status(const Runtime& runtime){
    auto result=weapon_receipt_status(runtime.weapon_receipt,runtime.weapon_toon);
    result["bootstrap_confirmed"]=runtime.weapon_bootstrap_context&&*runtime.weapon_bootstrap_context==runtime.controller.bound_context()&&runtime.weapon_bootstrap_binding==runtime.controller.bound_binding();
    result["last_owned_hook"]=runtime.weapon_hook_receipt.owned&&runtime.weapon_hook_receipt.hook?
        weapon_receipt_status(runtime.weapon_hook_receipt,runtime.weapon_hook_toon):nlohmann::json(nullptr);
    return result;
}
}
