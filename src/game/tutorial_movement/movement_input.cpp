#include "movement_input.h"
#include <cmath>
#include <exception>

namespace mt2mp::game::tutorial_movement {
namespace {
bool reject(std::string& e,const std::string& cause){e=cause;return false;}
bool uuid(const std::string& value){
    if(value.size()!=36)return false;
    for(size_t i=0;i<value.size();++i){if(i==8||i==13||i==18||i==23){if(value[i]!='-')return false;}else if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return false;}return true;
}
bool same_owner(const Snapshot& a,const Snapshot& b){return a.origin==b.origin&&a.epoch==b.epoch&&a.generation==b.generation&&a.window==b.window&&a.hwnd==b.hwnd&&a.window_id==b.window_id;}
struct Busy {bool& flag;explicit Busy(bool& value):flag(value){flag=true;}~Busy(){flag=false;}};
}
bool Fixture::thread(std::string& e){return initialized_&&services_.thread(services_.user)==thread_||reject(e,"tutorial movement Mainthread/initialization differs");}
bool Fixture::snapshot(Snapshot& out,std::string& e){try{return services_.snapshot(services_.user,out,e);}catch(const std::exception& cause){return reject(e,cause.what());}}
bool Fixture::fail(const std::string& cause,std::string& e){if(receipt_.error.empty())receipt_.error=cause.empty()?"tutorial movement operation failed without cause":cause;receipt_.phase=Phase::Failed;return reject(e,receipt_.error);}
bool Fixture::initialize(uint32_t thread_id,const Services& io,std::string& e){
    if(initialized_)return reject(e,"tutorial movement already initialized");
    if(!thread_id||!io.thread||!io.milliseconds||!io.snapshot||!io.post||io.thread(io.user)!=thread_id)return reject(e,"tutorial movement services/Mainthread missing");
    thread_=thread_id;services_=io;initialized_=true;e.clear();return true;
}
bool Fixture::release(uint64_t frame_id,std::string& e){
    if(receipt_.up_attempted)return receipt_.up_queued||reject(e,receipt_.release_error);
    receipt_.up_attempted=true;receipt_.up_frame=frame_id;release_deadline_=services_.milliseconds(services_.user)+1000;
    try{receipt_.up_queued=services_.post(services_.user,receipt_.before,false,receipt_.release_error);}
    catch(const std::exception& cause){receipt_.release_error=cause.what();}
    if(!receipt_.up_queued){if(receipt_.release_error.empty())receipt_.release_error="tutorial movement release queue failed without cause";return fail(receipt_.release_error,e);}
    if(receipt_.phase!=Phase::Failed)receipt_.phase=Phase::UpQueued;
    e.clear();return true;
}
bool Fixture::start(const std::string& token,const std::string& origin,uint64_t frame_id,std::string& e){
    if(!thread(e))return false;if(busy_)return reject(e,"tutorial movement reentrant");Busy busy(busy_);
    if(used_)return reject(e,"tutorial movement token already consumed; no replay");
    if(!uuid(token)||!uuid(origin))return reject(e,"tutorial movement token/origin requires lowercase UUID");
    Snapshot before,after;if(!snapshot(before,e)||!snapshot(after,e))return false;
    if(before!=after||before.origin!=origin||!before.epoch||!before.generation||!before.window||!before.hwnd||!before.window_id)return reject(e,"tutorial movement exact native origin/window changed");
    if(!before.keyboard_focus||!before.input_enabled)return reject(e,"tutorial movement actual SDL focus/native keyboard admission not ready");
    if(before.key_down||before.axis!=0||!std::isfinite(before.progress)||before.progress<0||before.progress>=90)return reject(e,"tutorial movement input already held or progress outside0..90");
    used_=true;receipt_={};receipt_.token=token;receipt_.before=before;receipt_.after=before;start_frame_=frame_id;deadline_=services_.milliseconds(services_.user)+1000;
    receipt_.down_attempted=true;receipt_.down_frame=frame_id;
    try{receipt_.down_queued=services_.post(services_.user,before,true,e);}
    catch(const std::exception& cause){receipt_.down_unknown=true;e=cause.what();fail(e,e);std::string release_error;release(frame_id,release_error);return false;}
    if(!receipt_.down_queued)return fail(e.empty()?"tutorial movement down queue failed without cause":e,e);
    receipt_.phase=Phase::DownQueued;e.clear();return true;
}
bool Fixture::poll(uint32_t window_id,uint32_t keyboard_id,uint32_t scancode,uint32_t keycode,bool down,bool repeat,uint64_t frame_id,std::string& e){
    if(!thread(e))return false;if(!used_||receipt_.axis_released||(!receipt_.down_queued&&!receipt_.down_unknown))return true;
    if((window_id!=receipt_.before.window_id&&(down||window_id!=0||!receipt_.up_queued))||keyboard_id!=0||scancode!=26||keycode!='w')return true;
    auto& count=down?receipt_.down_events:receipt_.up_events;++count;
    if(repeat||count!=1||(down&&receipt_.up_attempted)||(!down&&!receipt_.up_queued)||frame_id<start_frame_)return fail("tutorial movement unexpected or duplicate owned key edge",e);
    if(down)receipt_.down_polled=true;else receipt_.up_polled=true;
    e.clear();return true;
}
bool Fixture::frame(uint64_t frame_id,std::string& e){
    if(!thread(e))return false;if(busy_)return reject(e,"tutorial movement reentrant");Busy busy(busy_);
    if(!used_||(!receipt_.down_queued&&!receipt_.up_queued)||receipt_.phase==Phase::Confirmed)return true;
    Snapshot current;std::string cause;const bool read=snapshot(current,cause);
    const auto now=services_.milliseconds(services_.user);
    if(!read||!same_owner(receipt_.before,current))fail(read?"tutorial movement native origin/window changed during input":cause,e);
    if(read&&same_owner(receipt_.before,current)){
        receipt_.after=current;
        if(!std::isfinite(current.axis)||!std::isfinite(current.progress))fail("tutorial movement nonfinite native input/progress",e);
        if(!receipt_.up_attempted&&(!current.keyboard_focus||!current.input_enabled))fail("tutorial movement native keyboard focus changed during Down",e);
        if(!receipt_.up_attempted&&receipt_.down_polled&&current.key_down&&current.axis>0)receipt_.axis_down=true;
        if(receipt_.axis_down&&std::isfinite(current.progress)&&current.progress>receipt_.before.progress&&
            (!receipt_.up_attempted||now<=release_deadline_))receipt_.progress_changed=true;
        if(receipt_.up_polled&&!current.key_down&&current.axis==0)receipt_.axis_released=true;
    }
    if(!receipt_.up_attempted&&(frame_id>start_frame_||now>=deadline_||receipt_.phase==Phase::Failed)){
        if(!receipt_.axis_down)fail("tutorial movement Down/axis not confirmed in bounded native frame",e);
        if(!release(frame_id,cause))return reject(e,receipt_.error);
    }
    if(receipt_.axis_released){
        if(receipt_.error.empty()&&receipt_.down_events==1&&receipt_.up_events==1&&receipt_.axis_down&&receipt_.progress_changed){receipt_.phase=Phase::Confirmed;e.clear();return true;}
        if(!receipt_.error.empty())return fail(receipt_.error,e);
        if(receipt_.down_events!=1||receipt_.up_events!=1||!receipt_.axis_down)return fail("tutorial movement incomplete down/up receipt",e);
        if(now>=release_deadline_)return fail("tutorial movement progress not observed before release deadline; no replay",e);
        e.clear();return true;
    }
    if(receipt_.up_attempted&&now>=release_deadline_)return fail("tutorial movement release not acknowledged; no replay",e);
    if(!receipt_.error.empty())return reject(e,receipt_.error);e.clear();return true;
}
bool Fixture::suspend(const std::string& cause,std::string& e){
    if(!thread(e))return false;if(!used_||!unresolved())return true;
    fail(cause,e);std::string release_error;release(start_frame_,release_error);return reject(e,receipt_.error);
}
}
