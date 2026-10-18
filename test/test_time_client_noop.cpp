#define main existing_client_regression_main
#include "host_time/test_client_regression.cpp"
#undef main

namespace {
struct CountedClient : Fixture {
    size_t memory_reads=0, fail_at=0;
    uintptr_t mutate_at=0, mutate_field=0, mutate_value=0;
    bool change_array_after_write=false;
    static bool counted_read(void* user,uintptr_t at,void* out,size_t count,std::string& e) {
        auto& f=*static_cast<CountedClient*>(user);++f.memory_reads;
        if(f.memory_reads==f.fail_at){e="noop_fixture_read_failure";return false;}
        if(at==f.mutate_at){f.put(f.mutate_field,f.mutate_value);f.mutate_at=0;}
        return Fixture::read(static_cast<Fixture*>(&f),at,out,count,e);
    }
    static bool counted_write(void* user,uintptr_t at,int32_t before,int32_t target,std::string& e) {
        auto& f=*static_cast<CountedClient*>(user);
        const bool okay=Fixture::write(static_cast<Fixture*>(&f),at,before,target,e);
        if(okay&&f.change_array_after_write)f.put(inventory+8,array+8);
        return okay;
    }
    explicit CountedClient(size_t slots=4,size_t occupied=4,float selected=3,float pause=1) {
        put(clock+0x58,0.0f);put(clock+0x5c,1.0f);put(inventory+0x10,int32_t(slots));
        for(size_t i=0;i<4;++i)put(item(i)+0x38,int32_t(scales[i]==selected*pause?0:7));
        for(size_t i=4;i<slots;++i){
            put(array+i*8,i<occupied?item(i):uintptr_t{});
            if(i>=occupied)continue;
            put(item(i),base+0x17437f0);put(item(i)+0x68,type(4));
            put(item(i)+0x70,proxy0+64);put(proxy0+64,type(4)+0x20);put(type(4),base+0x123456);
        }
        auto io=backend();io.user=this;io.read=counted_read;io.compare_write=counted_write;
        check(bridge.initialize(config(),io,error),"noop_initialize_actual_body");
        check(bridge.bind(19,error),"noop_bind_actual_body");
        check(bridge.set_host_state(19,1,selected,pause,error),"noop_host_state_actual_body");
        memory_reads=0;
    }
    bool write_now(WriteReceipt& receipt){return bridge.write_presentation(receipt,error);}
};
void equal_receipts(const WriteReceipt& r){
    check(r.attempted==0&&r.completed==0&&!r.partial_or_uncertain&&r.readback_confirmed,"noop_receipt_confirmed_without_attempt");
    check(r.before.epoch==r.after.epoch&&r.before.revision==r.after.revision&&r.before.capture_enabled==r.after.capture_enabled&&
          r.before.selected_scale==r.after.selected_scale&&r.before.pause_scale==r.after.pause_scale&&r.before.effective_scale==r.after.effective_scale,
          "noop_receipt_before_after_same_state");
    for(size_t i=0;i<4;++i)check(r.before.items[i].native_state==r.after.items[i].native_state&&
        r.before.items[i].active==r.after.items[i].active,"noop_receipt_each_item_unchanged");
}
}
int main(){try{
    for(const auto shape:std::array<std::pair<size_t,size_t>,3>{{{4,4},{17,17},{17,6}}}){
        CountedClient f(shape.first,shape.second);const size_t scan=54+shape.first+5*shape.second;
        WriteReceipt r;check(f.write_now(r)&&f.memory_reads==scan&&f.writes==0,"noop_one_full_scan_exact_reads");equal_receipts(r);
        Counters counters;check(f.bridge.counters(counters,f.error)&&counters.presentation_confirmed==1&&counters.presentation_failed==0,"noop_original_confirmed_counter");
        f.memory_reads=0;check(f.write_now(r)&&f.memory_reads==scan,"noop_next_call_fresh_scan");
        // A failure positioned solely in the removed second scan is intentionally not observed.
        f.memory_reads=0;f.fail_at=scan+1;check(f.write_now(r)&&f.memory_reads==scan,"noop_no_second_scan_only_failure_window");
        f.fail_at=0;f.memory_reads=0;f.put(Fixture::item(2)+0x38,int32_t(1));
        check(f.write_now(r)&&r.attempted==1&&r.completed==1&&r.readback_confirmed&&f.memory_reads==2*scan&&f.writes==1,"changed_next_call_keeps_both_scans_and_cas");
        std::cout<<"COUNT slots="<<shape.first<<" occupied="<<shape.second<<" noop="<<scan<<" changed="<<2*scan<<'\n';
    }
    {
        CountedClient f;check(f.bridge.enable_capture(3,f.error),"noop_enable_original_client_capture");
        f.memory_reads=0;const auto writes=f.writes;
        for(size_t i=0;i<4;++i){const auto r=f.bridge.on_update(reinterpret_cast<void*>(Fixture::type(i)),reinterpret_cast<void*>(Fixture::item(i)));
            check(r.matched&&r.decision==Decision::Suppress&&r.presentation_confirmed&&r.error.empty(),"noop_each_client_update_still_confirmed_suppressed");}
        check(f.memory_reads==4*78&&f.writes==writes,"four_noop_updates_four_scans_no_cas");
        const auto r=f.activate(1);check(r.request_queued&&r.decision==Decision::Suppress&&r.intent==Intent::Normal,"noop_client_click_still_queued");
    }
    for(float scale:std::array<float,4>{0,1,3,9}){
        CountedClient f(4,4,scale);WriteReceipt r;
        check(f.write_now(r)&&f.memory_reads==78&&f.writes==0,"noop_all_supported_host_scales");equal_receipts(r);
    }
    {
        CountedClient f(4,4,9,0);WriteReceipt r;
        check(f.write_now(r)&&r.after.effective_scale==0&&r.after.items[0].active&&f.writes==0,"noop_modal_pause_preserved");
        f.memory_reads=0;f.fail_at=1;
        check(!f.write_now(r)&&f.error.find("noop_fixture_read_failure")!=std::string::npos&&!r.attempted,"noop_next_call_read_failure_not_cached");
        f.fail_at=0;f.put(Fixture::clock+0x58,1.0f);
        check(!f.write_now(r)&&f.error.find("client_native_clock_not_paused")==0&&!r.attempted,"noop_next_call_clock_guard");
    }
    {
        CountedClient f;f.put(Fixture::mode+0x58,Fixture::base+0x2210000);WriteReceipt r;
        check(!f.write_now(r)&&f.error=="bound_world_or_actor_changed"&&!r.attempted,"noop_fresh_owner_rejected");
        CountedClient g;g.put(Fixture::item(3)+0x70,uintptr_t{});
        check(!g.write_now(r)&&g.error.find("item_type_weak_reference_not_live")==0&&!r.attempted,"noop_fresh_weak_rejected");
        CountedClient h;h.mutate_at=Fixture::name0+3*32;h.mutate_field=Fixture::inventory+8;h.mutate_value=Fixture::array+8;
        check(!h.write_now(r)&&h.error=="inventory_or_world_changed_during_scan"&&!r.attempted,"noop_keeps_first_scan_end_header_guard");
    }
    {
        CountedClient f;f.put(Fixture::item(2)+0x38,int32_t(1));f.compare_race=true;WriteReceipt r;
        check(!f.write_now(r)&&r.attempted==1&&r.completed==0&&r.partial_or_uncertain&&f.error=="presentation_compare_write_failed: fixture_before_mismatch","cas_race_original_failure");
        const auto writes=f.writes;f.memory_reads=0;
        check(!f.write_now(r)&&f.error=="previous_presentation_write_uncertain_requires_unbind"&&f.memory_reads==0&&f.writes==writes,"cas_race_no_replay");
    }
    {
        CountedClient f;f.put(Fixture::item(0)+0x38,int32_t(0));f.put(Fixture::item(2)+0x38,int32_t(1));f.fail_write=2;WriteReceipt r;
        check(!f.write_now(r)&&r.attempted==2&&r.completed==1&&r.partial_or_uncertain&&f.error=="presentation_compare_write_failed: fixture_injected_store_failure","partial_cas_original_failure");
    }
    {
        CountedClient f;f.put(Fixture::item(2)+0x38,int32_t(1));f.fail_at=79;WriteReceipt r;
        check(!f.write_now(r)&&r.attempted==1&&r.completed==1&&r.partial_or_uncertain&&f.memory_reads==79&&f.error.find("presentation_readback_failed: ")==0&&f.error.find("noop_fixture_read_failure")!=std::string::npos,"attempted_cas_keeps_second_scan_failure");
        Counters counters;check(f.bridge.counters(counters,f.error)&&counters.presentation_failed==1&&counters.presentation_confirmed==0,"attempted_readback_failure_counter_preserved");
    }
    {
        CountedClient f;f.put(Fixture::item(2)+0x38,int32_t(1));f.lie_write=true;WriteReceipt r;
        check(!f.write_now(r)&&r.attempted==1&&r.completed==1&&r.partial_or_uncertain&&f.memory_reads==156&&f.error.find("presentation_readback_state_mismatch item_index=2 actual=1 required=0")==0,"successful_cas_callback_requires_fresh_value_readback");
        CountedClient g;g.put(Fixture::item(2)+0x38,int32_t(1));g.change_array_after_write=true;
        check(!g.write_now(r)&&r.attempted==1&&r.completed==1&&r.partial_or_uncertain&&g.error.find("presentation_readback_failed: ")==0,"cas_callback_array_change_rejected");
    }
    {
        CountedClient f;f.current=8;WriteReceipt r;
        check(!f.write_now(r)&&f.error=="wrong_game_thread"&&f.memory_reads==0&&f.writes==0,"noop_wrong_thread_no_reads_or_effects");
        CountedClient g;g.context_error=true;
        check(!g.write_now(r)&&g.error=="context_failed: fixture_context_failure"&&!r.attempted,"noop_original_context_error");
    }
    std::cout<<"PASS "<<passed<<" Client no-store time checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: Client no-store fixture "<<e.what()<<'\n';return 1;}}
