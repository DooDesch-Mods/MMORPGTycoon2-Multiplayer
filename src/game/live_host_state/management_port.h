#pragma once
#include "state.h"
#include "management_live_state.h"
namespace mt2mp::game::live_host_state {
struct ManagementReceipt {bool attempted=false,confirmed=false,unknown=false;uint32_t attempted_stores=0,stores=0;std::string cause;};
struct ManagementPort {
    virtual ~ManagementPort()=default;
    virtual bool bind(const Context&,const Binding&,uint64_t,std::string&)=0;
    virtual bool suspend(std::string&)=0;
    virtual bool capture(const Context&,const Binding&,management_simulation::LiveState&,std::string&)=0;
    virtual bool finish_capture(const Context&,const Binding&,Frame& frame,std::string& error){
        if(frame.management&&frame.management->subscriber_interventions){error="Management late authoritative capture provider absent";return false;}
        error.clear();return true;
    }
    virtual bool sent(const Frame& frame,std::string& error){if(frame.management&&frame.management->actor_lifecycle){error="Actor lifecycle sent-Frame provider absent";return false;}error.clear();return true;}
    virtual bool acknowledge(uint64_t,uint64_t,std::string& error){error.clear();return true;}
    virtual bool admit(const Frame& frame,const Frame*,std::string& error){if(frame.management&&frame.management->actor_lifecycle){error="Actor lifecycle full-Frame admission provider absent";return false;}error.clear();return true;}
    virtual bool apply(const Context&,const Binding&,const Frame&,ManagementReceipt&,std::string&)=0;
    virtual nlohmann::json status()const=0;
};
}
