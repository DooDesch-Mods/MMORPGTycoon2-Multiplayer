#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <random>
#include "vendor/nlohmann/json.hpp"

using json=nlohmann::json;
json pending_frame,pending_world,pending_overlay,pending_crash_dismiss,pending_snapshot,pending_placement,pending_load,pending_camera,deferred_response;
#include "diagnostic_idle_body.inc"
namespace game::lobby_operations {
#include "lobby_executor_idle_body.inc"
bool take_request(json& out){if(request.is_null())return false;out=request;request=nullptr;return true;}
bool receive(const json& result){if(active.empty()||result.at("id")!=active)return false;response=result;return true;}
}
struct Config {bool diagnostic_mailbox_enabled=true;static Config& instance(){static Config value;return value;}};
std::string run_id="fixture-run",last_request,last_mailbox_request;
std::string ordinary_save_id;
namespace game::typed_action_runtime {
bool held=false;std::string failure;unsigned snapshot_failures=0;
json ordered_snapshot_status(){return {{"runtime_failure",failure}};}
bool ordered_snapshot_active(){return held;}
void fail_ordered_snapshot(const std::string&){++snapshot_failures;}
}
namespace game::session_reload {
bool active=false;std::string capture_id;
bool recovery_active(){return active;}
bool recovery_capture_pending(const std::string& id){return !capture_id.empty()&&id==capture_id;}
}
json external_request;
unsigned reads=0,writes=0;bool write_fails=false;json written_response;
std::vector<std::string> attempts;
std::string runtime_root="fixture-profile";
unsigned executor_ticks=0;
#include "diagnostic_tick_gate.inc"
    ++executor_ticks;
}
json read_request(const std::filesystem::path&){++reads;return external_request;}
void write_json(const std::filesystem::path&,const json& value){++writes;if(write_fails)throw std::runtime_error("original response write failure");written_response=value;}
json status(){return json::object();}
#define LOG_INFO(...) ((void)0)
#define LOG_WARN(...) ((void)0)
template<class... T>void ignore_log(T&&...){}
#define LOG_ERROR(...) ignore_log(__VA_ARGS__)
#include "diagnostic_respond_body.inc"
namespace runtime_paths {std::filesystem::path diagnostics(const std::string&){return std::filesystem::current_path();}}
namespace game::normal_load_identity {void cancel(const std::string&) {}}
namespace game::building_capture_identity {unsigned cancellations=0;void cancel(const std::string&){++cancellations;}}
namespace game::network_identity {unsigned cancellations=0;void cancel_pending(const std::string&){++cancellations;}}
struct LoadWatch {void finish(){}} load_watch;
std::string pending_ordered_snapshot,diagnostic_failure;size_t diagnostic_failure_count=0;
bool snapshot_observer_begun=false,snapshot_network_observer_begun=false;
void actual_failure(const std::exception& error,const std::string& request_in_progress){
#include "diagnostic_failure_body.inc"
}
void poll(){
    const auto dir=std::filesystem::current_path();
    std::string request_in_progress;
#include "diagnostic_admit_body.inc"
    attempts.push_back(id);
    last_request=id;
}
void check(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
namespace game::lobby_prepare {
bool leaving_value=false;unsigned ticks=0;
bool leaving(){return leaving_value;}
bool tick(std::string&){++ticks;return true;}
}
namespace game::session_reload {
bool busy_value=false;unsigned ticks=0;
bool busy(){return busy_value;}
void tick(){++ticks;}
}
enum class FrameTimingPart {Prepare};
namespace game_update_pair {enum class Section {Prepare};}
struct FrameTimingScope{explicit FrameTimingScope(FrameTimingPart){}};
struct GameUpdateWorkScope{explicit GameUpdateWorkScope(game_update_pair::Section){}};
std::string native_lobby_error;
void tick_lifecycle(){
#include "lobby_leave_schedule.inc"
}
json request(const std::string& id){return {{"id",id},{"schema_version",1},{"run_id",run_id},{"op","load_snapshot"}};}
json reply(const std::string& id,bool ok){return {{"id",id},{"schema_version",1},{"run_id",run_id},{"ok",ok},{"error",{{"message","original native load failure"}}}};}
int main() try {
    const auto mailbox=std::filesystem::current_path()/"request.json";
    check(!std::filesystem::exists(mailbox),"fixture refuses to overwrite an existing mailbox");
    {std::ofstream file(mailbox);check(bool(file),"fixture mailbox creation failed");file<<"{}";check(bool(file),"fixture mailbox write failed");}
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove(path,error);if(error)std::cerr<<"fixture cleanup failed: "<<error.message()<<'\n';}} cleanup{mailbox};
    const std::string first="11111111-1111-4111-8111-111111111111",internal="22222222-2222-4222-8222-222222222222",next="33333333-3333-4333-8333-333333333333";
    namespace ops=game::lobby_operations;
    ops::configure(run_id,diagnostic_executor_idle);
    tick_lifecycle();check(game::session_reload::ticks==1&&game::lobby_prepare::ticks==1,"idle lifecycle did not advance");
    game::session_reload::busy_value=true;
    tick_lifecycle();check(game::session_reload::ticks==2&&game::lobby_prepare::ticks==1,"preparation competed with world reload");
    game::lobby_prepare::leaving_value=true;
    tick_lifecycle();check(game::session_reload::ticks==2&&game::lobby_prepare::ticks==2,"pending Leave starved behind world reload");
    Config::instance().diagnostic_mailbox_enabled=false;
    ops::active=internal;ops::request=request(internal);
    diagnostics_tick();
    check(executor_ticks==1,"installed lobby operation blocked by disabled diagnostic mailbox");
    poll();check(attempts.size()==1&&reads==0,"internal installed request not dispatched without mailbox reads");
    const auto writes_before=writes;
    respond({},reply(internal,true));
    check(ops::response.at("id")==internal&&writes==writes_before,"installed completion escaped to external mailbox");
    ops::active.clear();ops::response=nullptr;
    external_request=request(first);poll();check(reads==0,"disabled external mailbox was read");
    runtime_root.clear();diagnostics_tick();check(executor_ticks==1,"missing runtime profile admitted executor");
    runtime_root="fixture-profile";
    attempts.clear();last_request.clear();last_mailbox_request.clear();
    Config::instance().diagnostic_mailbox_enabled=true;
    check(ops::executor_idle(),"idle executor not available to automatic adoption");
    for(json* pending:{&pending_frame,&pending_world,&pending_overlay,&pending_crash_dismiss,&pending_snapshot,&pending_placement,&pending_load,&pending_camera,&deferred_response}){
        *pending={{"id",first}};check(!ops::executor_idle(),"pending diagnostic operation appeared idle");*pending=nullptr;
    }
    external_request=request(first);poll();check(attempts.size()==1&&last_mailbox_request==first,"external mutation not claimed before dispatch");
    pending_load={{"id",first}};poll();check(attempts.size()==1,"pending external mutation admitted twice");pending_load=nullptr;
    respond({},reply(first,true));
    ops::active=internal;ops::request=request(internal);check(!ops::executor_idle(),"internal pending operation appeared idle");
    poll();check(attempts.size()==2&&last_request==internal&&last_mailbox_request==first,"internal operation changed external dedupe");
    respond({},reply(internal,true));check(ops::response.at("id")==internal,"internal response routed externally");
    ops::active.clear();ops::response=nullptr;poll();check(attempts.size()==2,"completed external load replayed after internal capture");
    external_request=request(next);poll();check(attempts.size()==3,"new external request suppressed");
    write_fails=true;try{respond({},reply(next,false));throw std::runtime_error("expected publication failure absent");}
    catch(const std::runtime_error& error){check(std::string(error.what())=="original response write failure","original publication cause lost");}
    check(deferred_response.at("ok")==false&&deferred_response.at("error").at("message")=="original native load failure","failure response lost during publication");
    check(!ops::executor_idle(),"deferred failed response appeared idle");
    write_fails=false;deferred_response=nullptr;last_request=internal;poll();check(attempts.size()==3,"failed external operation replayed");
    external_request=request(first);external_request["run_id"]="old-run";poll();check(attempts.size()==3,"foreign run executed");
    check(last_mailbox_request==first,"foreign mailbox identity not retained");
    last_request=internal;poll();check(attempts.size()==3,"foreign mailbox replayed after internal ID change");
    game::typed_action_runtime::held=true;last_mailbox_request.clear();external_request=request(next);external_request["op"]="read_native_campaign_ui";
    pending_snapshot={{"id",internal}};ordinary_save_id=internal;const auto before_denial=writes;poll();
    check(attempts.size()==3&&writes==before_denial+2&&written_response.at("id")==next&&!written_response.at("ok").get<bool>()&&written_response.at("error").at("code")=="SNAPSHOT_HELD"&&pending_snapshot.at("id")==internal&&ordinary_save_id==internal&&last_mailbox_request==next,"held snapshot rejection lost request identity or snapshot owner");
    poll();check(writes==before_denial+2&&attempts.size()==3,"held rejection replayed");pending_snapshot=nullptr;ordinary_save_id.clear();
    pending_snapshot={{"id",internal}};ordinary_save_id=internal;external_request=request(first);external_request["op"]="read_native_campaign_ui";write_fails=true;
    bool publication_failed=false;try{poll();}catch(const std::runtime_error& error){publication_failed=std::string(error.what())=="original response write failure";}
    check(publication_failed&&deferred_response.at("id")==first&&!deferred_response.at("ok").get<bool>()&&deferred_response.at("error").at("code")=="SNAPSHOT_HELD"&&pending_snapshot.at("id")==internal,"held rejection publication failure lost original error or deferred response");
    write_fails=false;deferred_response=nullptr;pending_snapshot=nullptr;ordinary_save_id.clear();
    external_request=request(next);external_request["op"]="read_ordered_world";poll();check(attempts.size()==4,"held snapshot blocked current read-only evidence");
    game::typed_action_runtime::held=false;game::session_reload::active=true;last_mailbox_request.clear();external_request=request(next);
    poll();check(attempts.size()==4&&written_response.at("id")==next&&written_response.at("error").at("code")=="SNAPSHOT_HELD","active recovery admitted an unrelated external mutation");
    ops::active=internal;ops::request=request(internal);bool internal_failed=false;try{poll();}catch(const std::runtime_error& error){internal_failed=std::string(error.what()).find("permits status and read-only state")!=std::string::npos;}
    check(internal_failed&&attempts.size()==4,"internal held failure was silently handled");
    ops::active=internal;ops::request=request(internal);ops::request["op"]="capture_recovery_snapshot";
    game::session_reload::capture_id=internal;poll();check(attempts.size()==5&&attempts.back()==internal,"retained recovery capture was blocked");
    ops::active.clear();ops::request=nullptr;ops::response=nullptr;game::session_reload::active=false;
    pending_snapshot={{"id",internal},{"bind_snapshot_identities",true}};pending_ordered_snapshot=internal;
    actual_failure(std::runtime_error("original native snapshot fault"),next);
    check(game::typed_action_runtime::snapshot_failures==1&&game::building_capture_identity::cancellations==1&&game::network_identity::cancellations==1&&pending_snapshot.is_null()&&written_response.at("id")==internal&&!written_response.at("ok").get<bool>()&&written_response.at("error").at("message")=="original native snapshot fault","actual native snapshot failure did not remain fatal and visible");
    std::cout<<"PASS actual broker selection/response and idle bodies: internal completion cannot replay external mutation, failure response retained, pending operations defer adoption\n";
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
