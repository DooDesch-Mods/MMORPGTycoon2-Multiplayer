#pragma once
#include "typed_action/tick_authority.h"
namespace mt2mp::game::logo_runtime {
inline bool execution_admitted(const action_session::Context& context,const action_session::Binding& binding,
    const action_session::Request& request,const action_session::Evidence* host,bool pending,const std::string& action_uuid,std::string& cause) {
    namespace a=action_session;
    if(a::tick_commands::authority::active()) {
        if(host){cause="Ordered Logo execution must not adopt Host result state";return false;}
        return a::tick_commands::authority::matches(context,binding,request,cause);
    }
    if(context.snapshot_uuid!=request.snapshot_uuid||!pending||action_uuid!=request.action_uuid||
       (host!=nullptr)!=(context.role==a::Role::Client)) {cause="Logo exact Controller/session/role differs";return false;}
    return true;
}
}
