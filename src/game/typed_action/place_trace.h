#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#ifdef MT2MP_FRAMEWORK_PLUGIN
#include "mt2mp/core/logger.h"
#endif

namespace mt2mp::action_session::place_trace {
inline void event(const std::string& action,const char* stage,const char* boundary,int result,
                  uint64_t elapsed_us,const std::string& cause,uint64_t tick=0,uint64_t order=0){
#ifdef MT2MP_FRAMEWORK_PLUGIN
    if(!action.empty()){
        if(std::string_view(boundary)=="exception"||(std::string_view(boundary)=="return"&&!cause.empty()))
            LOG_ERROR("[OrderedPlaceTrace] action={} stage={} boundary={} result={} elapsed_us={} tick={} order={} cause={}",
                action,stage,boundary,result,elapsed_us,tick,order,cause);
        else LOG_INFO("[OrderedPlaceTrace] action={} stage={} boundary={} result={} elapsed_us={} tick={} order={} cause={}",
                action,stage,boundary,result,elapsed_us,tick,order,cause);
    }
#else
    (void)action;(void)stage;(void)boundary;(void)result;(void)elapsed_us;(void)cause;(void)tick;(void)order;
#endif
}
// One synchronous entry and return per selected placement stage. An entry with
// no return locates a blocked read or native continuation without replaying it.
template<class Call>auto run(const std::string& action,const char* stage,const std::string& cause,Call&& call,
                             uint64_t tick=0,uint64_t order=0){
    if(action.empty())return call();
    const auto began=std::chrono::steady_clock::now();
    const auto elapsed=[&](){return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-began).count());};
    event(action,stage,"enter",-1,0,cause,tick,order);
    try {
        auto result=call();event(action,stage,"return",int(result),elapsed(),cause,tick,order);return result;
    } catch(...) {
        event(action,stage,"exception",-1,elapsed(),cause,tick,order);throw;
    }
}
}
