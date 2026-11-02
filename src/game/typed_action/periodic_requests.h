#pragma once
#include "tick_wire.h"

namespace mt2mp::action_session::tick_commands {
class PeriodicRequests {
    std::array<std::optional<wire::PeriodicRequest>,5> pending_{},seen_{},last_triggered_{};
    uint64_t deadline_=0,trigger_count_=0;
public:
    bool receive(const Context& context,uint64_t sender,const wire::PeriodicRequest& value,uint64_t now,uint64_t timeout,std::string& error){
        if(context.role!=Role::Host||sender<2||sender>4||sender!=value.peer||!(context.peer_mask&(1u<<(sender-1)))||
           value.session!=context.session||value.snapshot_uuid!=context.snapshot_uuid||!value.tick||!value.invocation){
            error="Periodic request authenticated owner differs";return false;
        }
        auto& seen=seen_[sender];auto& pending=pending_[sender];
        if(seen&&*seen==value){error.clear();return true;}
        if(pending||(seen&&value.tick<=seen->tick)){
            error="Periodic request conflicts with retained Client owner";return false;
        }
        if(!timeout||now>UINT64_MAX-timeout){error="Periodic request deadline invalid";return false;}
        if(!deadline_)deadline_=now+timeout;
        seen=value;pending=value;error.clear();return true;
    }
    bool ready(uint64_t tick,uint64_t now,bool busy,bool& result,std::string& error)const{
        result=false;if(!deadline_){error.clear();return true;}
        if(now>=deadline_){error="Client periodic request timed out before original Host Save";return false;}
        if(busy){error.clear();return true;}
        for(const auto& value:pending_)if(value&&value->tick>tick){error.clear();return true;}
        result=true;error.clear();return true;
    }
    void completed(){last_triggered_=pending_;for(auto& value:pending_)value.reset();deadline_=0;++trigger_count_;}
    uint64_t trigger_count()const{return trigger_count_;}
    const auto& last_triggered()const{return last_triggered_;}
};
}
