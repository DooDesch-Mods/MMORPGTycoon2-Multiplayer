#include "shared_time.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mt2mp::game::shared_time {
namespace {
constexpr char hash[]="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
constexpr std::array<float,4> scales{0,1,3,9};
constexpr std::array<const char*,4> names{"@Time Pause","@Time Normal","@Time Fast","@Time Fastest"};
bool scale_valid(float s) { return std::find(scales.begin(),scales.end(),s)!=scales.end(); }
bool failure(std::string& error,const std::string& reason) { error=reason; return false; }
struct Guard { uintptr_t rva; const char* bytes; size_t size; };
constexpr Guard guards[]{
    {0x20e6d0,"\x41\x54\x55\x57\x56\x53\x48\x81\xec\x90\x00\x00\x00\x48\x89\xcb",16},
    {0x20e8d0,"\x48\x8b\x05\xc9\x10\x1d\x01\x48\x85\xc0\x74\x25\xf3\x0f\x10\x40",16},
    {0x207820,"\x53\x48\x83\xec\x20\x48\x89\xcb\x48\x8b\x49\x70\x48\x85\xc9\x74",16},
    {0xd2fc0,"\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x83\xec\x30\x0f\x11",16},
    {0x206df0,"\x56\x53\x48\x83\xec\x28\x48\x63\xda\x48\x89\xce\x85\xdb\x78\x05",16},
    {0x580957,"\xe8\xc4\x6e\xc8\xff",5},
    {0x4a2a72,"\xe8\x49\x06\xc3\xff",5},
    {0x4a2d8e,"\xe8\x2d\x03\xc3\xff",5},
    {0x4a2ab9,"\xe8\x02\x06\xc3\xff",5},
    {0x4a2b00,"\xe8\xbb\x05\xc3\xff",5},
    {0x4a2b47,"\xe8\x74\x05\xc3\xff",5},
    {0x4a2b92,"\xe8\x29\x05\xc3\xff",5},
};
}
bool Bridge::bytes(uintptr_t p,void* output,size_t n,std::string& e) const {
    if (!p || !n || p>UINTPTR_MAX-(n-1)) return failure(e,"memory_range_overflow");
    if (!io_.read(io_.user,p,output,n,e)) {
        e="memory_read_failed: "+e; return false;
    }
    return true;
}
bool Bridge::thread(std::string& e) const {
    e.clear();
    if (!ready_) return failure(e,"resolver_not_initialized");
    if (io_.thread_id(io_.user)!=config_.game_thread) return failure(e,"wrong_game_thread");
    return true;
}
bool Bridge::vtable(uintptr_t p,uintptr_t expected,const char* label,std::string& e) const {
    uintptr_t actual=0;
    if (!at(p,0,actual,e)) { e=std::string(label)+"_vtable_unreadable: "+e; return false; }
    return actual==config_.image+expected || failure(e,std::string(label)+"_vtable_mismatch");
}
bool Bridge::initialize(const Config& c,const Backend& backend,std::string& e) {
    e.clear();
    if (ready_) return failure(e,"already_initialized");
    if (!backend.read || !backend.compare_write || !backend.thread_id || !backend.context)
        return failure(e,"incomplete_backend");
    if (!c.image || c.image>UINTPTR_MAX-0x2000000 || !c.game_thread || c.verified_executable_sha256!=hash)
        return failure(e,"exact_build_receipt_required");
    config_=c; io_=backend;
    for (const auto& guard:guards) {
        std::array<char,16> actual{};
        if (!bytes(c.image+guard.rva,actual.data(),guard.size,e)) return false;
        if (std::memcmp(actual.data(),guard.bytes,guard.size))
            return failure(e,"entry_bytes_mismatch_rva="+std::to_string(guard.rva));
    }
    ready_=true; e.clear(); return true;
}
bool Bridge::live(uintptr_t& world,uintptr_t& player,uintptr_t& inventory,std::string& e) {
    if (!thread(e)) return false;
    Context context{};
    if (!io_.context(io_.user,context,e)) { e="context_failed: "+e; return false; }
    if (bound_ && (context.host!=bound_host_ || context.client==context.host)) {
        if (!unbind(e)) return false;
        return failure(e,"bound_role_changed_requires_explicit_bind");
    }
    if (context.client==context.host)
        return failure(e,context.host?"ambiguous_host_client_role":"client_role_required");
    if (!context.epoch || context.epoch!=epoch_) return failure(e,"world_epoch_mismatch");
    uintptr_t game=0, mode=0, active=0, clock=0;
    int32_t state=-1; uint8_t quitting=1; float scale=-1;
    if (!at(config_.image,0x13df8f0,game,e) || !vtable(game,0x1743740,"game",e) ||
        !at(config_.image,0x13df320,mode,e) || !vtable(mode,0x16e6730,"mode",e) ||
        !at(game,0x28,active,e)) return false;
    if (active!=mode) return failure(e,"active_mode_mismatch");
    if (!at(mode,0x2c4,state,e) || !at(mode,0x304,quitting,e) || !at(mode,0x58,world,e)) return false;
    if (state!=4 || quitting || !world) return failure(e,"active_world_state4_required actual_state="+
        std::to_string(state)+" quitting="+std::to_string(quitting)+" world_present="+std::to_string(world!=0));
    if (!at(config_.image,0x13df9a0,clock,e) || !vtable(clock,0x17499d0,"clock",e) || !at(clock,0x58,scale,e)) return false;
    if (context.client && scale!=0) return failure(e,"client_native_clock_not_paused actual="+std::to_string(scale)+" required=0");
    if (context.host) {
        float pause=0;
        if (!at(clock,0x5c,pause,e)) return false;
        if (!scale_valid(scale) || !std::isfinite(pause) || pause<0 || !std::isfinite(scale*pause))
            return failure(e,"host_native_scale_outside_capture_domain selected="+std::to_string(scale)+" pause="+std::to_string(pause));
        observed_scale_=scale; observed_pause_=pause;
    }
    observed_host_=context.host;
    if (!at(config_.image,0x13df9f0,player,e) || !vtable(player,0x17873b0,"player",e) ||
        !at(player,0x1d0,inventory,e) || !vtable(inventory,0x16df0f0,"inventory",e)) return false;
    if (bound_ && (world!=world_ || player!=player_ || inventory!=inventory_))
        return failure(e,"bound_world_or_actor_changed");
    return true;
}
bool Bridge::scan(std::array<Handle,4>& result,bool require_bound,std::string& e) {
    uintptr_t world=0,player=0,inventory=0,array=0;
    if (!live(world,player,inventory,e)) return false;
    const bool initial_host=observed_host_;
    int32_t count=0;
    if (!at(inventory,8,array,e) || !at(inventory,0x10,count,e)) return false;
    if (!array || count<=0 || count>4096) return failure(e,"inventory_count_outside_diagnostic_bound_1_4096 actual="+
        std::to_string(count)+" array_present="+std::to_string(array!=0));
    result={};
    for (int32_t slot=0;slot<count;++slot) {
        uintptr_t item=0,type=0,proxy=0,target=0,table=0;
        if (!at(array,size_t(slot)*8,item,e)) return false;
        if (!item) continue;
        if (!vtable(item,0x17437f0,"inventory_item",e) || !at(item,0x68,type,e) || !at(item,0x70,proxy,e)) return false;
        if (!type || !proxy || !at(proxy,0,target,e) || !target)
            return failure(e,"item_type_weak_reference_not_live: "+e);
        if (!at(type,0,table,e)) return false;
        if (table!=config_.image+0x1723f50) continue;
        float scale=0; uintptr_t name=0; uint64_t length=0; int32_t state=0;
        if (!at(type,0x15c,scale,e) || !at(type,0x48,name,e) || !at(type,0x50,length,e) || !at(item,0x38,state,e)) return false;
        const auto found=std::find(scales.begin(),scales.end(),scale);
        if (found==scales.end()) return failure(e,"time_item_scale_invalid");
        const auto index=size_t(found-scales.begin());
        const auto expected_length=std::strlen(names[index]); std::array<char,16> value{};
        if (length!=expected_length || !bytes(name,value.data(),expected_length,e) ||
            std::memcmp(value.data(),names[index],expected_length)) return failure(e,"time_item_name_scale_mismatch: "+e);
        if (result[index].item) return failure(e,"duplicate_time_item_scale");
        result[index]={item,type,scale,state};
    }
    for (size_t i=0;i<4;++i) {
        if (!result[i].item) return failure(e,"missing_time_item_index="+std::to_string(i));
        if (require_bound && (result[i].item!=handles_[i].item || result[i].type!=handles_[i].type))
            return failure(e,"bound_time_item_replaced");
    }
    uintptr_t end_world=0,end_player=0,end_inventory=0,end_array=0; int32_t end_count=0;
    if (!live(end_world,end_player,end_inventory,e) || !at(inventory,8,end_array,e) || !at(inventory,0x10,end_count,e)) return false;
    if (initial_host!=observed_host_) return failure(e,"role_changed_during_scan");
    if (end_world!=world || end_player!=player || end_inventory!=inventory || end_array!=array || end_count!=count)
        return failure(e,"inventory_or_world_changed_during_scan");
    return true;
}
void Bridge::describe(const std::array<Handle,4>& h,Presentation& p) const {
    p={}; p.epoch=epoch_; p.revision=revision_; p.host_state_available=host_available_; p.capture_enabled=capture_;
    p.local_host=bound_host_;
    p.selected_scale=bound_host_?observed_scale_:selected_;
    p.pause_scale=bound_host_?observed_pause_:pause_;
    p.effective_scale=p.selected_scale*p.pause_scale;
    for (size_t i=0;i<4;++i) p.items[i]={h[i].scale,h[i].state,h[i].state==0};
}
bool Bridge::bind(uint64_t epoch,std::string& e) {
    if (!thread(e)) return false;
    if (bound_) return failure(e,"already_bound_requires_explicit_unbind");
    epoch_=epoch; std::array<Handle,4> h{};
    if (!scan(h,false,e) || !live(world_,player_,inventory_,e)) return false;
    Context context{};
    if (!io_.context(io_.user,context,e)) return failure(e,"bind_context_failed: "+e);
    if (context.client==context.host || context.epoch!=epoch || context.host!=observed_host_) return failure(e,"bind_role_or_epoch_changed");
    bound_host_=context.host; handles_=h; bound_=true; e.clear(); return true;
}
bool Bridge::unbind(std::string& e) {
    if (!thread(e)) return false;
    counters_.cancelled_on_unbind+=queue_.size();
    bound_=false; capture_=false; host_available_=false; uncertain_=false; bound_host_=false;
    epoch_=revision_=0; world_=player_=inventory_=0; handles_={}; queue_.clear(); e.clear(); return true;
}
bool Bridge::read(Presentation& out,std::string& e) {
    out={}; if (!thread(e)) return false;
    if (!bound_) return failure(e,"not_bound");
    std::array<Handle,4> h{}; if (!scan(h,true,e)) return false;
    describe(h,out); e.clear(); return true;
}
bool Bridge::set_host_state(uint64_t epoch,uint64_t revision,float selected,float pause,std::string& e) {
    Presentation current{}; if (!read(current,e)) return false;
    if (epoch!=epoch_ || !revision) return failure(e,"host_epoch_or_revision_invalid");
    if (!scale_valid(selected) || !std::isfinite(pause) || pause<0 || !std::isfinite(selected*pause))
        return failure(e,"host_scale_invalid");
    if (bound_host_ && (selected!=current.selected_scale || pause!=current.pause_scale))
        return failure(e,"host_revision_receipt_differs_from_native_clock");
    if (host_available_ && revision<=revision_) {
        if (revision==revision_ && selected==selected_ && pause==pause_) { e.clear(); return true; }
        return failure(e,"stale_or_conflicting_host_revision");
    }
    selected_=selected; pause_=pause; revision_=revision; host_available_=true; e.clear(); return true;
}
bool Bridge::write_presentation(WriteReceipt& receipt,std::string& e) {
    receipt={}; if (!thread(e)) return false;
    if (bound_host_) return failure(e,"host_presentation_is_native_original");
    if (!bound_ || !host_available_) return failure(e,"bound_host_state_required");
    if (uncertain_) return failure(e,"previous_presentation_write_uncertain_requires_unbind");
    std::array<Handle,4> h{}; if (!scan(h,true,e)) return false;
    describe(h,receipt.before);
    for (size_t i=0;i<4;++i) {
        const bool match=h[i].scale==selected_*pause_;
        const int32_t target=match?0:(h[i].state==0?1:h[i].state);
        if (target==h[i].state) continue;
        ++receipt.attempted;
        if (!io_.compare_write(io_.user,h[i].item+0x38,h[i].state,target,e)) {
            uncertain_=true; receipt.partial_or_uncertain=true;
            ++counters_.presentation_failed; counters_.last_error="presentation_compare_write_failed: "+e;
            e=counters_.last_error; return false;
        }
        ++receipt.completed;
    }
    if (!scan(h,true,e)) {
        uncertain_=receipt.attempted!=0; receipt.partial_or_uncertain=uncertain_;
        ++counters_.presentation_failed; counters_.last_error="presentation_readback_failed: "+e;
        e=counters_.last_error; return false;
    }
    describe(h,receipt.after);
    for (size_t i=0;i<4;++i) {
        const bool match=h[i].scale==selected_*pause_;
        const int32_t prior=receipt.before.items[i].native_state;
        const int32_t target=match?0:(prior==0?1:prior);
        if (h[i].state!=target) {
            uncertain_=true; receipt.partial_or_uncertain=true;
            ++counters_.presentation_failed; counters_.last_error="presentation_readback_state_mismatch item_index="+
                std::to_string(i)+" actual="+std::to_string(h[i].state)+" required="+std::to_string(target);
            return failure(e,counters_.last_error);
        }
    }
    receipt.readback_confirmed=true; ++counters_.presentation_confirmed; e.clear(); return true;
}
bool Bridge::enable_capture(uint32_t mask,std::string& e) {
    if (!thread(e)) return false;
    if (bound_host_) {
        if (mask!=1 && mask!=3) return failure(e,"host_activate_hook_required");
        Presentation current{}; if (!read(current,e)) return false;
        if (!host_available_) return failure(e,"bound_host_state_required");
        capture_=true; e.clear(); return true;
    }
    if (mask!=3) return failure(e,"both_time_item_hooks_required");
    WriteReceipt receipt{}; if (!write_presentation(receipt,e)) return false;
    capture_=true; e.clear(); return true;
}
bool Bridge::disable_capture(std::string& e) { if (!thread(e)) return false; capture_=false; e.clear(); return true; }
HookResult Bridge::reject(HookResult r,const std::string& e) {
    r.error=e; ++counters_.rejected; counters_.last_error=e; return r;
}
HookResult Bridge::on_activate(void* type,void* item,uintptr_t pc) {
    HookResult r{}; std::string e;
    if (!thread(e)) { r.error=e; return r; }
    if (!bound_) return r;
    const auto t=reinterpret_cast<uintptr_t>(type), p=reinterpret_cast<uintptr_t>(item);
    const auto found=std::find_if(handles_.begin(),handles_.end(),[&](const Handle& h){return h.type==t && h.item==p;});
    if (found==handles_.end()) return r;
    r.matched=true; r.decision=capture_?Decision::Suppress:Decision::Forward;
    const bool was_host=bound_host_;
    std::array<Handle,4> h{};
    if (!scan(h,true,e)) {
        if (was_host || !bound_) {
            r.decision=Decision::Forward;
            if (was_host) ++counters_.host_original_fallback;
        }
        return reject(r,e);
    }
    if (uncertain_) return reject(r,"presentation_uncertain");
    const size_t index=size_t(found-handles_.begin());
    Request q{epoch_,0,revision_,static_cast<Intent>(index),Origin::Keyboard};
    const uintptr_t caller=pc>=config_.image?pc-config_.image:UINTPTR_MAX;
    if (caller==0x58095c) q.origin=Origin::Mouse;
    else if (caller==0x4a2a77 || caller==0x4a2d93) {
        if (index!=(caller==0x4a2a77?0u:1u)) return reject(r,"toggle_caller_item_mismatch");
        q.intent=Intent::TogglePauseNormal;
    } else {
        constexpr std::array<uintptr_t,4> direct{0x4a2abe,0x4a2b05,0x4a2b4c,0x4a2b97};
        if (direct[index]!=caller) return reject(r,"unknown_or_mismatched_time_activation_origin");
    }
    ++counters_.captured;
    r.origin_valid=true; r.intent=q.intent; r.origin=q.origin;
    if (!capture_) return r;
    if (queue_.size()==32 || sequence_==UINT64_MAX) {
        ++counters_.queue_full; return reject(r,"request_queue_or_sequence_exhausted");
    }
    q.sequence=++sequence_; queue_.push_back(q); ++counters_.queued; r.request_queued=true; return r;
}
HookResult Bridge::on_update(void* type,void* item) {
    HookResult r{}; std::string e;
    if (!thread(e)) { r.error=e; return r; }
    if (!bound_ || !capture_) return r;
    const auto t=reinterpret_cast<uintptr_t>(type), p=reinterpret_cast<uintptr_t>(item);
    if (std::none_of(handles_.begin(),handles_.end(),[&](const Handle& h){return h.type==t && h.item==p;})) return r;
    r.matched=true;
    if (bound_host_) {
        std::array<Handle,4> h{};
        if (!scan(h,true,e)) return reject(r,e);
        return r;
    }
    r.decision=Decision::Suppress;
    WriteReceipt receipt{};
    if (!write_presentation(receipt,e)) return reject(r,e);
    r.presentation_confirmed=true; return r;
}
bool Bridge::pop_request(Request& request,bool& available,std::string& e) {
    available=false; request={}; if (!thread(e)) return false;
    std::array<Handle,4> h{};
    if (!bound_) return failure(e,"request_pop_not_bound");
    if (!scan(h,true,e)) return failure(e,"request_pop_world_guard: "+e);
    if (!queue_.empty()) { request=queue_.front(); queue_.pop_front(); available=true; }
    e.clear(); return true;
}
bool Bridge::counters(Counters& out,std::string& e) { if (!thread(e)) return false; out=counters_; e.clear(); return true; }
bool Bridge::resolve(Intent intent,float scale,float pause,float& target,std::string& e) {
    if (!scale_valid(scale) || !std::isfinite(pause) || pause<0 || !std::isfinite(scale*pause))
        return failure(e,"host_native_scales_invalid");
    switch (intent) {
    case Intent::Pause: target=0; break;
    case Intent::Normal: target=1; break;
    case Intent::Fast: target=3; break;
    case Intent::Fastest: target=9; break;
    case Intent::TogglePauseNormal: target=scale*pause==0?1.0f:0.0f; break;
    default: return failure(e,"unknown_time_intent");
    }
    e.clear(); return true;
}

#ifdef _WIN32
namespace {
DWORD guarded_copy(uintptr_t p,void* result,size_t n) {
    __try { std::memcpy(result,reinterpret_cast<const void*>(p),n); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
DWORD guarded_compare_exchange(uintptr_t p,int32_t expected,int32_t target,LONG& previous) {
    __try { previous=InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(p),target,expected); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
bool safe_read(void*,uintptr_t p,void* result,size_t n,std::string& e) {
    MEMORY_BASIC_INFORMATION m{};
    if (!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || p<uintptr_t(m.BaseAddress) ||
        n>m.RegionSize || p-uintptr_t(m.BaseAddress)>m.RegionSize-n) return failure(e,"unreadable_memory_region");
    const DWORD exception=guarded_copy(p,result,n);
    return !exception || failure(e,"read_exception="+std::to_string(exception));
}
bool safe_write(void*,uintptr_t p,int32_t expected,int32_t target,std::string& e) {
    MEMORY_BASIC_INFORMATION m{};
    if (p%alignof(int32_t) || !VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || !(m.Protect&(PAGE_READWRITE|PAGE_EXECUTE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_WRITECOPY)) ||
        p<uintptr_t(m.BaseAddress) || m.RegionSize<4 || p-uintptr_t(m.BaseAddress)>m.RegionSize-4)
        return failure(e,"unwritable_or_unaligned_item_state");
    LONG previous=0;
    const DWORD exception=guarded_compare_exchange(p,expected,target,previous);
    if (exception) return failure(e,"write_exception="+std::to_string(exception));
    return previous==expected || failure(e,"item_state_expected_before_mismatch actual="+
        std::to_string(previous)+" required="+std::to_string(expected));
}
uint32_t current_thread(void*) { return GetCurrentThreadId(); }
}
Backend process_backend(void* user,bool(*context)(void*,Context&,std::string&)) {
    return {user,safe_read,safe_write,current_thread,context};
}
#endif
}
