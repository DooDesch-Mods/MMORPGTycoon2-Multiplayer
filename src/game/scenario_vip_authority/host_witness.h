#pragma once
#include "native.h"
#include "host_transitions.h"
#include "../typed_action/action_session.h"

namespace mt2mp::game::scenario_vip_authority {
bool host_bind(const action_session::Context&,const action_session::Binding&,uint64_t common_epoch,std::string&);
bool host_suspend(const wire::Origin&,std::string&);
bool host_reset(const wire::Origin&,std::string&);
bool host_capture(const wire::Origin&,wire::Batch&,std::string&);
bool host_sent(const wire::Origin&,uint64_t frame,const wire::Batch&,std::string&);
bool host_acknowledge(const wire::Origin&,uint32_t peer,uint64_t frame,std::string&);
void host_update(uintptr_t receiver,float dt,uintptr_t caller,HostUpdate original);
}
