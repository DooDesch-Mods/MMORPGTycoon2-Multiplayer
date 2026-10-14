#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace mt2mp::game::native_time_input {
enum class Key:uint8_t {Space,F1,F2,F3,F4};
enum class Phase {Queued,Observed,Confirmed,Rejected,Partial,Unknown};
struct Context {
    uint64_t epoch=0,revision=0;
    uint32_t peer=0,window_id=0;
    uintptr_t owned_window=0;
    bool owned_test=false,world_owned=false,time_active=false,overlay_hidden=false,no_pending_operation=false;
};
struct Config {uintptr_t image=0;uint32_t game_thread=0;std::string executable_sha256;};
struct Event {uint32_t window_id=0,scancode=0,keycode=0;bool down=false;uintptr_t owned_window=0;uint64_t timestamp_ns=0;};
struct Services {
    void* user=nullptr;
    uint32_t (*thread_id)(void*)=nullptr;
    bool (*context)(void*,Context&,std::string&)=nullptr;
    bool (*read)(void*,uintptr_t,void*,size_t,std::string&)=nullptr;
    bool (*push)(void*,const Event&,std::string&)=nullptr;
    uint64_t (*timestamp_ns)(void*)=nullptr;
};
struct Binding {
    uint32_t axis=0,scancode=0;uint32_t matches=0,collisions=0;bool customized=false;
    bool ready=false;float axis_value=0;std::string blocker;
};
struct Collision {
    Key key=Key::Space;
    uint32_t expected_axis=0,axis=0,control_index=0;
    int32_t control_type=0,control_code=0;
    bool ctrl=false,customized=false;
    float axis_value=0;
};
struct Readback {
    uint64_t epoch=0,revision=0;
    uint32_t peer=0,window_id=0,axis_count=0,controls_scanned=0;
    bool scan_complete=false,panel_visible=false,input_clear=false;
    std::array<Binding,5> bindings{};
    uint32_t collision_count=0,collision_sample_count=0;
    bool collision_samples_truncated=false;
    std::array<Collision,32> collision_samples{};
};
struct Request {std::string token;Key key=Key::Space;uint64_t epoch=0,revision=0,expected_sequence=0;};
struct Edge {bool attempted=false,queued=false,poll_forwarded=false;uint64_t queued_frame=0,poll_frame=0,timestamp_ns=0;};
struct Receipt {
    Request request;Phase phase=Phase::Rejected;Edge down{},up{};
    uint32_t captured_count=0;uintptr_t caller_rva=0;
    bool native_route_observed=false,native_down_consumed=false,native_up_consumed=false,host_confirmed=false;
    uint64_t sequence=0,host_revision=0;
    float selected_scale=0,pause_scale=0;
    std::string error;
};
struct HostReceipt {
    uint64_t epoch=0,sequence=0,observed_revision=0,revision=0;
    uint32_t peer=0;
    bool confirmed=false,no_change=false,state_present=false,uncertain=true;
    Key intent=Key::Space;
    float selected_scale=0,pause_scale=0;
    std::string error;
};
class Adapter {
public:
    Adapter();~Adapter();
    Adapter(const Adapter&)=delete;Adapter& operator=(const Adapter&)=delete;
    bool initialize(const Config&,const Services&,std::string&);
    bool read(Readback&,std::string&);
    bool queue(const Request&,uint64_t frame,Receipt&,std::string&);
    bool observe_poll(const Event&,uint64_t frame,std::string&);
    bool observe_leaf(uintptr_t caller_rva,Key intent,bool matched,bool queued,uint64_t frame,std::string&);
    bool observe_frame(uint64_t frame,std::string&);
    bool confirm(const std::string& token,const HostReceipt&,Receipt&,std::string&);
    bool receipt(const std::string& token,Receipt&,std::string&);
    bool suspend(const std::string& reason,std::string&);
private:struct Impl;std::unique_ptr<Impl> impl_;
};
// Caller supplies the bound-world context. This backend never calls native Input methods.
Services process_services(void* user,bool(*context)(void*,Context&,std::string&));
}
