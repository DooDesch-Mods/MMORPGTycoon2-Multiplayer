#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/session_binding.h"
#include <windows.h>
#include <bcrypt.h>
#include "mt2mp/net/steam_module.h"
#include "mt2mp/net/steam_matchmaking.h"
#include "mt2mp/net/steam_networking.h"
#include "mt2mp/net/local_session.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "vendor/nlohmann/json.hpp"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <chrono>

namespace mt2mp::net::session_wire {
namespace {
using json=nlohmann::json;
using Clock=std::chrono::steady_clock;
struct Header {char magic[4];uint32_t revision;uint64_t lobby,session;};
static_assert(sizeof(Header)==24);
using Send=bool(*)(void*,uint64_t,const void*,uint32_t,int,int);
using Available=bool(*)(void*,uint32_t*,int);
using Read=bool(*)(void*,void*,uint32_t,uint32_t*,uint64_t*,int);
void* api=nullptr;Send raw_send=nullptr;Available raw_available=nullptr;Read raw_read=nullptr;
std::map<uint8_t,SidecarHandler> handlers;
std::array<uint64_t,5> ids{};
uint64_t lobby=0,own=0;
uint8_t mask=0,hello_ack=0;
bool bound=false,hello_sent=false,ended=false,actions_suspended=false;
std::string failure,last_logged;
uint64_t repeats=0,retiring_packets=0;
std::vector<std::string> retired_local_snapshots;
Clock::time_point hello_started{};
bool local(){return Config::instance().local_test;}
bool fail(const std::string& cause,std::string& error) {
    error=cause;
    if(!cause.starts_with("info:")){ended=true;if(failure.empty())failure=cause;}
    if(last_logged!=cause){
        if(cause.starts_with("info:"))LOG_NET_INFO("[SessionWire] {} previous_repeats={}",cause,repeats);
        else LOG_NET_ERROR("[SessionWire] {} previous_repeats={}",cause,repeats);
        last_logged=cause;repeats=0;
    }else ++repeats;
    return false;
}
#include "session_membership.inc"
bool initialize_api(std::string& error) {
    if(api)return true;
    auto dll=mt2mp::net::steam_module();using Access=void*(*)();
    auto accessor=dll?reinterpret_cast<Access>(GetProcAddress(dll,"SteamAPI_SteamNetworking_v006")):nullptr;
    raw_send=dll?reinterpret_cast<Send>(GetProcAddress(dll,"SteamAPI_ISteamNetworking_SendP2PPacket")):nullptr;
    raw_available=dll?reinterpret_cast<Available>(GetProcAddress(dll,"SteamAPI_ISteamNetworking_IsP2PPacketAvailable")):nullptr;
    raw_read=dll?reinterpret_cast<Read>(GetProcAddress(dll,"SteamAPI_ISteamNetworking_ReadP2PPacket")):nullptr;
    api=accessor?accessor():nullptr;
    if(!api||!raw_send||!raw_available||!raw_read)return fail("installed Steam flat P2P API unavailable",error);
    return true;
}
bool emit(uint8_t channel,const uint8_t* bytes,uint32_t size,uint64_t target,std::string& error) {
    if(channel<3||channel>31||!bytes||!size||size>65536||target>4||!bound||!api)return fail("Steam wire send channel/length/target/authority invalid",error);
    if(target&&(!ids[target]||target==own))return fail("Steam wire target absent or local",error);
    std::vector<uint8_t> packet(sizeof(Header)+size);
    Header header{{'M','T','2','W'},2,lobby,current_session};
    std::memcpy(packet.data(),&header,sizeof(header));std::memcpy(packet.data()+sizeof(header),bytes,size);
    for(size_t i=1;i<ids.size();++i)if(ids[i]&&i!=own&&(!target||target==i)) {
        if(!raw_send(api,ids[i],packet.data(),uint32_t(packet.size()),2,channel))return fail("Steam wire packet queue rejected channel="+std::to_string(channel)+" slot="+std::to_string(i),error);
    }
    error.clear();return true;
}
#include "session_control.inc"

}
bool prepare(std::string& error) {
    if(local()){
        if(!connected()){error="info: awaiting local physical handshake";return false;}
        return prepare_local_membership(error);
    }
    try {
        if(!binding(error,true)||!initialize_api(error))return false;
        if(!hello_sent) {
            if(!local_instance)local_instance=nonce();
            const auto bytes=json{{"type","hello"},{"session",current_session},{"lobby",lobby},{"instance",local_instance},{"for_instance",uint64_t(0)}}.dump();
            if(!emit(3,reinterpret_cast<const uint8_t*>(bytes.data()),uint32_t(bytes.size()),0,error))return false;
            hello_sent=true;hello_started=Clock::now();hello_ack=uint8_t(1u<<(own-1));
            LOG_NET_INFO("[SessionWire] Symmetric hello queued; awaiting physical peer ACK mask={} required={}",hello_ack,mask);
        }
        error.clear();return true;
    }catch(const std::exception& e){return fail(e.what(),error);}
}
bool tick(std::string& error) {
    if(local()){error.clear();return true;}
    try {
        if(!binding(error,false)||!prepare(error))return false;
        std::vector<uint8_t> channels{3};for(const auto& [channel,handler]:handlers)channels.push_back(channel);
        for(auto channel:channels) {
        if(channel!=3&&!connected())continue;
        for(unsigned count=0;count<16;++count) {
            uint32_t size=0;if(!raw_available(api,&size,channel))break;
            if(size<=sizeof(Header)||size>65536+sizeof(Header))return fail("Steam wire advertised packet length invalid",error);
            std::vector<uint8_t> packet(size);uint32_t actual=0;uint64_t physical_sender=0;
            if(!raw_read(api,packet.data(),size,&actual,&physical_sender,channel)||actual!=size)return fail("Steam wire advertised packet read/length failed",error);
            const auto found=std::find(ids.begin()+1,ids.end(),physical_sender);
            if(found==ids.end()||physical_sender==ids[own]){if(++discarded_unknown_packets==1)LOG_NET_WARN("[SessionWire] Ignored packet outside current physical binding; repeats counted");continue;}
            Header header{};std::memcpy(&header,packet.data(),sizeof(header));
            if(std::memcmp(header.magic,"MT2W",4)||header.revision!=2||header.lobby!=lobby)return fail("Steam wire packet protocol/lobby differs",error);
            if(header.session!=current_session){if(++discarded_epoch_packets==1)LOG_NET_WARN("[SessionWire] Ignored packet from retired session epoch; repeats counted");continue;}
            const auto peer=uint64_t(found-ids.begin());const auto* data=packet.data()+sizeof(header);const auto length=actual-uint32_t(sizeof(header));
            if(channel==3){if(!control(peer,data,length,error))return false;}
            else {
                if(!connected())return fail("Steam application packet arrived before physical handshake ACK",error);
                if(channel==9&&actions_suspended){if(++retiring_packets==1)LOG_NET_WARN("[SessionWire] Retiring action channel dropped old session packet; repeats counted");}
                else handlers.at(channel)(peer,data,length);
            }
        }
        }
        if(hello_sent&&hello_ack!=mask&&Clock::now()-hello_started>std::chrono::seconds(20))return fail("Steam handshake timed out ACK mask="+std::to_string(hello_ack)+" required="+std::to_string(mask),error);
        if(last_logged.starts_with("info:")){LOG_NET_INFO("[SessionWire] Recovered wait={} repeats={}",last_logged,repeats);last_logged.clear();repeats=0;}
        error.clear();return true;
    }catch(const std::exception& e){return fail(e.what(),error);}
}
bool connected(){return local()?SidecarClient::instance().connected()&&SidecarClient::instance().ready()&&local_session_connected():bound&&!ended&&hello_sent&&hello_ack==mask;}
size_t peers(){return local()?local_session_peers():connected()?size_t(std::count_if(ids.begin()+1,ids.end(),[](auto id){return id!=0;}))-1:0;}
uint8_t peer_mask(){return local()?local_session_peer_mask():connected()?mask:0;}
uint64_t local_peer(){return local()?Config::instance().peer_id:own;}
uint64_t session_id(){return local()?local_membership_generation():current_session;}
bool send(uint8_t channel,const uint8_t* bytes,uint32_t size,uint64_t target) {
    if(local()){if(channel==9&&actions_suspended)return false;return SidecarClient::instance().send(channel,bytes,size,target);}
    std::string error;
    try {if(!binding(error,false)||!connected())return fail("Steam application send requires physical handshake ACK",error);return emit(channel,bytes,size,target,error);}
    catch(const std::exception& e){return fail(e.what(),error);}
}
void register_handler(uint8_t channel,SidecarHandler handler) {
    if(local()){SidecarClient::instance().register_handler(channel,std::move(handler));return;}
    std::string error;if(!claim_handler(channel,std::move(handler),error))throw std::runtime_error(error);
}
bool claim_handler(uint8_t channel,SidecarHandler handler,std::string& error) {
    if(local()){
        if(channel!=9)return SidecarClient::instance().claim_handler(channel,std::move(handler),error);
        if(!handler){error="local action claim requires a handler";return false;}
        auto guarded=[handler=std::move(handler)](uint64_t peer,const uint8_t* bytes,uint32_t size){
            bool retired=false;
            if(!actions_suspended&&bytes&&size&&size<=65536&&!retired_local_snapshots.empty()){
                const auto value=json::parse(bytes,bytes+size,nullptr,false);
                const json* identity=&value;
                if(value.is_object()&&value.contains("request"))identity=&value.at("request");
                else if(value.is_object()&&value.contains("frame"))identity=&value.at("frame");
                if(identity->is_object()&&identity->contains("snapshot_uuid")&&identity->at("snapshot_uuid").is_string()){
                    const auto old=identity->at("snapshot_uuid").get<std::string>();
                    retired=std::find(retired_local_snapshots.begin(),retired_local_snapshots.end(),old)!=retired_local_snapshots.end();
                }
            }
            // Keep malformed and unknown packets on the existing codec failure path.
            if(actions_suspended||retired){if(++retiring_packets==1)LOG_NET_WARN("[SessionWire] Local retired action packet dropped; repeats counted");return;}
            handler(peer,bytes,size);
        };
        if(!SidecarClient::instance().claim_handler(channel,std::move(guarded),error))return false;
        actions_suspended=false;error.clear();return true;
    }
    if(channel<4||channel>31||!handler||handlers.contains(channel)){error="Steam application handler channel already owned or invalid";return false;}
    handlers.emplace(channel,std::move(handler));if(channel==9)actions_suspended=false;error.clear();return true;
}
std::string failure_reason(){return local()?SidecarClient::instance().failure_reason():failure;}
bool read_binding(std::array<uint64_t,5>& out,uint64_t& id,uint64_t& mine,std::string& error) {
    if(local()){error="Steam binding requested for local Sidecar adapter";return false;}
    try {if(!binding(error,false)||!connected()){if(error.empty())error="Steam physical handshake incomplete";return false;}out=ids;id=lobby;mine=own;error.clear();return true;}
    catch(const std::exception& e){return fail(e.what(),error);}
}
}

namespace mt2mp::net::session_wire {
#include "session_refresh.inc"
void suspend_actions(){
    if(local()){const auto old=current_snapshot_id();if(!old.empty()&&std::find(retired_local_snapshots.begin(),retired_local_snapshots.end(),old)==retired_local_snapshots.end())retired_local_snapshots.push_back(old);}
    actions_suspended=true;
}
bool retire_lobby(std::string& error){
    if(local()){
        if(!publish_local_host_leave(error))return false;
        if(!SidecarClient::instance().retire_action_handler(error))return false;
        return leave_local_lobby(error);
    }
    if(SteamMatchmaking::instance().in_lobby()){error="wire retirement requires confirmed lobby absence";return false;}
    handlers.erase(9);ids={};lobby=own=current_session=pending_session=last_refresh_session=0;mask=hello_ack=0;bound=hello_sent=ended=actions_suspended=false;
    refresh_allowed=restart_pending=false;remote_instances={};observed_slots={};discarded_epoch_packets=discarded_unknown_packets=0;
    failure.clear();last_logged.clear();repeats=retiring_packets=0;hello_started={};error.clear();return true;
}
}
