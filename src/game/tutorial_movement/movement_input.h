#pragma once
#include <cstdint>
#include <string>

namespace mt2mp::game::tutorial_movement {
struct Snapshot {
    std::string origin;
    uint64_t epoch=0,generation=0;
    uintptr_t window=0,hwnd=0;
    uint32_t window_id=0;
    float axis=0,progress=0;
    bool key_down=false;
    bool keyboard_focus=false,input_enabled=false;
    bool operator==(const Snapshot&)const=default;
};
struct Services {
    void* user=nullptr;
    uint32_t(*thread)(void*)=nullptr;
    uint64_t(*milliseconds)(void*)=nullptr;
    bool(*snapshot)(void*,Snapshot&,std::string&)=nullptr;
    bool(*post)(void*,const Snapshot&,bool down,std::string&)=nullptr;
};
enum class Phase {Idle,DownQueued,UpQueued,Confirmed,Failed};
struct Receipt {
    std::string token,error,release_error;
    Phase phase=Phase::Idle;
    Snapshot before{},after{};
    bool down_attempted=false,down_queued=false,down_unknown=false,down_polled=false,axis_down=false;
    bool up_attempted=false,up_queued=false,up_polled=false,axis_released=false;
    bool progress_changed=false;
    uint32_t down_events=0,up_events=0;
    uint64_t down_frame=0,up_frame=0;
};
class Fixture {
public:
    bool initialize(uint32_t thread,const Services&,std::string&);
    bool start(const std::string& token,const std::string& origin,uint64_t frame,std::string&);
    bool poll(uint32_t window_id,uint32_t keyboard_id,uint32_t scancode,uint32_t keycode,bool down,bool repeat,uint64_t frame,std::string&);
    bool frame(uint64_t completed_frame,std::string&);
    bool suspend(const std::string& cause,std::string&);
    const Receipt& receipt()const{return receipt_;}
    bool unresolved()const{return (receipt_.down_queued||receipt_.down_unknown)&&!receipt_.axis_released;}
private:
    Services services_{};uint32_t thread_=0;
    bool initialized_=false,used_=false,busy_=false;
    uint64_t deadline_=0,start_frame_=0,release_deadline_=0;
    Receipt receipt_{};
    bool thread(std::string&);
    bool snapshot(Snapshot&,std::string&);
    bool release(uint64_t frame,std::string&);
    bool fail(const std::string& cause,std::string&);
};
// Initialize only for an admitted native test profile.
bool initialize(uintptr_t image,uint32_t thread,const std::string& run_id,const std::string& dll_sha,std::string&);
bool start(const std::string& token,const std::string& native_origin_uuid,uint64_t frame,std::string&);
void poll(void* event,uint64_t next_frame);
void frame(uint64_t completed_frame);
void suspend(const std::string& cause);
const Receipt& receipt();
struct FocusWindowState {
    bool observed=false;
    uint64_t sdl_flags=0;
    bool hidden=false,minimized=false,not_focusable=false,input_focus_flag=false;
    bool visible=false,enabled=false,iconic=false;
    bool foreground_owned=false,thread_focus_owned=false,thread_active_owned=false;
    bool force_raise=false,activate_when_raised=true;
};
struct FocusReceipt {
    std::string origin,error;
    bool requested=false,attempted=false,returned=false;
    bool keyboard_focus=false,input_enabled=false,ready=false;
    FocusWindowState window_state{};
};
bool request_focus(const std::string& native_origin_uuid,std::string&);
bool read_focus(FocusReceipt&,std::string&);
}
