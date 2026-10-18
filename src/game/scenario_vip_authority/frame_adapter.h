#pragma once
#include "../live_host_state/vip_port.h"
#include "alert_client_native.h"
#include <memory>

namespace mt2mp::game::scenario_vip_authority {
class FrameAdapter final:public live_host_state::VipPort {
    using Origin=live_host_state::vip::Origin;
    using Batch=live_host_state::vip::Batch;
    using Transition=live_host_state::vip::Transition;
    using Context=action_session::Context;
    using Binding=action_session::Binding;
    Context context_;
    Binding binding_;
    uint64_t epoch_=0;
    bool bound_=false,suspended_=false;
    std::optional<Origin> origin_;
    client_alert::Authorize authorize_;
    client_alert::Lifecycle lifecycle_;
    std::unique_ptr<client_alert::NativePort> native_;
    std::string cleanup_cause_;
    bool client_origin(const Origin&,std::string&);
public:
    explicit FrameAdapter(client_alert::Authorize authorize):authorize_(authorize){}
    bool bind(const Context&,const Binding&,uint64_t,std::string&) override;
    bool host_capture(const Origin&,Batch&,std::string&) override;
    bool host_sent(const Origin&,uint64_t,const Batch&,std::string&) override;
    bool host_acknowledge(const Origin&,uint32_t,uint64_t,std::string&) override;
    bool suspend(const Context&,const Binding&,uint64_t,const std::optional<Origin>&,std::string&) override;
    bool reset(const Origin&,std::string&) override;
    bool client_preflight(const Origin&,const std::vector<Transition>&,std::string&) override;
    bool client_apply(const Origin&,const std::vector<Transition>&,live_host_state::VipApplyReceipt&,std::string&) override;
    bool before_deinit(uintptr_t,std::string&);
    void after_deinit(uintptr_t,bool);
    void retire_lobby();
    nlohmann::json status()const;
};
}
