#pragma once
#include "live_host_state/management_port.h"
namespace mt2mp::game::management_runtime {
class LivePort final:public live_host_state::ManagementPort {
    action_session::Context context_;action_session::Binding binding_;uint64_t epoch_=0;
    bool bound_=false,suspended_=false,locked_=false;
    nlohmann::json last_=nullptr;std::string cause_;
public:
    bool bind(const action_session::Context&,const action_session::Binding&,uint64_t,std::string&)override;
    bool suspend(std::string&)override;
    bool capture(const action_session::Context&,const action_session::Binding&,management_simulation::LiveState&,std::string&)override;
    bool finish_capture(const action_session::Context&,const action_session::Binding&,live_host_state::Frame&,std::string&)override;
    bool sent(const live_host_state::Frame&,std::string&)override;
    bool acknowledge(uint64_t,uint64_t,std::string&)override;
    bool admit(const live_host_state::Frame&,const live_host_state::Frame*,std::string&)override;
    bool apply(const action_session::Context&,const action_session::Binding&,const live_host_state::Frame&,live_host_state::ManagementReceipt&,std::string&)override;
    nlohmann::json status()const override;
};
}
