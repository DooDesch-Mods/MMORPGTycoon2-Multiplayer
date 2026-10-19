#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "production.h"
#include "protocol.h"
#include "typed_action_runtime.h"
#include "prepare_flow.h"
#include "lobby_operations.h"
#include "time_integration.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/snapshot_sync.h"
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <cstdio>

namespace mt2mp::game::session_reload {
namespace {
using json=nlohmann::json;
enum class Phase {Idle,Capture,ReturnTitle,Retire,Peers,Handshake,Prepare,Playing,Failed};
Phase phase=Phase::Idle;
Barrier barrier;
DWORD thread=0;
bool initialized=false,manual_requested=false,was_host=false,had_world=false,title_requested=false;
bool client_request_pending=false;
uint64_t client_request_began=0;
uint64_t began=0,request_count=0,completed_count=0,rejected_count=0;
uint64_t old_session=0;
float resume_scale=0;
std::string operation,saved_snapshot,first_failure,last_wait;
std::string last_failure;
bool bind_thread(std::string& error){
    if(thread){
        if(thread==GetCurrentThreadId())return true;
        error="world reload caller differs from bound game thread";return false;
    }
    uintptr_t image=0;uint32_t actual=0;std::string sha;
    if(!native::read_resolver_receipt(image,actual,sha,error))return false;
    if(!image||!actual||actual!=GetCurrentThreadId()||
       sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"){
        error="world reload exact native image/game-thread binding differs";return false;
    }
    thread=actual;error.clear();return true;
}
const char* label(){
    switch(phase){
    case Phase::Idle:return "Connected";
    case Phase::Capture:return "Saving shared world";
    case Phase::ReturnTitle:case Phase::Retire:case Phase::Peers:return "Preparing shared world";
    case Phase::Handshake:return "Connecting players";
    case Phase::Prepare:return "Loading shared world";
    case Phase::Playing:return "Synchronizing world";
    case Phase::Failed:return "Session unavailable";
    }
    return "Session unavailable";
}
void enter(Phase value){phase=value;began=GetTickCount64();last_wait.clear();LOG_INFO("[WorldReload] {}",label());}
void wait(const std::string& cause){
    if(last_wait!=cause){LOG_INFO("[WorldReload] Waiting: {}",cause);last_wait=cause;}
}
std::string token(std::string& error){
    std::array<uint8_t,16> bytes{};
    const auto result=BCryptGenRandom(nullptr,bytes.data(),ULONG(bytes.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(result<0){error="world reload token generation NTSTATUS="+std::to_string(result);return {};}
    bytes[6]=uint8_t((bytes[6]&15)|64);bytes[8]=uint8_t((bytes[8]&63)|128);
    std::array<char,37> text{};
    std::snprintf(text.data(),text.size(),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
    return text.data();
}
bool send(uint64_t peer,const Packet& packet,std::string& error){
    if(!valid(packet)){error="world reload outbound identity invalid";return false;}
    const auto raw=encode(packet).dump(-1,' ',false,json::error_handler_t::replace);
    if(!net::session_wire::send(channel,reinterpret_cast<const uint8_t*>(raw.data()),uint32_t(raw.size()),peer)){
        error="world reload send not acknowledged: "+net::session_wire::failure_reason();return false;
    }
    error.clear();return true;
}
bool send_required(const Packet& packet,std::string& error){
    for(uint64_t peer=2;peer<=4;++peer)if(packet.required&(1u<<(peer-1)))if(!send(peer,packet,error))return false;
    return true;
}
void fail(const std::string& cause,bool notify=true){
    if(first_failure.empty())first_failure=cause.empty()?"world reload failed without cause":cause;
    last_failure=first_failure;
    if(phase!=Phase::Failed){
        LOG_ERROR("[WorldReload] Failed phase={} cause={}",label(),first_failure);
        if(notify&&barrier.active()&&net::session_wire::connected()&&net::session_wire::session_id()==old_session){
            auto packet=barrier.packet();packet.kind=Kind::Failed;packet.cause=first_failure.substr(0,512);
            std::string error;
            const bool queued=was_host?send_required(packet,error):send(1,packet,error);
            if(!queued)LOG_ERROR("[WorldReload] Failure notification not acknowledged: {}",error);
        }
        phase=Phase::Failed;
    }
}
bool freeze(std::string& error){
    if(native::in_game()&&!native::set_clock(true,1,error))return false;
    return typed_action_runtime::begin_world_reload(error);
}
bool host_begin(std::string& error){
    const auto controller=typed_action_runtime::controller_status();
    if(controller.value("pending",false)||lobby_operations::pending()||!lobby_operations::executor_idle()){
        error="current action or native operation is still active";return false;
    }
    if(controller.value("locked",false)||controller.value("live_failure_terminal",false)){
        error="existing Controller failed: "+controller.value("cause",std::string{});fail(error);return false;
    }
    const auto prepare=lobby_prepare::status();
    if(prepare.value("state",0)!=0&&!prepare.value("playing",false)){
        error="initial shared world preparation is still active";return false;
    }
    const auto id=token(error);if(id.empty()){fail(error);return false;}
    Packet packet{Kind::Begin,net::session_wire::session_id(),net::session_wire::membership_retirement_mask(),id,{}};
    if(!barrier.begin(packet,1,error)){fail(error);return false;}
    old_session=packet.session;was_host=true;had_world=native::in_game();resume_scale=0;title_requested=false;
    if(had_world){native::ClockState clock;if(!native::read_clock(clock,error)){fail(error);return false;}resume_scale=clock.paused?0:clock.time_scale;}
    if(!freeze(error)){fail(error);return false;}
    if(!send_required(packet,error)){fail(error);return false;}
    manual_requested=false;++request_count;
    if(had_world){
        if(!lobby_operations::submit("capture_local_snapshot",json::object(),operation,error)){fail(error);return false;}
        enter(Phase::Capture);
    }else enter(Phase::ReturnTitle);
    return true;
}
void receive(uint64_t sender,const uint8_t* bytes,uint32_t size){
    Packet packet;std::string error;
    if(!bind_thread(error)){fail(error,false);return;}
    if(!decode(bytes,size,packet,error)){++rejected_count;LOG_WARN("[WorldReload] Packet rejected sender={} cause={}",sender,error);return;}
    if(packet.session!=net::session_wire::session_id()){++rejected_count;return;}
    const auto local=net::session_wire::local_peer();
    const auto mask=net::session_wire::peer_mask();
    if(sender<1||sender>4||sender==local||!(mask&(1u<<(sender-1)))){++rejected_count;LOG_WARN("[WorldReload] Physical sender outside current binding");return;}
    if(packet.kind==Kind::Request){
        if(local!=1||phase!=Phase::Idle||packet.required!=mask){++rejected_count;return;}
        manual_requested=true;return;
    }
    if(packet.kind==Kind::Begin){
        if(sender!=1||local==1||!(packet.required&(1u<<(local-1)))||(packet.required&~mask)){
            ++rejected_count;LOG_WARN("[WorldReload] Begin requires current physical Host/quorum");return;
        }
        if(barrier.active()){
            if(barrier.packet()!=packet){++rejected_count;LOG_WARN("[WorldReload] Conflicting Begin while world retirement active");}
            return;
        }
        if(phase!=Phase::Idle||!barrier.begin(packet,uint8_t(local),error)){fail(error.empty()?"world reload Begin while native operation active":error);return;}
        old_session=packet.session;was_host=false;had_world=native::in_game();title_requested=false;++request_count;
        client_request_pending=false;
        if(!freeze(error)){fail(error);return;}
        enter(Phase::ReturnTitle);return;
    }
    if(!barrier.active()||packet.token!=barrier.packet().token||packet.required!=barrier.packet().required){++rejected_count;return;}
    if(packet.kind==Kind::Failed){fail("peer="+std::to_string(sender)+" world reload: "+packet.cause,false);return;}
    if(packet.kind!=Kind::Ready||local!=1||!barrier.acknowledge(sender,packet,error)){
        ++rejected_count;LOG_WARN("[WorldReload] Ready rejected: {}",error);return;
    }
}
void clear(){
    barrier.clear();phase=Phase::Idle;manual_requested=was_host=had_world=title_requested=client_request_pending=false;old_session=began=client_request_began=0;
    operation.clear();saved_snapshot.clear();first_failure.clear();last_wait.clear();
}
}
bool initialize(std::string& error){
    if(initialized){error.clear();return true;}
    if(!net::session_wire::claim_handler(channel,receive,error))return false;
    initialized=true;error.clear();return true;
}
bool busy(){return phase!=Phase::Idle;}
bool release_for_leave(std::string& error){
    if(GetCurrentThreadId()!=thread||lobby_operations::pending()){
        error="world reload Leave waits for the current native operation; it was not cancelled";return false;
    }
    clear();error.clear();return true;
}
bool request_resync(std::string& error){
    if(!initialized||!bind_thread(error)||phase!=Phase::Idle||manual_requested||client_request_pending||!net::session_wire::connected()){
        error="resync requires the idle current game-thread session";return false;
    }
    if(net::session_wire::local_peer()==1){manual_requested=true;error.clear();return true;}
    const auto id=token(error);if(id.empty())return false;
    if(!send(1,{Kind::Request,net::session_wire::session_id(),net::session_wire::peer_mask(),id,{}},error))return false;
    client_request_pending=true;client_request_began=GetTickCount64();return true;
}
void tick()try{
    if(!initialized)return;
    std::string thread_error;if(!bind_thread(thread_error)){fail(thread_error);return;}
    if(phase==Phase::Idle){
        if(client_request_pending&&GetTickCount64()-client_request_began>=uint64_t(net::snapshot_native_load_timeout_seconds)*1000){
            fail("world reload request timed out waiting for the Host begin; request was not replayed");return;
        }
        if(net::session_wire::connected()&&net::session_wire::local_peer()==1&&
           (manual_requested||net::session_wire::membership_pending())){
            std::string error;if(!host_begin(error)&&phase!=Phase::Failed)wait(error);
        }
        return;
    }
    if(phase==Phase::Failed)return;
    if(GetTickCount64()-began>=uint64_t(net::snapshot_native_load_timeout_seconds)*1000){
        fail("world reload timeout phase="+std::string(label())+" required<900s; native operation retained");return;
    }
    if(!net::session_wire::connected()&&phase!=Phase::Handshake&&phase!=Phase::Prepare&&phase!=Phase::Playing){
        fail("world reload lost original transport: "+net::session_wire::failure_reason());return;
    }
    std::string error;
    if(phase==Phase::Capture){
        json response;if(!lobby_operations::take_response(operation,response))return;
        if(!response.value("ok",false)){fail(response.value("error",json::object()).value("message",std::string("shared capture failed")));return;}
        const auto& result=response.at("result");
        if(!result.value("building_bindings_confirmed",false)){fail("world reload capture identity incomplete: "+result.value("building_identity_error",std::string{}));return;}
        saved_snapshot=result.at("snapshot_id").get<std::string>();
        if(!uuid(saved_snapshot)){fail("world reload capture snapshot ID invalid");return;}
        operation.clear();enter(Phase::ReturnTitle);
    }
    if(phase==Phase::ReturnTitle){
        native::WorldStatus actual;if(!native::read_world_status(actual,error)){fail(error);return;}
        if(actual.active_mode_index==2&&!actual.title_transition_pending&&actual.saves_in_progress<=0){enter(Phase::Retire);}
        else if(!title_requested&&!actual.title_transition_pending){
            if(actual.saves_in_progress>0){wait("native save is still active");return;}
            if(!native::in_game()){wait("native mode transition is still active");return;}
            title_requested=true;
            if(!native::request_return_to_title(error)){fail(error);return;}
            return;
        }else return;
    }
    if(phase==Phase::Retire){
        bool done=false;
        if(!typed_action_runtime::finish_world_reload(done,error)){fail(error);return;}
        if(!done)return;
        if(!lobby_prepare::reset_after_world_reload(error)){fail(error);return;}
        if(was_host){
            if(!barrier.acknowledge(1,barrier.ready_packet(),error)){fail(error);return;}
            enter(Phase::Peers);
        }else{
            if(!send(1,barrier.ready_packet(),error)){fail(error);return;}
            enter(Phase::Handshake);
        }
    }
    if(phase==Phase::Peers){
        if(!barrier.retain_members(net::session_wire::membership_retirement_mask(),error)){fail(error);return;}
        if(!barrier.complete())return;
        if(!net::session_wire::refresh_membership(error)){
            if(error.starts_with("info:")){wait(error);return;}
            fail(error);return;
        }
        enter(Phase::Handshake);
    }
    if(phase==Phase::Handshake){
        if(!net::session_wire::connected())return;
        if(!Config::instance().local_test&&net::session_wire::session_id()==old_session)return;
        if(was_host&&had_world){
            if(!lobby_prepare::prepare_snapshot(saved_snapshot,error)||!lobby_prepare::start(error)){fail(error);return;}
            enter(Phase::Prepare);
        }else if(was_host){++completed_count;clear();return;}
        else enter(Phase::Playing);
    }
    if(phase==Phase::Prepare){
        if(!lobby_prepare::tick(error)){fail(error);return;}
        if(lobby_prepare::status().value("playing",false))enter(Phase::Playing);
    }
    if(phase==Phase::Playing){
        if(!was_host){
            if(!lobby_prepare::tick(error)){fail(error);return;}
            if(!lobby_prepare::status().value("playing",false))return;
        }
        if(was_host&&resume_scale>0){
            const auto intent=resume_scale==1?time_session::Intent::Normal:resume_scale==3?time_session::Intent::Fast:time_session::Intent::Fastest;
            if(resume_scale!=1&&resume_scale!=3&&resume_scale!=9){fail("world reload previous speed outside original1/3/9 scales");return;}
            const auto receipt=time_integration::instance().submit(intent);
            if(receipt.disposition!=time_session::Disposition::Accepted&&receipt.disposition!=time_session::Disposition::Duplicate){
                fail("world reload time restoration rejected: "+receipt.cause);return;
            }
            native::ClockState actual;
            if(!native::read_clock(actual,error)||actual.paused||actual.time_scale!=resume_scale){
                fail("world reload time restoration readback differs scale="+std::to_string(actual.time_scale)+
                    " required="+std::to_string(resume_scale)+" cause="+error);return;
            }
        }
        ++completed_count;LOG_INFO("[WorldReload] Shared reload completed session={} snapshot={}",net::session_wire::session_id(),net::current_snapshot_id());clear();
    }
}catch(const std::exception& cause){fail(std::string("world reload: ")+cause.what());}
json status(){return {{"active",busy()},{"state",int(phase)},{"label",label()},{"session",old_session},
    {"token",barrier.active()?barrier.packet().token:std::string{}},{"pending_peer_mask",barrier.pending()},
    {"queued",manual_requested||client_request_pending},
    {"operation_id",operation},{"snapshot_id",saved_snapshot},{"requested",request_count},{"completed",completed_count},
    {"rejected_packets",rejected_count},{"wait",last_wait},{"error",first_failure},{"last_failure",last_failure}};}
}
