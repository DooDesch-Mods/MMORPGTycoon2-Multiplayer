#pragma once
#include "action_session.h"
#include "mt2mp/net/sidecar_client.h"

namespace mt2mp::action_session {
// Use for the local sidecar backend only. Steam must implement Transport separately.
class SidecarTransport final:public Transport {
public:
    bool claim(Handler,std::string& cause) override;
    bool send(uint64_t,const std::vector<uint8_t>&,std::string& cause) override;
};
class LocalSession final:public Session {
public:
    bool current(Context&,std::string& cause) override;
};
}
