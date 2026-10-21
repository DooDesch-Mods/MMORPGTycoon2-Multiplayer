#include "sidecar_adapter.h"
#include "mt2mp/core/config.h"
#include "mt2mp/net/local_session.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/net/session_wire.h"

namespace mt2mp::action_session {
bool SidecarTransport::claim(Handler handler,std::string& cause) {
    return net::session_wire::claim_handler(channel,[handler=std::move(handler)](uint64_t peer,const uint8_t* bytes,uint32_t size){handler(peer,bytes,size);},cause);
}
bool SidecarTransport::send(uint64_t target,const std::vector<uint8_t>& bytes,std::string& cause) {
    auto& client=net::SidecarClient::instance();
    if(!client.connected()||!client.ready()){cause="Sidecar action transport is not connected and ready: "+client.failure_reason();return false;}
    if(bytes.empty()||bytes.size()>max_transport_payload){cause="Sidecar Channel9 transport payload length invalid";return false;}
    if(!net::session_wire::send(channel,bytes.data(),uint32_t(bytes.size()),target)){cause="Sidecar refused action packet: "+client.failure_reason();return false;}
    cause.clear();return true;
}
bool LocalSession::current(Context& out,std::string& cause) {
    const auto& config=Config::instance();auto& transport=net::SidecarClient::instance();
    if(!config.local_test){cause="local action adapter requires local_test; Steam action transport not implemented";return false;}
    if(!net::local_session_connected()||!transport.connected()||!transport.ready()){cause="local action session disconnected: "+transport.failure_reason();return false;}
    const auto snapshot=net::current_snapshot_id();const auto epoch=net::current_world_epoch();const auto mask=net::local_session_peer_mask();
    const auto session=net::session_wire::session_id();
    if(snapshot.empty()||!epoch||!mask||!session){cause="current snapshot session authority unavailable";return false;}
    out={session,config.peer_id,1,mask,config.is_host?Role::Host:Role::Client,snapshot};
    cause.clear();return true;
}
}
