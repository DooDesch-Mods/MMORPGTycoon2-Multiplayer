namespace {
bool vip_alert_authorize(const live_host_state::vip::Origin& origin,const action::Context& context,
                         const action::Binding& binding,bool teardown,std::string& error){
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||runtime->thread!=GetCurrentThreadId()||!runtime->controller.status().started||
       context.role!=action::Role::Client||runtime->controller.bound_context()!=context||
       runtime->controller.bound_binding()!=binding||!live_host_state::vip::valid_origin(origin)||
       origin.session!=context.session||origin.snapshot!=context.snapshot_uuid){
        error="VIP native presentation current Mainthread/Controller/Origin differs";return false;
    }
    // Teardown uses the retained exact native owner graph. Session/world origins
    // may already be invalidated when the original Mode Deinit begins.
    if(teardown){error.clear();return true;}
    if(runtime->lobby_retiring||runtime->controller.status().locked||!runtime->live_state||
       runtime->live_state->locked()||live_host_state::economic_fence().locked()||!runtime->live_error.empty()){
        error="VIP native presentation authority locked: "+runtime->live_error;return false;
    }
    action::Context actual;
    if(!runtime->session.current(actual,error))return false;
    if(actual!=context||origin.epoch!=net::current_world_epoch()){
        error="VIP native presentation current Session/common epoch differs";return false;
    }
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,error))return false;
    if(world.world_epoch!=binding.native_epoch||world.generation!=binding.native_generation){
        error="VIP native presentation local World Binding differs";return false;
    }
    error.clear();return true;
}
void vip_alert_before_deinit(uintptr_t mode) noexcept {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||!runtime->vip_presentation)return;
    try{
        std::string error;
        if(!runtime->vip_presentation->before_deinit(mode,error)){
            if(error.empty())error="VIP pre-Deinit cleanup failed without cause";
            if(runtime->live_error.empty())runtime->live_error=error;
            LOG_ERROR("[VIPPresentation] Mode Deinit proceeds after cleanup failure: {}",error);
        }
    }catch(const std::exception& exception){
        LOG_ERROR("[VIPPresentation] pre-Deinit cleanup exception: {}; original Deinit proceeds",exception.what());
    }catch(...){LOG_ERROR("[VIPPresentation] pre-Deinit cleanup nonstandard exception; original Deinit proceeds");}
}
void vip_alert_after_deinit(uintptr_t mode,bool abnormal) noexcept {
    auto* runtime=published_runtime.load(std::memory_order_acquire);
    if(!runtime||!runtime->vip_presentation)return;
    try{runtime->vip_presentation->after_deinit(mode,abnormal);}
    catch(const std::exception& exception){LOG_ERROR("[VIPPresentation] post-Deinit receipt exception: {}",exception.what());}
    catch(...){LOG_ERROR("[VIPPresentation] post-Deinit receipt nonstandard exception");}
}
bool vip_alert_initialized=false,vip_alert_initialize_attempted=false;
std::string vip_alert_initialize_error;
bool vip_alert_initialize(std::string& error){
    namespace hook=scenario_vip_authority::client_alert::cleanup_hook;
    if(vip_alert_initialized)return hook::verify(error);
    if(vip_alert_initialize_attempted){error=vip_alert_initialize_error;return false;}
    vip_alert_initialize_attempted=true;
    vip_alert_initialized=hook::initialize(weapon_image,{vip_alert_before_deinit,vip_alert_after_deinit},vip_alert_initialize_error);
    error=vip_alert_initialize_error;return vip_alert_initialized;
}
}
