#include "mt2mp/net/local_session.h"
#include "mt2mp/net/local_membership.h"
#include "mt2mp/net/sidecar_client.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/overlay/lobby_overlay.h"
#include "vendor/nlohmann/json.hpp"
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <windows.h>
#include <bcrypt.h>

namespace mt2mp::net {
using json = nlohmann::json;
using clock = std::chrono::steady_clock;
namespace {
std::mutex state_mutex;
std::map<uint64_t, clock::time_point> peers;
local_membership::Registry membership;
bool connected = false, ended = false, is_host = false, session_started = false;
bool participating=true,membership_refresh=false;
uint64_t local_peer = 0, session_id = 0, sequence = 0;
clock::time_point started{}, last_send{};
std::string status = "Relay wird verbunden";
std::atomic<bool> end_notice{false};
std::string host_left_snapshot, published_leave_snapshot, publish_leave_error;
bool departure_snapshot(const std::string& value) {
    if(value.size()!=36)return false;
    for(size_t i=0;i<value.size();++i){
        if(i==8||i==13||i==18||i==23){if(value[i]!='-')return false;}
        else if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return false;
    }
    return true;
}
void end_session(const std::string& reason) {
    if (ended) return;
    ended = true; connected = false; peers.clear(); status = reason; end_notice = true;
    LOG_NET_ERROR("[LOCAL] SESSION_ENDED: {}", reason);
    SidecarClient::instance().request_disconnect(reason);
}
bool transmit(const json& message, uint64_t target = 0) {
    const auto bytes = message.dump();
    if (!SidecarClient::instance().send(3, reinterpret_cast<const uint8_t*>(bytes.data()), uint32_t(bytes.size()), target)) {
        LOG_NET_ERROR("[LOCAL] Session control queue failed target={} cause={}", target, SidecarClient::instance().failure_reason()); return false;
    }
    return true;
}
json control_message(const char* type){
    if(sequence==(std::numeric_limits<uint64_t>::max)())throw std::runtime_error("local control sequence exhausted");
    json value={{"version",2},{"session",session_id},{"sequence",++sequence},
        {"instance",membership.instance()},{"participating",participating},{"type",type}};
    if(is_host){value["generation"]=membership.generation();value["slots"]=membership.bound();}
    return value;
}
bool publish_membership(std::string& error){
    if(!membership.commit_host(error))return false;
    if(!transmit(control_message("binding"))){error="local Host membership queue failed: "+SidecarClient::instance().failure_reason();return false;}
    LOG_NET_INFO("[LOCAL] Host roster published generation={} mask={}; shared world ACK still required",membership.generation(),membership.mask());
    error.clear();return true;
}
void receive(uint64_t sender, const uint8_t* bytes, uint32_t length) {
    std::lock_guard lock(state_mutex);
    if (ended) return;
    if (sender < 1 || sender > 4 || sender == local_peer || length > 4096) { LOG_NET_ERROR("[LOCAL] Invalid session sender={} bytes={}", sender, length); return; }
    try {
        const auto message = json::parse(bytes, bytes + length);
        if(!message.is_object()||!message.at("version").is_number_integer()||message.at("version")!=2||
           !message.at("session").is_number_unsigned()||message.at("session")!=session_id||
           !message.at("sequence").is_number_unsigned()||!message.at("instance").is_number_unsigned()||
           !message.at("participating").is_boolean()||!message.at("type").is_string())
            throw std::runtime_error("local control version/session/process schema differs");
        const auto seq = message.at("sequence").get<uint64_t>();
        const auto type = message.at("type").get<std::string>();
        const auto instance=message.at("instance").get<uint64_t>();
        const auto joined=message.at("participating").get<bool>();
        if((is_host&&sender==1)||(!is_host&&sender!=1))throw std::runtime_error("local control sender differs from Host topology");
        const auto fields=size_t(sender==1?8:6)+(type=="host_left"?1:0);
        if(message.size()!=fields)throw std::runtime_error("local control exact fields differ");
        if(type!="hello"&&type!="welcome"&&type!="heartbeat"&&type!="binding"&&type!="host_left"&&type!="left")
            throw std::runtime_error("unknown local session control type");
        if((type=="welcome"||type=="binding"||type=="host_left")&&sender!=1)
            throw std::runtime_error("local Host control type sent by a Client");
        if(type=="left"&&(sender==1||joined))throw std::runtime_error("local departure requires a nonparticipating Client");
        bool fresh=false;std::string error;
        if(!membership.observe(sender,instance,seq,fresh,error,joined)){
            if(sender==1&&membership.host_instance()&&membership.host_instance()!=instance){end_session(error);return;}
            throw std::runtime_error(error);
        }
        if(!fresh)return;
        if(sender==1){
            if(!message.at("generation").is_number_unsigned()||!message.at("slots").is_array()||message.at("slots").size()!=5)
                throw std::runtime_error("local Host roster generation/slots schema differs");
            local_membership::Slots slots{};
            for(size_t i=0;i<slots.size();++i){
                if(!message.at("slots").at(i).is_number_unsigned())throw std::runtime_error("local Host roster slot is not an unsigned process identity");
                slots[i]=message.at("slots").at(i).get<uint64_t>();
            }
            if(!membership.stage(message.at("generation").get<uint64_t>(),slots,error))throw std::runtime_error(error);
            if(!membership.generation()||membership_refresh){
                if(membership.adopt_client(error))membership_refresh=false;
                else if(!error.starts_with("info:"))throw std::runtime_error(error);
            }
        }
        if(type=="host_left"){
            if(is_host||sender!=1||!peers.contains(1)||!message.at("snapshot").is_string())
                throw std::runtime_error("Host departure requires current confirmed Host and exact control fields");
            const auto snapshot=message.at("snapshot").get<std::string>();
            if(!departure_snapshot(snapshot))throw std::runtime_error("Host departure snapshot is not a canonical UUID");
            host_left_snapshot=snapshot;
            LOG_NET_INFO("[LOCAL] Host departure received for shared snapshot={}",snapshot);
            return;
        }
        if (type == "hello" && is_host) {
            if (!transmit(control_message("welcome"), sender)) { end_session("Sitzung beendet: Welcome konnte nicht gesendet werden"); return; }
            peers[sender] = clock::now();
            if (!connected) { connected = true; status = "Lokale Sitzung verbunden"; LOG_NET_INFO("[LOCAL] SESSION_CONNECTED peer={} host=1", sender); }
        } else if (type == "welcome" && !is_host && sender == 1) {
            peers[sender] = clock::now();
            if (!connected) { connected = true; status = "Lokale Sitzung verbunden"; LOG_NET_INFO("[LOCAL] SESSION_CONNECTED peer={} host=0", local_peer); }
        } else if(type=="heartbeat"||type=="binding"||type=="left")peers[sender]=clock::now();
    } catch (const std::exception& error) { LOG_NET_ERROR("[LOCAL] Session packet rejected sender={} cause={}", sender, error.what()); }
}
void maintain() {
    std::lock_guard lock(state_mutex);
    if (ended) return;
    auto& transport = SidecarClient::instance();
    if (!transport.connected()) { end_session("Sitzung beendet: " + transport.failure_reason()); return; }
    const auto now = clock::now();
    if (transport.ready() && now - last_send >= std::chrono::seconds(1)) {
        last_send = now;
        if (!transmit(control_message(connected ? "heartbeat" : "hello"), is_host ? 0 : 1)) {
            end_session("Sitzung beendet: Heartbeat konnte nicht gesendet werden"); return;
        }
    }
    for (auto peer = peers.begin(); peer != peers.end();) {
        const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - peer->second).count();
        if (age > 10000) {
            LOG_NET_WARN("[LOCAL] Peer heartbeat timeout peer={} age_ms={} required<=10000; transport worker active", peer->first, age);
            if (!is_host && peer->first == 1) { end_session("Sitzung beendet: Host nicht erreichbar"); return; }
            membership.departed(peer->first);peer = peers.erase(peer);
        } else ++peer;
    }
    if (!connected && now - started > std::chrono::seconds(30)) end_session("Sitzungsaufbau fehlgeschlagen: keine Bestätigung nach 30 Sekunden");
}
}
bool start_local_session() {
    const auto& config = Config::instance();
    {
        std::lock_guard lock(state_mutex);
        if (session_started) { LOG_NET_ERROR("[LOCAL] Session start refused: existing session cannot be implicitly reset; no reconnect"); return false; }
        session_started = true;
        peers.clear(); connected = ended = false; end_notice = false;
        local_peer = config.peer_id; session_id = config.session_id; is_host = config.is_host; sequence = 0;
        uint64_t instance=0;
        const auto result=BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&instance),sizeof instance,BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        std::string error;
        if(result<0||!membership.initialize(local_peer,instance,error)){
            status=result<0?"local process identity generation failed NTSTATUS="+std::to_string(result):error;
            ended=true;LOG_NET_ERROR("[LOCAL] {}",status);return false;
        }
        participating=true;membership_refresh=false;
        started = clock::now(); last_send = {}; status = "Relay wird verbunden";
        host_left_snapshot.clear();published_leave_snapshot.clear();publish_leave_error.clear();
    }
    auto& transport = SidecarClient::instance();
    transport.register_session_handler(receive, maintain);
    return transport.init(session_id, local_peer, is_host);
}
void tick_local_session() {
    if (end_notice.exchange(false)) overlay::LobbyOverlay::instance().set_visible(true);
    if (local_session_connected()) SidecarClient::instance().tick();
}
bool local_session_connected() { std::lock_guard lock(state_mutex); return connected && !ended && SidecarClient::instance().connected(); }
size_t local_session_peers() {
    std::lock_guard lock(state_mutex);const auto mask=membership.mask();size_t count=0;
    for(unsigned i=0;i<4;++i)if(mask&(1u<<i))++count;
    return count?count-1:0;
}
uint8_t local_session_peer_mask() {
    std::lock_guard lock(state_mutex);
    if(!connected||ended||!SidecarClient::instance().connected()||local_peer<1||local_peer>4) return 0;
    return membership.mask();
}
std::string local_session_status() { std::lock_guard lock(state_mutex); return status; }
bool publish_local_host_leave(std::string& error) {
    std::lock_guard lock(state_mutex);
    if(!is_host){error.clear();return true;}
    const auto snapshot=current_snapshot_id();
    if(snapshot.empty()){error.clear();return true;}
    if(snapshot==published_leave_snapshot){error=publish_leave_error;return error.empty();}
    if(!departure_snapshot(snapshot)){error="local Host departure requires a canonical current snapshot";return false;}
    if(!connected||ended||!SidecarClient::instance().connected()||!SidecarClient::instance().ready()){
        error="local Host departure transport unavailable: "+status+"; original="+SidecarClient::instance().failure_reason();return false;
    }
    // Retain this attempt even when the queue rejects it; Leave must not replay it.
    published_leave_snapshot=snapshot;
    auto message=control_message("host_left");message["snapshot"]=snapshot;
    if(!transmit(message)){
        publish_leave_error="local Host departure queue failed: "+SidecarClient::instance().failure_reason();
        error=publish_leave_error;return false;
    }
    LOG_NET_INFO("[LOCAL] Host departure queued for shared snapshot={}; peer native retirement still required",snapshot);
    error.clear();return true;
}
bool local_host_left(const std::string& snapshot) {
    std::lock_guard lock(state_mutex);
    return !is_host&&!snapshot.empty()&&snapshot==host_left_snapshot;
}
bool prepare_local_membership(std::string& error){
    std::lock_guard lock(state_mutex);
    if(!connected||ended){error="info: awaiting physical local handshake";return false;}
    if(membership.generation()){error.clear();return true;}
    if(is_host)return publish_membership(error);
    return membership.adopt_client(error);
}
bool local_membership_pending(){std::lock_guard lock(state_mutex);return membership.changed();}
uint64_t local_membership_generation(){std::lock_guard lock(state_mutex);return membership.generation();}
uint8_t local_membership_retirement_mask(){std::lock_guard lock(state_mutex);return membership.retirement_mask();}
void retire_local_membership(){std::lock_guard lock(state_mutex);membership_refresh=true;}
bool refresh_local_membership(std::string& error){
    std::lock_guard lock(state_mutex);
    if(!is_host||!membership_refresh){error="local membership refresh requires retired Host world";return false;}
    if(!publish_membership(error))return false;
    membership_refresh=false;error.clear();return true;
}
bool join_local_lobby(std::string& error){
    std::lock_guard lock(state_mutex);
    if(!connected||ended){error="local lobby join requires the physical Host connection";return false;}
    participating=true;
    if(!transmit(control_message("hello"),is_host?0:1)){error="local lobby join queue failed";return false;}
    error.clear();return true;
}
bool leave_local_lobby(std::string& error){
    std::lock_guard lock(state_mutex);
    participating=false;membership_refresh=true;
    if(!is_host&&!transmit(control_message("left"),1)){error="local lobby departure queue failed";return false;}
    error.clear();return true;
}
} // namespace mt2mp::net
