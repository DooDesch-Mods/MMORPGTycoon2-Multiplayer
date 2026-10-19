#include "steam_action_adapter.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/core/config.h"
#include <algorithm>
namespace mt2mp::action_session {
bool SteamTransport::initialize(std::string& cause){return net::session_wire::prepare(cause);}
bool SteamTransport::claim(Handler handler,std::string& cause) {
    if(!handler){cause="Steam action handler missing";return false;}
    return net::session_wire::claim_handler(channel,[handler=std::move(handler)](uint64_t peer,const uint8_t* bytes,uint32_t size){handler(peer,bytes,size);},cause);
}
bool SteamTransport::send(uint64_t target,const std::vector<uint8_t>& bytes,std::string& cause) {
    if(bytes.empty()||bytes.size()>max_transport_payload){cause="Steam Channel9 transport payload length invalid";return false;}
    if(!net::session_wire::send(channel,bytes.data(),uint32_t(bytes.size()),target)){cause="Steam action packet queue rejected: "+net::session_wire::failure_reason();return false;}
    cause.clear();return true;
}
bool SteamTransport::poll(std::string& cause){return net::session_wire::tick(cause);}
bool SteamSession::current(Context& out,std::string& cause) {
    if(Config::instance().local_test){cause="Steam action adapter cannot bind local sidecar session";return false;}
    std::array<uint64_t,5> ids{};uint64_t lobby=0,host=0,local=0;
    uint64_t own=0;
    if(!net::session_wire::read_binding(ids,lobby,own,cause))return false;
    host=ids[1];local=ids[own];
    const auto p=net::snapshot_progress();const auto uuid=net::current_snapshot_id();const auto epoch=net::current_world_epoch();
    if(p.value("phase",std::string{})!="active"||!p.value("world_authority_valid",false)||uuid.size()!=36||!epoch||
       !p.value("network_identity_error",std::string{}).empty()) {
        cause="Steam action session requires actual native snapshot LoadACK/network identity";return false;
    }
    const auto member_count=std::count_if(ids.begin()+1,ids.end(),[](auto id){return id!=0;});
    if(local==host) {
        if(p.value("peers_expected",0)!=member_count-1||p.value("peers_loaded",0)!=member_count-1){cause="Steam Host snapshot LoadACK count differs from frozen lobby membership";return false;}
        const auto loads=p.value("peer_loads",nlohmann::json::array());
        if(loads.size()!=size_t(member_count-1)){cause="Steam Host peer LoadACK receipts missing";return false;}
        uint8_t ack_slots=0;
        for(const auto& load:loads) {
            const auto peer=load.value("peer",uint64_t{});
            if(peer<2||peer>4||!ids[peer]||(ack_slots&(1u<<(peer-1)))||!load.value("loaded",false)||!load.value("network_verified",false)) {
                cause="Steam peer native network identity LoadACK absent or wrong slot";return false;
            }
            ack_slots|=uint8_t(1u<<(peer-1));
        }
    }
    uint8_t mask=0;own=0;
    for(size_t i=1;i<ids.size();++i)if(ids[i]){mask|=uint8_t(1u<<(i-1));if(ids[i]==local)own=i;}
    out={net::session_wire::session_id(),own,1,mask,local==host?Role::Host:Role::Client,uuid};cause.clear();return true;
}
}
