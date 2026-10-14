#include "hud_time_writer.h"
#include <algorithm>
#include <cmath>
#include <exception>
namespace mt2mp::game::hud_time {
namespace {
struct Busy {bool& value;explicit Busy(bool& v):value(v){value=true;}~Busy(){value=false;}};
struct Scope {uintptr_t& slot;uintptr_t previous;Scope(uintptr_t& s,uintptr_t p):slot(s),previous(s){slot=p;}~Scope(){slot=previous;}};
bool fail(std::string& e,const char* cause){e=cause;return false;}
bool scale(float v){return v==0||v==1||v==3||v==9;}
}
bool Writer::initialize(const Config& config,const Services& reads,const WriterServices& io,std::string& e) {
    std::lock_guard lock(mutex_);
    if(busy_)return fail(e,"hud_writer_reentrant");Busy busy(busy_);
    if(attempted_)return fail(e,"hud_writer_initialize_already_attempted");attempted_=true;
    try {
        if(!io.authority||!io.store_i32||!io.call_clock)return fail(e,"hud_writer_missing_callback");
        io_=io;
        if(!reader_.initialize(config,reads,e))return false;
        // Window.Show is a separate hook; its unchanged entry is checked before installation.
        constexpr unsigned char expected[]{0x84,0xd2,0x75,0x09,0x80,0xb9,0x77,0x03,0,0,0,0x75,0x23};
        unsigned char actual[sizeof(expected)]{};
        if(!reader_.bytes(config.image+0x81ec20,actual,sizeof(actual),e))return false;
        if(!std::equal(std::begin(expected),std::end(expected),std::begin(actual)))return fail(e,"hud_writer_show_prologue_mismatch");
        status_.initialized=true;e.clear();return true;
    }catch(const std::exception& ex){e=std::string("hud_writer_initialize_exception: ")+ex.what();return false;}
     catch(...){return fail(e,"hud_writer_initialize_unknown_exception");}
}
bool Writer::current_authority(Authority& a,std::string& e) {
    if(!io_.authority(io_.user,a,e)){e="hud_writer_authority_read_failed: "+e;return false;}
    if(!a.available||!a.client){retire();return fail(e,"hud_writer_authority_not_client_ready");}
    if(!a.epoch||!a.revision||!scale(a.selected_scale)||!std::isfinite(a.pause_scale)||a.pause_scale<0||a.pause_scale>1)
        return fail(e,"hud_writer_authority_invalid");
    return true;
}
bool Writer::same_roots(const Reader::Witness& a,const Reader::Witness& b) const {
    // Pause-window creation may occur after binding; the current reader still checks its complete ownership.
    return std::equal(a.roots.begin(),a.roots.begin()+7,b.roots.begin());
}
bool Writer::preflight(State& state,Reader::Witness& witness,Authority& a,bool require_bound,std::string& e) {
    if(!status_.initialized)return fail(e,"hud_writer_not_initialized");
    if(reader_.io_.thread_id(reader_.io_.user)!=reader_.config_.game_thread)return fail(e,"hud_writer_wrong_thread");
    if(status_.locked)return fail(e,"hud_writer_locked_unknown_effect");
    if(status_.hook_mask!=7)return fail(e,"hud_writer_hooks_not_ready");
    if(require_bound&&!status_.bound)return fail(e,"hud_writer_not_bound");
    if(!current_authority(a,e))return false;
    if(a.epoch==retired_epoch_)return fail(e,"hud_writer_epoch_retired");
    if(require_bound&&a.epoch!=authority_.epoch){retire();return fail(e,"hud_writer_epoch_changed");}
    if(require_bound&&a.revision<authority_.revision)return fail(e,"hud_writer_revision_stale");
    if(require_bound&&a.revision==authority_.revision&&a!=authority_)return fail(e,"hud_writer_revision_payload_conflict");
    if(!reader_.read_bound(state,witness,e))return false;
    if(state.native_selected_scale!=0)return fail(e,"hud_writer_client_clock_not_zero");
    for(const auto& item:state.items)if(item.actor_owner_present)return fail(e,"hud_writer_hud_actor_owner_unsupported");
    if(require_bound&&!same_roots(binding_,witness)){retire();return fail(e,"hud_writer_world_or_panel_changed");}
    Authority after{};
    if(!current_authority(after,e))return false;
    if(a!=after)return fail(e,"hud_writer_authority_changed_during_read");
    return true;
}
bool Writer::bind(std::string& e) {
    std::lock_guard lock(mutex_);
    if(busy_)return fail(e,"hud_writer_reentrant");Busy busy(busy_);
    try {
        if(clock_scope_)return fail(e,"hud_writer_bind_inside_clock_scope");
        if(status_.bound)return fail(e,"hud_writer_already_bound");
        State state{};Reader::Witness witness{};Authority a{};
        if(!preflight(state,witness,a,false,e)){status_.error=e;return false;}
        binding_=witness;authority_=a;status_.bound=true;status_.epoch=a.epoch;status_.revision=a.revision;status_.error.clear();e.clear();return true;
    }catch(const std::exception& ex){e=std::string("hud_writer_bind_exception: ")+ex.what();status_.error=e;return false;}
     catch(...){status_.error="hud_writer_bind_unknown_exception";return fail(e,"hud_writer_bind_unknown_exception");}
}
bool Writer::suspend(const std::string& reason,std::string& e) {
    std::lock_guard lock(mutex_);
    if(busy_)return fail(e,"hud_writer_reentrant");Busy busy(busy_);
    if(clock_scope_)return fail(e,"hud_writer_suspend_inside_clock_scope");
    retire();status_.error=reason;e.clear();return true;
}
void Writer::retire(){if(status_.bound)retired_epoch_=authority_.epoch;status_.bound=false;binding_={};authority_={};}
bool Writer::set_hook_mask(uint32_t mask,std::string& e){
    std::lock_guard lock(mutex_);
    if(busy_)return fail(e,"hud_writer_reentrant");Busy busy(busy_);
    try {
        if(!status_.initialized)return fail(e,"hud_writer_not_initialized");
        if(reader_.io_.thread_id(reader_.io_.user)!=reader_.config_.game_thread)return fail(e,"hud_writer_wrong_thread");
        if(mask&~7u)return fail(e,"hud_writer_invalid_hook_mask");
        status_.hook_mask=mask;if(mask!=7){retire();status_.error="hud_writer_hook_coverage_lost";}e.clear();return true;
    }catch(const std::exception& ex){e=std::string("hud_writer_hook_mask_exception: ")+ex.what();return false;}
     catch(...){return fail(e,"hud_writer_hook_mask_unknown_exception");}
}
void Writer::lock_unknown(const std::string& cause){status_.locked=true;status_.error=cause;++status_.item_failed;}
UpdateResult Writer::on_update(void* type,void* item) {
    std::lock_guard lock(mutex_);
    UpdateResult result;
    if(busy_){result.error="hud_writer_reentrant";return result;}Busy busy(busy_);
    if(!status_.bound){++status_.item_forwarded;if(status_.locked)result.error="hud_writer_locked_unknown_effect";return result;}
    try {
        State state{};Reader::Witness witness{};Authority a{};
        if(!preflight(state,witness,a,true,result.error)){++status_.item_forwarded;status_.error=result.error;return result;}
        const auto at=std::find(witness.items.begin(),witness.items.end(),reinterpret_cast<uintptr_t>(item));
        if(at==witness.items.end()){++status_.item_forwarded;result.error.clear();return result;}
        const size_t index=static_cast<size_t>(at-witness.items.begin());
        if(witness.types[index]!=reinterpret_cast<uintptr_t>(type)){++status_.item_forwarded;result.error="hud_writer_type_item_mismatch";status_.error=result.error;return result;}
        result.forward_original=false;result.epoch=a.epoch;result.revision=a.revision;
        const int32_t desired=state.items[index].scale==a.selected_scale*a.pause_scale?0:1;
        if(state.items[index].native_state!=desired){
            result.attempted=true;++status_.item_attempted;
            if(!io_.store_i32(io_.user,*at+0x38,desired,result.error)){
                result.error="hud_writer_item_store_unknown: "+result.error;result.unknown=true;lock_unknown(result.error);return result;
            }
            result.changed=true;
        }
        State after{};Reader::Witness after_witness{};Authority after_a{};
        if(!preflight(after,after_witness,after_a,true,result.error)||a!=after_a||!(witness==after_witness)||after.items[index].native_state!=desired){
            if(result.error.empty())result.error="hud_writer_item_readback_changed";
            if(result.attempted){result.unknown=true;lock_unknown(result.error);}else{status_.error=result.error;++status_.item_failed;}
            return result;
        }
        authority_=a;status_.revision=a.revision;result.confirmed=true;++status_.item_confirmed;status_.error.clear();return result;
    }catch(const std::exception& ex){result.error=std::string("hud_writer_update_exception: ")+ex.what();}
     catch(...){result.error="hud_writer_update_unknown_exception";}
    if(result.attempted){result.unknown=true;lock_unknown(result.error);}else{++status_.item_failed;status_.error=result.error;}
    return result;
}
ShowResult Writer::on_show(void* window,bool native_visible) {
    std::lock_guard lock(mutex_);
    ShowResult result;result.visible=native_visible;
    if(busy_){result.error="hud_writer_reentrant";return result;}Busy busy(busy_);
    try {
        if(!clock_scope_){++status_.show_forwarded;return result;}
        State state{};Reader::Witness witness{};Authority a{};
        if(!preflight(state,witness,a,true,result.error)){++status_.show_forwarded;status_.error=result.error;return result;}
        if(clock_scope_!=witness.roots[5]||reinterpret_cast<uintptr_t>(window)!=witness.roots[7]||!state.pause_window_present){++status_.show_forwarded;return result;}
        result.visible=state.native_pause_indicator&&state.pause_option_enabled&&(a.selected_scale*a.pause_scale==0);
        result.replaced=true;result.epoch=a.epoch;result.revision=a.revision;++status_.show_replaced;
        authority_=a;status_.revision=a.revision;status_.error.clear();return result;
    }catch(const std::exception& ex){result.error=std::string("hud_writer_show_exception: ")+ex.what();}
     catch(...){result.error="hud_writer_show_unknown_exception";}
    status_.error=result.error;++status_.show_forwarded;return result;
}
bool Writer::clock_update(void* receiver,float dt,OriginalClockUpdate original,std::string& e) {
    std::lock_guard lock(mutex_);
    e.clear();
    if(!original)return fail(e,"hud_writer_missing_original_clock");
    if(!io_.call_clock)return fail(e,"hud_writer_missing_original_clock_forwarder");
    uintptr_t scope=0;
    if(!busy_&&!clock_scope_&&status_.bound){
        Busy busy(busy_);
        try {
            State state{};Reader::Witness witness{};Authority a{};
            if(preflight(state,witness,a,true,e)&&reinterpret_cast<uintptr_t>(receiver)==witness.roots[5])scope=witness.roots[5];
            else if(e.empty())e="hud_writer_foreign_clock_receiver";
        }catch(const std::exception& ex){e=std::string("hud_writer_clock_guard_exception: ")+ex.what();}
         catch(...){e="hud_writer_clock_guard_unknown_exception";}
    }else if(busy_||clock_scope_)e="hud_writer_clock_reentrant_scope_disabled";
    Scope frame(clock_scope_,scope);++status_.clock_entered;
    try {++status_.clock_forwarded;std::string cause;if(!io_.call_clock(io_.user,original,receiver,dt,cause)){e="hud_writer_original_clock_failed: "+cause;status_.error=e;return false;}}
    catch(const std::exception& ex){e=std::string("hud_writer_original_clock_exception: ")+ex.what();status_.error=e;return false;}
    catch(...){e="hud_writer_original_clock_unknown_exception";status_.error=e;return false;}
    if(!e.empty()){status_.error=e;return false;}
    return true;
}
}
