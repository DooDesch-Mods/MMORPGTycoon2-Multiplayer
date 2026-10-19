#include "economic_fence.h"
#include "management_runtime.h"
#include <intrin.h>
#include "native_guards.h"
#include "subscriber_needs.h"
#include "day_rollover/sync.h"
#include "day_rollover/rebind_time.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/hooks/hook_engine.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstring>
namespace mt2mp::game::live_host_state {
namespace {
using CashVoid=void(*)(void*,int32_t,int32_t);using CashBool=bool(*)(void*,int32_t,int32_t);
using StringBool=bool(*)(void*,const void*);using DayVoid=void(*)(void*,int32_t);using EventVoid=void(*)(void*,const void*);
using Lookup=void*(*)(void*,int32_t);
CashVoid original_add=nullptr;CashBool original_subtract=nullptr,original_pay=nullptr;
StringBool original_take=nullptr,original_repay=nullptr;DayVoid original_make=nullptr;EventVoid original_event=nullptr;
placement_ui_probe::ProcessMemory memory;
bool fail(std::string& e,const std::string& s){e=s;return false;}
template<class T>bool read(uintptr_t p,T& value,std::string& e){return p&&memory.copy(p,&value,sizeof value,e);}
DWORD call_cash_void(CashVoid f,void* p,int32_t a,int32_t c)noexcept{__try{f(p,a,c);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD call_cash_bool(CashBool f,void* p,int32_t a,int32_t c,bool* result)noexcept{__try{*result=f(p,a,c);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD call_string_bool(StringBool f,void* p,const void* s,bool* result)noexcept{__try{*result=f(p,s);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD call_day(DayVoid f,void* p,int32_t day)noexcept{__try{f(p,day);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD call_event(EventVoid f,void* p,const void* event)noexcept{__try{f(p,event);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD call_lookup(Lookup f,void* p,int32_t id,void** out)noexcept{__try{*out=f(p,id);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
void add(void* p,int32_t a,int32_t c){auto& f=economic_fence();if(!f.allow(EconomicLeaf::AddCash,p,a,c))return;const auto error=call_cash_void(original_add,p,a,c);if(error)f.original_failed(EconomicLeaf::AddCash,error);}
bool subtract(void* p,int32_t a,int32_t c){auto& f=economic_fence();if(!f.allow(EconomicLeaf::SubtractCash,p,a,c))return false;bool result=false;const auto error=call_cash_bool(original_subtract,p,a,c,&result);if(error)f.original_failed(EconomicLeaf::SubtractCash,error);return !error&&result;}
bool take(void* p,const void* s){bool consumed=false;if(management_runtime::capture_loan(false,p,s,uintptr_t(_ReturnAddress()),consumed))return consumed;auto& f=economic_fence();if(!f.allow(EconomicLeaf::TakeLoan,p))return false;bool result=false;const auto error=call_string_bool(original_take,p,s,&result);if(error)f.original_failed(EconomicLeaf::TakeLoan,error);return !error&&result;}
bool repay(void* p,const void* s){bool consumed=false;if(management_runtime::capture_loan(true,p,s,uintptr_t(_ReturnAddress()),consumed))return consumed;auto& f=economic_fence();if(!f.allow(EconomicLeaf::RepayLoan,p))return false;bool result=false;const auto error=call_string_bool(original_repay,p,s,&result);if(error)f.original_failed(EconomicLeaf::RepayLoan,error);return !error&&result;}
bool pay(void* p,int32_t a,int32_t c){auto& f=economic_fence();if(!f.allow(EconomicLeaf::Pay,p,a,c))return false;bool result=false;const auto error=call_cash_bool(original_pay,p,a,c,&result);if(error)f.original_failed(EconomicLeaf::Pay,error);return !error&&result;}
void make(void* p,int32_t day){auto& f=economic_fence();if(!f.allow(EconomicLeaf::MakeDay,p,day))return;std::string cause;if(!day::observe_host_make(true,0,cause))f.journal_failed(cause);const auto error=call_day(original_make,p,day);if(error)f.original_failed(EconomicLeaf::MakeDay,error);if(!day::observe_host_make(false,error,cause))f.journal_failed(cause);}
void event(void* p,const void* e){auto& f=economic_fence();if(!f.allow(EconomicLeaf::Event,p))return;const auto error=call_event(original_event,p,e);if(error)f.original_failed(EconomicLeaf::Event,error);}
uintptr_t detours[]={uintptr_t(&add),uintptr_t(&subtract),uintptr_t(&take),uintptr_t(&make),uintptr_t(&event),uintptr_t(&repay),uintptr_t(&pay)};
void** originals[]={reinterpret_cast<void**>(&original_add),reinterpret_cast<void**>(&original_subtract),reinterpret_cast<void**>(&original_take),reinterpret_cast<void**>(&original_make),reinterpret_cast<void**>(&original_event),reinterpret_cast<void**>(&original_repay),reinterpret_cast<void**>(&original_pay)};
bool routed(uintptr_t source,uintptr_t expected,std::string& e){for(unsigned hop=0;hop<2;++hop){unsigned char code[6]{};if(!memory.copy(source,code,sizeof code,e))return false;int32_t displacement=0;uintptr_t next=0;if(code[0]==0xe9){std::memcpy(&displacement,code+1,4);next=source+5+intptr_t(displacement);}else if(code[0]==0xff&&code[1]==0x25){std::memcpy(&displacement,code+2,4);if(!read(source+6+intptr_t(displacement),next,e))return false;}else return fail(e,"live economic hook route is not actual MinHook E9/FF25");if(next==expected){e.clear();return true;}source=next;}return fail(e,"live economic hook route differs from own detour");}
}
EconomicFence& economic_fence(){static EconomicFence f;return f;}
bool EconomicFence::prepare_before_hooks(std::string& e){if(prepared_)return fail(e,"live economic hook preparation repeated; no reinstall");const auto image=uintptr_t(GetModuleHandleW(nullptr));if(!image||uintptr_t(native::entrypoint(native::EntryPoint::BusinessNextDayFinances))!=image+0x57570||uintptr_t(native::entrypoint(native::EntryPoint::SubscriberNewDay))!=image+0x2eb530)return fail(e,"live economic actual initialized native module/entrypoints differ");
    for(const auto& body:guards::bodies){std::vector<unsigned char> actual(body.size);if(!memory.copy(image+body.rva,actual.data(),actual.size(),e))return false;if(std::memcmp(actual.data(),body.bytes,body.size))return fail(e,"live economic original whole-body differs rva="+std::to_string(body.rva));}
    prepared_=true;pin_.image=image;
    for(size_t i=0;i<7;++i)if(!hooks::create_hook(reinterpret_cast<void*>(image+guards::bodies[i].rva),reinterpret_cast<void*>(detours[i]),originals[i])||!*originals[i]){locked_=true;return fail(e,"live economic create hook failed index="+std::to_string(i)+"; module/storage retained");}e.clear();return true;}
bool EconomicFence::bind_existing_day_routes(uintptr_t day,uintptr_t subscriber,std::string& e){if(!prepared_||!day||!subscriber||existing_day_||existing_subscriber_day_)return fail(e,"live economic existing typed day routes absent/rebound");existing_day_=day;existing_subscriber_day_=subscriber;e.clear();return true;}
bool EconomicFence::routes_ready(std::string& e){for(size_t i=0;i<7;++i)if(!*originals[i]||!routed(pin_.image+guards::bodies[i].rva,detours[i],e))return false;if(!existing_day_||!existing_subscriber_day_||!routed(pin_.image+0x57570,existing_day_,e)||!routed(pin_.image+0x2eb530,existing_subscriber_day_,e))return fail(e,e.empty()?"live economic actual existing day routes missing":e);e.clear();return true;}
bool EconomicFence::arm(const Context& c,const Binding& b,std::function<void(const char*,const std::string&)> logger,std::string& e){if(!prepared_||locked_||armed_||!logger)return fail(e,"live economic arm unprepared/locked/repeated/logger missing");NativePort reader(c,b,nullptr);Pin actual;if(!reader.read(actual,e))return false;if(actual.image!=pin_.image||!native::subscriber_needs_hooks_ready()||!routes_ready(e))return fail(e,e.empty()?"live economic prepared/actual module differs":e);if(c.role==action_session::Role::Client&&actual.selected!=0)return fail(e,"live economic Client native Clock must be0");context_=c;binding_=b;pin_=actual;logger_=std::move(logger);armed_=true;e.clear();return true;}
bool EconomicFence::enter_game_update(std::string& e){if(!armed_||locked_||GetCurrentThreadId()!=pin_.thread||frame_depth_)return fail(e,"live economic actual GameUpdate scope not armed/locked/thread/reentry");++frame_depth_;e.clear();return true;}
void EconomicFence::leave_game_update(){if(frame_depth_!=1||action_){locked_=true;cause_="live economic GameUpdate exit scope/action mismatch";if(logger_)logger_("error",cause_);}frame_depth_=0;}
bool EconomicFence::read_current_fence(const Pin& expected,std::string& e){if(suspended_||!armed_||locked_||!prepared_||frame_depth_!=1||action_||GetCurrentThreadId()!=pin_.thread||expected.context!=context_||expected.binding!=binding_||expected.image!=pin_.image)return fail(e,"live economic actual Client GameUpdate fence unavailable/locked/active action");return routes_ready(e);}
bool EconomicFence::begin_action(const action_session::Message& result,const Pin& actual,std::string& e){if(result.kind!=action_session::Kind::HostResult||result.outcome!=action_session::Outcome::Confirmed||result.session!=context_.session||result.request.snapshot_uuid!=context_.snapshot_uuid||ledger(actual.values)!=result.evidence.before||!read_current_fence(actual,e))return fail(e,e.empty()?"live economic authoritative cash scope origin/before differs":e);NativePort reader(context_,binding_,nullptr);if(!reader.current_owners(actual,e))return false;action_=result;action_pin_=actual;action_cash_calls_=0;e.clear();return true;}
bool EconomicFence::end_action(std::string& e){if(!action_)return fail(e,"live economic action exit has no actual scope");const auto expected=action_->evidence.after;action_.reset();if(locked_)return fail(e,cause_.empty()?"live economic action already locked":cause_);NativePort reader(context_,binding_,nullptr);Pin actual;if(!reader.read(actual,e)||ledger(actual.values)!=expected){locked_=true;cause_=e.empty()?"live economic authoritative full Ledger after differs":e;return fail(e,cause_);}e.clear();return true;}
bool EconomicFence::allow_needs(void* receiver){
    if(!armed_||suspended_||context_.role==action_session::Role::Host)return true;
    if(locked_)return false;
    std::string error;bool owned=false;
    auto at=[](uintptr_t base,size_t offset,auto& value,std::string& e){if(!base||base>UINTPTR_MAX-offset)return fail(e,"Subscriber Needs receiver address extent invalid");return read(base+offset,value,e);};
    if(!native::subscriber_needs_hooks_ready()||!subscriber_needs::owns_receiver(at,pin_.image,uintptr_t(receiver),owned,error)){
        journal_failed(error.empty()?"Subscriber Needs live write hooks unavailable":error);return false;
    }
    if(!owned)return true;
    NativePort reader(context_,binding_,nullptr);
    if(GetCurrentThreadId()!=pin_.thread||!reader.current_owners(pin_,error)){
        journal_failed(error.empty()?"Subscriber Needs registered receiver on wrong thread":error);
    }
    return false;
}
bool EconomicFence::allow(EconomicLeaf leaf,void* receiver,int32_t amount,int32_t category){if(!armed_||context_.role==action_session::Role::Host)return true;if(suspended_){const auto i=size_t(leaf);if(i<suppressed_.size())++suppressed_[i];return false;}std::string reason;
    const bool cash_scope=!locked_&&action_&&frame_depth_==1&&
        (leaf==EconomicLeaf::AddCash||leaf==EconomicLeaf::SubtractCash)&&!action_cash_calls_;
    // History cash and Region Purchase use their original cash leaf inside one EditOperation.
    const bool edit_cash=cash_scope&&
        ((leaf==EconomicLeaf::AddCash&&
          (action_->request.action==action_session::Action::RemoveSnapshotBuilding||
           (category==6&&(action_->request.action==action_session::Action::RemovePath||
                          action_->request.action==action_session::Action::UndoBuilding||
                          action_->request.action==action_session::Action::RedoBuilding))))||
         (leaf==EconomicLeaf::SubtractCash&&category==7&&
          action_->request.action==action_session::Action::EditRegionProperty&&
          action_->request.region_property.kind==action_session::RegionPropertyKind::Purchase));
    const bool management_loan=!locked_&&action_&&frame_depth_==1&&action_->request.action==action_session::Action::ManagementCommand&&
        ((leaf==EconomicLeaf::TakeLoan&&action_->request.management.op==action_session::ManagementOp::TakeLoan)||
         (leaf==EconomicLeaf::RepayLoan&&action_->request.management.op==action_session::ManagementOp::RepayLoan));
    const auto& owner_pin=(cash_scope||management_loan)?action_pin_:pin_;
    bool owner=GetCurrentThreadId()==owner_pin.thread;NativePort reader(context_,binding_,nullptr);
    // PathInfoWindow and SelectionManager each open one native EditOperation before the Road refund.
    const int32_t expected_edits=edit_cash?(action_->request.action==action_session::Action::RemovePath?2:1):0;
    if(owner)owner=reader.current_owners(owner_pin,reason,expected_edits);if(owner&&(leaf==EconomicLeaf::Pay||leaf==EconomicLeaf::SubscriberDay)){uintptr_t ap=0,manager=0,map=0;int32_t id=-1;void* actual=nullptr;owner=read(uintptr_t(receiver),ap,reason)&&ap==pin_.image+0x16e75f0&&read(uintptr_t(receiver)+0x20,id,reason)&&id>=0&&read(pin_.image+0x13df690,manager,reason)&&read(manager,map,reason)&&map==pin_.image+0x1725f40;if(owner){const auto code=call_lookup(reinterpret_cast<Lookup>(pin_.image+0x2ee130),reinterpret_cast<void*>(manager),id,&actual);owner=!code&&actual==receiver;if(code)reason="live Subscriber ownership lookup SEH="+std::to_string(code);}}
    else if(owner)owner=uintptr_t(receiver)==owner_pin.business;
    if(!owner&&!locked_){locked_=true;cause_=reason.empty()?"live economic callback actual owner/Mainthread differs":reason;}
    if(owner&&!locked_&&management_loan)return true;
    if(owner&&!locked_&&cash_scope){action_session::Ledger predicted;if(predicted_cash(action_->evidence.before,leaf==EconomicLeaf::SubtractCash,amount,category,predicted,reason)&&predicted==action_->evidence.after){++action_cash_calls_;return true;}locked_=true;cause_=reason.empty()?"live economic native cash arguments differ from exact Host after Ledger":reason;}
    const auto index=size_t(leaf);if(index>=suppressed_.size()){locked_=true;cause_="live economic unknown leaf";if(logger_)logger_("error",cause_);return false;}++suppressed_[index];if(logger_&&suppressed_[index]==1)logger_(locked_?"error":"warn","live Client economic leaf suppressed index="+std::to_string(index)+(locked_?" original="+cause_:" reason=Host authority; no native original"));return false;}
void EconomicFence::original_failed(EconomicLeaf leaf,uint32_t seh){locked_=true;cause_="live economic native original SEH leaf="+std::to_string(size_t(leaf))+" code="+std::to_string(seh)+"; no replay";if(logger_)logger_("error",cause_);}
}

namespace mt2mp::game::live_host_state {
void EconomicFence::journal_failed(const std::string& e){locked_=true;cause_=e;if(logger_)logger_("error",cause_);}
bool EconomicFence::adopt_day_commit(std::string& e){if(suspended_||!armed_||locked_||action_||frame_depth_!=1||GetCurrentThreadId()!=pin_.thread)return fail(e,"Day fence adoption requires actual idle GameUpdate");NativePort reader(context_,binding_,nullptr);Pin actual;if(!reader.read(actual,e)||actual.world!=pin_.world||actual.clock!=pin_.clock||actual.business!=pin_.business||actual.image!=pin_.image||actual.common_epoch!=pin_.common_epoch||actual.selected!=0||!routes_ready(e))return fail(e,e.empty()?"Day actual fence owner/readback differs":e);pin_=actual;e.clear();return true;}
bool EconomicFence::suspend_snapshot(std::string& e){if(!armed_||locked_||suspended_||action_||GetCurrentThreadId()!=pin_.thread)return fail(e,"live snapshot fence requires actual idle bound owner");suspended_=true;e.clear();return true;}
bool EconomicFence::suspend_lobby(std::string& e){
    if(!armed_||suspended_||action_||frame_depth_>1||GetCurrentThreadId()!=pin_.thread)
        return fail(e,"live lobby fence suspension requires actual idle bound owner");
    suspended_=lobby_retiring_=true;e.clear();return true;
}
bool EconomicFence::rebind_snapshot(const Context& c,const Binding& b,std::string& e){if(lobby_retiring_||!suspended_||locked_||action_||c.session!=context_.session||c.role!=context_.role||c.local_peer!=context_.local_peer||c.host_peer!=context_.host_peer||c.peer_mask!=context_.peer_mask||GetCurrentThreadId()!=pin_.thread)return fail(e,"live snapshot fence rebind requires suspended same Controller");NativePort reader(c,b,nullptr);Pin actual;if(!reader.read(actual,e)||actual.image!=pin_.image||(c.role==action_session::Role::Client&&actual.selected!=0)||!routes_ready(e))return fail(e,e.empty()?"live snapshot actual new World/Time/Clock0 unavailable":e);time_integration::Status time_state;if(!time_integration::instance().status(time_state,e)||!day::matches_rebound_time(time_state,actual.common_epoch,e))return false;uint64_t time=0;if(!read_time_authority(c,time,e))return false;context_=c;binding_=b;pin_=actual;suspended_=false;e.clear();return true;}
}

namespace mt2mp::game::live_host_state {
bool EconomicFence::retire_lobby(std::string& e){
    if(!armed_){e.clear();return true;}
    if(!suspended_||(!lobby_retiring_&&locked_)||action_||frame_depth_||GetCurrentThreadId()!=pin_.thread)return fail(e,"lobby fence retirement requires suspended idle owner, no active action/frame or unscoped Unknown");
    native::WorldStatus actual;if(!native::read_world_status(actual,e))return false;
    if(actual.active_mode_index!=2||actual.title_transition_pending||actual.saves_in_progress>0){return fail(e,"lobby fence retirement requires actual idle Title after old World end");}
    retired_cause_=cause_;
    if(logger_&&!retired_cause_.empty())logger_("warn","live lobby retired after actual Title; retained original="+retired_cause_);
    const auto image=pin_.image;context_={};binding_={};pin_={};pin_.image=image;action_pin_={};armed_=suspended_=lobby_retiring_=locked_=false;
    action_cash_calls_=0;suppressed_={};logger_={};cause_.clear();e.clear();return true;
}
bool retire_lobby_fence(std::string& e){return economic_fence().retire_lobby(e);}
}
