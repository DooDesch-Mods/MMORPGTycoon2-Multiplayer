#pragma once
#include "live_host_state/marketing_port.h"
#include <memory>
namespace mt2mp::game::marketing_authority {
class NativePort final:public live_host_state::MarketingPort {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    NativePort();
    ~NativePort();
    bool bind(const action_session::Context&,const action_session::Binding&,uint64_t,std::string&)override;
    bool suspend(std::string&)override;
    bool sent(const live_host_state::Frame&,std::string&)override;
    bool acknowledge(uint64_t,uint64_t,std::string&)override;
    bool admit(const live_host_state::Frame&,const live_host_state::Frame*,std::string&)override;
    bool read_current(const action_session::Context&,const action_session::Binding&,const live_host_state::Frame&,State&,std::string&);
    bool capture(const action_session::Context&,const action_session::Binding&,const live_host_state::Frame&,State&,std::string&)override;
    bool apply(const action_session::Context&,const action_session::Binding&,const live_host_state::Frame&,live_host_state::MarketingReceipt&,std::string&)override;
    Json status()const override;
};
}
