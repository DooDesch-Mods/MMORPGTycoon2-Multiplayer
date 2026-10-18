#pragma once
#include <chrono>
namespace mt2mp::game {
class SaveCacheRefresh {
public:
    using Clock=std::chrono::steady_clock;
    enum class Action { Idle, Wait, Queue, Ready, TimedOut };
    void begin(Clock::time_point now){waiting_=true;queued_=false;deadline_=now+std::chrono::seconds(120);}
    void cancel(){waiting_=false;}
    bool waiting()const{return waiting_;}
    Action tick(bool native_task_active,Clock::time_point now){
        if(!waiting_)return Action::Idle;
        if(now>=deadline_){waiting_=false;return Action::TimedOut;}
        if(native_task_active)return Action::Wait;
        if(!queued_){queued_=true;return Action::Queue;}
        waiting_=false;return Action::Ready;
    }
private:
    bool waiting_=false,queued_=false;
    Clock::time_point deadline_{};
};
}
