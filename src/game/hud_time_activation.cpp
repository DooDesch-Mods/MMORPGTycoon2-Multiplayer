#include "hud_time_activation.h"
#include <exception>
#include <utility>

namespace mt2mp::game::hud_time {
namespace {
struct Busy {bool& value;explicit Busy(bool& v):value(v){value=true;}~Busy(){value=false;}};
bool ready(const ActivationContext& c) {
    return c.active&&c.epoch&&c.hook_mask==3&&c.local_peer>=1&&c.local_peer<=4&&c.peer_mask<=15&&
        (c.peer_mask&1)&&(c.peer_mask&(1u<<(c.local_peer-1)))&&c.host==(c.local_peer==1);
}
shared_time::Intent intent(float scale) {
    return scale==0?shared_time::Intent::Pause:scale==1?shared_time::Intent::Normal:
        scale==3?shared_time::Intent::Fast:shared_time::Intent::Fastest;
}
time_session::Intent wire_intent(shared_time::Intent value) {
    switch(value){
        case shared_time::Intent::Pause:return time_session::Intent::Pause;
        case shared_time::Intent::Normal:return time_session::Intent::Normal;
        case shared_time::Intent::Fast:return time_session::Intent::Fast;
        default:return time_session::Intent::Fastest;
    }
}
}
bool Activation::initialize(uint32_t thread,Reader& reader,ActivationServices io,std::string& e) {
    if(attempted_){e="hud_activation_initialize_already_attempted";return false;}
    attempted_=true;
    try {
        if(!thread||!io.thread_id||!io.context||!io.submit){e="hud_activation_callback_missing";return false;}
        if(io.thread_id()!=thread){e="hud_activation_initialize_wrong_thread";return false;}
        game_thread_=thread;reader_=&reader;io_=std::move(io);state_.initialized=true;e.clear();return true;
    }catch(const std::exception& cause){e=std::string("hud_activation_initialize_exception: ")+cause.what();}
     catch(...){e="hud_activation_initialize_unknown_exception";}
    state_.error=e;return false;
}
shared_time::HookResult Activation::on_activate(uintptr_t type,uintptr_t item,uintptr_t caller) {
    shared_time::HookResult result;
    auto reject=[&](const std::string& cause){++state_.rejected;state_.error=cause;result.error=cause;return result;};
    if(!state_.initialized)return reject("hud_activation_not_initialized");
    ++state_.attempted;
    try {
        if(io_.thread_id()!=game_thread_)return reject("hud_activation_wrong_thread");
        if(caller!=mouse_caller_rva)return result;
        if(io_.client)result.decision=shared_time::Decision::Suppress;
        if(busy_)return reject("hud_activation_reentrant");
        Busy busy(busy_);ActivationBinding binding;std::string error;ActivationContext before,after;
        if(!io_.context(before,error))return reject("hud_activation_context_failed: "+error);
        if(before.host==io_.client)return reject("hud_activation_role_changed");
        if(!reader_->classify_activation(type,item,binding,error))return reject(error);
        if(!binding.matched){
            if(result.decision==shared_time::Decision::Suppress)return reject("hud_activation_mouse_pair_not_owned");
            return result;
        }
        ++state_.matched;result.matched=true;result.origin_valid=true;result.origin=shared_time::Origin::Mouse;
        result.intent=intent(binding.scale);result.decision=shared_time::Decision::Suppress;
        if(!binding.visible||!binding.enabled)return reject("hud_activation_button_not_available");
        if(!ready(before))return reject("hud_activation_authority_not_ready");
        if(!before.host&&binding.native_selected_scale!=0)return reject("hud_activation_client_clock_not_zero");
        ActivationBinding current;
        if(!reader_->classify_activation(type,item,current,error))return reject(error);
        if(!(binding==current))return reject("hud_activation_binding_changed");
        if(!io_.context(after,error))return reject("hud_activation_context_failed: "+error);
        if(before!=after||!ready(after))return reject("hud_activation_authority_changed");
        const auto report=io_.submit(wire_intent(result.intent));
        state_.last_epoch=after.epoch;state_.last_sequence=report.sequence;
        if(!report.okay())return reject("hud_activation_submit_failed: "+report.cause);
        ++state_.submitted;state_.error.clear();result.request_queued=true;return result;
    }catch(const std::exception& cause){return reject(std::string("hud_activation_exception: ")+cause.what());}
     catch(...){return reject("hud_activation_unknown_exception");}
}
}
