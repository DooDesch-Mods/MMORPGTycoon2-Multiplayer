bool Fixture::frame_threshold(uint64_t frame_id,std::string& e){
    if(!thread(e))return false;if(busy_)return reject(e,"tutorial movement reentrant");Busy busy(busy_);
    if(!used_||(!receipt_.down_queued&&!receipt_.up_queued)||receipt_.phase==Phase::Confirmed)return true;
    if(receipt_.phase==Phase::Failed&&receipt_.axis_released)return reject(e,receipt_.error);
    Snapshot current;std::string cause;const bool read=snapshot(current,cause,true);
    const auto now=services_.milliseconds(services_.user);
    if(!read||!same_owner(receipt_.before,current))fail(read?"tutorial threshold native origin/window changed":cause,e);
    if(read&&same_owner(receipt_.before,current)){
        receipt_.after=current;
        if(!std::isfinite(current.axis))fail("tutorial threshold native axis nonfinite",e);
        if(!receipt_.up_attempted&&!current.threshold_reached&&(!current.keyboard_focus||!current.input_enabled))
            fail("tutorial threshold keyboard focus changed during Down",e);
        if(!receipt_.up_attempted&&receipt_.down_polled&&current.key_down&&current.axis>0)receipt_.axis_down=true;
        if(receipt_.axis_down&&current.threshold_reached)receipt_.progress_changed=true;
        if(current.ordered_release){
            if(receipt_.before.granted==UINT64_MAX||receipt_.before.released==UINT64_MAX||receipt_.before.history_count==UINT32_MAX||
               current.granted!=receipt_.before.granted+1||current.released!=receipt_.before.released+1||
               current.history_count!=receipt_.before.history_count+1)
                fail("tutorial threshold expected exactly one ordered grant/release/history entry",e);
            else{receipt_.completion_confirmed=true;receipt_.completion=current;}
        }
    }
    if(now>=deadline_&&!receipt_.completion_confirmed)fail("tutorial WASD completion timed out after 15000ms; no replay",e);
    if(!receipt_.up_attempted&&(receipt_.error.size()||now>=deadline_||(read&&current.threshold_reached))){
        if(!receipt_.axis_down)fail("tutorial threshold Down/axis not confirmed before release",e);
        if(!release(frame_id,cause))return reject(e,receipt_.error);
    }
    // Release readback is scoped to the original input window. A terminal world
    // or tutorial error must not prevent observing the cleanup key-up.
    if(receipt_.up_polled&&!receipt_.axis_released){
        Snapshot input;bool observed=false;
        try{observed=services_.release_snapshot(services_.user,receipt_.before,input,cause);}
        catch(const std::exception& failure){cause=failure.what();}
        if(observed&&same_owner(receipt_.before,input)&&!input.key_down&&std::isfinite(input.axis)&&input.axis==0){
            receipt_.axis_released=true;receipt_.after.key_down=false;receipt_.after.axis=0;
        }else if(!observed){receipt_.release_error=cause.empty()?"tutorial threshold key-up observation failed":cause;fail(receipt_.release_error,e);}
        else if(!same_owner(receipt_.before,input))fail("tutorial threshold release input owner changed",e);
    }
    if(receipt_.up_attempted&&!receipt_.axis_released&&now>=release_deadline_)
        fail("tutorial threshold release not acknowledged within 1000ms; no replay",e);
    if(receipt_.axis_released&&receipt_.completion_confirmed&&receipt_.error.empty()){
        if(receipt_.down_events!=1||receipt_.up_events!=1||!receipt_.axis_down||!receipt_.progress_changed)
            return fail("tutorial threshold incomplete down/up/completion receipt",e);
        receipt_.phase=Phase::Confirmed;e.clear();return true;
    }
    if(!receipt_.error.empty())return reject(e,receipt_.error);
    e.clear();return true;
}
