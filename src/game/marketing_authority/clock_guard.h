#pragma once
#include "state.h"
#include <bit>
#include <cmath>
namespace mt2mp::game::marketing_authority {
struct ClockSample {int32_t day=0,seconds=0;float selected=0,pause=0;bool paused=false;};
inline bool clock_unchanged(const ClockSample& a,const ClockSample& b,bool scope_zero,std::string& e){
    const float pause=scope_zero?0.f:a.pause;
    const bool paused=scope_zero?true:a.paused;
    if(a.day==b.day&&a.seconds==b.seconds&&std::bit_cast<uint32_t>(a.selected)==std::bit_cast<uint32_t>(b.selected)&&paused==b.paused&&std::isfinite(b.pause)&&(scope_zero?b.pause==0:std::bit_cast<uint32_t>(a.pause)==std::bit_cast<uint32_t>(b.pause)))return true;
    return fail(e,"Clock changed phase="+std::string(scope_zero?"held/closed":"before/restored")+" measured="+Json{{"day",b.day},{"seconds",b.seconds},{"selected",b.selected},{"pause",b.pause},{"paused",b.paused}}.dump()+" required="+Json{{"day",a.day},{"seconds",a.seconds},{"selected",a.selected},{"pause",pause},{"paused",paused}}.dump());
}
}
