#define main client_regression_entry
#include "test_client_regression.cpp"
#undef main
#include <limits>
namespace {
void host(Fixture& f,float scale=3,float pause=1) {
    f.context.client=false;f.context.host=true;
    f.put(Fixture::clock+0x58,scale);f.put(Fixture::clock+0x5c,pause);f.bind();
}
void host_arm(Fixture& f,float scale=3,float pause=1) {
    host(f,scale,pause);check(f.bridge.set_host_state(19,1,scale,pause,f.error),"seed_host_observed_revision");
    check(f.bridge.enable_capture(1,f.error),"host_activate_only_hook_mask");
}
HookResult update(Fixture& f,size_t i) {return f.bridge.on_update(reinterpret_cast<void*>(Fixture::type(i)),reinterpret_cast<void*>(Fixture::item(i)));}
void native_item_update(Fixture& f,size_t i) {
    const float effective=f.get<float>(Fixture::clock+0x58)*f.get<float>(Fixture::clock+0x5c);
    const int32_t before=f.get<int32_t>(Fixture::item(i)+0x38);
    if(effective==scales[i]) f.put(Fixture::item(i)+0x38,int32_t(0));
    else if(before==0) f.put(Fixture::item(i)+0x38,int32_t(1));
}
}
#ifdef MT2MP_TIME_HOST_CAPTURE
namespace {
struct CountedHost : Fixture {
    size_t memory_reads=0, fail_at=0;
    uintptr_t mutate_at=0, mutate_field=0, mutate_value=0;
    static bool counted_read(void* user,uintptr_t at,void* out,size_t count,std::string& e) {
        auto& f=*static_cast<CountedHost*>(user);
        ++f.memory_reads;
        if(f.memory_reads==f.fail_at){e="focused_read_failure";return false;}
        if(at==f.mutate_at){f.put(f.mutate_field,f.mutate_value);f.mutate_at=0;}
        return Fixture::read(static_cast<Fixture*>(&f),at,out,count,e);
    }
    explicit CountedHost(size_t slots=4,size_t occupied=4) {
        context.client=false;context.host=true;put(clock+0x58,3.0f);put(clock+0x5c,1.0f);
        put(inventory+0x10,int32_t(slots));
        for(size_t i=4;i<slots;++i){
            put(array+i*8,i<occupied?item(i):uintptr_t{});
            if(i>=occupied)continue;
            put(item(i),base+0x17437f0);put(item(i)+0x68,type(4));
            put(item(i)+0x70,proxy0+64);put(proxy0+64,type(4)+0x20);
            put(type(4),base+0x123456);
        }
        auto io=backend();io.user=this;io.read=counted_read;
        check(bridge.initialize(config(),io,error),"combined_initialize_actual_body");
        check(bridge.bind(19,error),"combined_bind_actual_body");
        memory_reads=0;
    }
    bool combine(uint64_t revision=1,float selected=3,float pause=1,uint32_t mask=1) {
        return bridge.set_host_state_and_enable_capture(19,revision,selected,pause,mask,error);
    }
};
void combined_host_checks() {
    for(const auto shape:std::array<std::pair<size_t,size_t>,3>{{{4,4},{17,17},{17,6}}}){
        CountedHost f(shape.first,shape.second);
        const size_t scan_reads=56+shape.first+5*shape.second;
        check(f.bridge.set_host_state(19,1,3,1,f.error)&&f.bridge.enable_capture(1,f.error),"legacy_host_pair_success");
        check(f.memory_reads==2*scan_reads,"legacy_pair_two_full_scans_exact_reads");
        check(f.bridge.disable_capture(f.error),"combined_disable_before_same_revision");
        f.memory_reads=0;
        check(f.combine(),"combined_same_revision_rearms_capture");
        check(f.memory_reads==scan_reads&&f.writes==0,"combined_one_full_scan_exact_reads_no_stores");
        f.memory_reads=0;
        check(f.combine(1,3,1,3)&&f.memory_reads==scan_reads,"combined_next_call_scans_fresh_mask3");
        Presentation p;check(f.bridge.read(p,f.error)&&p.capture_enabled&&p.revision==1,"combined_capture_confirmed");
        std::cout<<"COUNT slots="<<shape.first<<" occupied="<<shape.second<<" legacy="<<2*scan_reads<<" combined="<<scan_reads<<'\n';
    }
    {
        CountedHost f;check(f.combine(),"combined_seed_revision");
        f.put(Fixture::clock+0x58,9.0f);
        check(!f.combine()&&f.error=="host_revision_receipt_differs_from_native_clock","same_revision_fresh_clock_mismatch");
        check(!f.combine(1,9)&&f.error=="stale_or_conflicting_host_revision","same_revision_conflicting_pair");
        check(f.combine(2,9,1,3),"new_revision_accepts_fresh_clock");
        check(!f.combine(1,9)&&f.error=="stale_or_conflicting_host_revision","older_revision_rejected");
        f.put(Fixture::clock+0x5c,0.0f);
        check(!f.combine(2,9)&&f.error=="host_revision_receipt_differs_from_native_clock","fresh_modal_pause_mismatch");
        check(f.combine(3,9,0),"fresh_modal_pause_accepted");
    }
    {
        CountedHost f;
        check(!f.combine(1,3,1,0)&&f.error=="host_activate_hook_required","combined_bad_mask_original_cause");
        Presentation p;check(f.bridge.read(p,f.error)&&p.host_state_available&&p.revision==1&&!p.capture_enabled,"bad_mask_retains_accepted_state_but_not_capture");
        check(f.combine(),"combined_enable_after_bad_mask");
        check(!f.combine(2,3,1,2)&&f.error=="host_activate_hook_required","enabled_bad_mask_original_cause");
        check(f.bridge.read(p,f.error)&&p.revision==2&&p.capture_enabled,"bad_mask_preserves_prior_capture");
        check(!f.bridge.set_host_state_and_enable_capture(20,3,3,1,1,f.error)&&f.error=="host_epoch_or_revision_invalid","combined_wrong_epoch");
        check(!f.combine(0)&&f.error=="host_epoch_or_revision_invalid","combined_zero_revision");
        check(!f.combine(3,3,std::numeric_limits<float>::quiet_NaN())&&f.error=="host_scale_invalid","combined_invalid_payload");
    }
    {
        CountedHost f;f.fail_at=40;
        check(!f.combine()&&f.error.find("focused_read_failure")!=std::string::npos&&f.writes==0,"combined_midscan_failure_retains_cause_no_effect");
        f.fail_at=0;Presentation p;check(f.bridge.read(p,f.error)&&!p.capture_enabled&&!p.host_state_available,"failed_scan_no_admission");
        check(f.combine(),"combined_recovery_fresh_read");
        f.put(Fixture::item(3)+0x70,uintptr_t{});
        check(!f.combine()&&f.error.find("item_type_weak_reference_not_live")==0,"same_revision_fresh_weak_guard");
    }
    {
        CountedHost f;f.mutate_at=Fixture::name0+3*32;f.mutate_field=Fixture::inventory+8;f.mutate_value=Fixture::array+8;
        check(!f.combine()&&f.error=="inventory_or_world_changed_during_scan","combined_keeps_end_header_read");
        CountedHost g;g.put(Fixture::array+24,Fixture::item(2));
        check(!g.combine()&&g.error=="duplicate_time_item_scale","combined_duplicate_time_item_guard");
        CountedHost h;h.put(Fixture::mode+0x58,Fixture::base+0x2210000);
        check(!h.combine()&&h.error=="bound_world_or_actor_changed","combined_live_world_guard");
    }
    {
        CountedHost f;f.current=8;
        check(!f.combine()&&f.error=="wrong_game_thread"&&f.memory_reads==0,"combined_wrong_thread_no_read");
        CountedHost g;g.context_error=true;
        check(!g.combine()&&g.error=="context_failed: fixture_context_failure","combined_original_context_failure");
        Fixture client;client.bind();
        check(!client.bridge.set_host_state_and_enable_capture(19,1,3,1,3,client.error)&&client.error=="local_host_capture_required"&&client.writes==0,"combined_rejects_client_without_effect");
        CountedHost h;check(h.combine(),"combined_seed_role_drift");h.context.host=false;h.context.client=true;
        check(!h.combine()&&h.error=="bound_role_changed_requires_explicit_bind","combined_keeps_role_unbind");
    }
    {
        CountedHost f;check(f.combine(),"combined_arm_mouse_keyboard");
        auto mouse=f.activate(3);check(mouse.request_queued&&mouse.decision==Decision::Suppress&&mouse.intent==Intent::Fastest,"combined_mouse_capture_unchanged");
        Request q;bool available=false;check(f.bridge.pop_request(q,available,f.error)&&available&&q.sequence==1,"combined_mouse_queue_once");
        auto key=f.activate(1,0x4a2d93);check(key.request_queued&&key.intent==Intent::TogglePauseNormal&&key.origin==Origin::Keyboard,"combined_keyboard_toggle_unchanged");
        check(f.bridge.pop_request(q,available,f.error)&&available&&q.sequence==2,"combined_keyboard_queue_once");
        check(update(f,1).decision==Decision::Forward&&f.writes==0,"combined_host_update_original_no_store");
    }
}
}

#endif
int main() {
try {
#ifdef MT2MP_TIME_HOST_CAPTURE
    combined_host_checks();
#endif
    {
        Fixture f;host(f);Presentation p;
        check(f.bridge.read(p,f.error)&&p.local_host&&p.selected_scale==3&&p.pause_scale==1&&p.effective_scale==3,"host_actual_clock_read");
        auto r=f.activate(1);check(r.decision==Decision::Forward&&r.origin_valid&&!r.request_queued,"host_passive_original_forward");
        check(!f.bridge.enable_capture(1,f.error)&&f.error=="bound_host_state_required","host_requires_observed_revision");
        check(!f.bridge.set_host_state(19,1,1,1,f.error)&&f.error=="host_revision_receipt_differs_from_native_clock","host_receipt_must_match_native");
        WriteReceipt receipt;check(!f.bridge.write_presentation(receipt,f.error)&&f.error=="host_presentation_is_native_original"&&f.writes==0,"host_presentation_writer_rejected");
    }
    {
        Fixture f;host_arm(f);auto r=f.activate(3);
        check(r.decision==Decision::Suppress&&r.request_queued&&r.intent==Intent::Fastest,"host_valid_mouse_capture");
        check(f.get<float>(Fixture::clock+0x58)==3&&f.writes==0,"capture_does_not_execute_or_store");
        Request q;bool available=false;check(f.bridge.pop_request(q,available,f.error)&&available&&q.sequence==1&&q.observed_host_revision==1,"host_request_once_available");
        check(f.bridge.pop_request(q,available,f.error)&&!available,"host_request_not_queued_twice");
        for(size_t i=0;i<4;++i) {
            r=update(f,i);check(r.decision==Decision::Forward&&!r.presentation_confirmed&&r.error.empty(),"host_update_always_original");
            native_item_update(f,i);
        }
        check(f.get<int32_t>(Fixture::item(2)+0x38)==0&&f.get<int32_t>(Fixture::item(0)+0x38)==1&&f.writes==0,"original_host_presentation_matches_clock");
    }
    {
        constexpr uintptr_t pcs[]{0x4a2a77,0x4a2d93,0x4a2abe,0x4a2b05,0x4a2b4c,0x4a2b97};
        constexpr size_t items[]{0,1,0,1,2,3};
        for(size_t i=0;i<6;++i) {
            Fixture f;host_arm(f);auto r=f.activate(items[i],pcs[i]);
            check(r.decision==Decision::Suppress&&r.request_queued&&r.origin==Origin::Keyboard,"host_native_keyboard_origin");
            check(r.intent==(i<2?Intent::TogglePauseNormal:static_cast<Intent>(i-2)),"host_keyboard_typed_intent");
        }
        Fixture f;host_arm(f);auto r=f.activate(3,0x4a2b05);
        check(!r.request_queued&&r.decision==Decision::Suppress&&!r.error.empty(),"known_host_item_wrong_origin_rejected");
    }
    {
        Fixture f;host_arm(f,9,0);Presentation p;check(f.bridge.read(p,f.error)&&p.effective_scale==0,"host_modal_pause_observed");
        for(size_t i=0;i<4;++i) {check(update(f,i).decision==Decision::Forward,"modal_update_original");native_item_update(f,i);}
        check(f.get<int32_t>(Fixture::item(0)+0x38)==0,"modal_pause_button_native_active");
        Request q;bool available=false;
        // An active native mouse item enters Deactivate, whose TimeScale body is RET.
        check(f.get<int32_t>(Fixture::item(0)+0x38)==0&&!f.activate(1,0).origin_valid,"fixture_active_state_and_unknown_origin");
        check(f.bridge.pop_request(q,available,f.error)&&!available,"active_noop_adds_no_synthetic_toggle");
        auto r=f.activate(1,0x4a2d93);check(r.request_queued&&r.intent==Intent::TogglePauseNormal,"modal_keyboard_preserves_toggle_intent");
        check(f.bridge.pop_request(q,available,f.error)&&available,"modal_toggle_request");
        float target=-1;check(Bridge::resolve(q.intent,9,0,target,f.error)&&target==1,"host_toggle_resolves_against_effective_pause");
        // The parent executor calls native set_clock; this fixture models its readback.
        f.put(Fixture::clock+0x58,target);
        check(f.get<float>(Fixture::clock+0x5c)==0,"host_replay_does_not_clear_modal_pause");
        check(f.bridge.set_host_state(19,2,1,0,f.error),"native_readback_revision_ack");
        check(f.bridge.pop_request(q,available,f.error)&&!available,"direct_clock_replay_does_not_recapture");
    }
    {
        Fixture f;host_arm(f);f.put(Fixture::clock+0x58,2.0f);
        auto r=f.activate(1);check(r.decision==Decision::Forward&&!r.request_queued&&r.error.find("host_native_scale_outside_capture_domain")==0,"unsupported_host_scale_visible_original_forward");
        check(update(f,1).decision==Decision::Forward,"unsupported_host_scale_update_original");
        Counters c;check(f.bridge.counters(c,f.error)&&c.host_original_fallback==1,"host_fallback_visible_count");
        check(f.get<float>(Fixture::clock+0x58)==2&&f.writes==0,"unsupported_host_scale_never_coerced");
    }
    {
        Fixture f;host_arm(f);f.activate(1);f.context.client=true;f.context.host=false;
        auto r=f.activate(1);check(r.decision==Decision::Forward&&r.error=="bound_role_changed_requires_explicit_bind"&&!r.request_queued,"role_change_invalidates_bound_capture");
        Counters c;check(f.bridge.counters(c,f.error)&&c.cancelled_on_unbind==1,"role_change_queue_cancel_visible");
        check(!f.bridge.enable_capture(3,f.error)&&f.error=="bound_host_state_required","no_auto_rebind");
        Presentation p;check(!f.bridge.read(p,f.error)&&f.error=="not_bound","role_change_binding_released");
    }
    {
        Fixture f;f.context.host=true;f.init();check(!f.bridge.bind(19,f.error)&&f.error=="ambiguous_host_client_role","ambiguous_role_rejected");
        Fixture client;client.arm();client.put(Fixture::clock+0x58,1.0f);
        auto r=client.activate(1);check(r.decision==Decision::Suppress&&!r.request_queued&&r.error.find("client_native_clock_not_paused")==0,"client_pause_guard_not_relaxed");
    }
    {
        Fixture f;host_arm(f);f.current=8;
        auto r=f.activate(1);check(r.decision==Decision::Forward&&r.error=="wrong_game_thread"&&!r.request_queued,"host_wrong_thread_visible_original");
        check(f.writes==0,"all_host_paths_no_item_stores");
    }
    std::cout<<"PASS "<<passed<<" host time checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: host fixture "<<e.what()<<'\n';return 1;}
}
