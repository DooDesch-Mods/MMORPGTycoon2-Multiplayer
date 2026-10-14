#include "native_time_input.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <SDL3/SDL.h>
#endif
namespace mt2mp::game::native_time_input {
namespace {
constexpr const char* hash="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
constexpr std::array<uint32_t,5> axes{21,7,8,9,10},scans{44,58,59,60,61},codes{32,0x4000003a,0x4000003b,0x4000003c,0x4000003d};
constexpr std::array<uintptr_t,5> origins{0,0x4a2abe,0x4a2b05,0x4a2b4c,0x4a2b97};
bool reject(std::string& e,const std::string& cause){e=cause;return false;}
bool token_valid(const std::string& s){if(s.size()!=36)return false;for(size_t i=0;i<s.size();++i){if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;}return true;}
bool same(const Request&a,const Request&b){return a.token==b.token&&a.key==b.key&&a.epoch==b.epoch&&a.revision==b.revision&&a.expected_sequence==b.expected_sequence;}
bool terminal(Phase p){return p==Phase::Confirmed||p==Phase::Rejected||p==Phase::Partial||p==Phase::Unknown;}
}
struct Adapter::Impl {
    Config config;Services services{};bool initialized=false,attempted=false,busy=false,blocked=false;
    std::recursive_mutex mutex;std::map<std::string,Receipt> memo;std::string active;
    Context bound{};std::array<uintptr_t,6> lease{};
    uintptr_t axis_data=0;int32_t axis_count=0;uint64_t last_timestamp=0;
    struct Busy{Impl& x;bool entered;Busy(Impl& v):x(v),entered(!v.busy){if(entered)x.busy=true;}~Busy(){if(entered)x.busy=false;}};
    bool thread(std::string& e){if(!initialized)return reject(e,"time_input_not_initialized");if(services.thread_id(services.user)!=config.game_thread)return reject(e,"time_input_wrong_thread");return true;}
    template<class T>bool get(uintptr_t p,T& v,std::string& e){if(!p||p>UINTPTR_MAX-sizeof(T))return reject(e,"time_input_address_overflow");return services.read(services.user,p,&v,sizeof(v),e);}
    bool vt(uintptr_t p,uintptr_t expected,std::string& e){uintptr_t actual=0;if(!get(p,actual,e))return false;if(actual!=config.image+expected)return reject(e,"time_input_vtable_mismatch expected_rva="+std::to_string(expected));return true;}
    bool context(Context& c,std::string& e){
        if(!services.context(services.user,c,e))return false;
        if(!c.owned_test)return reject(e,"time_input_owned_MPTest_required");
        if(!c.world_owned)return reject(e,"time_input_world_ownership_missing");
        if(!c.time_active)return reject(e,"time_input_runtime_not_active");
        if(!c.overlay_hidden)return reject(e,"time_input_overlay_visible");
        if(!c.no_pending_operation)return reject(e,"time_input_other_operation_pending");
        if(!c.owned_window||!c.window_id)return reject(e,"time_input_owned_window_missing");
        if(c.peer<1||c.peer>4)return reject(e,"time_input_peer_out_of_range");
        if(!c.epoch)return reject(e,"time_input_epoch_zero");
        if(!c.revision)return reject(e,"time_input_revision_zero");return true;
    }
    bool roots(std::array<uintptr_t,6>& v,std::string& e){
        uintptr_t other=0;int32_t state=0;uint8_t quitting=0;
        if(!get(config.image+0x13df8f0,v[0],e)||!vt(v[0],0x1743740,e)||!get(config.image+0x13df320,v[1],e)||!vt(v[1],0x16e6730,e)||!get(v[0]+0x28,other,e))return false;
        if(other!=v[1])return reject(e,"time_input_active_mode_mismatch");
        if(!get(v[1]+0x2c4,state,e)||!get(v[1]+0x304,quitting,e)||!get(v[1]+0x58,v[2],e))return false;
        if(state!=4||quitting||!v[2])return reject(e,"time_input_active_world_required");
        if(!get(config.image+0x13df990,v[3],e)||!vt(v[3],0x1749430,e)||!get(config.image+0x13df8b0,v[4],e)||!vt(v[4],0x1742750,e)||!get(v[1]+0x128,other,e))return false;
        if(other!=v[4])return reject(e,"time_input_hud_owner_mismatch");
        return get(v[4]+0x60,v[5],e);
    }
    struct Array {uintptr_t vptr=0,data=0;int32_t count=0,capacity=0;};
    bool array(uintptr_t p,uintptr_t expected,size_t stride,Array& a,std::string& e){
        if(!get(p,a,e))return false;
        if(a.vptr!=config.image+expected)return reject(e,"time_input_array_vtable_mismatch expected_rva="+std::to_string(expected));
        if(a.count<0)return reject(e,"time_input_array_negative_count actual="+std::to_string(a.count));
        if(a.capacity<a.count)return reject(e,"time_input_array_capacity_below_count actual="+std::to_string(a.capacity)+" count="+std::to_string(a.count));
        if(a.count>4096)return reject(e,"time_input_diagnostic_scan_incomplete count="+std::to_string(a.count));
        if(a.count&&(!a.data||uint64_t(a.count)>uint64_t(UINTPTR_MAX-a.data)/stride))return reject(e,"time_input_array_span_overflow");return true;
    }
    bool stable(uintptr_t p,const Array& a,std::string& e){Array b;if(!get(p,b,e))return false;return (a.vptr==b.vptr&&a.data==b.data&&a.count==b.count&&a.capacity==b.capacity)||reject(e,"time_input_array_changed_during_read");}
    bool scan(Readback& out,std::string& e,bool bind=false){
        out={};Context c;std::array<uintptr_t,6> r;if(!context(c,e)||!roots(r,e))return false;
        out.epoch=c.epoch;out.revision=c.revision;out.peer=c.peer;out.window_id=c.window_id;
        uint8_t panel=0,a6=0,a8=0;if(!get(r[5]+0x45f,panel,e)||!get(r[3]+0xa6,a6,e)||!get(r[3]+0xa8,a8,e))return false;
        out.panel_visible=panel==1;out.input_clear=a6==0&&a8==0;
        if(!out.panel_visible)return reject(e,"time_input_clock_panel_hidden");if(!out.input_clear)return reject(e,"time_input_native_input_blocked");
        Array input;if(!array(r[3]+0x48,0x17461c0,0x70,input,e))return false;out.axis_count=uint32_t(input.count);
        for(size_t k=0;k<5;++k){out.bindings[k].axis=axes[k];out.bindings[k].scancode=scans[k];}
        for(int32_t i=0;i<input.count;++i){
            const auto axis=input.data+size_t(i)*0x70;Array controls;float value=0;uint8_t customized=0;
            if(!array(axis+0x40,0x1746310,0x14,controls,e)||!get(axis+0x5c,value,e)||!get(axis+0x62,customized,e))return false;
            if(!std::isfinite(value)||customized>1)return reject(e,"time_input_axis_value_invalid");
            for(size_t k=0;k<5;++k)if(uint32_t(i)==axes[k]){out.bindings[k].customized=customized!=0;out.bindings[k].axis_value=value;}
            for(int32_t j=0;j<controls.count;++j){
                if(out.controls_scanned==4096)return reject(e,"time_input_diagnostic_scan_incomplete controls=4096");++out.controls_scanned;
                struct Control{int32_t type,code;uint8_t ctrl;uint8_t pad[3];int32_t extra1,extra2;} control{};
                static_assert(sizeof(Control)==0x14);
                if(!get(controls.data+size_t(j)*0x14,control,e))return false;
                if(control.type!=6&&control.type!=7)continue;
                if(control.ctrl>1)return reject(e,"time_input_control_modifier_invalid");
                if(control.ctrl)continue;
                for(size_t k=0;k<5;++k){
                    if(uint32_t(control.code)!=(control.type==6?scans[k]:codes[k]))continue;
                    if(uint32_t(i)==axes[k])++out.bindings[k].matches;
                    else {
                        ++out.bindings[k].collisions;++out.collision_count;
                        if(out.collision_sample_count<out.collision_samples.size())out.collision_samples[out.collision_sample_count++]={Key(k),axes[k],uint32_t(i),uint32_t(j),control.type,control.code,control.ctrl!=0,customized!=0,value};
                        else out.collision_samples_truncated=true;
                    }
                }
            }
            if(!stable(axis+0x40,controls,e))return false;
        }
        std::array<uintptr_t,6> again;Context after;
        if(!stable(r[3]+0x48,input,e)||!roots(again,e)||!context(after,e))return false;
        if(r!=again||c.epoch!=after.epoch||c.revision!=after.revision||c.peer!=after.peer||c.owned_window!=after.owned_window||c.window_id!=after.window_id)return reject(e,"time_input_read_lease_changed");
        out.scan_complete=true;
        for(auto& b:out.bindings){
            if(!b.matches)b.blocker="time_input_expected_binding_missing axis="+std::to_string(b.axis);
            else if(b.axis_value!=0)b.blocker="time_input_axis_already_held axis="+std::to_string(b.axis);
            else if(b.collisions){
                const auto end=out.collision_samples.begin()+out.collision_sample_count;
                const auto first=std::find_if(out.collision_samples.begin(),end,[&](const Collision& c){return c.expected_axis==b.axis;});
                std::string cause="time_input_binding_collision axis="+std::to_string(b.axis)+" count="+std::to_string(b.collisions);
                if(first!=end)cause+=" other_axis="+std::to_string(first->axis)+" control_index="+std::to_string(first->control_index)+" type="+std::to_string(first->control_type)+" code="+std::to_string(first->control_code);
                b.blocker=cause;
            }
            b.ready=b.blocker.empty();
        }
        if(bind){bound=c;lease=r;axis_data=input.data;axis_count=input.count;}return true;
    }
    bool lease_ok(std::string& e){Context c;std::array<uintptr_t,6> r;if(!context(c,e)||!roots(r,e))return false;return (r==lease&&c.epoch==bound.epoch&&c.peer==bound.peer&&c.window_id==bound.window_id&&c.owned_window==bound.owned_window)||reject(e,"time_input_request_lease_changed");}
    bool fail_active(const std::string& e){if(!active.empty()){auto& r=memo.at(active);if(!terminal(r.phase)){r.phase=Phase::Unknown;r.error=e;}blocked=true;}return false;}
};
Adapter::Adapter():impl_(std::make_unique<Impl>()){}Adapter::~Adapter()=default;
bool Adapter::initialize(const Config& c,const Services& s,std::string& e){auto& x=*impl_;std::lock_guard lock(x.mutex);Impl::Busy busy(x);if(!busy.entered)return reject(e,"time_input_reentrant");if(x.attempted)return reject(e,"time_input_initialize_already_attempted");x.attempted=true;
    try{if(!s.read||!s.context||!s.push||!s.thread_id||!s.timestamp_ns||!c.image||c.image>UINTPTR_MAX-0x1800000||!c.game_thread||c.executable_sha256!=hash)return reject(e,"time_input_config_build_guard");if(s.thread_id(s.user)!=c.game_thread)return reject(e,"time_input_wrong_thread");x.config=c;x.services=s;
        struct Guard{uintptr_t rva;const char* bytes;size_t size;};
        const Guard guards[]={{0x9e7f20,"\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53",12},{0x9e8120,"\x41\x57\x41\x56",4},{0x9e6e00,"\x48\x83\xec\x28\x48\x83\xc1\x48",8},{0x62ec50,"\x48\x8b\x41\x60\x0f\xb6\x80\x5f\x04\x00\x00\xc3",12}};
        for(const auto& g:guards){std::array<char,16> bytes{};if(!s.read(s.user,c.image+g.rva,bytes.data(),g.size,e))return false;if(std::memcmp(bytes.data(),g.bytes,g.size))return reject(e,"time_input_prologue_mismatch rva="+std::to_string(g.rva));}x.initialized=true;return true;
    }catch(const std::exception& ex){return reject(e,ex.what());}catch(...){return reject(e,"time_input_initialize_exception");}}
bool Adapter::read(Readback& out,std::string& e){auto& x=*impl_;std::lock_guard lock(x.mutex);Impl::Busy busy(x);if(!busy.entered)return reject(e,"time_input_reentrant");try{return x.thread(e)&&x.scan(out,e);}catch(const std::exception& ex){return reject(e,ex.what());}catch(...){return reject(e,"time_input_read_exception");}}
bool Adapter::queue(const Request& q,uint64_t frame,Receipt& out,std::string& e){auto& x=*impl_;std::lock_guard lock(x.mutex);Impl::Busy busy(x);if(!busy.entered)return reject(e,"time_input_reentrant");
    try{if(!x.thread(e))return false;auto found=x.memo.find(q.token);if(found!=x.memo.end()){out=found->second;if(!same(q,out.request))return reject(e,"time_input_token_payload_conflict");return reject(e,"time_input_token_already_attempted_no_replay");}
        if(x.blocked||!x.active.empty())return reject(e,"time_input_previous_effect_unresolved");if(x.memo.size()==64)return reject(e,"time_input_diagnostic_memo_full");
        if(!token_valid(q.token)||size_t(q.key)>=5||!q.epoch||!q.revision||!q.expected_sequence)return reject(e,"time_input_request_invalid");
        Readback state;if(!x.scan(state,e,true))return false;if(state.epoch!=q.epoch||state.revision!=q.revision)return reject(e,"time_input_expected_epoch_revision_mismatch");
        if(!state.bindings[size_t(q.key)].ready)return reject(e,state.bindings[size_t(q.key)].blocker);
        const auto stamp=x.services.timestamp_ns(x.services.user);if(!stamp||stamp==UINT64_MAX)return reject(e,"time_input_timestamp_unavailable");if(stamp<=x.last_timestamp)return reject(e,"time_input_timestamp_not_unique");
        Receipt fresh;fresh.request=q;fresh.phase=Phase::Queued;x.memo.emplace(q.token,fresh);x.active=q.token;x.last_timestamp=stamp+1;auto& r=x.memo.at(q.token);
        Event event{x.bound.window_id,scans[size_t(q.key)],codes[size_t(q.key)],true,x.bound.owned_window,stamp};
        r.down.attempted=true;r.down.queued_frame=frame;r.down.timestamp_ns=stamp;
        if(!x.services.push(x.services.user,event,e)){r.phase=Phase::Rejected;r.error=e.empty()?"SDL key down queue rejected without cause":e;x.active.clear();out=r;return reject(e,r.error);}r.down.queued=true;
        if(!x.lease_ok(e)){r.phase=Phase::Partial;r.error=e;x.blocked=true;out=r;return false;}
        r.up.attempted=true;r.up.queued_frame=frame;r.up.timestamp_ns=stamp+1;event.down=false;event.timestamp_ns=stamp+1;
        if(!x.services.push(x.services.user,event,e)){r.phase=Phase::Partial;r.error=e.empty()?"SDL key up queue rejected without cause":e;x.blocked=true;out=r;return reject(e,r.error);}r.up.queued=true;out=r;return true;
    }catch(const std::exception& ex){e=ex.what();x.fail_active(e);if(!x.active.empty())out=x.memo.at(x.active);return false;}catch(...){e="time_input_push_effect_unknown";x.fail_active(e);if(!x.active.empty())out=x.memo.at(x.active);return false;}}
bool Adapter::observe_poll(const Event& event,uint64_t frame,std::string& e){auto& x=*impl_;std::lock_guard lock(x.mutex);Impl::Busy busy(x);if(!busy.entered)return reject(e,"time_input_reentrant");try{if(!x.thread(e))return false;if(x.active.empty())return true;auto& r=x.memo.at(x.active);if(terminal(r.phase))return reject(e,"time_input_terminal_receipt");if(!x.lease_ok(e))return x.fail_active(e);
    const auto key=size_t(r.request.key);auto& edge=event.down?r.down:r.up;if(event.window_id!=x.bound.window_id||event.scancode!=scans[key]||event.keycode!=codes[key]||event.timestamp_ns!=edge.timestamp_ns)return true;
    if(!edge.queued||edge.poll_forwarded||frame<=edge.queued_frame||(!event.down&&!r.down.poll_forwarded)){e="time_input_duplicate_unqueued_or_reordered_event";return x.fail_active(e);}edge.poll_forwarded=true;edge.poll_frame=frame;return true;
    }catch(const std::exception& ex){e=ex.what();return x.fail_active(e);}catch(...){e="time_input_poll_exception";return x.fail_active(e);}}
bool Adapter::observe_leaf(uintptr_t caller,Key intent,bool matched,bool queued,uint64_t frame,std::string& e){auto& x=*impl_;std::lock_guard lock(x.mutex);Impl::Busy busy(x);if(!busy.entered)return reject(e,"time_input_reentrant");try{if(!x.thread(e))return false;if(x.active.empty())return true;auto& r=x.memo.at(x.active);if(terminal(r.phase))return reject(e,"time_input_terminal_receipt");if(!x.lease_ok(e))return x.fail_active(e);
    ++r.captured_count;r.caller_rva=caller;const auto key=size_t(r.request.key);const bool origin=key==0?(caller==0x4a2a77||caller==0x4a2d93):caller==origins[key];
    if(r.captured_count!=1||!matched||!queued||!origin||intent!=r.request.key||!r.down.poll_forwarded||frame<r.down.poll_frame){e="time_input_native_leaf_correlation_failed";return x.fail_active(e);}r.native_route_observed=true;r.phase=Phase::Observed;return true;
    }catch(const std::exception& ex){e=ex.what();return x.fail_active(e);}catch(...){e="time_input_leaf_exception";return x.fail_active(e);}}
bool Adapter::observe_frame(uint64_t frame,std::string& e){auto& x=*impl_;std::lock_guard lock(x.mutex);Impl::Busy busy(x);if(!busy.entered)return reject(e,"time_input_reentrant");try{if(!x.thread(e))return false;if(x.active.empty())return true;auto& r=x.memo.at(x.active);if(terminal(r.phase))return reject(e,"time_input_terminal_receipt");if(!x.lease_ok(e))return x.fail_active(e);
    Impl::Array a;if(!x.array(x.lease[3]+0x48,0x17461c0,0x70,a,e))return x.fail_active(e);
    if(a.data!=x.axis_data||a.count!=x.axis_count){e="time_input_axes_changed_during_request";return x.fail_active(e);}
    const auto axis=a.data+size_t(axes[size_t(r.request.key)])*0x70;uint8_t pressed=0,released=0;
    if(!x.get(axis+0x60,pressed,e)||!x.get(axis+0x61,released,e))return x.fail_active(e);
    if(pressed>1||released>1){e="time_input_axis_edge_invalid";return x.fail_active(e);}
    if(r.down.poll_forwarded&&r.down.poll_frame==frame&&pressed)r.native_down_consumed=true;
    if(r.up.poll_forwarded&&r.up.poll_frame==frame&&released)r.native_up_consumed=true;
    return true;
    }catch(const std::exception& ex){e=ex.what();return x.fail_active(e);}catch(...){e="time_input_frame_exception";return x.fail_active(e);}}
bool Adapter::confirm(const std::string& token,const HostReceipt& h,Receipt& out,std::string& e){auto& x=*impl_;std::lock_guard lock(x.mutex);Impl::Busy busy(x);if(!busy.entered)return reject(e,"time_input_reentrant");try{if(!x.thread(e))return false;auto it=x.memo.find(token);if(it==x.memo.end())return reject(e,"time_input_token_unknown");auto& r=it->second;out=r;if(terminal(r.phase))return reject(e,"time_input_terminal_receipt");if(token!=x.active)return reject(e,"time_input_not_active");if(!x.lease_ok(e)){x.fail_active(e);out=r;return false;}
    if(!r.down.poll_forwarded||!r.up.poll_forwarded||!r.native_down_consumed||!r.native_up_consumed||!r.native_route_observed)return reject(e,"time_input_waiting_for_native_observation");
    if(h.epoch!=r.request.epoch||h.peer!=x.bound.peer||h.sequence!=r.request.expected_sequence||h.observed_revision!=r.request.revision||h.intent!=r.request.key||!h.state_present||h.uncertain||h.confirmed==h.no_change||!h.error.empty()||h.revision<r.request.revision||!std::isfinite(h.selected_scale)||!std::isfinite(h.pause_scale)||h.pause_scale<0||(h.selected_scale!=0&&h.selected_scale!=1&&h.selected_scale!=3&&h.selected_scale!=9)){e=h.error.empty()?"time_input_host_receipt_rejected":h.error;x.fail_active(e);out=r;return false;}
    constexpr std::array<float,5> targets{0,0,1,3,9};
    if((r.request.key==Key::Space&&h.selected_scale!=0&&h.selected_scale!=1)||(r.request.key!=Key::Space&&h.selected_scale!=targets[size_t(r.request.key)])){e="time_input_host_receipt_target_mismatch";x.fail_active(e);out=r;return false;}
    r.host_confirmed=true;r.sequence=h.sequence;r.host_revision=h.revision;r.selected_scale=h.selected_scale;r.pause_scale=h.pause_scale;r.phase=Phase::Confirmed;out=r;x.active.clear();return true;
    }catch(const std::exception& ex){e=ex.what();x.fail_active(e);if(!x.active.empty())out=x.memo.at(x.active);return false;}catch(...){e="time_input_confirmation_exception";x.fail_active(e);return false;}}
bool Adapter::receipt(const std::string& token,Receipt& out,std::string& e){auto& x=*impl_;std::lock_guard lock(x.mutex);Impl::Busy busy(x);if(!busy.entered)return reject(e,"time_input_reentrant");try{if(!x.thread(e))return false;auto it=x.memo.find(token);if(it==x.memo.end())return reject(e,"time_input_token_unknown");out=it->second;return true;}catch(const std::exception& ex){return reject(e,ex.what());}catch(...){return reject(e,"time_input_receipt_exception");}}
bool Adapter::suspend(const std::string& reason,std::string& e){auto& x=*impl_;std::lock_guard lock(x.mutex);Impl::Busy busy(x);if(!busy.entered)return reject(e,"time_input_reentrant");try{if(!x.thread(e))return false;if(!x.active.empty()){e="time_input_suspended: "+reason;return x.fail_active(e);}return true;}catch(const std::exception& ex){return reject(e,ex.what());}catch(...){return reject(e,"time_input_suspend_exception");}}
}
