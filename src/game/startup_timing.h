#pragma once
#include "mt2mp/core/logger.h"
#include <chrono>
#include <exception>
#include <string>

namespace mt2mp::game::startup_timing {
// Startup phases are synchronous and attempted once by their existing owners.
class Phase {
    const char* name_;
    const std::string& error_;
    std::chrono::steady_clock::time_point began_=std::chrono::steady_clock::now();
    int exceptions_=std::uncaught_exceptions();
    bool completed_=false;
public:
    Phase(const char* name,const std::string& error):name_(name),error_(error){}
    Phase(const Phase&)=delete;
    Phase& operator=(const Phase&)=delete;
    bool complete(bool result){completed_=result;return result;}
    ~Phase(){
        const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-began_).count();
        if(std::uncaught_exceptions()>exceptions_)
            LOG_ERROR("[StartupTiming] phase={} elapsed_us={} result=exception cause={}; original exception continues",name_,us,error_);
        else if(!completed_)
            LOG_ERROR("[StartupTiming] phase={} elapsed_us={} result=failed cause={}",name_,us,error_);
        else LOG_INFO("[StartupTiming] phase={} elapsed_us={} result=completed",name_,us);
    }
};
template<class Call> bool run(const char* name,const std::string& error,Call&& call){
    Phase phase(name,error);return phase.complete(call());
}
}
