#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include "vendor/nlohmann/json.hpp"
using json=nlohmann::json;
namespace fixture {
bool captures=true,unresolved=false,pending=false,ingame=true,leave_ok=true,leave_ready=false;
unsigned controller_reads=0,full_reads=0,submits=0,starts=0,begin_leave=0,finish_leave=0,title_requests=0,fail_logs=0;
uint32_t thread=7;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
}
uint32_t GetCurrentThreadId(){return fixture::thread;}
namespace mt2mp {
struct Config{bool is_host=true,local_test=false;static Config& instance(){static Config config;return config;}};
namespace net {
constexpr int snapshot_native_load_timeout_seconds=900;
namespace session_wire {bool connected(){return true;}std::string failure_reason(){return {};}
bool prepare(std::string&){return true;}}
json snapshot_progress(){return {{"phase","active"}};}
std::string snapshot_sync_status(){return "unexpected snapshot failure";}
}
namespace game {
namespace native {
struct WorldStatus{int active_mode_index=3;bool title_transition_pending=false;int saves_in_progress=0;};
struct SimulationState{struct{bool paused=true;float time_scale=0;int32_t flat_day=1;}clock;
bool record_present=true;int32_t record_day=1,record_cash=1,cash=1;};
bool in_game(){return fixture::ingame;}
bool request_load(const std::string&,std::string&){throw std::runtime_error("unexpected native load");}
bool request_return_to_title(std::string&){++fixture::title_requests;return true;}
bool read_world_status(WorldStatus& world,std::string&){world={fixture::ingame?3:2,false,0};return true;}
bool read_simulation_state(SimulationState&,std::string&){throw std::runtime_error("unexpected economy read");}
}
namespace native_lobby {bool admitted(){return true;}}
namespace lobby_operations {
bool available(){return true;}bool pending(){return fixture::pending;}
bool submit(const char*,const json&,std::string&,std::string&){++fixture::submits;return true;}
bool take_response(const std::string&,json&){throw std::runtime_error("unexpected snapshot response read");}
}
#define CAPTURE_SERVICE(name) namespace name {bool unresolved(){return fixture::unresolved;} json status(){return {{"cause","original unresolved capture"},{"enabled",fixture::captures}};}}
CAPTURE_SERVICE(ui_network_runtime)
CAPTURE_SERVICE(ui_path_runtime)
CAPTURE_SERVICE(ui_terrain_runtime)
CAPTURE_SERVICE(ui_build_runtime)
CAPTURE_SERVICE(ui_remove_runtime)
CAPTURE_SERVICE(ui_move_runtime)
CAPTURE_SERVICE(ui_region_runtime)
#undef CAPTURE_SERVICE
namespace typed_action_runtime {
struct ControllerStatus{bool started=true,locked=false,pending=false;uint64_t host_revision=1;
std::string action_uuid,cause;int outcome=0;};
struct Controller{ControllerStatus value;ControllerStatus status(){++fixture::controller_reads;return value;}};
struct Runtime{uint32_t thread=7;bool lobby_retiring=false;Controller controller;std::string live_error;};
std::atomic<Runtime*> published_runtime{nullptr};
#include "sync_failure_controller.inc"
json status(){++fixture::full_reads;throw std::runtime_error("unexpected full diagnostic economy scan");}
bool enable_live_state(std::string&){++fixture::starts;return true;}
bool begin_lobby_leave(std::string& error){++fixture::begin_leave;if(!fixture::leave_ok){error="original uncertain native Leave";return false;}return true;}
bool finish_lobby_leave(bool& ready,std::string&){++fixture::finish_leave;ready=fixture::leave_ready;return true;}
}
namespace lobby_prepare {
#define LOG_ERROR(...) (++fixture::fail_logs)
#define LOG_INFO(...) ((void)0)
#include "sync_failure_state.inc"
#include "sync_failure_completed.inc"
bool captures_ready(){return fixture::captures;}
bool all_ack(std::string&){throw std::runtime_error("unexpected preparation ACK scan");}
bool start_runtime(std::string&){++fixture::starts;return true;}
#include "sync_failure_entry.inc"
#include "sync_failure_leave.inc"
#include "sync_failure_tick.inc"
#include "sync_failure_status.inc"
// The extracted status includes the lobby_prepare namespace close.
}
}
namespace prepare=mt2mp::game::lobby_prepare;
namespace runtime=mt2mp::game::typed_action_runtime;
using fixture::check;
int main(){try{
    runtime::Runtime owner;runtime::published_runtime.store(&owner);
    prepare::step=prepare::Step::Playing;std::string error="stale caller error";
    check(prepare::tick(error)&&error.empty(),"healthy Playing failed");
    auto state=prepare::status();
    check(state.at("ready")==true&&state.at("playing")==true&&state.at("label")=="Verbunden","healthy label/readiness changed");
    check(fixture::controller_reads==1&&!fixture::full_reads&&!fixture::submits&&!fixture::starts,"healthy frame performed extra native work");
    owner.live_error="automatic Subscriber capture changed actual Frame cohort/scalars id=69 save_scope_closed=1 original=capture readback differs";
    const auto original=owner.live_error;
    check(runtime::controller_status().at("live_error")==original,"cheap status lost original live cause");
    check(!prepare::tick(error)&&error==original&&prepare::failure==original,"Playing hid the retained live failure");
    state=prepare::status();
    check(state.at("ready")==false&&state.at("playing")==false&&state.at("state")==int(prepare::Step::Failed)
        &&state.at("error")==original&&state.at("label")=="Sitzung nicht verfügbar","failed status still claims connected or ready");
    const auto reads=fixture::controller_reads;
    check(!prepare::tick(error)&&error==original&&fixture::controller_reads==reads&&fixture::fail_logs==1,"Failed tick rescanned/relogged or lost cause");
    check(!prepare::start(error)&&!prepare::prepare_current(error)&&!fixture::submits&&!fixture::starts,"failed sync automatically replayed preparation");
    check(owner.live_error==original&&!fixture::full_reads,"failure status changed runtime cause or scanned economy");
    fixture::unresolved=true;
    check(!prepare::leave(error)&&error=="native Leave blocked by unresolved Network cause=original unresolved capture"
        &&!fixture::begin_leave&&owner.live_error==original,"failed sync bypassed unresolved Leave guard");
    fixture::unresolved=false;fixture::pending=true;
    check(!prepare::leave(error)&&!fixture::begin_leave,"failed sync bypassed pending snapshot guard");
    fixture::pending=false;
    check(prepare::leave(error)&&prepare::step==prepare::Step::Leaving&&fixture::begin_leave==1
        &&fixture::title_requests==1&&owner.live_error==original,"Failed state could not enter existing Leave route");
    check(!prepare::leave(error)&&fixture::begin_leave==1&&fixture::title_requests==1,"Leave replayed native request");
    check(prepare::tick(error)&&prepare::step==prepare::Step::Leaving&&prepare::failure==original,"pending Leave cleared cause before acknowledgement");
    fixture::leave_ready=true;fixture::ingame=false;
    check(prepare::tick(error)&&prepare::step==prepare::Step::Idle&&prepare::failure.empty()&&prepare::leave_confirmed==1,"confirmed Leave did not use existing cleanup");
    check(owner.live_error==original&&!fixture::full_reads&&!fixture::submits&&!fixture::starts,"UI cleanup replayed or erased runtime failure");
    prepare::step=prepare::Step::Playing;owner.live_error.clear();owner.controller.value.locked=true;owner.controller.value.cause="original Controller lock";
    check(!prepare::tick(error)&&error=="native playing authority/capture readiness lost: original Controller lock","existing Controller lock cause changed");
    prepare::step=prepare::Step::Playing;owner.controller.value.locked=false;fixture::captures=false;
    check(!prepare::tick(error)&&!prepare::status().at("ready").get<bool>(),"existing capture-readiness loss accepted");
    fixture::thread=8;
    check(runtime::controller_status().at("error")=="typed action status requires bound Mainthread","cheap status lost bound-thread guard");
    runtime::published_runtime.store(nullptr);
    check(runtime::controller_status().at("initialized")==false,"uninitialized status changed");
    std::cout<<"PASS actual cheap Controller status and prepare Playing/Failed/Leave bodies\n";return 0;
}catch(const std::exception& error){std::cerr<<"error: "<<error.what()<<'\n';return 1;}}
