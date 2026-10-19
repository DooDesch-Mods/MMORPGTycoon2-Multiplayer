#pragma once
#include "core.h"
#include <functional>

namespace mt2mp::game::coop_cursor {
struct Ports {
    std::function<bool(Context&,std::string&)> context;
    std::function<bool(Sample&,std::string&)> read;
    std::function<bool(uint8_t,const uint8_t*,uint32_t,std::string&)> send;
    std::function<bool(uint64_t,const std::array<float,3>&,std::string&)> show;
    std::function<bool(uint64_t,std::string&)> hide;
};
struct Status {
    uint64_t sent=0,received=0,stale=0,other_world=0,rejected=0,send_failed=0,display_failed=0,read_failed=0;
    std::string cause;
};
class Runtime {
public:
    explicit Runtime(Ports ports):ports_(std::move(ports)){}
    bool tick(uint64_t now){
        status_.cause.clear();Context context;std::string error;
        if(!ports_.context||!ports_.read||!ports_.send||!ports_.show||!ports_.hide){status_.cause="cursor runtime ports incomplete";return false;}
        if(!ports_.context(context,error)){
            core_.configure({});status_.cause="cursor context read failed: "+error;
            const auto cause=status_.cause;const bool retired=draw(now);if(retired)status_.cause=cause;return false;
        }
        const bool changed=core_.context().session!=context.session||core_.context().snapshot!=context.snapshot;
        core_.configure(context);
        bool okay=true;
        if(valid(core_.context())&&(changed||!sent_once_||now<last_send_||now-last_send_>=100)){
            Sample sample;
            if(!ports_.read(sample,error)){
                ++status_.read_failed;status_.cause="cursor native read failed: "+error;okay=false;
                sample.position.reset();
            }
            Packet packet;
            if(!core_.publish(sample,packet,error)){status_.cause=error;okay=false;}
            else{
                const auto bytes=encode(packet).dump();last_send_=now;sent_once_=true;
                if(!ports_.send(channel,reinterpret_cast<const uint8_t*>(bytes.data()),uint32_t(bytes.size()),error)){
                    ++status_.send_failed;status_.cause="cursor send failed: "+error;okay=false;
                }else ++status_.sent;
            }
        }
        return draw(now)&&okay;
    }
    Reception receive(uint64_t sender,const uint8_t* bytes,uint32_t size,uint64_t now){
        Packet packet;std::string error;
        if(!decode(bytes,size,packet,error)){++status_.rejected;status_.cause=error;return Reception::Rejected;}
        const auto result=core_.receive(sender,packet,now,error);
        switch(result){
        case Reception::Applied:++status_.received;break;
        case Reception::Stale:++status_.stale;break;
        case Reception::OtherWorld:++status_.other_world;break;
        case Reception::Rejected:++status_.rejected;status_.cause=error;break;
        }
        return result;
    }
    bool retire(uint64_t now){core_.configure({});sent_once_=false;return draw(now);}
    const Status& status()const{return status_;}
    const Core& core()const{return core_;}
private:
    bool draw(uint64_t now){
        bool okay=true;std::string error;
        for(uint64_t peer=1;peer<=4;++peer){
            const auto point=core_.visible(peer,now);
            if(point==drawn_[peer]&&(!point||drawn_context_[peer]==core_.context()))continue;
            const bool done=point?ports_.show(peer,*point,error):ports_.hide(peer,error);
            if(done){drawn_[peer]=point;drawn_context_[peer]=core_.context();}
            else{++status_.display_failed;status_.cause="cursor marker update failed peer="+std::to_string(peer)+" cause="+error;okay=false;}
        }
        return okay;
    }
    Ports ports_;
    Core core_;
    Status status_;
    std::array<std::optional<std::array<float,3>>,5> drawn_{};
    std::array<Context,5> drawn_context_{};
    uint64_t last_send_=0;
    bool sent_once_=false;
};
}
