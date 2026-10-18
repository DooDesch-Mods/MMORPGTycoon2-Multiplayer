#pragma once
#include "live_host_state/marketing_port.h"
#include <memory>
namespace mt2mp::game::marketing_authority::notice {
class Runtime {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    Runtime();~Runtime();
    bool bind(const action_session::Context&,const action_session::Binding&,uint64_t,std::string&);
    bool suspend(std::string&);
    bool capture(const live_host_state::Frame&,Batch&,std::string&);
    bool sent(const live_host_state::Frame&,std::string&);
    bool acknowledge(uint64_t peer,uint64_t revision,std::string&);
    bool admit(const live_host_state::Frame&,const live_host_state::Frame*,std::string&);
    bool apply(const live_host_state::Frame&,live_host_state::MarketingReceipt&,std::string&);
    Json status()const;
};
}
