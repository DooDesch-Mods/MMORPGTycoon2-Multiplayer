#pragma once
#include "capture_readback.h"
namespace mt2mp::game::live_host_state::day::economy {
// SaveScope sets Clock.pause to zero; the original Mode update normally restores it
// on the next frame. A synchronous Host capture must restore it before publication.
template<class Restore,class Read>
bool restore_capture_pause(const Owners& before,Owners& after,Restore restore,Read read,std::string& error){
    if(before.history.pin.pause==after.history.pin.pause)return capture_readback(before,after,error,true);
    if(before.history.pin.pause!=1.0f||after.history.pin.pause!=0.0f)return capture_readback(before,after,error,true);
    auto held=before;held.history.pin.pause=after.history.pin.pause;
    if(!capture_readback(held,after,error,true))return false;
    if(!restore(before.history.pin,error)){
        error="Subscriber capture pause restoration failed original="+error+"; no replay";return false;
    }
    Owners restored;
    if(!read(restored,error)||!capture_readback(before,restored,error,true)){
        error="Subscriber capture pause restoration readback failed original="+error+"; no replay";return false;
    }
    after=std::move(restored);error.clear();return true;
}
}
