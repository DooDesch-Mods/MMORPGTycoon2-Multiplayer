#include "control_runtime.h"
#include "control_lease.h"
#include "control_native_runtime.h"
#include "control_local_native.h"
#include "../intervention_census.h"
#include "../release_ui_owner.h"
#include "../typed_action_runtime.h"
#include "../session_provider.h"
#include "../time_integration.h"
#include "../live_host_state/native_port.h"
#include "../live_host_state/economic_fence.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/hooks/minhook_backend.h"
#include "mt2mp/net/session_wire.h"
#include <Windows.h>
#include <bcrypt.h>
#include <intrin.h>
#include <atomic>
#include <cstdio>
#include <deque>

namespace mt2mp::control_input::runtime {
namespace {
namespace a=action_session;namespace intervention=game::interventions;
using Update=void(*)(void*,float);using Pair=void(*)(void*,void*);using Flag=void(*)(void*,bool);using One=void(*)(void*);
struct Route {uintptr_t rva;size_t bytes,stolen;const char* sha;void* original=nullptr;game::release_ui_owner::Owner owner;};
struct Queued {std::string id;a::InterventionIntent intent;};
struct OperationReceipt {a::Binding binding;a::Request request;a::Evidence evidence;bool confirmed=false,retired=false;std::string cause;};
struct Runtime {
    DWORD thread=GetCurrentThreadId();uintptr_t image{};bool install_attempted{},ready{},bound{},suspended{},locked{},presenting{},replay{};
    a::Context context;a::Binding binding;uint64_t epoch{},frame{},local_sequence{},local_jump{},last_input_sent{};
    uintptr_t presented_actor{};Leases leases;ReplicaState last_frame;
    ::placement_ui_probe::ProcessMemory memory;std::deque<Queued> queue;std::optional<Queued> after_release;std::optional<intervention::Control> sample;
    std::map<std::string,OperationReceipt> receipts;std::string cause;
    std::array<Route,9> routes;
};
std::atomic<Runtime*> published{nullptr};
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
void lock_runtime(Runtime& r,const std::string& cause){if(r.locked)return;r.locked=true;r.cause=cause;LOG_ERROR("[Control] {}",cause);game::live_host_state::economic_fence().journal_failed("Control Unknown: "+cause);}
template<class T>bool at(Runtime& r,uintptr_t pointer,size_t offset,T& value,std::string& error){if(!pointer||pointer>UINTPTR_MAX-offset)return fail(error,"Control native address extent invalid");return r.memory.copy(pointer+offset,&value,sizeof value,error);}
bool ap(Runtime& r,uintptr_t pointer,uintptr_t expected,std::string& error){uintptr_t value{};return at(r,pointer,0,value,error)&&(value==r.image+expected||fail(error,"Control actual native class differs"));}
bool active(Runtime& r){return r.bound&&!r.suspended&&net::session_wire::connected();}
std::string uuid(std::string& error){unsigned char bytes[16];if(BCryptGenRandom(nullptr,bytes,sizeof bytes,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0){error="Control operation UUID generation failed";return {};}
    bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;char text[37];std::snprintf(text,sizeof text,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);return text;}
Intent lease_intent(const a::InterventionIntent& request){const auto& c=*request.control;return {Operation(uint8_t(request.command)-7),c.lease,request.target,c.sequence,c.jump,c.input,c.selected,c.ability,c.prototype};}
bool verify_route(Runtime& r,const Route& route,std::string& error){
    const auto& owner=route.owner;MT2FW_MT2_RouteReceipt current{};current.size=sizeof current;
    if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),current))return fail(error,"Control route receipt failed: "+hooks::framework::process_backend().error());
    if(current.state!=MT2FW_MT2_ROUTE_ENABLED||current.generation!=owner.generation||current.trampoline!=owner.trampoline||current.detour!=owner.detour||std::strcmp(current.owner,"doodesch.multiplayer"))return fail(error,"Control route owner/generation/trampoline changed");
    return game::release_ui_owner::verify(owner,[&](auto address,auto data,auto size,auto& cause){return r.memory.copy(address,data,size,cause);},[](const auto& bytes){return mt2mp::sha256(bytes);},route.sha,error);
}
bool routes(Runtime& r,std::string& error){
    for(const auto& route:r.routes)if(!verify_route(r,route,error))return false;
    struct Pin{uintptr_t rva;size_t bytes;const char* sha;};
    static constexpr Pin helpers[]{
#include "control_helper_pins.inc"
    };
    for(const auto& pin:helpers){std::vector<unsigned char> bytes(pin.bytes);if(!r.memory.copy(r.image+pin.rva,bytes.data(),bytes.size(),error)||mt2mp::sha256(bytes)!=pin.sha)return fail(error,error.empty()?"Control original local helper fingerprint differs RVA="+std::to_string(pin.rva):error);}
    return true;
}
bool current(Runtime& r,const a::Context& context,const a::Binding& binding,std::string& error){
    if(!r.ready||!active(r)||r.locked||GetCurrentThreadId()!=r.thread||r.context!=context||r.binding!=binding)return fail(error,"Control current bound Mainthread/Context/Binding unavailable: "+r.cause);
    a::Context observed;a::Binding actual;if(!a::selected_session().current(observed,error)||observed!=context||!game::typed_action_runtime::network_binding(context.snapshot_uuid,actual,error)||actual!=binding)return fail(error,error.empty()?"Control current native context changed":error);
    return routes(r,error);
}
bool owner(Runtime& r,uintptr_t pointer,Target& target,std::string& error){intervention::Census census;if(!intervention::census(r.context,r.binding,census,error))return false;
    const auto found=std::find_if(census.actors.begin(),census.actors.end(),[&](const auto& row){return row.object==pointer;});if(found==census.actors.end())return fail(error,"Control receiver has no canonical current Actor identity");target=found->target;return true;
}
bool presentation(Runtime& r,uintptr_t actor,PresentationBinding& out,std::string& error){
    uintptr_t player{},input{},game{},camera{},settings{},selection{},bar{};
    if(!at(r,r.image,0x13df9f0,player,error)||!ap(r,player,0x17873b0,error)||!at(r,player,0x1d8,bar,error)||!bar||
       !at(r,r.image,0x13df990,input,error)||!ap(r,input,0x1749430,error)||!at(r,r.image,0x13df8f0,game,error)||!at(r,game,0x90,camera,error)||!ap(r,camera,0x1786c20,error)||
       !at(r,r.image,0x13df1e0,settings,error)||!settings||!at(r,r.image,0x13df630,selection,error)||!selection)return fail(error,error.empty()?"Control local Player/action bar/input/camera/settings/selection unavailable":error);
    Target canonical;if(!owner(r,actor,canonical,error))return false;
    out={r.image,player,actor,input,camera,settings,selection};return true;
}
bool proof(Runtime& r,const Target& actor,intervention::Proof& out,std::string& error){
    NativePort port(r.context,r.binding,true);ActorState actual;if(!port.read(actor,actual,error))return false;out={};out.target=actor;intervention::ControlProof state;
    state.controlled=actual.controlled;state.moving=actual.moving;state.selected=actual.target;
    for(const auto& lease:r.leases.values())if(lease&&lease->actor==actor){state.peer=lease->peer;state.lease=lease->id;state.sequence=lease->input_sequence;state.jump=lease->jump_sequence;}
    out.control=state;return intervention::valid(out)||fail(error,"Control current native state and lease ownership disagree");
}
bool action_proof(Runtime& r,const a::InterventionIntent& intent,intervention::Proof& out,std::string& error){
    if(!proof(r,intent.target,out,error))return false;
    // Motion is acknowledged by each native step and the subsequent complete Frame.
    out.control->moving=false;
    if(intent.command==intervention::Command::ControlAcquire||intent.command==intervention::Command::ControlRelease)out.control->selected.reset();
    return true;
}
bool ledger(Runtime& r,a::Ledger& out,std::string& error){game::live_host_state::NativePort port(r.context,r.binding,nullptr);game::live_host_state::Pin pin;if(!port.read(pin,error))return false;out=game::live_host_state::ledger(pin.values);return true;}
uint32_t original_pair(Pair method,uintptr_t player,uintptr_t actor)noexcept{__try{method(reinterpret_cast<void*>(player),reinterpret_cast<void*>(actor));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool present(Runtime& r,const Target& target,bool acquire,std::string& error){
    NativePort port(r.context,r.binding,true);ActorState actor;if(!port.read(target,actor,error))return false;PresentationBinding local;if(!presentation(r,actor.pointer,local,error))return false;
    uintptr_t before{};if(!at(r,local.player,0x148,before,error)||(acquire?before!=0:before!=actor.pointer))return fail(error,error.empty()?"Control local Player before-pointer differs":error);
    r.presenting=true;r.presented_actor=actor.pointer;
    const auto exception=original_pair(reinterpret_cast<Pair>(r.routes[acquire?1:2].original),local.player,actor.pointer);
    r.presenting=false;r.presented_actor=0;
    uintptr_t after{};if(exception||!at(r,local.player,0x148,after,error)||after!=(acquire?actor.pointer:0))return fail(error,"Control original local presentation failed/readback differs SEH="+std::to_string(exception)+" original="+error);
    return true;
}
bool enqueue(Runtime& r,a::InterventionIntent intent,std::string& error){
    if(r.locked||r.queue.size()>=4)return fail(error,"Control current input queue unavailable");auto id=uuid(error);if(id.empty())return false;
    if(intent.command==intervention::Command::ControlAcquire)intent.control->lease=id;
    if(!intervention::valid(intent))return fail(error,"Control captured intent is invalid");
    r.queue.push_back({id,std::move(intent)});error.clear();return true;
}
void rejected(Runtime& r,const std::string& error){if(r.cause!=error){r.cause=error;LOG_WARN("[Control] {}",error);}}
}
bool verify_item_activate(uintptr_t image,size_t bytes,const std::string& sha,bool& owned,std::string& error){
    owned=false;auto* r=published.load(std::memory_order_acquire);
    if(!r){error.clear();return true;}
    if(!r->ready||GetCurrentThreadId()!=r->thread||r->image!=image)return fail(error,"Control shared ItemActivate owner/image/Mainthread unavailable");
    const auto& route=r->routes[7];
    if(route.rva!=0x207820||route.bytes!=bytes||route.sha!=sha||route.owner.entry!=image+route.rva)
        return fail(error,"Control shared ItemActivate original fingerprint contract differs");
    if(!verify_route(*r,route,error))return false;
    owned=true;error.clear();return true;
}
bool capture_acquire(uintptr_t receiver,std::string& error){auto* r=published.load(std::memory_order_acquire);if(!r||!current(*r,r->context,r->binding,error))return false;
    Target target;if(!owner(*r,receiver,target,error))return false;
    if(!r->queue.empty()||r->after_release)return fail(error,"Control local request already queued");
    a::InterventionIntent intent;intent.target=target;intent.command=intervention::Command::ControlAcquire;intent.control=intervention::Control{};
    if(const auto* lease=r->leases.find(r->context.local_peer)){
        if(lease->actor==target){error.clear();return true;}
        a::InterventionIntent release;release.target=lease->actor;release.command=intervention::Command::ControlRelease;release.control=intervention::Control{};release.control->lease=lease->id;
        if(!enqueue(*r,release,error))return false;auto id=uuid(error);if(id.empty())return false;intent.control->lease=id;r->after_release=Queued{id,std::move(intent)};return true;
    }
    return enqueue(*r,std::move(intent),error);
}
bool pending(){const auto* r=published.load(std::memory_order_acquire);return r&&(!r->queue.empty()||r->after_release.has_value()||r->presenting||r->replay);}
bool replaying(){const auto* r=published.load(std::memory_order_acquire);return r&&GetCurrentThreadId()==r->thread&&r->replay;}
void tick(){auto* r=published.load(std::memory_order_acquire);if(!r||!active(*r)||r->locked)return;std::string error;
    const auto controller=game::typed_action_runtime::controller_status();if(controller.value("locked",true)){rejected(*r,"Control waits for unlocked ordered Controller");return;}if(controller.value("pending",true))return;
    if(r->after_release&&!r->leases.find(r->context.local_peer)&&r->queue.empty()){r->queue.push_back(std::move(*r->after_release));r->after_release.reset();}
    const auto now=GetTickCount64();
    if(r->queue.empty()&&r->sample&&r->leases.find(r->context.local_peer)&&now-r->last_input_sent>=50){
        a::InterventionIntent intent;const auto* lease=r->leases.find(r->context.local_peer);intent.target=lease->actor;intent.command=intervention::Command::ControlInput;intent.control=*r->sample;
        if(r->local_sequence==UINT64_MAX){lock_runtime(*r,"Control local input sequence exhausted");return;}intent.control->sequence=++r->local_sequence;intent.control->jump=std::min(intent.control->jump,lease->jump_sequence+(lease->jump_sequence!=UINT64_MAX));
        if(!enqueue(*r,std::move(intent),error)){rejected(*r,error);return;}r->last_input_sent=now;
    }
    if(r->queue.empty())return;const auto next=r->queue.front();
    if(!current(*r,r->context,r->binding,error)||!game::typed_action_runtime::submit_intervention(next.id,next.intent,error)){rejected(*r,error);r->queue.pop_front();return;}
    r->queue.pop_front();r->cause.clear();
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* host,std::string& error){
    auto* r=published.load(std::memory_order_acquire);if(!r||!current(*r,r->context,binding,error)||request.action!=a::Action::Intervention||!intervention::valid(request.intervention)||!intervention::control_command(request.intervention.command))return a::Outcome::RejectedNoEffect;
    const auto controller=game::typed_action_runtime::controller_status();
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid||request.snapshot_uuid!=r->context.snapshot_uuid||(host!=nullptr)!=(r->context.role==a::Role::Client)||r->receipts.contains(request.action_uuid)){error="Control requires exact pending ordered Controller/role/token";return a::Outcome::RejectedNoEffect;}
    const auto intent=lease_intent(request.intervention);
    if(!r->leases.preflight(request.peer,request.action_uuid,intent,error))return a::Outcome::RejectedNoEffect;
    a::Evidence evidence;intervention::Proof before;
    if(!action_proof(*r,request.intervention,before,error)||!ledger(*r,evidence.before,error))return a::Outcome::RejectedNoEffect;evidence.intervention_before=before;
    if(host&&(!host->intervention_before||!host->intervention_after||*host->intervention_before!=before||host->before!=evidence.before)){error="Control native before-proof/full Ledger differs from Host";return a::Outcome::RejectedNoEffect;}
    if(request.peer==r->context.local_peer&&(intent.operation==Operation::Acquire||intent.operation==Operation::Release)){
        NativePort native(r->context,binding,true);ActorState actor;PresentationBinding local;
        if(!native.read(intent.actor,actor,error)||!presentation(*r,actor.pointer,local,error))return a::Outcome::RejectedNoEffect;
    }
    if(r->receipts.size()>=a::history_limit){auto oldest=r->receipts.end();for(auto it=r->receipts.begin();it!=r->receipts.end();++it)if(it->second.retired&&(oldest==r->receipts.end()||it->second.request.host_revision<oldest->second.request.host_revision))oldest=it;
        if(oldest==r->receipts.end()){error="Control unretired receipt capacity reached";return a::Outcome::RejectedNoEffect;}r->receipts.erase(oldest);}
    auto& operation=r->receipts.emplace(request.action_uuid,OperationReceipt{binding,request,evidence}).first->second;
    const auto unknown=[&](){operation.cause=error;lock_runtime(*r,error);return a::Outcome::Unknown;};
    NativePort native(r->context,binding,true);StepReceipt receipt;bool okay=true;r->replay=true;
    if(intent.operation==Operation::Acquire||intent.operation==Operation::Release)okay=native.controlled(intent.actor,intent.operation==Operation::Acquire,receipt,error);
    else if(intent.operation==Operation::Input){receipt.attempted=true;okay=native.set_target(intent.actor,intent.selected,receipt.exception,error);receipt.confirmed=okay;receipt.unknown=!okay;receipt.cause=error;}
    else okay=native.ability(intent.actor,intent.ability,intent.prototype,intent.operation==Operation::Activate,receipt,error);
    r->replay=false;
    if(!okay){if(receipt.attempted)return unknown();r->receipts.erase(request.action_uuid);return a::Outcome::RejectedNoEffect;}
    if(intent.operation==Operation::Acquire&&r->context.role==a::Role::Host){const auto report=game::time_integration::instance().submit(game::time_session::Intent::Normal);if(!report.okay()){error="Control original normal-time request failed: "+report.cause;return unknown();}}
    if(!r->leases.commit(request.peer,request.action_uuid,intent,GetTickCount64(),error))return unknown();
    if(request.peer==r->context.local_peer&&(intent.operation==Operation::Acquire||intent.operation==Operation::Release)){
        if(!present(*r,intent.actor,intent.operation==Operation::Acquire,error))return unknown();r->sample.reset();r->local_sequence=0;r->local_jump=0;
    }
    intervention::Proof after;if(!action_proof(*r,request.intervention,after,error)||!ledger(*r,operation.evidence.after,error))return unknown();operation.evidence.intervention_after=after;
    if(!intervention::effect(request.intervention,before,after)||(host&&(*host->intervention_after!=after||host->after!=operation.evidence.after))){error="Control original replica native proof/full Ledger differs from Host";return unknown();}
    operation.confirmed=true;error.clear();return a::Outcome::Confirmed;
}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& error){
    auto* r=published.load(std::memory_order_acquire);if(!r||!current(*r,r->context,binding,error))return false;const auto found=r->receipts.find(request.action_uuid);
    if(found==r->receipts.end()||!found->second.confirmed||found->second.binding!=binding||found->second.request!=request)return fail(error,"Control exact confirmed ordered operation receipt absent");
    intervention::Proof actual;if(!proof(*r,request.intervention.target,actual,error))return false;
    const auto& retained=*found->second.evidence.intervention_after->control;const auto& current=*actual.control;
    if(retained.peer!=current.peer||retained.lease!=current.lease||retained.sequence!=current.sequence||retained.jump!=current.jump||retained.controlled!=current.controlled)return fail(error,"Control ordered lease receipt changed before retirement");
    out=found->second.evidence;out.retirement_confirmed=found->second.retired;error.clear();return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence actual;if(!observe(binding,request,actual,error))return false;published.load()->receipts.at(request.action_uuid).retired=true;error.clear();return true;}
#include "control_hooks.inc"
}
