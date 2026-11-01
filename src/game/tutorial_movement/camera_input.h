#pragma once
#include "movement_input.h"
#include "vendor/nlohmann/json.hpp"
#include <array>
namespace mt2mp::game::tutorial_movement {
struct CameraStep {int32_t index,type,next;const char* id;const char* next_id;};
inline const CameraStep* camera_step(int32_t index){
    static constexpr CameraStep steps[]={{8,3,10,"zoom","rotate"},{10,2,12,"rotate","zoomout"},{12,4,15,"zoomout","mainheader"}};
    for(const auto& step:steps)if(step.index==index)return &step;return nullptr;
}
enum class CameraInput {WheelIn,WheelOut,RotateDown,RotateMotion,RotateUp};
struct CameraReceipt {
    std::string token,cause,release_cause;int32_t index=-1;
    Snapshot before{},after{},completion{};
    bool active=false,confirmed=false,button_attempted=false,button_returned=false,button_polled=false;
    bool up_attempted=false,up_returned=false,up_polled=false,button_released=false;
    uint32_t pulses_attempted=0,pulses_returned=0,pulses_polled=0;
    uint64_t last_frame=0,deadline=0,release_deadline=0;
};
struct CameraServices {
    void* user=nullptr;
    bool(*observe)(void*,int32_t,Snapshot&,std::string&)=nullptr;
    bool(*release_observe)(void*,const Snapshot&,Snapshot&,std::string&)=nullptr;
    bool(*send)(void*,const Snapshot&,CameraInput,std::string&)=nullptr;
    uint64_t(*now)(void*)=nullptr;
};
class CameraFixture {
    CameraServices services_;CameraReceipt receipt_;std::array<bool,3> used_{};
    bool failure(const std::string& cause,std::string& error){if(receipt_.cause.empty())receipt_.cause=cause;error=receipt_.cause;return false;}
    bool same_owner(const Snapshot& s)const{const auto& b=receipt_.before;return s.origin==b.origin&&s.epoch==b.epoch&&s.generation==b.generation&&s.window==b.window&&s.window_id==b.window_id&&s.hwnd==b.hwnd;}
    bool release(std::string& error){
        if(receipt_.index!=10||!receipt_.button_attempted){receipt_.button_released=true;return true;}
        if(receipt_.up_attempted)return receipt_.up_returned;
        receipt_.up_attempted=true;receipt_.release_deadline=services_.now(services_.user)+3000;
        receipt_.up_returned=services_.send(services_.user,receipt_.before,CameraInput::RotateUp,error);
        if(!receipt_.up_returned)receipt_.release_cause=error;
        return receipt_.up_returned;
    }
public:
    explicit CameraFixture(CameraServices services):services_(services){}
    const CameraReceipt& receipt()const{return receipt_;}
    bool start(int32_t index,const std::string& token,const std::string& origin,uint64_t frame,std::string& error){
        const auto* step=camera_step(index);if(!step)return failure("unsupported camera tutorial step",error);
        const size_t slot=size_t(index-8)/2;
        if(receipt_.active||used_[slot]||!receipt_.cause.empty()){error="camera tutorial input already consumed, failed or unresolved";return false;}
        Snapshot s;
        if(!services_.observe(services_.user,index,s,error))return false;
        if(token.empty()||s.origin!=origin||s.tutorial_index!=index||s.threshold_reached||!s.keyboard_focus||!s.input_enabled||s.key_down||s.axis!=0){error="camera tutorial requires exact idle owned input and current step";return false;}
        used_[slot]=true;receipt_={};receipt_.index=index;receipt_.token=token;receipt_.before=receipt_.after=s;receipt_.active=true;
        receipt_.last_frame=frame;receipt_.deadline=services_.now(services_.user)+15000;
        if(index==10){receipt_.button_attempted=true;receipt_.button_returned=services_.send(services_.user,s,CameraInput::RotateDown,error);if(!receipt_.button_returned){failure(error,error);std::string cleanup;release(cleanup);return false;}}
        error.clear();return true;
    }
    bool polled(CameraInput input,std::string& error){
        if(!receipt_.active)return true;
        if(input==CameraInput::RotateDown){if(!receipt_.button_attempted||receipt_.button_polled)return failure("unexpected duplicate original rotate down",error);receipt_.button_polled=true;}
        else if(input==CameraInput::RotateUp){if(!receipt_.up_attempted||receipt_.up_polled)return failure("unexpected duplicate original rotate up",error);receipt_.up_polled=true;}
        else{if(receipt_.pulses_polled>=receipt_.pulses_attempted)return failure("unexpected camera pulse acknowledgement",error);++receipt_.pulses_polled;}
        return true;
    }
    bool suspend(const std::string& cause,std::string& error){
        if(!receipt_.active)return true;
        failure(cause,error);std::string cleanup;release(cleanup);return false;
    }
    bool frame(uint64_t frame,std::string& error){
        if(!receipt_.active)return true;
        Snapshot actual;const auto now=services_.now(services_.user);
        if(receipt_.cause.empty()){
            if(!services_.observe(services_.user,receipt_.index,actual,error)||!same_owner(actual))failure(error.empty()?"camera tutorial owner changed":error,error);
            else{
                receipt_.after=actual;
                if(actual.threshold_reached)receipt_.completion=actual;
                if(now>=receipt_.deadline)failure("camera tutorial threshold or ordered release timed out after 15000ms",error);
            }
        }
        if(!receipt_.cause.empty()||receipt_.completion.threshold_reached){
            std::string cleanup;
            if(!release(cleanup)&&receipt_.cause.empty())failure(cleanup,error);
            if(receipt_.index==10&&!receipt_.button_released&&receipt_.up_attempted){
                Snapshot released;
                if(services_.release_observe(services_.user,receipt_.before,released,cleanup)&&same_owner(released)){
                    if(receipt_.up_polled&&!released.key_down&&released.axis==0)receipt_.button_released=true;
                }else{receipt_.release_cause=cleanup.empty()?"camera release owner changed":cleanup;failure(receipt_.release_cause,error);}
                if(!receipt_.button_released&&now>=receipt_.release_deadline){receipt_.release_cause="original rotate key-up not confirmed within 3000ms";failure(receipt_.release_cause,error);receipt_.active=false;}
            }
            if(receipt_.button_released){
                if(!receipt_.cause.empty()){receipt_.active=false;return failure(receipt_.cause,error);}
                const auto& b=receipt_.before;
                if(actual.ordered_release){
                    if(actual.history_count!=b.history_count+1||actual.granted!=b.granted+1||actual.released!=b.released+1||receipt_.pulses_polled!=receipt_.pulses_returned)
                        return suspend("camera tutorial ordered completion counters differ",error);
                    receipt_.completion=actual;receipt_.confirmed=true;receipt_.active=false;error.clear();return true;
                }
            }
            error=receipt_.cause;return receipt_.cause.empty();
        }
        if(frame<=receipt_.last_frame||receipt_.pulses_polled!=receipt_.pulses_returned)return true;
        if(!actual.keyboard_focus||!actual.input_enabled)return suspend("camera tutorial lost owned input focus",error);
        // The native camera owns the relative-mode transition, which flushes queued motion.
        if(receipt_.index==10&&(!receipt_.button_polled||!actual.key_down||actual.axis<=0||!actual.mouse_relative||!actual.mouse_focus))return true;
        if(receipt_.pulses_attempted>=256)return suspend("camera tutorial exceeded 256 original input pulses",error);
        const auto input=receipt_.index==8?CameraInput::WheelIn:receipt_.index==12?CameraInput::WheelOut:CameraInput::RotateMotion;
        ++receipt_.pulses_attempted;receipt_.last_frame=frame;
        if(!services_.send(services_.user,receipt_.before,input,error))return suspend(error,error);
        ++receipt_.pulses_returned;return true;
    }
};
bool camera_start(int32_t,const std::string&,const std::string&,uint64_t,std::string&);
bool camera_unresolved();
nlohmann::json camera_status();
}
