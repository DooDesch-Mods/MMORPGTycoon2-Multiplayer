#include "contract.h"
#include "native_guards.h"
#include "ui_remove/diagnostic/ui_diagnostic.h"
#include "placement_ui_probe/ui_probe.h"
#include "native_open_runtime.h"
#include "remove_diagnostics.h"
#include "scenery_kiss/native_catalog.h"
#include "ui_move_runtime.h"
#include "typed_action_runtime.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/net/local_session.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <map>
#include <vector>
#include <cstring>
#include <optional>
namespace mt2mp::game::move_ui_probe {
namespace {
using json=nlohmann::json;
namespace common=action_session::ui_diagnostic;
namespace ui=action_session::ui_capture;
struct Memory final:ui::Memory {
    placement_ui_probe::ProcessMemory process;
    bool copy(uintptr_t p,void* to,size_t n,std::string& e)override{return process.copy(p,to,n,e);}
    bool region(uintptr_t p,ui::Region& out,std::string& e)override{
        MEMORY_BASIC_INFORMATION page{};if(VirtualQuery(reinterpret_cast<const void*>(p),&page,sizeof page)!=sizeof page){e="move_ui_native_region_read_failed Win32="+std::to_string(GetLastError());return false;}
        const auto protect=page.Protect&0xff;out={uintptr_t(page.AllocationBase),page.State==MEM_COMMIT,!(page.Protect&PAGE_GUARD)&&(protect==PAGE_EXECUTE||protect==PAGE_EXECUTE_READ||protect==PAGE_EXECUTE_READWRITE||protect==PAGE_EXECUTE_WRITECOPY),page.Type==MEM_IMAGE,protect==PAGE_READWRITE||protect==PAGE_WRITECOPY||protect==PAGE_EXECUTE_READWRITE||protect==PAGE_EXECUTE_WRITECOPY};return true;
    }
};
struct Job {Bound bound;common::Context context;remove_diagnostics::LocalTarget target;uintptr_t cursor{};std::string native_stage="preflight";std::optional<move_kiss::State> ghost_before,ghost_after_mouse_over,ghost_after,final_intent_pose;};
std::map<std::string,Job> jobs;Job* active=nullptr;uint32_t thread=0;Memory memory;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
void unknown(Job& j,const std::string& cause){j.bound.receipt.phase=Phase::Unknown;j.bound.receipt.cause=cause;LOG_ERROR("[MoveUIProbe] token={} action={} stage={} cause={} requested={} observed_ghost={}; no replay",j.bound.input.token,j.bound.receipt.action_uuid,j.native_stage,cause,nlohmann::json(j.bound.input.position).dump(),j.ghost_after?nlohmann::json(std::array<float,3>{j.ghost_after->x,j.ghost_after->y,j.ghost_after->z}).dump():"null");}
template<class T>bool field(uintptr_t p,size_t offset,T& value,std::string& e){if(!p||p>UINTPTR_MAX-offset)return fail(e,"move_ui_native_field_address_overflow");return memory.copy(p+offset,&value,sizeof value,e);}
bool context(Job& j,std::string& e){
    uintptr_t image=0;uint32_t actual_thread=0;std::string sha;
    if(!native::read_resolver_receipt(image,actual_thread,sha,e))return false;
    if(actual_thread!=GetCurrentThreadId()||(thread&&thread!=actual_thread)||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"move_ui_actual_module_mainthread_differs");
    building_mixed_identity::opened_file::CurrentWorld w;if(!native_open_runtime::read_current_world(w,e))return false;
    const auto& q=j.bound.input;
    if(!w.state||!w.world||w.world_epoch!=q.epoch||w.generation!=q.generation||net::current_snapshot_id()!=q.snapshot)return fail(e,"move_ui_actual_world_session_snapshot_differs");
    remove_diagnostics::LocalTarget fresh;native::BuildingState actual;
    if(!remove_diagnostics::resolve_record_target(q.snapshot,q.key,fresh,e)||!scenery_kiss::read_action_prop(fresh.uid,actual,e))return false;
    if(fresh.snapshot_id!=q.snapshot||fresh.world_epoch!=q.epoch||fresh.world_generation!=q.generation||actual.type!=fresh.type||actual.variant!=fresh.variant||!fresh.object||!fresh.manager)return fail(e,"move_ui_actual_Building_binding_differs");
    if(!fresh.normal_building&&!fresh.normal_scenery)return fail(e,fresh.kind_cause.empty()?"move_ui_target_not_actual_normal_Building":fresh.kind_cause);
    if(j.target.object&&(fresh.object!=j.target.object||fresh.uid!=j.target.uid||fresh.manager!=j.target.manager))return fail(e,"move_ui_target_changed_before_callback");
    j.target=fresh;j.context={image,fresh.manager,fresh.object,fresh.uid,q.epoch,q.generation,sha,fresh.normal_scenery?common::Family::Scenery:common::Family::Building};
    if(!j.bound.armed.target){j.bound.armed.origin={q.snapshot,q.epoch,q.generation,uintptr_t(w.world)};j.bound.armed.target=fresh.object;j.bound.armed.local_uid=fresh.uid;j.bound.armed.object_key=q.key;j.bound.armed.before={actual.type,actual.variant,actual.prop.x,actual.prop.y,actual.prop.z,actual.rotation,actual.scale,true};}
    thread=actual_thread;return true;
}
uint32_t command(const common::Api* api,const common::Window* window,Evidence* r)noexcept{
    common::NativeRecordStorage record{};bool constructed=false;uint32_t exception=0;
    __try{
        r->attempted_mask|=2;api->construct(&record,"Move");constructed=true;r->returned_mask|=2;
        r->attempted_mask|=4;const bool label=api->label_equals(&record,"Move");r->returned_mask|=4;
        if(label){r->attempted_mask|=8;r->command_accepted=api->command(reinterpret_cast<void*>(window->window),&record);r->returned_mask|=8;}
    }__except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}
    if(constructed){r->attempted_mask|=16;__try{api->destroy(&record);r->returned_mask|=16;}__except(EXCEPTION_EXECUTE_HANDLER){r->cleanup_exception=GetExceptionCode();}}
    return exception;
}
uint32_t cursor_call(uintptr_t image,uintptr_t behaviour,uintptr_t cursor,const std::array<float,3>* position,unsigned step,Evidence* r)noexcept{
    __try{r->attempted_mask|=uint32_t{1}<<step;const uintptr_t rva=step==5?0x5c0200:step==6?0x5c05c0:0x5c01e0;reinterpret_cast<void(__fastcall*)(void*,void*,const float*)>(image+rva)(reinterpret_cast<void*>(behaviour),reinterpret_cast<void*>(cursor),position->data());r->returned_mask|=uint32_t{1}<<step;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool active_cursor(Job& j,bool pending,std::string& e){
    uintptr_t ap=0,b=0,data=0,ghost=0,target=0;int32_t count=0,capacity=0;unsigned char flags[2]{},duplicate=1;
    if(!field(j.context.image,0x13df9d0,j.cursor,e)||!field(j.cursor,0,ap,e)||ap!=j.context.image+0x1786dd0||!field(j.cursor,0x190,b,e)||!field(b,0,ap,e)||ap!=j.context.image+0x172f1f0||!field(b,0x460,data,e)||!field(b,0x468,count,e)||!field(b,0x46c,capacity,e)||count!=1||capacity<1||!field(data,8,ghost,e)||!field(data,0x10,target,e)||target!=j.target.object||!field(ghost,0,ap,e)||ap!=j.context.image+0x1705230||!memory.copy(b+0x400,flags,2,e)||!field(b,0x499,duplicate,e))return fail(e,e.empty()?"move_ui_actual_cursor_record_owner_differs":e);
    if(duplicate||flags[0]!=(pending?0:1)||flags[1]!=(pending?1:0))return fail(e,"move_ui_actual_native_placement_flags_differ required_placing="+std::to_string(pending?0:1)+" measured_placing="+std::to_string(flags[0])+" required_pending="+std::to_string(pending?1:0)+" measured_pending="+std::to_string(flags[1])+" required_duplicate=0 measured_duplicate="+std::to_string(duplicate));
    if(j.bound.armed.behaviour&&(b!=j.bound.armed.behaviour||ghost!=j.bound.armed.ghost))return fail(e,"move_ui_normal_cursor_or_ghost_changed");
    j.bound.armed.behaviour=b;j.bound.armed.record=data;j.bound.armed.ghost=ghost;
    auto after=j.bound.armed.before;
    if(!memory.copy(ghost+0x100,after.rotation.data(),16,e)||!field(ghost,0x110,after.x,e)||!field(ghost,0x114,after.y,e)||!field(ghost,0x118,after.z,e)||!memory.copy(ghost+0x11c,after.scale.data(),12,e))return false;
    j.bound.armed.after=after;j.ghost_after=after;return true;
}
json pose(const move_kiss::State& s){return {{"type",s.type},{"variant",s.variant},{"position",std::array<float,3>{s.x,s.y,s.z}},{"rotation",s.rotation},{"scale",s.scale}};}
json measured_pose(const std::optional<move_kiss::State>& s){return s?pose(*s):json(nullptr);}
json format(const Job& j){const auto& r=j.bound.receipt;return {{"acceptance_token",j.bound.input.token},{"snapshot_uuid",j.bound.input.snapshot},{"record_key",j.bound.input.key},{"phase",int(r.phase)},{"attempted_mask",r.attempted_mask},{"returned_mask",r.returned_mask},{"exception",r.exception},{"cleanup_exception",r.cleanup_exception},{"command_accepted",r.command_accepted},{"capture_count",r.captures},{"action_uuid",r.action_uuid},{"submitted",j.bound.submitted},{"confirmed",r.phase==Phase::Confirmed},{"cause",r.cause},{"native_stage",j.native_stage},{"requested_position",j.bound.input.position},{"target_before",j.bound.armed.target?pose(j.bound.armed.before):json(nullptr)},{"ghost_before",measured_pose(j.ghost_before)},{"ghost_after_mouse_over",measured_pose(j.ghost_after_mouse_over)},{"ghost_after",measured_pose(j.ghost_after)},{"final_intent_pose",measured_pose(j.final_intent_pose)}};}
}
bool unresolved()noexcept{for(const auto& [token,j]:jobs){(void)token;if(j.bound.receipt.phase!=Phase::Rejected&&j.bound.receipt.phase!=Phase::Confirmed)return true;}return false;}
bool probe(const json& args,json& out,std::string& e)try{
    out=json::object();Input q;if(!parse(args,q,e))return false;
    if(jobs.contains(q.token))return fail(e,"move_ui_token_already_attempted_no_replay");if(unresolved())return fail(e,"move_ui_previous_effect_unresolved");
    Job preflight;preflight.bound.input=q;preflight.bound.receipt.token=q.token;
    if(!context(preflight,e))return false;
    const auto controller=typed_action_runtime::status();if(!ui_move_runtime::enabled()||!net::local_session_connected()||!controller.value("started",false)||controller.value("pending",true)||controller.value("locked",true))return fail(e,"move_ui_actual_capture_or_controller_not_ready");
    common::Api api;if(!common::resolve_api(memory,preflight.context.image,preflight.context.exe_sha256,api,e,preflight.context.family))return false;
    for(const auto& guard:code::guards){std::vector<unsigned char> actual(guard.size);if(!memory.copy(preflight.context.image+guard.rva,actual.data(),actual.size(),e))return false;if(std::memcmp(actual.data(),guard.bytes,guard.size))return fail(e,"move_ui_full_native_code_guard_differs rva="+std::to_string(guard.rva));}
    auto& j=jobs.emplace(q.token,std::move(preflight)).first->second;active=&j;auto& r=j.bound.receipt;r.phase=Phase::Attempted;
    j.native_stage="select_show";common::Receipt selected;
    bool ok=common::process_calls().select(api,j.context,selected,e);r.attempted_mask|=selected.selection_attempted?1u:0u;r.returned_mask|=selected.selection_returned?1u:0u;r.exception=selected.native_exception;
    common::Window window;if(ok)ok=common::read_live_window(memory,j.context,window,e)&&context(j,e);
    if(ok){j.native_stage="Move_UICommand";r.exception=command(&api,&window,&r);ok=!r.exception&&!r.cleanup_exception&&r.command_accepted&&(r.returned_mask&30)==30;}
    if(ok){j.native_stage="active_cursor_before_input";ok=context(j,e)&&active_cursor(j,false,e);if(ok)j.ghost_before=j.bound.armed.after;}
    for(unsigned step=5;ok&&step<=7;++step){j.native_stage=step==5?"MouseOver":step==6?"Press":"Release";if(!context(j,e)){ok=false;break;}r.exception=cursor_call(j.context.image,j.bound.armed.behaviour,j.cursor,&q.position,step,&r);ok=!r.exception;if(ok&&step==5){ok=active_cursor(j,false,e);if(ok)j.ghost_after_mouse_over=j.bound.armed.after;}}
    if(ok){j.native_stage="active_cursor_after_Release";ok=active_cursor(j,true,e);if(ok)j.final_intent_pose=j.bound.armed.after;}
    if(!ok){if(e.empty())e="move_ui_native_callback_or_record_cleanup_failed SEH="+std::to_string(r.exception)+" cleanup="+std::to_string(r.cleanup_exception);unknown(j,e);out=format(j);return false;}
    j.native_stage="awaiting_GameUpdate_capture";r.phase=Phase::Armed;j.bound.armed_at=GetTickCount64();out=format(j);e.clear();LOG_INFO("[MoveUIProbe] token={} normal Move input armed; awaiting native GameUpdate capture",q.token);return true;
}catch(const std::exception& cause){e=std::string("move_ui_probe_exception: ")+cause.what();if(active){unknown(*active,e);out=format(*active);}return false;}
catch(...){e="move_ui_probe_unknown_exception";if(active){unknown(*active,e);out=format(*active);}return false;}
bool captured(const move_kiss::Intent& intent,const std::string& id,std::string& e)noexcept{
    if(!active)return true;
    try{active->native_stage="GameUpdate_capture";if(!context(*active,e)||!bind_capture(active->bound,intent,id,e)){unknown(*active,e);return false;}return true;}
    catch(const std::exception& cause){e=std::string("move_ui_capture_exception: ")+cause.what();unknown(*active,e);return false;}
    catch(...){e="move_ui_capture_unknown_exception";unknown(*active,e);return false;}
}
void submitted(const std::string& id)noexcept{if(active&&active->bound.receipt.action_uuid==id)active->bound.submitted=true;}
void failed(uintptr_t target,const std::string& cause)noexcept{if(active&&(!target||active->target.object==target))unknown(*active,"move_ui_actual_capture_cleanup_or_submit_failed: "+cause);}
bool read(const json& args,json& out,std::string& e){
    out=json::object();std::string token;if(!parse_token(args,token,e))return false;
    if(GetCurrentThreadId()!=thread)return fail(e,"move_ui_receipt_wrong_actual_mainthread");auto it=jobs.find(token);if(it==jobs.end())return fail(e,"move_ui_token_unknown");auto& j=it->second;
    if(j.bound.receipt.phase!=Phase::Confirmed&&j.bound.receipt.phase!=Phase::Unknown&&j.bound.receipt.phase!=Phase::Rejected){
        j.native_stage="authoritative_completion_read";if(!context(j,e)||!confirmation(j.bound,typed_action_runtime::status(),ui_move_runtime::status().value("last_native",json::object()),Config::instance().is_host,e))unknown(j,e);
        else if(j.bound.receipt.phase!=Phase::Confirmed&&GetTickCount64()-j.bound.armed_at>10000)unknown(j,"move_ui_observation_timeout_ms>10000_no_replay");
    }
    if(j.bound.receipt.phase==Phase::Confirmed&&active==&j)active=nullptr;
    out=format(j);e.clear();return true;
}
}
