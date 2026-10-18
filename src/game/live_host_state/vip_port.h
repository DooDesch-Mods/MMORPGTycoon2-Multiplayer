#pragma once
#include "state.h"

namespace mt2mp::game::live_host_state {
struct VipApplyReceipt {
    bool attempted=false,confirmed=false,unknown=false;
    std::vector<vip::Transition> applied;
    std::string cause;
};

// The runtime adapter owns native alert lifetimes. The Bridge owns portable
// transition admission and advances it only after exact native confirmation.
struct VipPort {
    virtual ~VipPort()=default;
    virtual bool bind(const Context&,const Binding&,uint64_t common_epoch,std::string&)=0;
    virtual bool host_capture(const vip::Origin&,vip::Batch&,std::string&)=0;
    virtual bool host_sent(const vip::Origin&,uint64_t revision,const vip::Batch&,std::string&)=0;
    virtual bool host_acknowledge(const vip::Origin&,uint32_t peer,uint64_t revision,std::string&)=0;
    virtual bool suspend(const Context&,const Binding&,uint64_t common_epoch,const std::optional<vip::Origin>&,std::string&)=0;
    virtual bool reset(const vip::Origin&,std::string&)=0;
    virtual bool client_preflight(const vip::Origin&,const std::vector<vip::Transition>&,std::string&)=0;
    virtual bool client_apply(const vip::Origin&,const std::vector<vip::Transition>&,VipApplyReceipt&,std::string&)=0;
};
}
