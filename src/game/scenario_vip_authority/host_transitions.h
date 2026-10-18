#pragma once
#include "../live_host_state/vip_transitions.h"

namespace mt2mp::game::scenario_vip_authority {
namespace wire=live_host_state::vip;
struct HostSample {
    uintptr_t scenario=0,alert=0,toon=0,proxy=0;
    bool active=false;
    int32_t subscriber=-1;
    std::string name;
    bool operator==(const HostSample&)const=default;
};
class HostTransitions {
    struct Active {HostSample sample;uint64_t arrival=0;};
    wire::Origin origin_;uint32_t peers_=0;
    wire::HostRetention retained_;
    std::map<uintptr_t,Active> active_,retired_;
    bool bound_=false,suspended_=false;
public:
    bool bound()const{return bound_;}
    bool armed()const{return bound_&&!suspended_;}
    const wire::Origin& origin()const{return origin_;}
    bool same_binding(const wire::Origin& o,uint32_t peers)const{return bound_&&origin_==o&&peers_==peers;}
    bool bind(const wire::Origin& o,uint32_t peers,std::string& e){
        if(bound_&&!suspended_)return wire::fail(e,"VIP Host witness already bound; explicit suspension required");
        if(origin_==o)return wire::fail(e,"VIP Host witness cannot reset sequence in the same Origin");
        wire::HostRetention fresh;if(!fresh.bind(o,peers,e))return false;
        retained_=std::move(fresh);origin_=o;peers_=peers;active_.clear();retired_.clear();bound_=true;suspended_=false;return true;
    }
    bool suspend(const wire::Origin& o,std::string& e){
        if(!bound_||origin_!=o)return wire::fail(e,"VIP Host suspend Origin differs");
        suspended_=true;e.clear();return true;
    }
    bool reset(const wire::Origin& o,std::string& e){
        if(!bound_||!suspended_||origin_!=o)return wire::fail(e,"VIP Host reset requires suspended exact Origin");
        retained_.reset();active_.clear();retired_.clear();bound_=false;suspended_=false;peers_=0;e.clear();return true;
    }
    bool needs_arrival(const HostSample& sample)const{
        const auto found=active_.find(sample.scenario);
        return sample.active&&(found==active_.end()||found->second.sample.alert!=sample.alert||
            found->second.sample.toon!=sample.toon||found->second.sample.proxy!=sample.proxy);
    }
    std::optional<HostSample> known(uintptr_t scenario)const{
        if(const auto it=active_.find(scenario);it!=active_.end())return it->second.sample;
        if(const auto it=retired_.find(scenario);it!=retired_.end())return it->second.sample;
        return std::nullopt;
    }
    void forget_removed(const std::vector<uintptr_t>& current){
        std::erase_if(retired_,[&](const auto& row){return std::find(current.begin(),current.end(),row.first)==current.end();});
    }
    bool observe(const wire::Origin& o,const HostSample& sample,std::string& e,const std::optional<std::string>& message=std::nullopt){
        if(!armed()||o!=origin_||!sample.scenario)return wire::fail(e,"VIP Host sample outside bound Origin");
        if(sample.active){
            if(!sample.alert||!sample.toon||!sample.proxy)return wire::fail(e,"VIP Host arrival lacks live native owners");
            if(!wire::valid(wire::Batch{0,{{1,1,sample.subscriber,sample.name,true}}},e))return false;
        }
        if(message&&sample.active)return wire::fail(e,"VIP disconnect cannot accompany an active arrival");
        auto found=active_.find(sample.scenario);
        const bool replace=needs_arrival(sample);
        if(found!=active_.end()&&sample.active&&!replace&&found->second.sample!=sample)
            return wire::fail(e,"VIP Host active arrival immutable identity/name changed");
        // Retire from the copied arrival payload. The native Toon may already be gone.
        if(found!=active_.end()&&(!sample.active||replace)){
            uint64_t sequence=0;const auto& old=found->second;
            if(!retained_.append(o,old.sample.subscriber,old.sample.name,false,old.arrival,sequence,e,message))return false;
            if(!message)retired_[sample.scenario]=old;else retired_.erase(sample.scenario);
            active_.erase(found);
        }
        else if(message){
            const auto prior=retired_.find(sample.scenario);
            if(prior==retired_.end())return wire::fail(e,"VIP witnessed disconnect lacks copied arrival identity");
            uint64_t sequence=0;
            if(!retained_.append(o,prior->second.sample.subscriber,prior->second.sample.name,false,prior->second.arrival,sequence,e,message))return false;
            retired_.erase(prior);
        }
        if(replace){uint64_t sequence=0;
            retired_.erase(sample.scenario);
            if(!sample.alert||!sample.toon||!sample.proxy)return wire::fail(e,"VIP Host arrival lacks live native owners");
            if(!retained_.append(o,sample.subscriber,sample.name,true,0,sequence,e))return false;
            active_.emplace(sample.scenario,Active{sample,sequence});
        }
        e.clear();return true;
    }
    std::vector<uintptr_t> tracked()const{std::vector<uintptr_t> rows;for(const auto& [owner,value]:active_){(void)value;rows.push_back(owner);}return rows;}
    bool capture(const wire::Origin& o,wire::Batch& out,std::string& e)const{
        out={};if(!armed()||o!=origin_)return wire::fail(e,"VIP Host capture outside bound Origin");
        out=retained_.pending();e.clear();return true;
    }
    bool sent(const wire::Origin& o,uint64_t frame,const wire::Batch& batch,std::string& e){
        if(!armed())return wire::fail(e,"VIP Host sent while suspended/unbound");
        return retained_.sent(o,frame,batch,e);
    }
    bool acknowledge(const wire::Origin& o,uint32_t peer,uint64_t frame,std::string& e){
        if(!armed())return wire::fail(e,"VIP Host ACK while suspended/unbound");
        return retained_.acknowledge(o,peer,frame,e);
    }
};
}
