#include "mt2mp/core/framework_runtime.h"
#include "mt2mp/core/game_update_pair.h"
#include "mt2mp/core/framework_input.h"
#include "mt2mp/core/dll_main.h"
#include "mt2mp/core/private_profile_paths.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/reporting.h"
#include "mt2mp/core/reporting_session.h"
#include "mt2mp/hooks/framework_backend.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/sidecar_client.h"
#include "mt2mp/net/steam_matchmaking.h"
#include "mt2mp/net/steam_networking.h"
#include "../game/native_lobby.h"
#include "../game/object_identity/native_free_runtime/native_free_runtime.h"
#include "../game/object_identity/remove_runtime/remove_runtime.h"
#include "../game/ui_build_runtime.h"
#include "../game/ui_move_runtime.h"
#include "../game/ui_remove_runtime.h"
#include <array>
#include <chrono>
#include <fstream>
#include <optional>

extern "C" MT2FW_EXPORT int MT2FW_CALL MT2FW_PluginLoad(const MT2FW_Host*);
namespace mt2mp::framework_runtime {
using json=nlohmann::json;
namespace fs=std::filesystem;
void retire();
namespace {
struct State {
    const MT2FW_Host* host=nullptr;
    HMODULE module=nullptr;
    MT2FW_MT2_ServiceV1 service{};
    MT2FW_MT2_RoutesV1 routes{};
    MT2FW_MT2_OwnershipV1 ownership{};
    framework_profile::Paths paths;
    std::optional<framework_profile::LocalTest> test;
    json modules;
    std::array<uint32_t,size_t(game_update_pair::Section::Count)> sections{};
    uint64_t update_token=0,render_token=0,stop_token=0,pair_token=0,entered_frames=0;
    DWORD thread=0;
    std::chrono::steady_clock::time_point reporting_previous_begin{};
    bool attempted=false,prepared=false,pair_active=false,ready=false,retired=false,stop_requested=false,reporting_started=false;
    std::string failure;
};
State state;
void stop_reporting() noexcept {
    try{reporting::shutdown();state.reporting_started=false;}
    catch(const std::exception& error){OutputDebugStringA((std::string("error: Multiplayer reporting shutdown failed: ")+error.what()+"\n").c_str());}
    catch(...){OutputDebugStringA("error: Multiplayer reporting shutdown failed: unknown native exception\n");}
}
void MT2FW_CALL stop(void*,const char*,size_t) noexcept;
void fail(const char* operation,const std::string& cause) noexcept {
    try {
        const auto message=std::string(operation)+": "+cause;
        if(state.failure==message)return;
        state.failure=message;
        OutputDebugStringA(("error: Multiplayer Framework "+message+"\n").c_str());
        LOG_ERROR("[Framework] {}",message);
        if(state.host){
            thread_local std::array<char,4*1024*1024> output;
            const auto input=json{{"level","error"},{"message",message}}.dump();
            const auto count=state.host->request(state.host->context,"log",input.c_str(),output.data(),output.size());
            if(!count||count>output.size()||output[count-1])OutputDebugStringA("error: Multiplayer Framework error log acknowledgement invalid; no replay\n");
        }
    } catch(const std::exception& error){OutputDebugStringA(error.what());}
    catch(...){OutputDebugStringA("error: Multiplayer Framework failure reporting threw unknown exception\n");}
}
bool callback(void* owner) {
    if(owner!=&state||!state.host||state.retired)throw std::runtime_error("Framework callback owner unavailable or retired");
    if(!state.thread)state.thread=GetCurrentThreadId();
    if(state.thread!=GetCurrentThreadId())throw std::runtime_error("Framework callback thread differs from game thread");
    return true;
}
bool reclaim(void*,uintptr_t,std::string& cause) {
    return game::ui_move_runtime::can_reclaim_global_minhook(cause)&&game::ui_build_runtime::can_reclaim_global_minhook(cause)&&
        game::ui_remove_runtime::can_reclaim_global_minhook(cause)&&remove_runtime::Runtime::instance().can_reclaim_global_minhook(cause)&&
        native_free_runtime::Runtime::instance().can_reclaim_global_minhook(cause);
}
void hook_log(void*,const char* operation,const std::string& cause){LOG_ERROR("[FrameworkHook] {} cause={}",operation,cause);}
game_update_pair::TraceToken trace_begin(void*,game_update_pair::Section part) {
    const auto token=state.service.scope_begin(state.service.context,state.sections.at(size_t(part)));
    return {token.epoch,token.token};
}
void trace_end(void*,game_update_pair::Section part,game_update_pair::TraceToken token) {
    state.service.scope_end(state.service.context,state.sections.at(size_t(part)),{token.epoch,token.token});
}
int MT2FW_CALL begin(void* owner,const MT2FW_MT2_Frame* frame) noexcept {
    try {
        if(!callback(owner)||!state.ready||state.stop_requested||!state.failure.empty())return 0;
        if(!frame||frame->size<sizeof(*frame)||frame->reserved)throw std::runtime_error("Framework begin receipt is invalid");
        const auto observed=std::chrono::steady_clock::now();
        std::string error;
        if(!game_update_pair::begin(&state,{frame->id,frame->game,frame->dt,frame->original_returned},error))throw std::runtime_error(error);
        if(state.reporting_previous_begin!=std::chrono::steady_clock::time_point{})
            reporting::frame(std::chrono::duration<double,std::milli>(observed-state.reporting_previous_begin).count());
        state.reporting_previous_begin=observed;
        ++state.entered_frames;
        return 1;
    }catch(const std::exception& error){fail("GameUpdate begin",error.what());}
    catch(...){fail("GameUpdate begin","unknown native exception");}
    return 0;
}
void MT2FW_CALL end(void* owner,const MT2FW_MT2_Frame* frame) noexcept {
    try {
        if(!callback(owner))return;
        if(!frame||frame->size<sizeof(*frame)||frame->reserved)throw std::runtime_error("Framework end receipt is invalid");
        std::string error;
        bool closed=false;
        const bool passed=game_update_pair::end(&state,{frame->id,frame->game,frame->dt,frame->original_returned},error,&closed);
        if(closed){
            if(!state.entered_frames)throw std::runtime_error("Framework end has no entered Multiplayer frame");
            --state.entered_frames;
        }
        if(!passed)fail("GameUpdate end",error);
        if(state.stop_requested&&!state.entered_frames)retire();
    }catch(const std::exception& error){fail("GameUpdate end",error.what());}
    catch(...){fail("GameUpdate end","unknown native exception");}
}
void prepare() {
    state.attempted=true;
    const auto provider=GetModuleHandleW(L"MT2Framework.dll");
    if(!provider)throw std::runtime_error("Loaded MT2Framework provider is absent");
    const auto query=reinterpret_cast<MT2FW_MT2_QueryServiceFn>(GetProcAddress(provider,"MT2FW_MT2_QueryService"));
    const auto query_routes=reinterpret_cast<MT2FW_MT2_QueryRoutesFn>(GetProcAddress(provider,"MT2FW_MT2_QueryRoutes"));
    const auto query_ownership=reinterpret_cast<MT2FW_MT2_QueryOwnershipFn>(GetProcAddress(provider,"MT2FW_MT2_QueryOwnership"));
    if(!query||!query_routes||!query_ownership)throw std::runtime_error("Framework private native services are unavailable");
    if(query(state.host,MT2FW_MT2_SERVICE_VERSION,&state.service,sizeof(state.service))!=MT2FW_MT2_OK||
       query_routes(state.host,MT2FW_MT2_ROUTES_VERSION,&state.routes,sizeof(state.routes))!=MT2FW_MT2_OK||
       query_ownership(state.host,MT2FW_MT2_OWNERSHIP_VERSION,&state.ownership,sizeof(state.ownership))!=MT2FW_MT2_OK)
        throw std::runtime_error("Framework native service query rejected active plugin owner");
    const auto& api=state.service;
    if(!api.frame_subscribe||!api.frame_unsubscribe||!api.section_register||!api.scope_begin||!api.scope_end)
        throw std::runtime_error("Framework native service lacks required paired update/profiling functions");
    state.paths=framework_profile::paths(api);
    state.modules=framework_profile::module_receipt(state.paths,state.module);
    state.reporting_started=true;
    reporting::initialize(state.paths.data,state.modules);
    private_profile::DirectoryPins diagnostic_pins;
    diagnostic_pins.verify(state.paths.diagnostics,true);
    const auto config=state.paths.data/L"local-test.json";
    if(fs::exists(config)) {
        if(!private_profile::equal(private_profile::canonical_file(config),config)||fs::file_size(config)>16384)
            throw std::runtime_error("Framework local test config path or bounded size differs");
        std::ifstream stream(config,std::ios::binary);
        if(!stream)throw std::runtime_error("Framework local test config open failed");
        const auto value=json::parse(stream);
        state.test=framework_profile::local_test(state.paths,value);
    }
    auto& backend=hooks::framework::process_backend();
    if(!backend.bind(state.routes,reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),framework_profile::plugin_id,state.thread,reclaim,hook_log,&state)||
       !backend.bind_ownership(state.ownership))throw std::runtime_error("Framework hook owner binding failed: "+backend.error());
    state.prepared=true;
    std::string error;
    if(!game::native_lobby::framework_bind(state.host,&state.routes,&state.ownership,error))throw std::runtime_error("Framework lobby bind failed: "+error);
    if(!initialize(state.module))throw std::runtime_error("Multiplayer engine initialization failed; see original runtime cause");
    if(!framework_input::bind(state.ownership,state.thread,error))throw std::runtime_error("Framework accepted-input observer bind failed: "+error);
    for(size_t i=0;i<state.sections.size();++i){
        state.sections[i]=api.section_register(api.context,game_update_pair::section_names[i]);
        if(!state.sections[i])throw std::runtime_error("Framework Multiplayer trace section registration failed");
    }
    if(!game_update_pair::activate(&state,{&state,trace_begin,trace_end},error))throw std::runtime_error(error);
    state.pair_active=true;
    state.pair_token=api.frame_subscribe(api.context,begin,end,&state);
    if(!state.pair_token)throw std::runtime_error("Framework actual Multiplayer GameUpdate pair registration failed");
    state.ready=true;
    publish_instance();
    LOG_INFO("[Framework] Actual Multiplayer engine/session runtime ready; SDL/renderer/filesystem owned by Framework");
}
void MT2FW_CALL update(void* owner,const char*,size_t) noexcept {
    try{if(callback(owner)){if(!state.attempted)prepare();if(state.ready)reporting::tick_session();}}
    catch(const std::exception& error){fail("initialization",error.what());if(state.prepared&&state.thread==GetCurrentThreadId())stop(owner,nullptr,0);else if(state.reporting_started)stop_reporting();}
    catch(...){fail("initialization","unknown native exception");if(state.prepared&&state.thread==GetCurrentThreadId())stop(owner,nullptr,0);else if(state.reporting_started)stop_reporting();}
}
void MT2FW_CALL render(void* owner,const char*,size_t) noexcept {
    try {
        if(!callback(owner)||!state.ready||state.stop_requested)return;
        std::string error;
        if(!game::native_lobby::framework_render(error))throw std::runtime_error(error);
        render_capture();
    }catch(const std::exception& error){fail("native menu render",error.what());}
    catch(...){fail("native menu render","unknown native exception");}
}
void MT2FW_CALL stop(void* owner,const char*,size_t) noexcept {
    try {
        if(!callback(owner)||state.stop_requested)return;
        state.stop_requested=true;
        const auto once=[](const char* operation,auto&& work) {
            try{work();}
            catch(const std::exception& error){fail(operation,error.what());}
            catch(...){fail(operation,"unknown native exception");}
        };
        once("stop session admission",[]{net::session_wire::suspend_actions();});
        once("stop Steam lobby",[]{net::SteamMatchmaking::instance().shutdown();});
        once("stop Steam transport",[]{net::SteamNetworking::instance().shutdown();});
        once("stop local transport",[]{net::SidecarClient::instance().shutdown();});
        if(state.entered_frames){LOG_INFO("[Framework] Stop waits for entered Multiplayer frames count={}",state.entered_frames);return;}
        retire();
    }catch(const std::exception& error){fail("stop",error.what());}
    catch(...){fail("stop","unknown native exception");}
}
}
bool prepared()noexcept{return state.prepared&&!state.retired;}
void configure(){if(!prepared())throw std::runtime_error("Framework runtime paths were never admitted");framework_profile::configure(Config::instance(),state.paths,state.test?&*state.test:nullptr);}
const framework_profile::Paths& paths(){if(!prepared())throw std::runtime_error("Framework paths unavailable");return state.paths;}
const json& modules(){if(!prepared())throw std::runtime_error("Framework module evidence unavailable");return state.modules;}
const MT2FW_MT2_RoutesV1& routes(){if(!prepared())throw std::runtime_error("Framework routes unavailable");return state.routes;}
const MT2FW_MT2_OwnershipV1& ownership(){if(!prepared())throw std::runtime_error("Framework ownership unavailable");return state.ownership;}
void publish_instance(){
    if(!state.test)return;
    const auto value=framework_profile::instance_receipt(state.paths,*state.test,state.modules);
    const auto path=state.paths.runtime/L"instance.json";auto temporary=path;temporary+=L".tmp";
    {std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);if(!stream)throw std::runtime_error("Framework instance temporary open failed");stream<<value.dump(2);stream.flush();if(!stream)throw std::runtime_error("Framework instance write failed");}
    const bool exists=fs::exists(path);
    if(exists?!ReplaceFileW(path.c_str(),temporary.c_str(),nullptr,0,nullptr,nullptr):!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Framework instance publication failed Win32="+std::to_string(GetLastError()));
}
int load(const MT2FW_Host* host) {
    if(state.host||!host||host->size<sizeof(*host)||host->abi_version!=MT2FW_ABI_VERSION||!host->context||!host->request||!host->subscribe||!host->unsubscribe)return 0;
    state.host=host;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&MT2FW_PluginLoad),&state.module))
        throw std::runtime_error("Framework Multiplayer module pin failed Win32="+std::to_string(GetLastError()));
    state.update_token=host->subscribe(host->context,"update",update,&state);
    state.render_token=host->subscribe(host->context,"render",render,&state);
    state.stop_token=host->subscribe(host->context,"stop",stop,&state);
    if(!state.update_token||!state.render_token||!state.stop_token)throw std::runtime_error("Framework Multiplayer event registration failed");
    return 1;
}
void retire(){
    if(!state.host||state.retired)return;
    if(state.entered_frames)throw std::runtime_error("Framework Multiplayer retirement has entered frames count="+std::to_string(state.entered_frames));
    if(state.pair_active){
        std::string error;
        if(!game_update_pair::retire(&state,error))throw std::runtime_error("Framework pair retirement failed: "+error);
        state.pair_active=false;
    }
    state.ready=false;
    if(state.pair_token){
        const auto result=state.service.frame_unsubscribe(state.service.context,state.pair_token);
        if(result!=MT2FW_MT2_OK&&result!=MT2FW_MT2_UNAVAILABLE)throw std::runtime_error("Framework pair unsubscribe failed result="+std::to_string(result));
        state.pair_token=0;
    }
    if(state.prepared){
        std::string error;
        if(!game::ui_build_runtime::framework_retire(error))throw std::runtime_error("Framework Create observer retirement unconfirmed: "+error);
        if(!game::ui_remove_runtime::framework_retire(error))throw std::runtime_error("Framework Remove authority retirement unconfirmed: "+error);
    }
    if(framework_input::read().subscribed){std::string error;if(!framework_input::unbind(error))throw std::runtime_error("Framework input observer retirement unconfirmed: "+error);}
    if(state.prepared){std::string error;if(!game::native_lobby::framework_unbind(error))throw std::runtime_error("Framework native menu retirement unconfirmed: "+error);}
    stop_reporting();
    shutdown();
    if(state.render_token)state.host->unsubscribe(state.host->context,state.render_token);
    if(state.update_token)state.host->unsubscribe(state.host->context,state.update_token);
    if(state.stop_token)state.host->unsubscribe(state.host->context,state.stop_token);
    state.retired=true;
}
}
extern "C" MT2FW_EXPORT int MT2FW_CALL MT2FW_PluginQuery(MT2FW_PluginInfo* info){
    if(!info||info->size<sizeof(*info)||info->abi_version!=MT2FW_ABI_VERSION)return 0;
    info->id=mt2mp::framework_profile::plugin_id;info->version=mt2mp::MOD_VERSION;return 1;
}
extern "C" MT2FW_EXPORT int MT2FW_CALL MT2FW_PluginLoad(const MT2FW_Host* host){
    try{return mt2mp::framework_runtime::load(host);}
    catch(const std::exception& error){mt2mp::framework_runtime::fail("load",error.what());}
    catch(...){mt2mp::framework_runtime::fail("load","unknown native exception");}
    try{mt2mp::framework_runtime::retire();}
    catch(const std::exception& error){mt2mp::framework_runtime::fail("partial load cleanup",error.what());}
    catch(...){mt2mp::framework_runtime::fail("partial load cleanup","unknown native exception");}
    return 0;
}
extern "C" MT2FW_EXPORT void MT2FW_CALL MT2FW_PluginShutdown(){
    try{mt2mp::framework_runtime::retire();}
    catch(const std::exception& error){mt2mp::framework_runtime::fail("shutdown",error.what());}
    catch(...){mt2mp::framework_runtime::fail("shutdown","unknown native exception");}
}
