#include "mt2mp/core/reporting_session.h"
#include "mt2mp/core/reporting.h"
#include "mt2mp/net/steam_matchmaking.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/core/config.h"
#include "../game/typed_action_runtime.h"
#include <chrono>

namespace mt2mp::reporting {
void tick_session(){
    using Clock=std::chrono::steady_clock;
    static auto next=Clock::time_point{};
    if(Clock::now()<next)return;
    next=Clock::now()+std::chrono::seconds(1);
    static uint64_t lobby=0;
    static std::string diagnostic,previous_action;
    static bool claimed=false;
    static auto last_sent=Clock::time_point{};
    static int previous_outcome=-1;
    auto& match=net::SteamMatchmaking::instance();
    const auto current=Config::instance().local_test?Config::instance().session_id:match.current_lobby_id();
    if(lobby!=current){
        if(lobby)end_session();lobby=current;diagnostic.clear();previous_action.clear();previous_outcome=-1;
        last_sent={};
    }
    if(!lobby)return;
    if(!claimed){
        std::string cause;
        if(!net::session_wire::claim_handler(31,[&](uint64_t peer,const uint8_t* bytes,uint32_t length){
            if(peer!=1||!bytes||length!=36){observe(2,"reporting_session.cpp",__LINE__,"Rejected diagnostic identity packet owner/size");return;}
            const std::string shared(reinterpret_cast<const char*>(bytes),length);
            if(!valid_id(shared)){observe(2,"reporting_session.cpp",__LINE__,"Rejected diagnostic identity packet format");return;}
            diagnostic=shared;
        },cause)){observe(2,"reporting_session.cpp",__LINE__,"Diagnostic channel unavailable: "+cause);return;}
        claimed=true;
    }
    const auto slot=net::session_wire::local_peer();
    if(slot==1){
        if(diagnostic.empty())diagnostic=random_id();
        if(net::session_wire::connected()&&(last_sent==Clock::time_point{}||Clock::now()-last_sent>=std::chrono::seconds(5))){
            last_sent=Clock::now();
            if(!net::session_wire::send(31,reinterpret_cast<const uint8_t*>(diagnostic.data()),uint32_t(diagnostic.size())))
                observe(2,"reporting_session.cpp",__LINE__,"Shared diagnostic identity send was not acknowledged by transport queue");
        }
    }
    if(valid_id(diagnostic)&&slot>=1&&slot<=4)bind_session(diagnostic,unsigned(slot));
    // This accessor reads retained Controller receipts and performs no world scan.
    const auto action=game::typed_action_runtime::controller_status();
    const auto id=action.value("action_uuid",std::string{});const auto outcome=action.value("outcome",-1);
    if(valid_id(id)&&(id!=previous_action||outcome!=previous_outcome)){
        const auto code=outcome==1?"action.confirmed":outcome==2?"action.rejected":outcome==3?"action.unknown":"action.pending";
        event(code,{{"revision",action.value("host_revision",uint64_t{})},{"peer_slot",slot}},id);
        previous_action=id;previous_outcome=outcome;
    }
}
}
