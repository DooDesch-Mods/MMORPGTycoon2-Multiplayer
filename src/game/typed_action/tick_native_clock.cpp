#include "tick_native_clock.h"
#include "tick_authority.h"
#include <cmath>
namespace mt2mp::action_session::tick_commands {
ClockPort ordered_native_clock(game::time_runtime::Native native) {
    auto observe=[native](game::time_session::Clock& out,std::string& cause){
        if(!native.read_clock){cause="Ordered original clock reader missing";return false;}
        if(!native.read_clock(native.user,out,cause))return false;
        if(!std::isfinite(out.selected)||out.selected<0||!std::isfinite(out.pause)||out.pause<0){cause="Ordered original clock outside domain";return false;}
        return true;
    };
    auto read=[observe](game::time_session::Clock& out,std::string& cause){
        TimeRequest request;
        return authority::current_time(request,cause)&&observe(out,cause);
    };
    auto set=[native,read](float target,std::string& cause){
        TimeRequest request;game::time_session::Clock before;
        if(!authority::current_time(request,cause)||!read(before,cause))return false;
        float expected=0;
        using game::time_session::Intent;
        switch(request.intent){
        case Intent::Pause:expected=0;break;
        case Intent::Normal:expected=1;break;
        case Intent::Fast:expected=3;break;
        case Intent::Fastest:expected=9;break;
        case Intent::TogglePauseNormal:expected=before.selected*before.pause==0?1.0f:0.0f;break;
        default:cause="Ordered original clock intent invalid";return false;
        }
        if(target!=expected){cause="Ordered original clock target differs from admitted intent";return false;}
        if(!native.set_clock){cause="Ordered original clock setter missing";return false;}
        return native.set_clock(native.user,target,cause);
    };
    return {std::move(read),std::move(set),std::move(observe)};
}
}
