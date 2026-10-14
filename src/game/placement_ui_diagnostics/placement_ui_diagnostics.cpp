#include "placement_ui_diagnostics.h"
#include "admission.h"
#include "frame_sample.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/config.h"
#include "mt2mp/net/local_session.h"
#include "native_open_runtime.h"
#include "typed_action_runtime.h"
#include "ui_build_runtime.h"
#include "scenery_kiss/native_catalog.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <map>
#include <memory>
#include <cmath>
namespace mt2mp::game::placement_ui_diagnostics {
namespace {
using json=nlohmann::json;
namespace ui=placement_ui_probe;
std::map<std::string,Job> jobs;
Job* active=nullptr;
uint32_t thread=0;
uint64_t completed_frame=0;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool terminal(const Job& j){return j.receipt.phase==ui::Phase::Rejected||j.receipt.phase==ui::Phase::HostConfirmed;}
bool actual(Job& j,ui::Context& out,std::string& e){
    uintptr_t image=0;uint32_t actual_thread=0;std::string sha;
    if(!native::read_resolver_receipt(image,actual_thread,sha,e))return false;
    if(actual_thread!=GetCurrentThreadId()||!image||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"build_ui_actual_module_mainthread_differs");
    if(thread&&thread!=actual_thread)return fail(e,"build_ui_bound_thread_changed");
    building_mixed_identity::opened_file::CurrentWorld w;
    if(!native_open_runtime::read_current_world(w,e))return false;
    if(w.uuid!=j.input.snapshot_uuid||w.world_epoch!=j.input.epoch||w.generation!=j.input.generation||!w.state||!w.world)return fail(e,"build_ui_actual_snapshot_epoch_generation_differs");
    native::WorldStatus s;if(!native::read_world_status(s,e))return false;
    if(!s.world_present||s.in_game_state!=4||s.edit_operations!=0||s.saves_in_progress!=0||s.quitting||s.title_transition_pending||s.crash_report_active||s.map_task_pending)return fail(e,"build_ui_actual_world_not_idle");
    // Mode+58 is the game state; the opened world has a separate native address.
    out={image,reinterpret_cast<uintptr_t>(w.state),w.world_epoch,w.generation,actual_thread};thread=actual_thread;return true;
}
json format(const Job& j){const auto& r=j.receipt;return {{"acceptance_token",j.input.native.token},{"snapshot_uuid",j.input.snapshot_uuid},{"world_epoch",j.input.epoch},{"generation",j.input.generation},{"phase",int(r.phase)},{"attempted_mask",r.attempted_mask},{"returned_mask",r.returned_mask},{"exception",r.exception},{"capture_count",r.capture_count},{"host_confirmed_count",r.host_confirmed_count},{"action_uuid",r.action_uuid},{"cause",r.cause},{"confirmed",r.phase==ui::Phase::HostConfirmed},{"capture_completed_frame",j.capture_frame},{"fresh_native_pose",j.fresh_pose},{"arm_observation",j.arm_observation},{"native_release_pose_measured",r.native_pose_measured},{"native_release_pose",{{"position",r.native_pose.position},{"rotation",r.native_pose.rotation},{"scale",r.native_pose.scale}}},{"frames_observed",j.frames_observed},{"build_entries_observed",j.build_entries_observed},{"first_frame",j.first_frame},{"last_frame",j.last_frame},{"first_build_entry",j.first_build_entry},{"last_build_entry",j.last_build_entry},{"observation_failures",j.observation_failures},{"first_observation_error",j.first_observation_error},{"last_observation_error",j.last_observation_error}};}
void record_frame(Job& j,void* receiver,float dt,const char* boundary,bool entry) {
    const bool first=entry?j.build_entries_observed++==0:j.frames_observed++==0;
    ui::Context context;std::string error;json observed;
    if(!actual(j,context,error)||context!=j.binding.context) {
        if(error.empty())error="build_ui_frame_actual_context_changed";
        observed={{"boundary",boundary},{"completed_frame",completed_frame},{"read_confirmed",false},{"cause",error}};
    } else {
        ui::ProcessMemory memory;observed=frame_readback::sample(memory,j.binding,reinterpret_cast<uintptr_t>(receiver),dt,completed_frame,boundary,error);
    }
    if(!error.empty()) {
        ++j.observation_failures;if(j.first_observation_error.empty())j.first_observation_error=error;
        if(j.last_observation_error!=error)LOG_WARN("[BuildUIProbe] Frame observation token={} boundary={} cause={}",j.input.native.token,boundary,error);
    } else if(!j.last_observation_error.empty())LOG_INFO("[BuildUIProbe] Frame observation recovered token={}",j.input.native.token);
    j.last_observation_error=error;
    if(entry){if(first)j.first_build_entry=observed;j.last_build_entry=std::move(observed);}
    else{if(first)j.first_frame=observed;j.last_frame=std::move(observed);}
}
void observation_exception(Job& j,const char* boundary,const std::string& cause) {
    ++j.observation_failures;if(j.first_observation_error.empty())j.first_observation_error=cause;
    if(j.last_observation_error!=cause)LOG_WARN("[BuildUIProbe] Frame observation token={} boundary={} cause={}",j.input.native.token,boundary,cause);
    j.last_observation_error=cause;
}
void mark_unknown(Job& j,const std::string& cause){j.receipt.phase=ui::Phase::Unknown;j.receipt.cause=cause;LOG_ERROR("[BuildUIProbe] token={} action={} cause={}; no replay",j.input.native.token,j.receipt.action_uuid,cause);}
bool observe(Job& j,std::string& e){
    const auto controller=typed_action_runtime::status();
    // Capture runs inside the next frame; read after a further normal frame returns.
    if(j.receipt.capture_count&&!may_read_fresh_pose(j,completed_frame)){e.clear();return true;}
    const auto receipts=ui_build_runtime::status().value("receipts",json::object());
    if(!observe_confirmation(j,controller,receipts,e,Config::instance().is_host))return false;
    if(j.receipt.phase!=ui::Phase::HostConfirmed)return true;
    native::BuildingState s;
    const auto uid=receipts.at(j.receipt.action_uuid).at("native_uid").get<uint64_t>();
    if(!scenery_kiss::read_action_prop(uid,s,e)){mark_unknown(j,"build_ui_fresh_native_pose_read_failed: "+e);return false;}
    if(s.prop.uid!=uid||s.type!=j.input.native.type||s.variant!=j.input.native.variant){mark_unknown(j,"build_ui_fresh_native_uid_type_variant_differs");return fail(e,j.receipt.cause);}
    j.fresh_pose={{"native_uid",uid},{"type",s.type},{"variant",s.variant},{"x",s.prop.x},{"y",s.prop.y},{"z",s.prop.z},{"rotation",s.rotation},{"scale",s.scale},{"completed_frame",completed_frame}};
    return true;
}
class Services final:public ui::RootServices {
    Job& job_;
public:
    explicit Services(Job& job):job_(job){}
    bool actual_context(ui::Context& out,std::string& e)override{return actual(job_,out,e);}
    bool actual_input_routes(const ui::Binding& b,std::string& e)override{
        return b.family!=ui::Family::Scenery||ui_build_runtime::validate_scenery_ui_routes(b.context.image,b.context.thread,e);
    }
    bool reserve_job_once(const ui::Request& q,const ui::Binding& b,std::string& e)override{
        if(job_.reserved||q.token!=job_.input.native.token||active)return fail(e,"build_ui_job_already_reserved_or_competing_no_replay");
        ui::Context c;if(!actual(job_,c,e)||c!=b.context)return fail(e,e.empty()?"build_ui_actual_reader_binding_changed":e);
        job_.binding=b;job_.reserved=true;active=&job_;return true;
    }
    bool read_actual_admission(const ui::Request&,ui::Receipt& out,std::string& e)override{
        if(!observe(job_,e))return false;
        out=job_.receipt;return true;
    }
    void record_receipt(const ui::Request&,const ui::Receipt& r)noexcept override{job_.receipt=r;}
};
}
bool unresolved()noexcept{for(const auto& [token,j]:jobs){(void)token;if(!terminal(j))return true;}return false;}
void after_game_update(uint64_t completed,float dt)noexcept {
    completed_frame=completed;
    if(!active||active->receipt.capture_count||completed<=active->armed_completed_frame)return;
    try{record_frame(*active,nullptr,dt,"after_actual_GameUpdate",false);}
    catch(const std::exception& error){observation_exception(*active,"after_actual_GameUpdate",error.what());}
    catch(...){observation_exception(*active,"after_actual_GameUpdate","build_ui_frame_observation_nonstandard_exception");}
}
void build_update_entered(void* receiver,float dt)noexcept {
    if(thread!=GetCurrentThreadId())return;
    if(!active||active->receipt.capture_count)return;
    try{record_frame(*active,receiver,dt,"actual_Build_Update_entry",true);}
    catch(const std::exception& error){observation_exception(*active,"actual_Build_Update_entry",error.what());}
    catch(...){observation_exception(*active,"actual_Build_Update_entry","build_ui_entry_observation_nonstandard_exception");}
}
bool probe(const json& args,json& out,std::string& e){
    out=json::object();Input input;if(!parse_input(args,input,e))return false;
    if(jobs.contains(input.native.token))return fail(e,"build_ui_token_already_attempted_no_replay");
    if(unresolved())return fail(e,"build_ui_previous_effect_unresolved");
    Job preflight;preflight.input=input;ui::Context context;
    if(!actual(preflight,context,e))return false;
    const auto controller=typed_action_runtime::status();
    if(!ui_build_runtime::enabled()||!net::local_session_connected()||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))return fail(e,"build_ui_capture_or_actual_controller_not_ready");

    bool scene=false;
    if(!scenery_kiss::resolve_ui_scenery_catalog(context.image,context.thread,input.native.type,input.native.variant,scene,input.native.category,input.native.variant_hash,e))return false;
    if(scene)input.native.family=ui::Family::Scenery;
    else{native::PropState grounded;
        if(!native::probe_building_placement(input.native.type,input.native.variant,input.native.position[0],input.native.position[2],grounded,e))return false;
        if(std::abs(grounded.y-input.native.position[1])>0.01f)return fail(e,"build_ui_requested_height_differs_from_actual_ground");}
    preflight.input=input;
    auto& j=jobs.emplace(input.native.token,std::move(preflight)).first->second;
    j.receipt.token=input.native.token;Services services(j);ui::Probe native_probe;
    if(!native_probe.initialize(context,e)){j.receipt.cause=e;out=format(j);return false;}
    j.receipt=native_probe.arm(input.native,services);j.armed_at=GetTickCount64();j.armed_completed_frame=completed_frame;
    if(j.reserved){try{std::string observed_error;ui::ProcessMemory memory;j.arm_observation=frame_readback::sample(memory,j.binding,j.binding.behaviour,std::nullopt,completed_frame,"after_native_UI_callbacks",observed_error);if(!observed_error.empty())observation_exception(j,"after_native_UI_callbacks",observed_error);}
        catch(const std::exception& error){observation_exception(j,"after_native_UI_callbacks",error.what());}
        catch(...){observation_exception(j,"after_native_UI_callbacks","build_ui_arm_observation_nonstandard_exception");}}
    if(terminal(j))active=nullptr;
    out=format(j);e=j.receipt.cause;
    if(j.receipt.phase!=ui::Phase::ArmedAwaitingGameUpdate&&j.receipt.phase!=ui::Phase::CapturedAwaitingHost){LOG_ERROR("[BuildUIProbe] token={} phase={} cause={}",input.native.token,int(j.receipt.phase),e);return false;}
    LOG_INFO("[BuildUIProbe] token={} native normal tool phase={} captured={}",input.native.token,int(j.receipt.phase),j.receipt.capture_count);e.clear();return true;
}
bool receipt(const json& args,json& out,std::string& e){
    out=json::object();std::string token;if(!parse_token(args,token,e))return false;
    if(thread!=GetCurrentThreadId())return fail(e,"build_ui_receipt_wrong_actual_mainthread");
    auto it=jobs.find(token);if(it==jobs.end())return fail(e,"build_ui_receipt_token_unknown");auto& j=it->second;
    if(!terminal(j)&&j.receipt.phase!=ui::Phase::Unknown){
        ui::Context c;
        if(!actual(j,c,e)||c!=j.binding.context)mark_unknown(j,e.empty()?"build_ui_world_changed_after_native_callbacks":e);
        else if(!observe(j,e))mark_unknown(j,e);
        else if(j.receipt.phase!=ui::Phase::HostConfirmed&&GetTickCount64()-j.armed_at>10000)mark_unknown(j,"build_ui_observation_timeout_required_ms<=10000_no_replay");
    }
    if(j.receipt.phase==ui::Phase::HostConfirmed&&active==&j)active=nullptr;
    out=format(j);out["unresolved"]=!terminal(j);e=j.receipt.cause;return true;
}
bool release_ready(void* receiver,std::string& e)noexcept{
    if(!active||reinterpret_cast<uintptr_t>(receiver)!=active->binding.behaviour)return true;
    try{
        ui::Context context;if(!actual(*active,context,e)||context!=active->binding.context)return false;
        ui::ProcessMemory memory;ui::NativePose pose;
        if(!ui::read_scenery_release_pose(memory,active->binding,active->input.native,pose,e))return false;
        return bind_release(*active,reinterpret_cast<uintptr_t>(receiver),pose,e);
    }catch(const std::exception& x){e=std::string("scenery_ui_release_read_exception: ")+x.what();return false;}
    catch(...){e="scenery_ui_release_read_nonstandard_exception";return false;}
}
bool capture_bound(void* receiver,const std::string& id,const action_session::BuildingState& state,std::string& e)noexcept{
    if(!active||reinterpret_cast<uintptr_t>(receiver)!=active->binding.behaviour)return true;
    try{ui::Context c;if(!actual(*active,c,e)||c!=active->binding.context){mark_unknown(*active,e.empty()?"build_ui_capture_actual_world_changed":e);return false;}
        if(!bind_capture(*active,reinterpret_cast<uintptr_t>(receiver),id,state,e)){mark_unknown(*active,e);return false;}active->capture_frame=completed_frame;return true;
    }catch(const std::exception& cause){e=std::string("build_ui_capture_exception: ")+cause.what();mark_unknown(*active,e);return false;}
    catch(...){e="build_ui_capture_unknown_exception";mark_unknown(*active,e);return false;}
}
void capture_failed(void* receiver,const std::string& cause)noexcept{
    if(active&&reinterpret_cast<uintptr_t>(receiver)==active->binding.behaviour)mark_unknown(*active,"build_ui_actual_capture_failed: "+cause);
}
}
