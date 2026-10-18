#include "tool_cancel.h"
#include "buildings_tab.h"
#include "buy_region.h"
#include "ui_region_runtime.h"
#include "mt2mp/core/config.h"
#include "scenery_kiss/lifecycle_native.h"
#include "codeguards.h"
#include "placement_ui_probe/ui_probe.h"
#include "native_open_runtime.h"
#include "typed_action_runtime.h"
#include "typed_action/action_session.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/net/snapshot_sync.h"
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
namespace mt2mp::game::native_tool_prepare {
namespace {
using json=nlohmann::json;
struct Job {Context context;uintptr_t image{},cursor{},old{},desired{};uint32_t thread{},attempted{},returned{},exception{};bool build{},scenery{},buy{},confirmed{},unknown{};building_mixed_identity::opened_file::CurrentWorld buy_origin;action_session::Binding buy_binding;buy_region::Pin buy_pin;uint64_t observed{};std::string cause;};
std::map<std::string,Job> jobs;Job* active_job=nullptr;placement_ui_probe::ProcessMemory memory;
struct SceneMemory final:scenery_kiss::native::Memory {bool copy(uintptr_t p,void* out,size_t size,std::string& e)override{return memory.copy(p,out,size,e);}} scene_memory;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool field(uintptr_t p,size_t o,T& v,std::string& e){if(!p||p>UINTPTR_MAX-o)return fail(e,"tool_prepare_native_address_range_invalid");return memory.copy(p+o,&v,sizeof v,e);}
bool world(Job& j,std::string& e){uintptr_t image=0;uint32_t thread=0;std::string sha;if(!native::read_resolver_receipt(image,thread,sha,e))return false;
    if(thread!=GetCurrentThreadId()||(j.thread&&j.thread!=thread)||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"tool_prepare_actual_module_mainthread_differs");
    building_mixed_identity::opened_file::CurrentWorld current;if(!native_open_runtime::read_current_world(current,e))return false;
    if(!current.world||!current.state||current.world_epoch!=j.context.epoch||current.generation!=j.context.generation||net::current_snapshot_id()!=j.context.snapshot)return fail(e,"tool_prepare_actual_world_snapshot_differs");j.thread=thread;j.image=image;return true;}
json format(const Job& j){return {{"acceptance_token",j.context.token},{"snapshot_uuid",j.context.snapshot},{"attempted_mask",j.attempted},{"returned_mask",j.returned},{"exception",j.exception},{"confirmed",j.confirmed},{"unknown",j.unknown},{"cause",j.cause},{"cleanup_observed_tick_ms",j.observed},{"cleanup_applicable",buy_region::preview_applicable(j.buy,j.old!=j.desired)}};}
bool ledger(action_session::Ledger& out,std::string& e){native::SimulationState s;if(!native::read_simulation_state(s,e))return false;out={s.cash,s.clock.flat_day,int32_t(s.record_present),s.record_day,s.record_cash,s.record_income,s.record_expense,s.line_counts,s.line_income};return true;}
bool pin_buy_region(Job& j,bool after,std::string& e){
    if(!world(j,e))return false;
    building_mixed_identity::opened_file::CurrentWorld current;action_session::Binding binding;
    if(!native_open_runtime::read_current_world(current,e))return false;
    if(!buy_region::same_world(current,j.buy_origin))return fail(e,"BuyRegion full NativeOpen owner changed");
    if(!typed_action_runtime::network_binding(j.context.snapshot,binding,e))return false;
    if(binding!=j.buy_binding)return fail(e,"BuyRegion Controller binding changed");
    native::SimulationState state;native::WorldStatus status;
    if(!native::read_simulation_state(state,e)||!native::read_world_status(status,e))return false;
    if(!buy_region::idle(Config::instance().local_test,typed_action_runtime::status(),ui_region_runtime::status(),
                         state.clock.time_scale,status.edit_operations,status.saves_in_progress,e))return false;
    buy_region::Pin current_pin;
    return buy_region::inspect(memory,j.image,j.buy_pin,after,current_pin,e)&&
        (current_pin==j.buy_pin||fail(e,"BuyRegion retained Cursor/SelectionManager owner changed"));
}
uint32_t calls(Job* j)noexcept{
    __try{
        struct NativeCalls {
            Job* job;
            void cancel(){reinterpret_cast<void(__fastcall*)(void*,void*)>(job->image+buy_region::cancel_rva(job->build,job->scenery,job->buy))(reinterpret_cast<void*>(job->old),reinterpret_cast<void*>(job->cursor));}
            void switch_default(){reinterpret_cast<void(__fastcall*)(void*,void*)>(job->image+0x5854e0)(reinterpret_cast<void*>(job->cursor),nullptr);}
        } native_calls{j};
        buy_region::sequence(*j,native_calls);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool observe(Job& j,std::string& e){Readback out;uintptr_t current=0,ap=0,manager=0,manager_ap=0;native::WorldStatus status;
    if(!world(j,e)||!field(j.cursor,0x190,current,e)||!field(current,0,ap,e))return false;
    out.actual_default=current==j.desired&&ap==j.image+0x17391a0;
    if(buy_region::preview_applicable(j.buy,j.old!=j.desired)){
        if(j.scenery){if(!field(j.old,0x6c0,out.placing,e)||!field(j.old,0x6c1,out.pending,e)||!field(j.old,0x6c2,out.cancel,e)||!field(j.old,0xb58,out.owned_a,e)||!field(j.old,0xb50,out.owned_b,e))return false;}
        else if(j.build){if(!field(j.old,0x660,out.placing,e)||!field(j.old,0x661,out.pending,e)||!field(j.old,0x662,out.cancel,e)||!field(j.old,0x6b8,out.owned_a,e)||!field(j.old,0x6c0,out.owned_b,e))return false;}
        else if(!field(j.old,0x400,out.placing,e)||!field(j.old,0x401,out.pending,e)||!field(j.old,0x402,out.cancel,e)||!field(j.old,0x468,out.records,e)||!field(j.old,0x490,out.owned_a,e))return false;
    }
    if(!field(j.image,0x13df630,manager,e)||!field(manager,0,manager_ap,e)||manager_ap!=j.image+0x17205d0||!field(manager,0xd1,out.manager_progress,e)||!field(manager,0xd3,out.manager_queue,e)||!native::read_world_status(status,e))return fail(e,e.empty()?"tool_prepare_actual_SelectionManager_class_or_readback_differs":e);
    out.edits=status.edit_operations;out.saves=status.saves_in_progress;
    if(j.buy&&!pin_buy_region(j,true,e))return false;
    if(!clean(j.build||j.scenery,out,e,buy_region::preview_applicable(j.buy,j.old!=j.desired)))return false;
    if(j.scenery&&!scenery_kiss::cancel_after(scene_memory,j.old,e))return false;j.confirmed=true;j.observed=GetTickCount64();j.cause.clear();return true;
}
}
#include "buildings_tab_runtime.inc"
bool unresolved()noexcept{if(buildings_unresolved())return true;for(const auto& [token,j]:jobs){(void)token;if(unresolved_effect(j.attempted,j.confirmed))return true;}return false;}
bool invoke(const json& args,json& out,std::string& e)try{active_job=nullptr;out=json::object();Context q;if(!parse(args,q,e))return false;if(jobs.contains(q.token)||unresolved())return fail(e,"tool_prepare_token_used_or_previous_effect_unresolved_no_replay");
    if(jobs.size()>=32)return fail(e,"tool_prepare_receipt_capacity_reached measured=32 required_less_than=32");
    Job initial;initial.context=q;if(!world(initial,e))return false;const auto controller=typed_action_runtime::status();if(!controller.value("started",false)||controller.value("pending",true)||controller.value("locked",true))return fail(e,"tool_prepare_actual_Controller_not_idle");
    for(const auto& guard:code::guards){std::vector<unsigned char> bytes(guard.size);if(!memory.copy(initial.image+guard.rva,bytes.data(),bytes.size(),e))return false;if(std::memcmp(bytes.data(),guard.bytes,guard.size))return fail(e,"tool_prepare_full_code_guard_differs rva="+std::to_string(guard.rva));}
    uintptr_t ap=0;unsigned char pending=0;
    if(!field(initial.image,0x13df9d0,initial.cursor,e)||!field(initial.cursor,0,ap,e)||ap!=initial.image+0x1786dd0||!field(initial.cursor,0x190,initial.old,e)||!field(initial.cursor,0x198,initial.desired,e)||!field(initial.desired,0,ap,e)||ap!=initial.image+0x17391a0)return fail(e,e.empty()?"tool_prepare_actual_default_cursor_class_differs":e);
    uintptr_t activation=0;unsigned char keeps_selection=0;if(!field(ap,0xa8,activation,e)||activation!=initial.image+0xbf6500||!field(initial.desired,0x120,keeps_selection,e)||keeps_selection!=1)return fail(e,"tool_prepare_Default_activation_or_selection_contract_differs");
    if(initial.old==initial.desired){auto& j=jobs.emplace(q.token,initial).first->second;const bool ok=observe(j,e);j.cause=e;out=format(j);return ok;}
    if(!field(initial.old,0,ap,e)||(ap!=initial.image+0x17327d0&&ap!=initial.image+0x172f1f0&&ap!=initial.image+0x17396b0&&ap!=initial.image+0x173c170))return fail(e,"tool_prepare_supports_current_Build_Move_single_Scenery_or_BuyRegion_only");initial.build=ap==initial.image+0x17327d0;initial.scenery=ap==initial.image+0x17396b0;initial.buy=ap==initial.image+0x173c170;
    if(initial.buy){
        if(!native_open_runtime::read_current_world(initial.buy_origin,e)||!typed_action_runtime::network_binding(q.snapshot,initial.buy_binding,e))return false;
        const buy_region::Pin expected{initial.cursor,initial.old,initial.desired,0};
        if(!buy_region::inspect(memory,initial.image,expected,false,initial.buy_pin,e)||!pin_buy_region(initial,false,e))return false;
    }
    if(initial.scenery&&(!scenery_kiss::cancel_code(scene_memory,initial.image,e)||!scenery_kiss::cancel_before(scene_memory,initial.image,initial.old,e)))return false;
    uintptr_t deactivation=0,cancellation=0;if(!initial.buy&&(!field(ap,0xb0,deactivation,e)||deactivation!=initial.image+(initial.scenery?0x5d04b0:(initial.build?0x5910e0:0x5c13e0))||!field(ap,0x128,cancellation,e)||cancellation!=initial.image+(initial.scenery?0x5d0690:(initial.build?0x591320:0x5c0900))))return fail(e,"tool_prepare_actual_Build_Move_virtual_cleanup_contract_differs");
    if(!initial.buy&&(!field(initial.old,initial.scenery?0x6c1:(initial.build?0x661:0x401),pending,e)||pending))return fail(e,e.empty()?"tool_prepare_native_commit_pending_no_cancel":e);
    action_session::Ledger before,after;if(!ledger(before,e))return false;
    auto& j=jobs.emplace(q.token,initial).first->second;active_job=&j;
    if(j.buy){
        struct BuyCalls {
            Job& job;const action_session::Ledger& before;
            bool pin(bool after,std::string& cause){return pin_buy_region(job,after,cause);}
            void invoke(){job.exception=calls(&job);}
            bool observe(std::string& cause){return native_tool_prepare::observe(job,cause);}
            bool unchanged(std::string& cause){action_session::Ledger actual;if(!ledger(actual,cause))return false;return actual==before||fail(cause,"tool_prepare_complete_ledger_changed");}
        } buy_calls{j,before};
        const bool okay=buy_region::prepare(j,buy_calls,e);j.cause=e;out=format(j);
        if(!okay)LOG_ERROR("[ToolPrepare] token={} BuyRegion transition failed cause={}; no replay",q.token,e);
        else LOG_INFO("[ToolPrepare] token={} original BuyRegion Cancel/default switch observed",q.token);
        return okay;
    }
    j.exception=calls(&j);
    if(j.exception||j.returned!=3){j.unknown=true;j.cause="tool_prepare_native_cancel_switch_SEH="+std::to_string(j.exception)+" no replay";e=j.cause;out=format(j);LOG_ERROR("[ToolPrepare] token={} cause={}",q.token,e);return false;}
    if(!observe(j,e)){j.unknown=true;j.cause=e;out=format(j);LOG_ERROR("[ToolPrepare] token={} cleanup unconfirmed cause={}",q.token,e);return false;}
    if(!ledger(after,e)||after!=before){j.confirmed=false;j.unknown=true;j.cause=e.empty()?"tool_prepare_complete_ledger_changed":e;e=j.cause;out=format(j);LOG_ERROR("[ToolPrepare] token={} ledger readback failed cause={}",q.token,e);return false;}
    out=format(j);LOG_INFO("[ToolPrepare] token={} original Cancel/default switch and preview cleanup observed",q.token);return true;
}catch(const std::exception& cause){e=std::string("tool_prepare_exception: ")+cause.what();if(active_job&&active_job->attempted){active_job->confirmed=false;active_job->unknown=true;active_job->cause=e;out=format(*active_job);}LOG_ERROR("[ToolPrepare] operation=native_cancel_tool cause={}",e);return false;}catch(...){e="tool_prepare_unknown_exception";if(active_job&&active_job->attempted){active_job->confirmed=false;active_job->unknown=true;active_job->cause=e;out=format(*active_job);}LOG_ERROR("[ToolPrepare] operation=native_cancel_tool cause={}",e);return false;}
bool read(const json& args,json& out,std::string& e){out=json::object();if(!args.is_object()||args.size()!=1||!args.contains("acceptance_token")||!args.at("acceptance_token").is_string())return fail(e,"tool_prepare_read_exact_token_required");const auto token=args.at("acceptance_token").get<std::string>();const auto found=jobs.find(token);if(found==jobs.end())return fail(e,"tool_prepare_token_unknown");const auto& j=found->second;if(GetCurrentThreadId()!=j.thread)return fail(e,"tool_prepare_read_wrong_actual_mainthread");out=format(j);e=j.cause;return j.confirmed;}
}
