#pragma once
#include "alert_lifecycle.h"
#include "../typed_action/action_session.h"

namespace mt2mp::game::scenario_vip_authority::client_alert {
using Authorize=bool(*)(const vip::Origin&,const action_session::Context&,
    const action_session::Binding&,bool teardown,std::string&);
class NativePort final:public Port {
    vip::Origin origin_;action_session::Context context_;action_session::Binding binding_;Authorize authorize_;int32_t message_type_=9;
public:
    NativePort(const vip::Origin& origin,const action_session::Context& context,
        const action_session::Binding& binding,Authorize authorize,int32_t message_type=9):
        origin_(origin),context_(context),binding_(binding),authorize_(authorize),message_type_(message_type){}
    bool owner(const Owner*,bool,Owner&,std::string&) override;
    bool subject(const Owner&,int32_t,const std::string&,Subject&,std::string&) override;
    bool inspect(const Owner&,uintptr_t,const Subject&,bool,int,uintptr_t&,std::string&) override;
    bool absent(const Owner&,uintptr_t,uintptr_t,std::string&) override;
    bool create(const Owner&,const Subject&,Call&,std::string&) override;
    bool show(const Owner&,uintptr_t,bool,Call&,std::string&) override;
    bool destroy(const Owner&,uintptr_t,Call&,std::string&) override;
    bool create_message(const Owner&,const std::string&,MessageCalls&,std::string&) override;
    bool inspect_message(const Owner&,uintptr_t,const std::string&,int,uintptr_t&,std::string&) override;
    bool fire_message(const Owner&,uintptr_t,Call&,std::string&) override;
    bool discard_message(const Owner&,uintptr_t,Call&,std::string&) override;
};
}
