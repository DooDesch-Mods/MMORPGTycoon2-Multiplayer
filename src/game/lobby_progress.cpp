#include "lobby_progress.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/steam_matchmaking.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include <array>
#include <chrono>
namespace mt2mp::game::lobby_progress {
namespace {
using Phase=lobby_presentation::Phase;
using Clock=std::chrono::steady_clock;
Phase received=Phase::Choosing;
bool claimed=false,was_member=false;
std::string last_error,published;
auto next=Clock::time_point{};
Phase sent=Phase::Failed;
void report(const std::string& cause){
    if(cause==last_error)return;
    if(cause.empty()){if(!last_error.empty())LOG_INFO("[LobbyProgress] Status publication recovered");}
    else LOG_WARN("[LobbyProgress] {}",cause);
    last_error=cause;
}
}
void tick(bool member,bool host,Phase phase){
    auto& match=net::SteamMatchmaking::instance();
    if(member!=was_member){received=Phase::Choosing;published.clear();next={};was_member=member;}
    if(!member)return;
    if(!claimed){
        std::string error;
        if(!net::session_wire::claim_handler(12,[](uint64_t sender,const uint8_t* data,uint32_t size){
            if(sender!=1||!data||size!=5||data[0]!='M'||data[1]!='P'||data[2]!='U'||data[3]!=1||data[4]>uint8_t(Phase::Creating)){
                report("Rejected loading-status packet: Host, format or phase differs");return;
            }
            const auto phase=Phase(data[4]);
            if(received!=phase)LOG_INFO("[LobbyProgress] Host loading phase received={}",unsigned(phase));
            received=phase;report({});
        },error)){report(error);return;}
        claimed=true;
    }
    if(!host){
        if(Clock::now()<next)return;
        next=Clock::now()+std::chrono::seconds(1);
        if(!Config::instance().local_test){
            const auto value=match.get_lobby_data("mt2mp_world_phase");
            Phase phase;
            if(lobby_presentation::parse_phase(value,phase))received=phase;
        }
        return;
    }
    const auto now=Clock::now();
    if(now<next&&phase==sent)return;
    next=now+std::chrono::seconds(2);
    if(sent!=phase)LOG_INFO("[LobbyProgress] Host loading phase published={}",unsigned(phase));
    sent=phase;
    const auto value=std::to_string(unsigned(phase));
    if(!Config::instance().local_test&&value!=published){
        std::string error;
        if(!match.publish_lobby_data("mt2mp_world_phase",value,error)){report(error);return;}
        published=value;
    }
    if(net::session_wire::connected()){
        const std::array<uint8_t,5> packet{'M','P','U',1,uint8_t(phase)};
        if(!net::session_wire::send(12,packet.data(),uint32_t(packet.size()))){report("Loading-status transport queue rejected packet");return;}
    }
    report({});
}
Phase remote_phase(){return Config::instance().is_host?sent:received;}
}
