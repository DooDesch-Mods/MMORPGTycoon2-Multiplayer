#include "staging/shared_time.cpp"
#include <iostream>
#include <vector>
#include <stdexcept>
using namespace mt2mp::game::shared_time;
namespace {
int passed=0;
void check(bool okay,const char* label) {
    if (!okay) throw std::runtime_error(label);
    ++passed;
}
struct Fixture {
    static constexpr uintptr_t base=0x10000000, game=base+0x2100000, mode=game+0x1000,
        clock=mode+0x1000, player=clock+0x1000, inventory=player+0x1000, array=inventory+0x1000,
        item0=array+0x1000, type0=item0+0x10000, proxy0=type0+0x10000, name0=proxy0+0x1000;
    std::vector<unsigned char> memory=std::vector<unsigned char>(0x2400000);
    Bridge bridge;
    std::string error;
    uint32_t current=7;
    Context context{19,true};
    size_t writes=0, fail_write=0;
    bool compare_race=false, lie_write=false, context_error=false;
    static uintptr_t item(size_t i) { return item0+i*0x200; }
    static uintptr_t type(size_t i) { return type0+i*0x200; }
    template<class T> void put(uintptr_t p,T value) { std::memcpy(memory.data()+p-base,&value,sizeof(value)); }
    template<class T> T get(uintptr_t p) { T value{}; std::memcpy(&value,memory.data()+p-base,sizeof(value)); return value; }
    static bool read(void* u,uintptr_t p,void* result,size_t n,std::string& e) {
        auto& f=*static_cast<Fixture*>(u);
        if (p<base || p-base>f.memory.size() || n>f.memory.size()-(p-base)) { e="fixture_unmapped"; return false; }
        std::memcpy(result,f.memory.data()+p-base,n); return true;
    }
    static bool write(void* u,uintptr_t p,int32_t before,int32_t target,std::string& e) {
        auto& f=*static_cast<Fixture*>(u); ++f.writes;
        if (f.fail_write==f.writes) { e="fixture_injected_store_failure"; return false; }
        if (f.compare_race || f.get<int32_t>(p)!=before) { e="fixture_before_mismatch"; return false; }
        if (!f.lie_write) f.put(p,target);
        return true;
    }
    static uint32_t thread(void* u) { return static_cast<Fixture*>(u)->current; }
    static bool ctx(void* u,Context& out,std::string& e) {
        auto& f=*static_cast<Fixture*>(u);
        if (f.context_error) { e="fixture_context_failure"; return false; }
        out=f.context; return true;
    }
    Backend backend() { return {this,read,write,thread,ctx}; }
    Config config() { return {base,7,hash}; }
    Fixture() {
        for (const auto& g:guards) std::memcpy(memory.data()+g.rva,g.bytes,g.size);
        put(base+0x13df8f0,game); put(game,base+0x1743740); put(game+0x28,mode);
        put(base+0x13df320,mode); put(mode,base+0x16e6730); put(mode+0x2c4,int32_t(4));
        put(mode+0x304,uint8_t(0)); put(mode+0x58,base+0x2200000);
        put(base+0x13df9a0,clock); put(clock,base+0x17499d0); put(clock+0x58,0.0f);
        put(base+0x13df9f0,player); put(player,base+0x17873b0); put(player+0x1d0,inventory);
        put(inventory,base+0x16df0f0); put(inventory+8,array); put(inventory+0x10,int32_t(4));
        for (size_t i=0;i<4;++i) {
            put(array+i*8,item(i)); put(item(i),base+0x17437f0); put(item(i)+0x68,type(i));
            put(item(i)+0x70,proxy0+i*16); put(proxy0+i*16,type(i)+0x20);
            put(type(i),base+0x1723f50); put(type(i)+0x15c,scales[i]);
            put(type(i)+0x48,name0+i*32); put(type(i)+0x50,uint64_t(std::strlen(names[i])));
            std::memcpy(memory.data()+name0+i*32-base,names[i],std::strlen(names[i]));
            put(item(i)+0x38,int32_t(i==0?0:1));
        }
    }
    void init() { check(bridge.initialize(config(),backend(),error),"init"); }
    void bind() { init(); check(bridge.bind(19,error),"bind"); }
    void arm() { bind(); check(bridge.set_host_state(19,1,3,1,error),"host_state"); check(bridge.enable_capture(3,error),"capture_arm"); }
    HookResult activate(size_t i,uintptr_t caller=0x58095c) {
        return bridge.on_activate(reinterpret_cast<void*>(type(i)),reinterpret_cast<void*>(item(i)),base+caller);
    }
};
}
int main() {
try {
    {
        Fixture f; auto cfg=f.config(); cfg.verified_executable_sha256="wrong";
        check(!f.bridge.initialize(cfg,f.backend(),f.error) && f.error=="exact_build_receipt_required","wrong_hash");
        f.memory[0x20e6d0]=0xcc;
        check(!f.bridge.initialize(f.config(),f.backend(),f.error) && f.error.find("entry_bytes_mismatch")!=std::string::npos,"modified_entry");
    }
    {
        Fixture f; f.bind(); Presentation p{};
        check(f.bridge.read(p,f.error) && p.items[0].active && !p.items[1].active,"native_pause_read");
        check(f.writes==0,"reader_no_stores");
        const auto observed=f.activate(1);
        check(observed.origin_valid && observed.decision==Decision::Forward && !observed.request_queued &&
            observed.intent==Intent::Normal,"passive_origin_observation_forwards_without_queue");
        f.current=8; check(!f.bridge.read(p,f.error) && f.error=="wrong_game_thread","reader_wrong_thread");
        check(f.activate(1).error=="wrong_game_thread" && f.writes==0,"hook_wrong_thread");
    }
    {
        Fixture f; f.init(); f.put(Fixture::mode+0x2c4,int32_t(3));
        check(!f.bridge.bind(19,f.error) && f.error.find("active_world_state4_required actual_state=3")==0,"state3_guard");
        f.put(Fixture::mode+0x2c4,int32_t(4)); f.context.client=false;
        check(!f.bridge.bind(19,f.error) && f.error=="client_role_required","host_role_guard");
        f.context.client=true; f.put(Fixture::clock+0x58,1.0f);
        check(!f.bridge.bind(19,f.error) && f.error.find("client_native_clock_not_paused actual=1")==0,"running_client_guard");
    }
    {
        Fixture f; f.init(); f.put(Fixture::inventory+0x10,int32_t(4097));
        check(!f.bridge.bind(19,f.error) && f.error.find("inventory_count")!=std::string::npos,"count_bound");
        f.put(Fixture::inventory+0x10,int32_t(3));
        check(!f.bridge.bind(19,f.error) && f.error=="missing_time_item_index=3","missing_owned_item");
        f.put(Fixture::inventory+0x10,int32_t(4)); f.put(Fixture::array+24,Fixture::item(2));
        check(!f.bridge.bind(19,f.error) && f.error=="duplicate_time_item_scale","duplicate_scale");
    }
    {
        Fixture f; f.init(); f.put(Fixture::type(3)+0x15c,8.0f);
        check(!f.bridge.bind(19,f.error) && f.error=="time_item_scale_invalid","wrong_scale");
        f.put(Fixture::type(3)+0x15c,9.0f); f.put(Fixture::type(3)+0x50,uint64_t(9000));
        check(!f.bridge.bind(19,f.error) && f.error.find("name_scale")!=std::string::npos,"bounded_native_string");
    }
    {
        Fixture f; f.arm(); Presentation p{};
        check(f.bridge.read(p,f.error) && !p.items[0].active && p.items[2].active,"host_fast_presentation");
        check(f.get<float>(Fixture::clock+0x58)==0,"clock_not_written");
        check(f.get<int32_t>(Fixture::item(0)+0x38)==1,"pause_mouse_can_activate");
        const auto q=f.activate(0); check(q.request_queued && q.decision==Decision::Suppress,"pause_captured");
        Request r{}; bool available=false;
        check(f.bridge.pop_request(r,available,f.error) && available && r.intent==Intent::Pause && r.sequence==1,"typed_pause_queue");
        check(f.bridge.pop_request(r,available,f.error) && !available,"empty_queue_expected");
        check(f.bridge.set_host_state(19,1,3,1,f.error),"same_revision_idempotent");
        check(!f.bridge.set_host_state(19,1,9,1,f.error),"same_revision_conflict");
        check(!f.bridge.set_host_state(18,2,9,1,f.error),"host_wrong_epoch");
        check(f.bridge.set_host_state(19,2,0,1,f.error),"host_paused_state");
        auto update=f.bridge.on_update(reinterpret_cast<void*>(Fixture::type(1)),reinterpret_cast<void*>(Fixture::item(1)));
        check(update.presentation_confirmed && update.decision==Decision::Suppress,"update_uses_host_state");
        check(f.get<int32_t>(Fixture::item(1)+0x38)==1,"resume_normal_button_activates");
        check(f.activate(1).request_queued,"normal_resume_captured");
    }
    {
        Fixture f; f.arm();
        constexpr std::array<uintptr_t,4> direct_callers{0x4a2abe,0x4a2b05,0x4a2b4c,0x4a2b97};
        for (size_t i=0;i<4;++i) {
            const auto direct=f.activate(i,direct_callers[i]);
            check(direct.request_queued && direct.intent==static_cast<Intent>(i) && direct.origin==Origin::Keyboard,
                  "each_explicit_keyboard_scale_is_preserved");
        }
        check(f.bridge.unbind(f.error),"unbind_cancels_diagnostic_queue");
        Counters stats{};
        check(f.bridge.counters(stats,f.error) && stats.cancelled_on_unbind==4,"cancelled_requests_are_counted");
    }
    {
        Fixture f; f.arm();
        check(f.activate(1,0x4a2d93).request_queued,"keyboard_toggle_normal_path");
        check(f.activate(0,0x4a2a77).request_queued,"keyboard_toggle_pause_path");
        Request r{}; bool available=false;
        check(f.bridge.pop_request(r,available,f.error) && r.intent==Intent::TogglePauseNormal,"toggle_intent_preserved");
        check(!f.activate(2,0x4a2a77).request_queued,"toggle_wrong_item_reject");
        check(!f.activate(0,0x4a2d93).request_queued,"toggle_normal_caller_requires_normal_item");
        check(!f.activate(1,0x4a2a77).request_queued,"toggle_pause_caller_requires_pause_item");
        check(!f.activate(0,0x4a2b97).request_queued,"explicit_wrong_item_reject");
        check(f.activate(3,0x4a2b97).request_queued,"keyboard_fastest_path");
        const auto unknown=f.activate(1,0xd4cfe);
        check(!unknown.request_queued && unknown.decision==Decision::Suppress && unknown.error.find("origin")!=std::string::npos,"autonomous_origin_rejected");
        const auto other=f.bridge.on_activate(reinterpret_cast<void*>(Fixture::type(1)),reinterpret_cast<void*>(Fixture::item(0)),Fixture::base+0x58095c);
        check(!other.matched && other.decision==Decision::Forward,"foreign_pair_forward");
    }
    {
        Fixture f; f.arm(); for (int i=0;i<32;++i) check(f.activate(1).request_queued,"queue_fill");
        const auto full=f.activate(1);
        check(!full.request_queued && full.decision==Decision::Suppress && full.error.find("exhausted")!=std::string::npos,"queue_full_no_local_execution");
        Counters c{}; check(f.bridge.counters(c,f.error) && c.queue_full==1 && c.queued==32,"queue_failure_visible");
    }
    {
        Fixture f; f.bind(); check(!f.bridge.enable_capture(1,f.error),"partial_hooks_rejected");
        check(f.bridge.set_host_state(19,1,3,1,f.error),"partial_test_hoststate");
        f.fail_write=2; WriteReceipt receipt{};
        check(!f.bridge.write_presentation(receipt,f.error) && receipt.attempted==2 && receipt.completed==1 && receipt.partial_or_uncertain,"partial_store_visible");
        const auto writes=f.writes;
        check(!f.bridge.write_presentation(receipt,f.error) && f.writes==writes,"partial_store_not_replayed");
        check(f.error.find("requires_unbind")!=std::string::npos,"partial_recovery_explicit");
    }
    {
        Fixture f; f.bind(); check(f.bridge.set_host_state(19,1,3,1,f.error),"race_hoststate");
        f.compare_race=true; WriteReceipt receipt{};
        check(!f.bridge.write_presentation(receipt,f.error) && receipt.completed==0 && receipt.partial_or_uncertain,"compare_before_race");
    }
    {
        Fixture f; f.bind(); check(f.bridge.set_host_state(19,1,3,1,f.error),"readback_hoststate");
        f.lie_write=true; WriteReceipt receipt{};
        check(!f.bridge.write_presentation(receipt,f.error) && f.error.find("presentation_readback_state_mismatch item_index=0 actual=0 required=1")==0,"successful_callback_insufficient");
    }
    {
        Fixture f; f.arm(); f.context.epoch=20;
        check(f.activate(1).decision==Decision::Suppress && !f.activate(1).request_queued,"epoch_transition_blocks_bound_request");
        Request r{}; bool available=false;
        check(!f.bridge.pop_request(r,available,f.error) && !available,"epoch_transition_no_queue_delivery");
        check(f.bridge.unbind(f.error),"unbind_after_world_change");
    }
    {
        Fixture f; f.arm(); check(f.activate(1).request_queued,"first_binding_request");
        Request r{}; bool available=false;
        check(f.bridge.pop_request(r,available,f.error) && available && r.sequence==1,"first_binding_sequence");
        check(f.bridge.unbind(f.error) && f.bridge.bind(19,f.error),"same_epoch_explicit_rebind");
        check(f.bridge.set_host_state(19,1,3,1,f.error) && f.bridge.enable_capture(3,f.error),"same_epoch_capture_rearm");
        check(f.activate(1).request_queued && f.bridge.pop_request(r,available,f.error) && r.sequence==2,
              "same_epoch_rebind_does_not_reuse_sequence");
    }
    {
        Fixture f; f.arm(); f.put(Fixture::item(1)+0x38,int32_t(7));
        WriteReceipt r{};
        check(f.bridge.write_presentation(r,f.error) && f.get<int32_t>(Fixture::item(1)+0x38)==7,"unmatched_nonzero_state_preserved");
        f.put(Fixture::clock+0x58,1.0f);
        check(!f.activate(1).request_queued && f.activate(1).error.find("client_native_clock_not_paused actual=1")==0,"autonomous_clock_change_visible");
    }
    {
        float target=-1; std::string e;
        check(Bridge::resolve(Intent::TogglePauseNormal,3,1,target,e) && target==0,"host_toggle_running");
        check(Bridge::resolve(Intent::TogglePauseNormal,3,0,target,e) && target==1,"host_toggle_modal_zero");
        check(Bridge::resolve(Intent::Normal,0,1,target,e) && target==1,"host_resume_normal");
        check(Bridge::resolve(Intent::Fastest,1,1,target,e) && target==9,"host_explicit_fastest");
        check(!Bridge::resolve(static_cast<Intent>(99),1,1,target,e),"unknown_intent");
        check(!Bridge::resolve(Intent::Normal,1,std::numeric_limits<float>::quiet_NaN(),target,e),"host_nan_pause");
    }
#ifdef _WIN32
    {
        struct Page {
            void* p=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
            ~Page() { if (p && !VirtualFree(p,0,MEM_RELEASE)) std::cerr<<"error: backend fixture cleanup "<<GetLastError()<<'\n'; }
        } page;
        check(page.p!=nullptr,"allocate_backend_fixture");
        auto backend=process_backend(nullptr,Fixture::ctx);
        const auto address=reinterpret_cast<uintptr_t>(page.p); int32_t actual=0; std::string e;
        *static_cast<int32_t*>(page.p)=1;
        check(backend.read(nullptr,address,&actual,4,e) && actual==1,"windows_backend_read");
        check(backend.compare_write(nullptr,address,1,0,e),"windows_backend_compare_write");
        check(!backend.compare_write(nullptr,address,1,7,e) && *static_cast<int32_t*>(page.p)==0,"windows_expected_before_failure");
        check(!backend.compare_write(nullptr,address+1,0,1,e),"windows_unaligned_write_rejected");
        DWORD previous=0;
        check(VirtualProtect(page.p,4096,PAGE_READONLY,&previous)!=0,"protect_backend_fixture");
        check(!backend.compare_write(nullptr,address,0,1,e) && e=="unwritable_or_unaligned_item_state","windows_readonly_write_rejected");
        check(!backend.read(nullptr,1,&actual,4,e) && e=="unreadable_memory_region","windows_unmapped_read_rejected");
        check(guarded_copy(1,&actual,4)!=0,"windows_seh_read_retains_exception");
        LONG result=0;
        check(guarded_compare_exchange(address,0,1,result)!=0,"windows_seh_write_retains_exception");
    }
#endif
    std::cout<<"{\"passed\":"<<passed<<",\"failed\":0,\"runtime_accepted\":false}\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr<<"error: shared_time fixture: "<<e.what()<<" passed="<<passed<<'\n'; return 1;
}
}
