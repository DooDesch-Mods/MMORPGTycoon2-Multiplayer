#pragma once
#include "time_integration.h"
namespace mt2mp::game::live_host_state::day {
inline bool matches_rebound_time(const time_integration::Status& s,uint64_t actual_epoch,std::string& e){
    if(!actual_epoch||!s.initialized||s.runtime.phase!=time_runtime::Phase::Active||!s.runtime.controller.started||s.runtime.controller.write_locked||s.runtime.epoch!=actual_epoch){e="live snapshot actual initialized Time binding differs measured="+std::to_string(s.runtime.epoch)+" required="+std::to_string(actual_epoch);return false;}e.clear();return true;
}
}
