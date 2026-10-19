#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "production.h"
#ifdef MT2MP_FRAMEWORK_PLUGIN
#include "runtime.h"
#include "native_read.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/steam_networking.h"
#include "typed_action_runtime.h"
#include "typed_action/action_session.h"
#include "dungeon_scene_runtime.h"
#include <mt2fw/plugin.h>
#include <Windows.h>
#include <memory>

namespace mt2mp::game::coop_cursor::production {
namespace {
using json=nlohmann::json;
const MT2FW_Host* host=nullptr;
DWORD thread=0;
std::unique_ptr<Runtime> runtime;
bool subscribed=false,active=false;
uint64_t framework_epoch=0,last_context_read=0;
Context cached_context;
std::string last_error;
std::string cached_context_error;
uint64_t repeated_errors=0;
bool text_is(const json& value,const char* field,const std::string& expected){
    return value.is_object()&&value.contains(field)&&value.at(field).is_string()&&value.at(field).get<std::string>()==expected;
}

bool request(const char* operation,const json& args,json& result,std::string& error){
    if(!host||GetCurrentThreadId()!=thread){error="cursor Framework Host/thread unavailable";return false;}
    try{
        std::array<char,16384> output{};
        const auto encoded=args.dump();
        const auto count=host->request(host->context,operation,encoded.c_str(),output.data(),output.size());
        if(!count||count>output.size()||output[count-1]){error="cursor Framework response invalid; request not replayed";return false;}
        const auto response=json::parse(output.data(),output.data()+count-1);
        if(!response.at("ok").get<bool>()){error=response.at("error").get<std::string>();return false;}
        result=response.at("result");error.clear();return true;
    }catch(const std::exception& cause){error=std::string("cursor Framework response: ")+cause.what();return false;}
}
bool hide(uint64_t peer,std::string& error){
    json result;
    const auto id="peer-"+std::to_string(peer);
    if(!request("ui.world_marker",{{"action","hide"},{"id",id}},result,error))return false;
    if(!text_is(result,"id",id)||!text_is(result,"status","hidden")){
        error="cursor Framework hide acknowledgement differs";return false;
    }
    return true;
}
bool refresh_context(Context& out,std::string& error){
    out={};
    action_session::Context actual;action_session::Binding binding;
    uint64_t publication=0;bool playing=false;
    if(!typed_action_runtime::bug_context(actual,binding,publication,playing,error))return false;
    if(!playing){error.clear();return true;}
    if(actual.session!=net::session_wire::session_id()||actual.local_peer!=net::session_wire::local_peer()||
       actual.peer_mask!=net::session_wire::peer_mask()){
        error="cursor Controller and transport session differ";return false;
    }
    json world;
    if(!request("world.status",json::object(),world,error))return false;
    try{
        if(!world.at("loaded").get<bool>()){
            framework_epoch=0;error.clear();return true;
        }
        const auto& epoch=world.at("epoch");
        if(!epoch.is_number_unsigned()||!epoch.get<uint64_t>()){error="cursor Framework world epoch invalid";return false;}
        framework_epoch=epoch.get<uint64_t>();
    }catch(const std::exception& cause){error=std::string("cursor Framework world status: ")+cause.what();return false;}
    std::string scene;
    if(!dungeon_scene_runtime::cursor_scene(scene,error))return false;
    out={actual.session,actual.local_peer,actual.peer_mask,actual.snapshot_uuid,std::move(scene)};
    error.clear();return true;
}
bool context(Context& out,std::string& error){
    if(!active||!native::in_game()||!net::session_wire::connected()){
        out={};cached_context={};cached_context_error.clear();last_context_read=0;error.clear();return true;
    }
    const auto now=GetTickCount64();
    if(!last_context_read||now<last_context_read||now-last_context_read>=100){
        last_context_read=now;
        Context current;
        if(refresh_context(current,cached_context_error))cached_context=std::move(current);
        else{
            cached_context={};
            if(cached_context_error.empty())cached_context_error="cursor context read failed without cause";
        }
    }
    out=cached_context;error=cached_context_error;return error.empty();
}
bool read_sample(Sample& out,std::string& error){
    uintptr_t image=0;uint32_t bound_thread=0;std::string sha;
    if(!native::read_resolver_receipt(image,bound_thread,sha,error))return false;
    if(bound_thread!=thread||GetCurrentThreadId()!=thread||
       sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"){
        error="cursor native resolver executable/thread differs";return false;
    }
    return read_native_sample(image,[](uintptr_t address,void* output,size_t bytes,std::string& cause){
        SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),output,bytes,&copied)||copied!=bytes){
            cause="cursor native read failed Win32="+std::to_string(GetLastError())+" bytes="+std::to_string(copied)+" required="+std::to_string(bytes);return false;
        }
        return true;
    },out,error);
}
std::string name(uint64_t peer){
    if(Config::instance().local_test)return peer==1?"Host":"Player "+std::to_string(peer);
    std::array<uint64_t,5> ids{};uint64_t lobby=0,local=0;std::string error;
    if(!net::session_wire::read_binding(ids,lobby,local,error))throw std::runtime_error(error);
    if(peer>=ids.size()||!ids[peer])throw std::runtime_error("cursor remote physical peer binding unavailable");
    auto value=net::SteamNetworking::instance().get_persona_name(ids[peer]);
    if(value.size()>64){size_t end=64;while(end&&(static_cast<unsigned char>(value[end])&0xc0)==0x80)--end;value.resize(end);}
    for(auto& c:value)if(static_cast<unsigned char>(c)<32||static_cast<unsigned char>(c)==127)c=' ';
    if(value.empty())value="Player "+std::to_string(peer);
    return value;
}
bool show(uint64_t peer,const std::array<float,3>& point,std::string& error){
    constexpr std::array<std::array<float,4>,4> colors{{{0.25f,0.78f,1,1},{1,0.7f,0.2f,1},{0.65f,0.4f,1,1},{0.3f,1,0.5f,1}}};
    if(!framework_epoch||peer<1||peer>4){error="cursor marker world/peer unavailable";return false;}
    try{
        json result;
        const auto id="peer-"+std::to_string(peer),epoch=std::to_string(framework_epoch);
        if(!request("ui.world_marker",{{"action","set"},{"id",id},
            {"epoch",epoch},{"world",point},{"name",name(peer)},{"color",colors[peer-1]}},result,error))return false;
        if(!text_is(result,"id",id)||!text_is(result,"status","accepted")||
           !text_is(result,"epoch",epoch)||!result.contains("revision")||!result.at("revision").is_number_unsigned()||!result.at("revision").get<uint64_t>()){
            error="cursor Framework set acknowledgement differs";return false;
        }
        return true;
    }catch(const std::exception& cause){error=std::string("cursor marker name: ")+cause.what();return false;}
}
void report(const std::string& cause){
    if(cause.empty()){
        if(!last_error.empty())LOG_INFO("[Cursor] Recovered after {} repeated failures",repeated_errors);
        last_error.clear();repeated_errors=0;return;
    }
    if(last_error!=cause){LOG_WARN("[Cursor] {} previous_repeats={}",cause,repeated_errors);last_error=cause;repeated_errors=0;}
    if(repeated_errors!=UINT64_MAX)++repeated_errors;
}
}
bool bind(const MT2FW_Host* provider,std::string& error){
    if(active){error="cursor Framework owner already bound";return false;}
    if(!provider||provider->size<sizeof(*provider)||provider->abi_version!=MT2FW_ABI_VERSION||!provider->request){error="cursor Framework Host ABI incomplete";return false;}
    if(!subscribed){
        if(!net::session_wire::claim_handler(channel,[](uint64_t sender,const uint8_t* bytes,uint32_t size){
            if(!active||!runtime)return;
            if(GetCurrentThreadId()!=thread){report("cursor packet arrived outside bound game thread");return;}
            if(runtime->receive(sender,bytes,size,GetTickCount64())==Reception::Rejected)report(runtime->status().cause);
        },error))return false;
        subscribed=true;
    }
    host=provider;thread=GetCurrentThreadId();active=true;framework_epoch=0;cached_context={};cached_context_error.clear();last_context_read=0;
    runtime=std::make_unique<Runtime>(Ports{context,read_sample,[](uint8_t number,const uint8_t* bytes,uint32_t size,std::string& cause){
        if(net::session_wire::send(number,bytes,size))return true;
        cause="cursor transport did not acknowledge send: "+net::session_wire::failure_reason();return false;
    },show,hide});
    error.clear();return true;
}
void tick(){
    if(!active||!runtime)return;
    if(GetCurrentThreadId()!=thread){report("cursor tick outside bound game thread");return;}
    const bool okay=runtime->tick(GetTickCount64());
    report(okay?std::string{}:runtime->status().cause);
}
bool retire(std::string& error){
    if(!active){error.clear();return true;}
    if(GetCurrentThreadId()!=thread){error="cursor retirement outside bound game thread";return false;}
    if(runtime&&!runtime->retire(GetTickCount64())){error=runtime->status().cause;report(error);return false;}
    json result;
    if(!request("ui.world_marker",{{"action","clear"}},result,error)){report(error);return false;}
    if(!text_is(result,"status","retired")){error="cursor Framework clear acknowledgement differs";report(error);return false;}
    runtime.reset();active=false;host=nullptr;cached_context={};framework_epoch=0;last_context_read=0;error.clear();return true;
}
json status(){
    if(!runtime)return {{"active",false},{"cause",last_error}};
    const auto& s=runtime->status();
    return {{"active",active},{"framework_epoch",framework_epoch},{"sent",s.sent},{"received",s.received},
        {"stale",s.stale},{"other_world",s.other_world},{"rejected",s.rejected},{"send_failed",s.send_failed},
        {"display_failed",s.display_failed},{"read_failed",s.read_failed},{"cause",last_error}};
}
}
#endif
