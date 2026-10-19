#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "host_failure_action_session.h"
using json=nlohmann::json;
namespace fixture {
const std::string id="11111111-1111-4111-8111-111111111111";
const std::string original="subscriber external reference identify failed subscriber_id=215 saved_link_id=3 class_addresspoint_rva=24423440 character_kind=0 character_id=2 original=external reference has no supported portable owner key";
size_t checks=0;
unsigned invalidations=0,queue_calls=0,action_ticks=0,shows=0,stops=0;
bool queue_ok=true;uint32_t show_code=0;
std::vector<json> packets;
void need(bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);}
template<class... T>void log(const T&...){ }
}
#define LOG_NET_INFO(...) ::fixture::log(__VA_ARGS__)
#define LOG_NET_WARN(...) ::fixture::log(__VA_ARGS__)
#define LOG_NET_ERROR(...) ::fixture::log(__VA_ARGS__)
#define LOG_INFO(...) ::fixture::log(__VA_ARGS__)
#define LOG_ERROR(...) ::fixture::log(__VA_ARGS__)
uint64_t GetTickCount64(){return 100;}
namespace mt2mp {
struct Config {bool is_host=true;static Config& instance(){static Config config;return config;}};
namespace game::network_identity {void invalidate_world(const std::string&){++fixture::invalidations;}}
namespace net {
static bool snapshot_failed=false,snapshot_world_valid=true,receiving=false,load_pending=false,outbound_active=false;
static uint64_t discarded_after_failure=0;
static std::string snapshot_failure,status,incoming_id=fixture::id,last_loaded=fixture::id,outbound_id=fixture::id;
static json host_failure_notice=nullptr;
struct Stoppable {void stop(){++fixture::stops;}} buffer,outbound;
namespace session_wire {
bool send(uint8_t channel,const uint8_t* bytes,uint32_t size,uint64_t target){
    fixture::need(channel==4,"failure notice changed existing snapshot channel");
    fixture::need(target==(Config::instance().is_host?0u:1u),"failure notice target differs");
    ++fixture::queue_calls;fixture::packets.push_back(json::parse(bytes,bytes+size));return fixture::queue_ok;
}
std::string failure_reason(){return fixture::queue_ok?"":"original queue failure";}
}
#include "host_failure_snapshot.inc"
uint64_t current_world_epoch(){return snapshot_world_valid&&!snapshot_failed?77:0;}
json snapshot_progress(){return {{"first_failure",snapshot_failure}};}
}
}
namespace action=mt2mp::action_session;
namespace mt2mp::action_session {
#include "host_failure_callback.inc"
#include "host_failure_controller.inc"
bool Controller::result(const Message&,std::string&){throw std::runtime_error("unexpected pending native result");}
bool Controller::finish(Outcome,const std::string&,std::string&){throw std::runtime_error("unexpected pending native finish");}
}
namespace fixture {
struct Session:action::Session {
    bool current(action::Context& out,std::string& e)override{
        if(!mt2mp::net::current_world_epoch()){e="world authority unavailable: "+mt2mp::net::snapshot_failure;return false;}
        out={41,mt2mp::Config::instance().is_host?1u:2u,1,3,mt2mp::Config::instance().is_host?action::Role::Host:action::Role::Client,id};return true;
    }
};
struct Engine:action::Engine {
    bool bind(const std::string&,action::Binding&,std::string&)override{throw std::runtime_error("unexpected native bind");}
    bool current(const action::Binding&,const std::string&,std::string&)override{return true;}
    bool ledger(const action::Binding&,action::Ledger&,std::string&)override{throw std::runtime_error("unexpected native ledger");}
    action::Outcome execute_remove(const action::Binding&,const action::Request&,std::string&)override{throw std::runtime_error("unexpected native execute");}
    action::Outcome apply_remove(const action::Binding&,const action::Request&,const action::Evidence&,std::string&)override{throw std::runtime_error("unexpected native apply");}
    bool observe_remove(const action::Binding&,const action::Request&,action::Evidence&,std::string&)override{throw std::runtime_error("unexpected native observation");}
    bool retire(const action::Binding&,const action::Request&,std::string&)override{throw std::runtime_error("unexpected native retire");}
};
struct Transport:action::Transport {
    bool claim(Handler,std::string&)override{throw std::runtime_error("unexpected new transport claim");}
    bool send(uint64_t,const std::vector<uint8_t>&,std::string&)override{throw std::runtime_error("unexpected gameplay send");}
};
void reset(bool host){
    mt2mp::Config::instance().is_host=host;
    using namespace mt2mp::net;snapshot_failed=false;snapshot_world_valid=true;receiving=load_pending=outbound_active=false;
    discarded_after_failure=0;snapshot_failure.clear();status.clear();incoming_id=last_loaded=outbound_id=id;host_failure_notice=nullptr;
    invalidations=queue_calls=action_ticks=shows=stops=0;queue_ok=true;show_code=0;packets.clear();
}
}
namespace mt2mp::game::live_host_state {
struct Fence {bool locked(){return false;}std::string cause(){return {};}};
Fence& economic_fence(){static Fence value;return value;}
}
namespace mt2mp::game::typed_action_runtime {
struct Live {unsigned captures=0;bool okay=false;std::string failure=fixture::original,wait;
    bool tick(uint64_t,bool,std::string& e){++captures;e=okay?std::string{}:failure;return okay;}
    const std::string& publication_wait(){return wait;}
};
struct Runtime {
    fixture::Session session;fixture::Engine engine;fixture::Transport transport;action::Controller controller;
    std::unique_ptr<Live> live_state=std::make_unique<Live>();
    bool lobby_retiring=false,live_failure_terminal=false;std::string live_error,live_publication_wait;
    Runtime():controller(session,engine,transport,{},10000){
        std::string e;fixture::need(session.current(controller.context_,e),e);controller.binding_={7,9};
        controller.status_.started=true;controller.status_.confirmed=1;controller.thread_=std::this_thread::get_id();
    }
};
std::atomic<Runtime*> published_runtime{nullptr};
void weapon_bootstrap(Runtime&){++fixture::action_ticks;}
#define IDLE_TICK(name) namespace name {void tick(){++fixture::action_ticks;}}
IDLE_TICK(ui_move_runtime) IDLE_TICK(ui_region_runtime) IDLE_TICK(ui_price_runtime)
IDLE_TICK(ui_terrain_runtime) IDLE_TICK(ui_path_runtime) IDLE_TICK(ui_network_runtime)
namespace developer_staff {IDLE_TICK(action_runtime)}
namespace bug_authority {IDLE_TICK(manual_action)}
namespace npc_probe {IDLE_TICK(action_runtime) IDLE_TICK(zone_action_runtime)
namespace monster_action_runtime {void tick(){++fixture::action_ticks;} IDLE_TICK(quest_authoring)}}
#include "host_failure_runtime.inc"
}
namespace panel_fixture {
std::string failure_notice_attempted,failure_notice_error,cause,ui_status;void* panel=reinterpret_cast<void*>(uintptr_t(1));
uint32_t show(void*,bool visible){fixture::need(visible,"terminal error hid the native panel");++fixture::shows;return fixture::show_code;}
bool fail(const std::string& detail){cause=detail;return false;}
#include "host_failure_panel.inc"
}
namespace {
void host_client_chain(){
    using namespace fixture;namespace runtime=mt2mp::game::typed_action_runtime;namespace net=mt2mp::net;
    reset(true);runtime::Runtime host;runtime::published_runtime=&host;runtime::tick();
    need(host.live_failure_terminal&&host.live_error==original&&host.live_state->captures==1&&!action_ticks,"Host failure did not stop the capture/action path once");
    need(net::snapshot_failed&&!net::snapshot_world_valid&&net::snapshot_failure==original&&invalidations==1,"Host authority remained live or cause changed");
    need(queue_calls==1&&net::host_failure_notice.at("attempted")==true&&net::host_failure_notice.at("queued")==true,"Host notice was not queued through existing route");
    need(host.controller.status().locked&&host.controller.status().outcome==action::Outcome::Unknown&&host.controller.can_retire_lobby(),"idle Host session did not lock while retaining Leave");
    const auto notice=packets.front();runtime::tick();runtime::tick();
    need(host.live_state->captures==1&&queue_calls==1&&host.live_error==original&&!action_ticks,"terminal Host capture/notice replayed or cause recovered");
    std::string e;need(!net::fail_host_world(id,"later error",e)&&net::snapshot_failure==original&&queue_calls==1,"late error replaced first cause or retried send");

    reset(false);runtime::Runtime client;runtime::published_runtime=&client;
    need(net::receive_host_world_failure(1,notice),"Client did not admit exact Host terminal notice");
    const auto expected="Host synchronization failed: "+original;
    need(net::snapshot_failed&&!net::snapshot_world_valid&&net::snapshot_failure==expected&&invalidations==1,"Client retained live authority or lost Host cause");
    runtime::tick();need(client.live_failure_terminal&&client.live_error==expected&&client.live_state->captures==0&&!action_ticks,"Client replayed cached live state after Host failure");
    need(client.controller.status().locked&&client.controller.status().outcome==action::Outcome::Unknown&&client.controller.can_retire_lobby(),"idle Client did not lock or Leave was trapped");
    need(client.controller.status().confirmed==1,"terminal state erased earlier accepted evidence");
    const auto sends=queue_calls;need(net::receive_host_world_failure(1,notice)&&net::snapshot_failure==expected&&queue_calls==sends&&invalidations==1,"duplicate notice replayed or replaced first failure");

    panel_fixture::failure_notice_attempted.clear();panel_fixture::failure_notice_error.clear();panel_fixture::cause.clear();panel_fixture::ui_status.clear();
    const json preparation={{"error",client.live_error},{"leaving",false}};
    need(panel_fixture::show_failed_panel_once(preparation,e)&&shows==1&&panel_fixture::cause==expected&&panel_fixture::ui_status=="Sitzung beendet: Synchronisierungsfehler","terminal Client error was not visible with retained cause");
    panel_fixture::ui_status="Sitzung nicht verfügbar";
    need(panel_fixture::show_failed_panel_once(preparation,e)&&shows==1&&panel_fixture::ui_status=="Sitzung beendet: Synchronisierungsfehler","terminal panel reopened each frame or lost state");
    runtime::published_runtime=nullptr;
}
void admission_and_queue_cases(){
    using namespace fixture;namespace net=mt2mp::net;std::string e;
    for(int wrong=0;wrong<7;++wrong){reset(false);auto value=json{{"type","snapshot_error"},{"id",id},{"cause",original}};uint64_t sender=1;
        switch(wrong){case 0:sender=2;break;case 1:value["id"]="22222222-2222-4222-8222-222222222222";break;case 2:value["cause"]="";break;case 3:value["extra"]=true;break;case 4:net::receiving=true;break;case 5:net::load_pending=true;break;case 6:net::last_loaded.clear();break;}
        bool caught=false;try{(void)net::receive_host_world_failure(sender,value);}catch(const std::runtime_error& x){caught=std::string(x.what()).starts_with("Host world failure");}
        need(caught&&!invalidations&&!queue_calls&&!net::snapshot_failed,"invalid Host terminal notice performed accepted failure effects");}
    reset(false);need(!net::receive_host_world_failure(1,{{"type","snapshot_begin"}})&&!net::snapshot_failed,"ordinary snapshot control route changed");
    reset(true);need(!net::receive_host_world_failure(2,{{"type","snapshot_error"}})&&!net::snapshot_failed,"Host's existing error-ACK owner was intercepted");
    reset(true);queue_ok=false;need(!net::fail_host_world(id,original,e)&&net::snapshot_failed&&net::snapshot_failure==original&&queue_calls==1&&net::host_failure_notice.at("queued")==false&&!net::host_failure_notice.at("send_error").get<std::string>().empty(),"queue failure restored local authority or falsely confirmed delivery");
    need(!net::fail_host_world(id,original,e)&&queue_calls==1,"uncertain queue failure was retried");
    reset(true);need(!net::fail_host_world(id,std::string(8192,'x'),e)&&net::snapshot_failed&&!queue_calls&&net::host_failure_notice.at("attempted")==false,"oversized failure crossed existing control bound or failed to stop local authority");
    reset(true);need(!net::fail_host_world("wrong",original,e)&&!net::snapshot_failed&&!queue_calls,"wrong local Origin invalidated current authority");
    reset(false);need(!net::fail_host_world(id,original,e)&&!net::snapshot_failed&&!queue_calls,"Client impersonated Host failure publication");
    panel_fixture::failure_notice_attempted.clear();panel_fixture::failure_notice_error.clear();show_code=77;
    need(!panel_fixture::show_failed_panel_once({{"error",original}},e)&&shows==1&&e.find(original)!=std::string::npos,"failed native panel Show hid original cause");
    const auto retained=e;need(!panel_fixture::show_failed_panel_once({{"error",original}},e)&&shows==1&&e==retained,"uncertain native panel Show replayed or false recovery");
    reset(true);mt2mp::game::typed_action_runtime::Runtime ordinary;
    ordinary.live_state->okay=true;mt2mp::game::typed_action_runtime::published_runtime=&ordinary;
    mt2mp::game::typed_action_runtime::tick();
    need(!ordinary.live_failure_terminal&&ordinary.live_error.empty()&&ordinary.live_state->captures==1&&action_ticks>0&&!queue_calls&&!net::snapshot_failed&&!ordinary.controller.status().locked,"ordinary confirmed Runtime path became terminal");
    mt2mp::game::typed_action_runtime::published_runtime=nullptr;
}
}
int main(){try{host_client_chain();admission_and_queue_cases();std::cout<<fixture::checks<<" actual Host live failure checks passed\n";return 0;}catch(const std::exception& x){std::cerr<<"error after "<<fixture::checks<<" checks: "<<x.what()<<'\n';return 1;}}
