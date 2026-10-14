#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "vendor/nlohmann/json.hpp"

using Json=nlohmann::json;
namespace fixture {
bool host=true,executor=true,ingame=true,connected=true,wire_ok=true,submit_ok=true,response_ready=false,load_ok=true;
std::string wire_error,transport_error,submit_error="original capture submit failure",load_error="original selected load failure";
unsigned wire_calls=0,load_calls=0,title_calls=0,take_calls=0,runtime_calls=0,simulation_reads=0;
bool simulation_ok=true;std::string simulation_error="original ledger read failure";
int32_t flat_day=1,record_day=1,cash=100000,record_cash=100000;bool clock_paused=true,record_present=true;float clock_scale=0;
std::string loaded;
std::vector<std::string> operations;
Json response;
void check(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
}
namespace mt2mp {
struct Config {
    bool is_host=true,local_test=false;
    static Config& instance(){static Config value;return value;}
};
namespace net {
constexpr int snapshot_native_load_timeout_seconds=900;
namespace session_wire {
bool connected(){return fixture::connected;}
std::string failure_reason(){return fixture::transport_error;}
bool prepare(std::string& error){++fixture::wire_calls;error=fixture::wire_error;return fixture::wire_ok;}
}
Json snapshot_progress(){return {{"phase","active"}};}
std::string snapshot_sync_status(){return "fixture snapshot failure";}
}
namespace game {
namespace native {
struct WorldStatus{int active_mode_index=2;bool title_transition_pending=false;};
struct ClockState{float time_scale=0;int32_t flat_day=1;bool paused=true;};
struct SimulationState{ClockState clock;int32_t cash=0;bool record_present=false;int32_t record_day=-1,record_cash=0;};
bool read_simulation_state(SimulationState& out,std::string& error){
    ++fixture::simulation_reads;if(!fixture::simulation_ok){error=fixture::simulation_error;return false;}
    out.clock={fixture::clock_scale,fixture::flat_day,fixture::clock_paused};out.cash=fixture::cash;
    out.record_present=fixture::record_present;out.record_day=fixture::record_day;out.record_cash=fixture::record_cash;return true;
}
bool in_game(){return fixture::ingame;}
bool request_load(const std::string& path,std::string& error){++fixture::load_calls;fixture::loaded=path;if(!fixture::load_ok)error=fixture::load_error;return fixture::load_ok;}
bool request_return_to_title(std::string&){++fixture::title_calls;return true;}
bool read_world_status(WorldStatus& world,std::string&){world=WorldStatus{};return true;}
}
namespace native_lobby {bool admitted(){return false;}}
namespace lobby_operations {
bool available(){return fixture::executor;}
bool submit(const char* op,const Json&,std::string& operation,std::string& error){
    fixture::operations.emplace_back(op);operation="fixture-operation";
    if(!fixture::submit_ok){error=fixture::submit_error;return false;}return true;
}
bool take_response(const std::string& operation,Json& response){
    fixture::check(operation=="fixture-operation","wrong native operation identity");++fixture::take_calls;
    if(!fixture::response_ready)return false;response=fixture::response;fixture::response_ready=false;return true;
}
}
namespace typed_action_runtime {
bool finish_lobby_leave(bool& ready,std::string&){ready=false;return true;}
Json controller_status(){return {{"started",true},{"locked",false}};}
Json status(){return {{"started",false},{"locked",false}};}
bool enable_live_state(std::string&){return true;}
}
namespace lobby_prepare {
#define LOG_ERROR(...) ((void)0)
#define LOG_INFO(...) ((void)0)
#include "prepare_current_state_body.inc"
#include "prepare_current_completed_body.inc"
bool captures_ready(){return true;}
bool all_ack(std::string& error){error="unrelated runtime ACK intentionally unavailable in fixture";return false;}
bool start_runtime(std::string& error){++fixture::runtime_calls;error="unrelated runtime start intentionally unavailable in fixture";return false;}
#include "prepare_current_entry_body.inc"
#include "prepare_current_tick_body.inc"
}
}
}
namespace prepare=mt2mp::game::lobby_prepare;
using fixture::check;
void reset(){
    using namespace fixture;
    host=executor=ingame=connected=wire_ok=submit_ok=load_ok=true;response_ready=false;
    wire_error.clear();transport_error.clear();wire_calls=load_calls=title_calls=take_calls=runtime_calls=simulation_reads=0;
    simulation_ok=true;simulation_error="original ledger read failure";flat_day=record_day=1;cash=record_cash=100000;
    clock_paused=record_present=true;clock_scale=0;
    loaded.clear();operations.clear();response=Json{};
    mt2mp::Config::instance().is_host=true;mt2mp::Config::instance().local_test=false;
    prepare::step=prepare::Step::Idle;prepare::operation.clear();prepare::snapshot.clear();prepare::failure.clear();prepare::selected_path.clear();
    prepare::start_requested=false;prepare::source_current_world=false;prepare::zero_day_reload_attempted=false;
    prepare::began=std::chrono::steady_clock::now();prepare::leave_confirmed=0;
}
int main(){
    std::string error;
    reset();prepare::selected_path="save/stale/missing.vrz";
    check(prepare::prepare_current(error),"connected current world failed admission");
    check(prepare::source_current_world&&prepare::selected_path.empty()&&prepare::step==prepare::Step::CaptureLocal,"current source state incorrect");
    check(fixture::operations==std::vector<std::string>{"capture_local_snapshot"}&&fixture::load_calls==0&&fixture::title_calls==0,"current world attempted Title/save load before capture");
    check(prepare::tick(error)&&fixture::operations.size()==1,"waiting capture was resubmitted");
    check(!prepare::prepare_current(error)&&error.find("no replay")!=std::string::npos&&fixture::operations.size()==1,"duplicate current preparation replayed capture");

    reset();fixture::connected=false;fixture::wire_ok=false;fixture::wire_error="info: awaiting handshake";
    check(prepare::prepare_current(error)&&error.empty()&&prepare::step==prepare::Step::Handshake,"incomplete handshake treated as failure");
    check(fixture::operations.empty()&&fixture::load_calls==0&&fixture::title_calls==0,"capture/load before handshake ACK");
    check(prepare::start(error)&&prepare::start_requested,"Start did not retain pending intent");
    check(prepare::tick(error)&&fixture::operations.empty()&&fixture::wire_calls==2,"pending handshake did not wait without native mutation");
    fixture::connected=true;fixture::wire_ok=true;fixture::wire_error.clear();
    check(prepare::tick(error)&&prepare::step==prepare::Step::CaptureLocal&&fixture::operations.size()==1,"connected handshake did not capture exactly once");
    check(fixture::operations[0]=="capture_local_snapshot"&&fixture::load_calls==0&&fixture::title_calls==0,"handshake selected wrong current-world route");
    check(prepare::tick(error)&&fixture::operations.size()==1,"post-ACK capture replayed");

    reset();fixture::connected=false;
    check(prepare::prepare_current(error)&&prepare::step==prepare::Step::Handshake&&fixture::operations.empty(),"wire prepare return was mistaken for connected ACK");
    fixture::connected=true;fixture::ingame=false;
    check(!prepare::tick(error)&&error=="current Host world left before capture"&&prepare::step==prepare::Step::Failed,"disappeared Host world lost immediate original cause");
    check(fixture::operations.empty()&&fixture::load_calls==0&&fixture::title_calls==0,"lost world caused fallback load or capture");

    reset();mt2mp::Config::instance().is_host=false;
    check(!prepare::prepare_current(error)&&error.find("confirmed Host")!=std::string::npos&&fixture::wire_calls==0,"Client admitted current-world preparation");
    reset();fixture::executor=false;
    check(!prepare::prepare_current(error)&&error.find("executor is unavailable")!=std::string::npos&&fixture::wire_calls==0,"missing executor admitted preparation");
    reset();fixture::ingame=false;
    check(!prepare::prepare_current(error)&&error.find("no longer active")!=std::string::npos&&fixture::wire_calls==0,"Title admitted as current world");

    reset();fixture::wire_ok=false;fixture::wire_error="original handshake rejection";
    check(!prepare::prepare_current(error)&&error==fixture::wire_error&&!prepare::source_current_world&&fixture::operations.empty(),"hard handshake rejection lost cause or current source flag");
    fixture::wire_ok=true;fixture::wire_error.clear();
    prepare::source_current_world=true;
    check(prepare::prepare("save/Selected/periodic.vrz",error)&&!prepare::source_current_world&&fixture::loaded=="save/Selected/periodic.vrz","selected-save route inherited rejected current source");

    reset();fixture::submit_ok=false;
    check(!prepare::prepare_current(error)&&error==fixture::submit_error&&prepare::failure==fixture::submit_error&&prepare::step==prepare::Step::Failed,"capture submission error hidden");
    check(!prepare::tick(error)&&error==fixture::submit_error&&fixture::operations.size()==1,"failed capture automatically retried");

    reset();check(prepare::prepare_current(error),"capture setup failed");
    fixture::response_ready=true;fixture::response={{"ok",false},{"error",{{"message","original native capture failed"}}}};
    check(!prepare::tick(error)&&error=="original native capture failed"&&fixture::title_calls==0&&fixture::load_calls==0,"failed capture triggered Title/load or lost cause");
    reset();check(prepare::prepare_current(error),"binding failure setup failed");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",{{"snapshot_id","captured"},{"building_bindings_confirmed",false},{"building_identity_error","actual binding mismatch"}}}};
    check(!prepare::tick(error)&&error.find("actual binding mismatch")!=std::string::npos&&fixture::title_calls==0,"incomplete bindings allowed Title transition");
    reset();check(prepare::prepare_current(error),"successful capture setup failed");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",{{"snapshot_id","captured"},{"building_bindings_confirmed",true}}}};
    check(prepare::tick(error)&&fixture::title_calls==1&&prepare::step==prepare::Step::ReturnTitle&&prepare::snapshot=="captured","capture ACK did not enter common snapshot flow");
    check(fixture::load_calls==0,"current path requested a missing source save");

    reset();check(prepare::prepare_current(error),"zero-day setup failed");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",{{"snapshot_id","first-local"},{"building_bindings_confirmed",true}}}};
    check(prepare::tick(error)&&prepare::step==prepare::Step::ReturnTitle,"first local capture did not return Title");
    check(prepare::tick(error)&&prepare::step==prepare::Step::LoadOrigin,"first origin load not queued");
    fixture::flat_day=fixture::record_day=0;fixture::response_ready=true;fixture::response={{"ok",true},{"result",nlohmann::json::object()}};
    check(prepare::tick(error)&&prepare::step==prepare::Step::CaptureLocal&&prepare::zero_day_reload_attempted,
        "day-zero origin did not request one additional original local capture");
    check(fixture::operations==std::vector<std::string>{"capture_local_snapshot","load_snapshot","capture_local_snapshot"},
        "day-zero origin distributed before native normalization");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",{{"snapshot_id","normalized-local"},{"building_bindings_confirmed",true}}}};
    check(prepare::tick(error)&&prepare::step==prepare::Step::ReturnTitle,"normalization local capture did not return Title");
    check(prepare::tick(error)&&prepare::step==prepare::Step::LoadOrigin,"normalization origin load not queued");
    fixture::flat_day=fixture::record_day=1;fixture::response_ready=true;fixture::response={{"ok",true},{"result",nlohmann::json::object()}};
    check(prepare::tick(error)&&prepare::step==prepare::Step::CaptureShared&&fixture::simulation_reads==2,
        "positive normalized origin did not distribute exactly once");
    check(fixture::operations==std::vector<std::string>{"capture_local_snapshot","load_snapshot","capture_local_snapshot","load_snapshot","capture_snapshot"},
        "normalization used wrong original operation order");

    reset();check(prepare::prepare_current(error),"positive-day setup failed");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",{{"snapshot_id","local"},{"building_bindings_confirmed",true}}}};
    check(prepare::tick(error),"positive-day local capture failed");check(prepare::tick(error),"positive-day Title/load failed");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",nlohmann::json::object()}};
    check(prepare::tick(error)&&prepare::step==prepare::Step::CaptureShared&&!prepare::zero_day_reload_attempted,
        "positive-day origin took unnecessary normalization pass");
    check(fixture::operations==std::vector<std::string>{"capture_local_snapshot","load_snapshot","capture_snapshot"},
        "positive-day original operation order changed");

    reset();check(prepare::prepare("save/Selected/periodic.vrz",error),"selected positive-day setup failed");
    check(prepare::step==prepare::Step::LoadSelected&&fixture::load_calls==1,"selected save did not load originally");
    check(prepare::tick(error)&&prepare::step==prepare::Step::CaptureLocal,"selected save did not start local capture");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",{{"snapshot_id","selected-local"},{"building_bindings_confirmed",true}}}};
    check(prepare::tick(error)&&prepare::tick(error),"selected save origin load failed");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",nlohmann::json::object()}};
    check(prepare::tick(error)&&prepare::step==prepare::Step::CaptureShared&&!prepare::zero_day_reload_attempted&&
        fixture::operations==std::vector<std::string>{"capture_local_snapshot","load_snapshot","capture_snapshot"},
        "selected positive-day world did not keep original shared capture route");

    reset();check(prepare::prepare_current(error),"bounded zero-day setup failed");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",{{"snapshot_id","first-local"},{"building_bindings_confirmed",true}}}};
    check(prepare::tick(error)&&prepare::tick(error),"bounded first load failed");
    fixture::flat_day=fixture::record_day=0;fixture::response_ready=true;fixture::response={{"ok",true},{"result",nlohmann::json::object()}};
    check(prepare::tick(error)&&prepare::zero_day_reload_attempted,"bounded extra pass not queued");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",{{"snapshot_id","second-local"},{"building_bindings_confirmed",true}}}};
    check(prepare::tick(error)&&prepare::tick(error),"bounded second load failed");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",nlohmann::json::object()}};
    check(!prepare::tick(error)&&error.find("zero-day normalization incomplete")!=std::string::npos&&
        prepare::step==prepare::Step::Failed&&fixture::operations.size()==4,"repeated day zero distributed or hid measured cause");

    reset();check(prepare::prepare_current(error),"ledger read failure setup failed");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",{{"snapshot_id","local"},{"building_bindings_confirmed",true}}}};
    check(prepare::tick(error)&&prepare::tick(error),"ledger read failure path did not reach origin");
    fixture::simulation_ok=false;fixture::response_ready=true;fixture::response={{"ok",true},{"result",nlohmann::json::object()}};
    check(!prepare::tick(error)&&error.find(fixture::simulation_error)!=std::string::npos&&
        fixture::operations.size()==2,"original native ledger read cause lost or shared capture attempted");

    reset();check(prepare::prepare_current(error),"ledger mismatch setup failed");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",{{"snapshot_id","local"},{"building_bindings_confirmed",true}}}};
    check(prepare::tick(error)&&prepare::tick(error),"ledger mismatch path did not reach origin");
    fixture::record_day=2;fixture::response_ready=true;fixture::response={{"ok",true},{"result",nlohmann::json::object()}};
    check(!prepare::tick(error)&&error.find("record_day=2")!=std::string::npos&&
        fixture::operations.size()==2,"mismatched actual CurrentRecord admitted without cause");

    reset();check(prepare::prepare_current(error),"unpaused origin setup failed");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",{{"snapshot_id","local"},{"building_bindings_confirmed",true}}}};
    check(prepare::tick(error)&&prepare::tick(error),"unpaused origin path did not reach load");
    fixture::clock_paused=false;fixture::clock_scale=1;fixture::response_ready=true;
    fixture::response={{"ok",true},{"result",nlohmann::json::object()}};
    check(!prepare::tick(error)&&error.find("paused=0")!=std::string::npos&&
        fixture::operations.size()==2,"unpaused Host origin distributed without measured cause");

    reset();check(prepare::prepare_current(error),"original load error setup failed");
    fixture::response_ready=true;fixture::response={{"ok",true},{"result",{{"snapshot_id","local"},{"building_bindings_confirmed",true}}}};
    check(prepare::tick(error)&&prepare::tick(error),"original load error path did not reach load");
    fixture::response_ready=true;fixture::response={{"ok",false},{"error",{{"message","original native origin load failed"}}}};
    check(!prepare::tick(error)&&error=="original native origin load failed"&&fixture::simulation_reads==0&&
        fixture::operations.size()==2,"original native load error lost or later capture attempted");

    reset();fixture::connected=false;check(prepare::prepare_current(error),"transport failure setup failed");
    fixture::transport_error="original Steam transport failure";
    check(!prepare::tick(error)&&error.find(fixture::transport_error)!=std::string::npos&&fixture::operations.empty(),"transport failure captured anyway or lost cause");
    reset();fixture::connected=false;check(prepare::prepare_current(error),"timeout setup failed");
    prepare::began-=std::chrono::seconds(901);
    check(!prepare::tick(error)&&error.find("timed out")!=std::string::npos&&fixture::operations.empty()&&fixture::load_calls==0,"timeout mutated world or was hidden");
    std::cout<<"PASS actual prepare_current/prepare/start/tick bodies: handshake, no missing-save load, capture once, source reset, failure causes and common capture continuation\n";
}
