#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include "vendor/nlohmann/json.hpp"
#include "mt2mp/net/local_session.h"

using Json=nlohmann::json;
namespace fixture {
constexpr const char* first="11111111-2222-3333-4444-555555555555";
constexpr const char* next="aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
unsigned checks=0,send_calls=0,retire_calls=0,leave_calls=0,fail_calls=0;
bool connected=true,ready=true,queue_ok=true,retire_ok=true,leave_ok=true,steam_lobby=false;
std::string snapshot=first,transport_error,leave_error="original native Leave failure";
std::vector<Json> packets;
void check(bool value,const std::string& error){++checks;if(!value)throw std::runtime_error(error);}
template<class... T> void log(const T&...){ }
}
#define LOG_NET_INFO(...) ::fixture::log(__VA_ARGS__)
#define LOG_NET_ERROR(...) ::fixture::log(__VA_ARGS__)
#define LOG_NET_WARN(...) ::fixture::log(__VA_ARGS__)
namespace mt2mp {
struct Config {
    bool local_test=true,is_host=true;uint64_t session_id=77,peer_id=1;
    static Config& instance(){static Config value;return value;}
};
namespace overlay {
struct LobbyOverlay {
    static LobbyOverlay& instance(){static LobbyOverlay value;return value;}
    void set_visible(bool){}
};
}
namespace net {
using SidecarHandler=std::function<void(uint64_t,const uint8_t*,uint32_t)>;
struct SidecarClient {
    static SidecarClient& instance(){static SidecarClient value;return value;}
    bool connected(){return fixture::connected;}
    bool ready(){return fixture::ready;}
    std::string failure_reason(){return fixture::transport_error;}
    void request_disconnect(const std::string& error){fixture::connected=false;fixture::transport_error=error;}
    bool send(uint8_t channel,const uint8_t* bytes,uint32_t size,uint64_t){
        fixture::check(channel==3,"departure used non-control channel");++fixture::send_calls;
        fixture::packets.push_back(Json::parse(bytes,bytes+size));return fixture::queue_ok;
    }
    void register_session_handler(SidecarHandler,std::function<void()>){ }
    bool init(uint64_t,uint64_t,bool){return true;}
    void tick(){}
    bool retire_action_handler(std::string& error){
        ++fixture::retire_calls;if(!fixture::retire_ok)error="original action owner retirement failure";return fixture::retire_ok;
    }
};
std::string current_snapshot_id(){return fixture::snapshot;}
struct SteamMatchmaking {
    static SteamMatchmaking& instance(){static SteamMatchmaking value;return value;}
    bool in_lobby(){return fixture::steam_lobby;}
    uint64_t host_steam_id(){return fixture::steam_lobby?999:0;}
};
}
}
#include "host_departure_local_body.inc"
}
namespace mt2mp::net::session_wire {
bool local(){return Config::instance().local_test;}
uint64_t session_id(){return Config::instance().session_id;}
std::map<int,int> handlers;
std::array<uint64_t,5> ids{};
uint64_t lobby=0,own=0,repeats=0,retiring_packets=0;
uint8_t mask=0,hello_ack=0;
bool bound=false,hello_sent=false,ended=false,actions_suspended=false;
std::string failure,last_logged;
std::chrono::steady_clock::time_point hello_started{};
#include "host_departure_wire_body.inc"
}
namespace mt2mp::game::lobby_prepare {
bool leave(std::string& error){++fixture::leave_calls;if(!fixture::leave_ok)error=fixture::leave_error;return fixture::leave_ok;}
}
namespace mt2mp::game::native_lobby {
bool local_admitted=true;
uint64_t owner=999,generation=5;
std::string departed_host_attempt,departed_host_error,cause,ui_status;
bool fail(const std::string& error){++fixture::fail_calls;cause=error;return false;}
#include "host_departure_lobby_body.inc"
}
namespace local=mt2mp::net;
namespace lobby=mt2mp::game::native_lobby;
void reset(bool host){
    mt2mp::Config::instance()={true,host,77,host?1u:2u};
    local::is_host=host;local::local_peer=host?1:2;local::session_id=77;
    local::connected=true;local::ended=false;local::session_started=true;
    local::peers.clear();local::received.clear();local::sequence=0;
    if(!host)local::peers[1]=local::clock::now();
    local::host_left_snapshot.clear();local::published_leave_snapshot.clear();local::publish_leave_error.clear();
    fixture::connected=fixture::ready=fixture::queue_ok=fixture::retire_ok=fixture::leave_ok=true;
    fixture::steam_lobby=false;fixture::snapshot=fixture::first;fixture::transport_error.clear();
    fixture::send_calls=fixture::retire_calls=fixture::leave_calls=fixture::fail_calls=0;fixture::packets.clear();
    lobby::local_admitted=true;lobby::owner=999;lobby::generation=5;
    lobby::departed_host_attempt.clear();lobby::departed_host_error.clear();lobby::cause.clear();
}
Json departure(uint64_t seq=1){return {{"version",1},{"session",uint64_t{77}},{"sequence",seq},{"type","host_left"},{"snapshot",fixture::first}};}
void receive(Json value,uint64_t sender=1){const auto bytes=value.dump();local::receive(sender,reinterpret_cast<const uint8_t*>(bytes.data()),uint32_t(bytes.size()));}
int main(){try{
    using fixture::check;std::string error;
    reset(true);
    check(mt2mp::net::session_wire::retire_lobby(error),error);
    check(fixture::send_calls==1&&fixture::retire_calls==1,"Host did not send exactly one departure before retiring Channel9");
    const auto packet=fixture::packets.front();
    check(packet==departure(),"Host control did not bind exact session, snapshot and monotone sequence");
    check(mt2mp::net::session_wire::retire_lobby(error)&&fixture::send_calls==1,"Host retirement replayed departure");
    reset(false);receive(packet);
    check(local::local_host_left(fixture::first),"current confirmed Host departure missing");
    check(local::local_session_connected(),"Host departure disconnected reusable local transport");
    check(!local::local_host_left(fixture::next)&&!local::local_host_left(""),"stale snapshot notice leaked into new world");
    Json preparation={{"snapshot_id",fixture::first},{"leaving",false}};
    check(lobby::retire_departed_host(preparation,error)&&fixture::leave_calls==1,"Client did not use existing native Leave");
    check(lobby::retire_departed_host(preparation,error)&&fixture::leave_calls==1,"Client replayed existing Leave before next status tick");
    check(lobby::ui_status=="Host hat die Sitzung beendet","Host departure not visible");
    receive(packet);check(local::received[1]==1,"duplicate advanced control receipt");
    reset(false);fixture::leave_ok=false;receive(departure());
    check(!lobby::retire_departed_host(preparation,error)&&error.find(fixture::leave_error)!=std::string::npos,"native Leave cause lost");
    check(!lobby::retire_departed_host(preparation,error)&&fixture::leave_calls==1&&fixture::fail_calls==1,"failed native Leave automatically replayed");
    check(!lobby::cause.empty(),"failure replaced by success-shaped UI state");
    check(lobby::retire_departed_host({{"leaving",true}},error)&&fixture::leave_calls==1,"pending retirement invoked another Leave");
    reset(false);receive(departure());lobby::local_admitted=false;
    check(lobby::retire_departed_host(preparation,error)&&!fixture::leave_calls,"unadmitted Client used old departure");
    lobby::local_admitted=true;
    check(lobby::retire_departed_host({{"snapshot_id",fixture::next}},error)&&!fixture::leave_calls,"rehost new snapshot used stale departure");
    check(lobby::retire_departed_host(Json::object(),error)&&!fixture::leave_calls,"Title before shared snapshot used stale departure");
    reset(false);receive(departure(),3);check(!local::local_host_left(fixture::first),"foreign peer ended Host session");
    reset(false);local::peers.clear();receive(departure());check(!local::local_host_left(fixture::first),"unconfirmed Host ended session");
    reset(true);receive(departure(),2);check(!local::local_host_left(fixture::first),"Client ended Host session");
    for(const auto& key:{"session","sequence"}){
        reset(false);auto bad=departure();bad[key]=-1;receive(bad);check(!local::local_host_left(fixture::first),std::string("negative control ")+key+" accepted");
        reset(false);bad=departure();bad[key]=77.0;receive(bad);check(!local::local_host_left(fixture::first),std::string("floating control ")+key+" accepted");
    }
    reset(false);auto bad=departure();bad["session"]=uint64_t{88};receive(bad);check(!local::local_host_left(fixture::first),"wrong session ended Client");
    reset(false);bad=departure();bad["extra"]=true;receive(bad);check(!local::local_host_left(fixture::first),"extra departure field accepted");
    reset(false);bad=departure(0);receive(bad);check(!local::local_host_left(fixture::first),"zero sequence accepted");
    reset(false);bad=departure();bad["snapshot"]="invalid";receive(bad);check(!local::local_host_left(fixture::first),"invalid snapshot accepted");
    reset(false);receive(departure(4));bad=departure(3);bad["snapshot"]=fixture::next;receive(bad);check(local::local_host_left(fixture::first)&&!local::local_host_left(fixture::next),"reordered departure replaced current notice");
    reset(true);fixture::queue_ok=false;fixture::transport_error="original queue failure";
    check(!mt2mp::net::session_wire::retire_lobby(error)&&error.find(fixture::transport_error)!=std::string::npos&&!fixture::retire_calls,"queue failure lost or owner released");
    fixture::queue_ok=true;check(!mt2mp::net::session_wire::retire_lobby(error)&&fixture::send_calls==1,"failed send retried on retirement tick");
    reset(true);fixture::retire_ok=false;check(!mt2mp::net::session_wire::retire_lobby(error)&&error=="original action owner retirement failure","owner retirement failure hidden");
    check(fixture::send_calls==1,"owner failure generated extra departure");
    reset(false);check(mt2mp::net::session_wire::retire_lobby(error)&&fixture::send_calls==0&&fixture::retire_calls==1,"Client broadcast Host departure");
    reset(true);fixture::snapshot.clear();check(mt2mp::net::session_wire::retire_lobby(error)&&fixture::send_calls==0,"unshared Title fabricated snapshot departure");
    reset(true);fixture::ready=false;fixture::transport_error="original transport failure";
    check(!mt2mp::net::session_wire::retire_lobby(error)&&!fixture::retire_calls&&error.find(fixture::transport_error)!=std::string::npos,"unavailable transport cause lost");
    reset(false);mt2mp::Config::instance().local_test=false;
    check(lobby::retire_departed_host(preparation,error)&&fixture::leave_calls==1,"original Steam Host-loss route lost");
    check(lobby::retire_departed_host(preparation,error)&&fixture::leave_calls==1,"Steam Host-loss replayed Leave");
    reset(true);check(lobby::retire_departed_host(preparation,error)&&!fixture::leave_calls,"Host retired itself on foreign notice");
    std::cout<<fixture::checks<<" Host departure actual-body assertions PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
