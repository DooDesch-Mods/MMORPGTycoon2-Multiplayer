#include "intervention_actor_live.h"
#include "management_runtime.h"
#include "native_open_runtime.h"
#include "root_saved_building.h"
#include "release_ui_owner.h"
#include "typed_action_runtime.h"
#include "session_provider.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/hooks/minhook_backend.h"
#include "live_host_state/economic_fence.h"
#include <Windows.h>
#include <intrin.h>
#include <cstring>

namespace mt2mp::game::interventions::actors::live {
namespace {
namespace a=action_session;namespace life=lifecycle;
using Update=void(*)(uintptr_t,float);using Void=void(*)(uintptr_t);
struct Route {uintptr_t rva;size_t size,stolen;const char* sha;uintptr_t original=0;release_ui_owner::Owner owner;};
struct Runtime {
    DWORD thread=GetCurrentThreadId();uintptr_t image=uintptr_t(GetModuleHandleW(nullptr));
    bool attempted=false,enabled=false,bound=false,suspended=true,locked=false,replaying=false,ordered=false;
    a::Context context;a::Binding binding;uint64_t epoch=0;uintptr_t root=0;life::journal::Origin origin;std::optional<life::journal::Origin> source;
    life::Host host;life::Client client;Locals locals;std::map<uintptr_t,std::string> pointers;
    std::string cause,action,native_snapshot;uint32_t ordinal=0;
    std::map<std::string,std::vector<life::Event>> local_events;size_t receipt_bytes=0;
    uint64_t observed=0,replayed=0,deduplicated=0,presented=0;
    std::array<Route,8> routes{{
        {0x3c8d50,816,6,"7c523b7c20db0d6c39ec594be62865fd48ef1b6891e4956ac692e4c2b019ca12"},
        {0x3c95c0,912,7,"d7b920d1da040ff56a1d59be8362d1e428515e557696c56f56d066e3fea90115"},
        {0x397a50,528,6,"76845116a6bc652111fef8ff1ba2f9129763b55d69ca14e2dd840438a5bee820"},
        {0x399660,608,7,"4e2d4a2aba643c4f8cde3a26c68a31affdb5332d6d913da7e5437b4325c2d1d2"},
        {0x239ba0,592,5,"2613bc5dc2eb38aac1903b8951115b0f054123fd65a602f650cfa81ab69695e1"},
        {0x39f920,208,7,"4df09753bf3f4b1bfe553506fbcd724d1736a9a10a0f9b2773297890ff393b48"},
        {0x39d560,96,6,"6b2cfd2d932839ba6f9ea33deeb8a73f232616b7a6b11fe155c3592103037896"},
        {0x39e760,496,7,"3c6b2d2f40bfb03057953423834024adcd5a098857a34ab786ba79584ca9f822"}}};
} state;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool reject(std::string& error,const std::string& cause){
    if(!state.locked){state.locked=true;state.cause=cause;live_host_state::economic_fence().journal_failed(cause);LOG_ERROR("[Actor24] {}",cause);}error=state.cause;return false;
}
template<class T>bool at(uintptr_t object,size_t offset,T& value,std::string& error){return root_saved_building::bytes(reinterpret_cast<void*>(object),offset,&value,sizeof value,error);}
bool route_current(Route& route,std::string& error,bool full){
    const auto& owner=route.owner;MT2FW_MT2_RouteReceipt actual{};actual.size=sizeof actual;
    if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),actual))return fail(error,"Actor24 named route read failed original="+hooks::framework::process_backend().error());
    if(actual.state!=MT2FW_MT2_ROUTE_ENABLED||actual.generation!=owner.generation||actual.entry!=owner.entry||actual.trampoline!=owner.trampoline||actual.detour!=owner.detour||std::strcmp(actual.owner,"doodesch.multiplayer"))return fail(error,"Actor24 named route owner/generation/trampoline changed");
    if(!full)return true;
    return release_ui_owner::verify(owner,[](auto address,auto out,auto bytes,auto& cause){return root_saved_building::bytes(reinterpret_cast<void*>(address),0,out,bytes,cause);},[](const auto& bytes){return mt2mp::sha256(bytes);},route.sha,error)&&hooks::framework::process_backend().verify_code(owner.entry,owner.pristine.data(),owner.pristine.size(),error);
}
bool routes(std::string& error){for(auto& route:state.routes)if(!route_current(route,error,true))return false;return true;}
bool world(std::string& error){
    if(!state.bound||state.suspended||state.locked||GetCurrentThreadId()!=state.thread)return fail(error,state.cause.empty()?"Actor24 callback is suspended, unbound or outside Mainthread":state.cause);
    a::Context actual;if(!a::selected_session().current(actual,error)||actual!=state.context)return fail(error,error.empty()?"Actor24 callback actual Session differs":error);
    building_mixed_identity::opened_file::CurrentWorld current;if(!native_open_runtime::read_current_world(current,error))return false;
    if(current.uuid!=state.native_snapshot||current.world_epoch!=state.binding.native_epoch||current.generation!=state.binding.native_generation)
        return fail(error,"Actor24 callback actual NativeOpen epoch/generation/snapshot differs: session_snapshot="+state.context.snapshot_uuid+
            " native_snapshot="+current.uuid+" bound_native_snapshot="+state.native_snapshot+
            " native_epoch="+std::to_string(current.world_epoch)+" binding_epoch="+std::to_string(state.binding.native_epoch)+
            " native_generation="+std::to_string(current.generation)+" binding_generation="+std::to_string(state.binding.native_generation));
    if(state.root!=uintptr_t(current.map)+0x1c0)return fail(error,"Actor24 callback cached current Root differs from actual NativeOpen");
    if(state.context.role==a::Role::Client){mt2mp::game::native::ClockState clock;if(!mt2mp::game::native::read_clock(clock,error)||clock.time_scale!=0)return fail(error,error.empty()?"Actor24 Client actual Clock is not zero":error);}
    return true;
}
bool install_cache(Locals values,std::string& error){
    std::map<uintptr_t,std::string> pointers;for(const auto& [key,local]:values)if(local.scope.root!=state.root||!pointers.emplace(local.object,key).second)return fail(error,"Actor24 confirmed current Root differs or pointer aliases two canonical actors");state.locals=std::move(values);state.pointers=std::move(pointers);return true;
}
bool owner(uintptr_t pointer,Local*& local,std::string& error){
    local=nullptr;const auto found=state.pointers.find(pointer);if(found==state.pointers.end())return true;
    if(!world(error))return false;const auto actual=state.locals.find(found->second);if(actual==state.locals.end()||actual->second.object!=pointer)return fail(error,"Actor24 point pointer/current canonical owner differs");
    uintptr_t registered=0;const auto code=native::registered(state.image,actual->second.uid,registered);if(code||registered!=pointer)return fail(error,"Actor24 point cached full UID no longer identifies current object SEH="+std::to_string(code));local=&actual->second;return true;
}
bool present(Local& local,std::string& error){
    struct Pin{uintptr_t character=0,actor=0,skeleton=0,root=0,region=0;uint64_t uid=0;subscriber_motion::Pose pose;uint8_t dead=2,on_map=2;bool operator==(const Pin&)const=default;};
    const auto read=[&](Pin& pin){return at(local.object,0x18,pin.uid,error)&&pin.uid==local.uid&&at(local.object,0x170,pin.root,error)&&pin.root==local.scope.shard&&at(local.object,0x178,pin.region,error)&&pin.region==local.scope.region&&at(local.object,0x6f0,pin.character,error)&&pin.character==local.render.character&&at(pin.character,0,pin.actor,error)&&pin.actor==local.render.actor&&at(pin.actor,0x40,pin.skeleton,error)&&pin.skeleton==local.skeleton&&at(local.object,0x100,pin.pose.orientation,error)&&at(local.object,0x110,pin.pose.position,error)&&at(local.object,0x11c,pin.pose.scale,error)&&at(local.object,0x4ac,pin.dead,error)&&pin.dead<=1&&at(local.object,0x368,pin.on_map,error)&&pin.on_map<=1;};
    Pin before,after;if(!read(before))return fail(error,error.empty()?"Actor24 local presentation current render/full UID/owner differs":error);
    const auto code=native::present(state.image,local.target.family,local.object,local.render);if(code)return fail(error,"Actor24 original presentation leaf failed SEH="+std::to_string(code));
    uintptr_t registered=0;const auto lookup=native::registered(state.image,local.uid,registered);uint8_t visible=2,distance=2;
    if(!read(after)||after!=before||lookup||registered!=local.object||!at(local.skeleton,0xe4,visible,error)||visible>1||!at(local.object,0x808,distance,error)||distance>1||bool(visible)!=(bool(before.on_map)&&bool(distance)))return fail(error,error.empty()?"Actor24 renderer-only presentation changed current pose/owner/map or visible readback differs":error);++state.presented;return true;
}
uint32_t call_void(uintptr_t function,uintptr_t receiver,bool& returned)noexcept{
    returned=false;__try{reinterpret_cast<Void>(function)(receiver);returned=true;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t expire_general_proxy(uintptr_t image,uintptr_t receiver,uintptr_t expected,bool& returned)noexcept{
    returned=false;__try{if(*reinterpret_cast<uintptr_t*>(receiver+0x28)!=expected)return ERROR_INVALID_DATA;
        if(expected)reinterpret_cast<Void>(image+0xbfa730)(expected);
        *reinterpret_cast<uintptr_t*>(receiver+0x28)=0;returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
void lifecycle_entry(size_t index,uintptr_t pointer,uintptr_t caller){
    auto& route=state.routes[index];if(!route.original)throw std::runtime_error("Actor24 lifecycle original route unavailable");
    if(!state.bound||state.suspended||state.replaying){reinterpret_cast<Void>(route.original)(pointer);return;}
    std::string error;Local* local=nullptr;
    try{
        if(!owner(pointer,local,error)){reject(error,error);return;}if(!local){reinterpret_cast<Void>(route.original)(pointer);return;}
        if(!route_current(route,error,false)){reject(error,error);return;}
        const auto effect=index==5?life::Effect::OnSpawn:index==6?life::Effect::ClearLivePointers:life::Effect::CharacterDie;life::Source source;
        if(caller<state.image||!life::source_for(local->target.family,effect,caller-state.image,source)){reject(error,"Actor24 owned lifecycle actual native caller is unsupported RVA="+std::to_string(caller>=state.image?caller-state.image:0));return;}
        const bool ordered=life::ordered_receipt(state.ordered,management_runtime::intervention_replaying(),source);
        if(state.context.role==a::Role::Client&&!ordered){reject(error,"Actor24 Client attempted a free-running native lifecycle without its Host invocation witness");return;}
        if(state.ordinal==UINT32_MAX&&ordered){reject(error,"Actor24 ordered native lifecycle ordinal exhausted");return;}
        const auto target=local->target;const auto uid=local->uid;uintptr_t proxy=1;
        if(source==life::Source::BossWait&&(!at(pointer,0x28,proxy,error)||proxy)){reject(error,error.empty()?"Actor24 Boss wait actual proxy expiry did not precede OnSpawn":error);return;}
        bool returned=false;const auto exception=call_void(route.original,pointer,returned);
        if(exception||!returned){reject(error,"Actor24 actual native lifecycle failed SEH="+std::to_string(exception)+"; no replay");return;}
        uintptr_t registered=0;const auto lookup=native::registered(state.image,uid,registered);if(lookup||registered!=pointer){reject(error,"Actor24 actual lifecycle returned with different full UID registration SEH="+std::to_string(lookup));return;}
        life::Event event{target,effect,source,ordered?state.action:std::string{},ordered?++state.ordinal:0};
        if(!life::valid(event)){reject(error,"Actor24 original returned but its canonical lifecycle receipt is invalid");return;}
        if(state.context.role==a::Role::Host){if(!life::append(state.host,state.origin,event,error)){reject(error,error);return;}}
        else{const auto bytes=life::encode(event).dump().size();if(bytes>live_host_state::max_logical_frame_payload-state.receipt_bytes){reject(error,"Actor24 unconsumed ordered native invocation receipts exceed existing Frame budget");return;}state.local_events[event.action].push_back(event);state.receipt_bytes+=bytes;}
        ++state.observed;
    }catch(const std::exception& exception){reject(error,std::string("Actor24 lifecycle observer exception: ")+exception.what());}
    catch(...){reject(error,"Actor24 lifecycle observer nonstandard exception");}
}
void on_spawn(uintptr_t pointer){lifecycle_entry(5,pointer,uintptr_t(_ReturnAddress()));}
void clear_pointers(uintptr_t pointer){lifecycle_entry(6,pointer,uintptr_t(_ReturnAddress()));}
void character_die(uintptr_t pointer){lifecycle_entry(7,pointer,uintptr_t(_ReturnAddress()));}
void update_entry(size_t index,uintptr_t pointer,float dt){
    auto& route=state.routes[index];if(!route.original)throw std::runtime_error("Actor24 update original route unavailable");
    if(!state.bound||state.suspended||state.context.role==a::Role::Host||management_runtime::intervention_replaying()){reinterpret_cast<Update>(route.original)(pointer,dt);return;}
    std::string error;try{Local* local=nullptr;if(!owner(pointer,local,error)){reject(error,error);return;}if(!local){reinterpret_cast<Update>(route.original)(pointer,dt);return;}
        if(!route_current(route,error,false)){reject(error,error);return;}
        if(index==0||index==2){if(!present(*local,error))reject(error,error);}
        // The owned Tick has no Client gameplay work. The Entity/manager paths
        // retain the original local renderer while Host state owns simulation.
    }catch(const std::exception& exception){reject(error,std::string("Actor24 update disposition exception: ")+exception.what());}
    catch(...){reject(error,"Actor24 update disposition nonstandard exception");}
}
void npc_update(uintptr_t pointer,float dt){update_entry(0,pointer,dt);}void npc_tick(uintptr_t pointer,float dt){update_entry(1,pointer,dt);}
void boss_update(uintptr_t pointer,float dt){update_entry(2,pointer,dt);}void boss_tick(uintptr_t pointer,float dt){update_entry(3,pointer,dt);}
void manager_update(uintptr_t pointer,float dt){
    auto& route=state.routes[4];if(!route.original)throw std::runtime_error("Actor24 BossManager original route unavailable");
    if(!state.bound||state.suspended||state.context.role!=a::Role::Client){reinterpret_cast<Update>(route.original)(pointer,dt);return;}std::string error;
    try{if(!world(error)||!route_current(route,error,false)){reject(error,error);return;}reinterpret_cast<Update>(route.original)(pointer,dt);
        for(auto& [key,local]:state.locals)if(local.target.family==Family::Boss&&local.scope.manager==pointer){if(!present(local,error)){reject(error,error);return;}}
    }catch(const std::exception& exception){reject(error,std::string("Actor24 BossManager presentation exception: ")+exception.what());}
    catch(...){reject(error,"Actor24 BossManager presentation nonstandard exception");}
}
bool exact(const a::Context& context,const a::Binding& binding,std::string& error){return context==state.context&&binding==state.binding&&world(error)||fail(error,error.empty()?"Actor24 exact bound Context/Binding differs":error);}
life::journal::Origin frame_origin(const live_host_state::Frame& frame){return {frame.session,frame.epoch,frame.source_generation,frame.snapshot};}
bool frame_bound(const live_host_state::Frame& frame,std::string& error){
    if(!world(error)||!frame.management||!frame.management->intervention_actors||!frame.management->actor_lifecycle)return fail(error,error.empty()?"Actor24 complete authoritative Frame component absent":error);
    const auto origin=frame_origin(frame);if(origin.session!=state.context.session||origin.snapshot!=state.context.snapshot_uuid||origin.epoch!=state.epoch||!live_host_state::vip::valid_origin(origin))return fail(error,"Actor24 Frame bound session/snapshot/common epoch differs");
    if(state.context.role==a::Role::Host)return origin==state.origin||fail(error,"Actor24 Host Frame source generation differs");
    if(state.source)return *state.source==origin||fail(error,"Actor24 Client Host source generation changed without rebind");
    if(!state.client.bind(origin,error))return false;state.source=origin;return true;
}
}
bool enable(std::string& error){
    if(state.enabled)return routes(error);if(state.attempted)return fail(error,state.cause.empty()?"Actor24 named route initialization was already attempted":state.cause);state.attempted=true;state.thread=GetCurrentThreadId();state.image=uintptr_t(GetModuleHandleW(nullptr));
    const uintptr_t entries[]={uintptr_t(&npc_update),uintptr_t(&npc_tick),uintptr_t(&boss_update),uintptr_t(&boss_tick),uintptr_t(&manager_update),uintptr_t(&on_spawn),uintptr_t(&clear_pointers),uintptr_t(&character_die)};
    const auto failed=[&](){state.cause=error;return false;};
    std::vector<unsigned char> detach(160);if(!root_saved_building::bytes(reinterpret_cast<void*>(state.image+0xbfa730),0,detach.data(),detach.size(),error))return failed();
    if(mt2mp::sha256(detach)!="f55694a3d019d6fec953667a2db29ef47d1c9413bbe721c879d3a4141ca6c684"){error="Actor24 original Boss proxy Detach complete body differs";return failed();}
    for(size_t n=0;n<state.routes.size();++n){auto& route=state.routes[n];auto& owner=route.owner;owner.entry=state.image+route.rva;owner.detour=entries[n];owner.stolen=route.stolen;owner.pristine.resize(route.size);
        if(!root_saved_building::bytes(reinterpret_cast<void*>(owner.entry),0,owner.pristine.data(),owner.pristine.size(),error))return failed();
        if(mt2mp::sha256(owner.pristine)!=route.sha){error="Actor24 original complete named route body differs RVA="+std::to_string(route.rva);return failed();}
        const auto prepared=MH_CreateHook(reinterpret_cast<void*>(owner.entry),reinterpret_cast<void*>(entries[n]),reinterpret_cast<void**>(&route.original));
        if(prepared!=MH_OK||!route.original){error="Actor24 named route prepare failed status="+std::to_string(prepared)+" original="+hooks::framework::process_backend().error();return failed();}owner.trampoline=route.original;
        const auto enabled=MH_EnableHook(reinterpret_cast<void*>(owner.entry));if(enabled!=MH_OK){error="Actor24 named route enable failed status="+std::to_string(enabled)+" original="+hooks::framework::process_backend().error();return failed();}
        MT2FW_MT2_RouteReceipt actual{};actual.size=sizeof actual;if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),actual)){error="Actor24 named route receipt failed original="+hooks::framework::process_backend().error();return failed();}owner.generation=actual.generation;
    }
    if(!routes(error))return failed();state.enabled=true;error.clear();return true;
}
bool bind(const a::Context& context,const a::Binding& binding,uint64_t epoch,std::string& error){
    if(!state.enabled||state.locked||state.ordered||state.replaying||GetCurrentThreadId()!=state.thread||!epoch)return fail(error,state.cause.empty()?"Actor24 bind requires enabled confirmed idle Mainthread owner":state.cause);
    if(state.bound&&!state.suspended)return fail(error,"Actor24 bind cannot replace an active current world");
    if(context.local_peer<1||context.local_peer>4||(context.peer_mask&~15u)||!(context.peer_mask&(1u<<(context.local_peer-1))))return fail(error,"Actor24 actual local peer/membership invalid");
    State values;Locals locals;building_mixed_identity::opened_file::CurrentWorld world;
    if(!routes(error)||!read_current(context,binding,values,locals,error)||!native_open_runtime::read_current_world(world,error))return false;
    const life::journal::Origin origin{context.session,epoch,binding.native_generation,context.snapshot_uuid};life::Host host;
    if(!live_host_state::vip::valid_origin(origin)||context.role==a::Role::Host&&(context.local_peer!=context.host_peer||!host.bind(origin,uint32_t(context.peer_mask&~(1u<<(context.local_peer-1))),error)))return fail(error,error.empty()?"Actor24 actual Host origin/membership invalid":error);
    if(world.uuid.empty()||world.world_epoch!=binding.native_epoch||world.generation!=binding.native_generation)
        return fail(error,"Actor24 bind actual NativeOpen differs: session_snapshot="+context.snapshot_uuid+" native_snapshot="+world.uuid+
            " native_epoch="+std::to_string(world.world_epoch)+" binding_epoch="+std::to_string(binding.native_epoch)+
            " native_generation="+std::to_string(world.generation)+" binding_generation="+std::to_string(binding.native_generation));
    state.context=context;state.binding=binding;state.epoch=epoch;state.origin=origin;state.root=uintptr_t(world.map)+0x1c0;state.native_snapshot=world.uuid;
    if(!install_cache(std::move(locals),error))return false;
    state.host=std::move(host);state.client={};state.source.reset();state.local_events.clear();state.receipt_bytes=0;state.action.clear();state.ordinal=0;
    state.bound=true;state.suspended=false;error.clear();return true;
}
bool suspend(std::string& error){
    if(GetCurrentThreadId()!=state.thread||state.ordered||state.replaying)return fail(error,"Actor24 suspend requires idle bound Mainthread");state.suspended=true;
    if(state.locked)return fail(error,state.cause);error.clear();return true;
}
bool capture(const a::Context& context,const a::Binding& binding,State& values,life::Batch& journal,std::string& error){
    if(context.role!=a::Role::Host||!exact(context,binding,error)||!routes(error))return fail(error,error.empty()?"Actor24 capture requires actual bound Host":error);
    Locals locals;if(!read_current(context,binding,values,locals,error))return false;
    for(const auto& row:state.host.pending().rows){const auto event=life::event(row);if(!event.action.empty())continue;const auto before=state.locals.find(event.target.key),after=locals.find(event.target.key);
        if(before==state.locals.end()||after==locals.end()||before->second.object!=after->second.object||before->second.uid!=after->second.uid||before->second.target!=after->second.target)return fail(error,"Actor24 pending lifecycle canonical actor retired or changed before its required Frame delivery key="+event.target.key);}
    if(!install_cache(std::move(locals),error))return false;journal=state.host.pending();return life::valid(journal,error);
}
bool begin_ordered(const a::Context& context,const a::Binding& binding,const std::string& action,std::string& error){
    if(!exact(context,binding,error)||!routes(error)||state.ordered||state.replaying||!monster_target_key::uuid(action))return fail(error,error.empty()?"Actor24 ordered original scope is absent, nested or invalid":error);
    const auto controller=typed_action_runtime::controller_status();if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=action)return fail(error,"Actor24 ordered original scope differs from actual pending Controller token");
    State values;Locals locals;if(!read_current(context,binding,values,locals,error)||!install_cache(std::move(locals),error))return false;
    if(state.local_events.contains(action))return fail(error,"Actor24 original token already has unconsumed native invocation receipts; no replay");
    state.ordered=true;state.action=action;state.ordinal=0;error.clear();return true;
}
bool end_ordered(bool confirmed,std::string& error){
    if(!state.ordered||GetCurrentThreadId()!=state.thread)return fail(error,"Actor24 ordered original end lacks its active Mainthread receipt");const auto count=state.ordinal;state.ordered=false;state.action.clear();state.ordinal=0;
    if(state.locked)return fail(error,state.cause);if(!confirmed&&count)return reject(error,"Actor24 original action failed after actual lifecycle invocations; no replay");error.clear();return true;
}
bool sent(const live_host_state::Frame& frame,std::string& error){return state.context.role==a::Role::Host&&frame_bound(frame,error)&&state.host.sent(state.origin,frame.revision,*frame.management->actor_lifecycle,error)||fail(error,error.empty()?"Actor24 sent Frame requires exact Host receipt":error);}
bool acknowledge(uint64_t peer,uint64_t revision,std::string& error){
    if(state.context.role!=a::Role::Host||peer>4||!world(error))return fail(error,error.empty()?"Actor24 ACK requires actual bound Host/peer":error);return state.host.acknowledge(state.origin,uint32_t(peer),revision,error);
}
bool admit(const live_host_state::Frame& frame,const live_host_state::Frame* prior,std::string& error){
    if(state.context.role!=a::Role::Client||!frame_bound(frame,error))return fail(error,error.empty()?"Actor24 admission requires actual Client":error);
    if(prior&&(!prior->management||!prior->management->actor_lifecycle||frame_origin(*prior)!=frame_origin(frame)))return fail(error,"Actor24 queued Frame lifecycle origin/component differs");
    return state.client.supersede(*state.source,*frame.management->actor_lifecycle,prior?&*prior->management->actor_lifecycle:nullptr,error);
}
bool apply(const a::Context& context,const a::Binding& binding,const live_host_state::Frame& frame,Receipt& receipt,std::string& error){
    receipt={};const auto failed=[&](){receipt.cause=error;receipt.unknown=receipt.attempted;if(receipt.unknown)reject(error,error);return false;};
    try{
    if(context.role!=a::Role::Client||!exact(context,binding,error)||!frame_bound(frame,error)||!routes(error)||state.ordered)return fail(error,error.empty()?"Actor24 apply requires complete bound Client Frame outside original action":error);
    const auto& journal=*frame.management->actor_lifecycle;std::vector<life::journal::Row> work;if(!life::valid(journal,error)||!state.client.prepare(*state.source,journal,work,error))return false;
    State before;Locals locals;if(!read_current(context,binding,before,locals,error)||!install_cache(std::move(locals),error))return false;
    std::vector<life::Event> consumed;
    for(const auto& row:work){const auto event=life::event(row);
        if(!event.action.empty()){
            const auto found=state.local_events.find(event.action);if(found==state.local_events.end()||event.ordinal>found->second.size()||found->second[size_t(event.ordinal-1)]!=event){error="Actor24 ordered lifecycle lacks its exact actual local original invocation receipt";return failed();}consumed.push_back(event);++state.deduplicated;continue;
        }
        const auto found=state.locals.find(event.target.key);if(found==state.locals.end()||found->second.target!=event.target){error="Actor24 Host lifecycle canonical current owner is absent";return failed();}
        Row actual;Local physical;if(!read_bound(found->second,actual,physical,error)||physical!=found->second)return failed();
        if(event.source==life::Source::BossWait){std::vector<unsigned char> body(160);if(!root_saved_building::bytes(reinterpret_cast<void*>(state.image+0xbfa730),0,body.data(),body.size(),error))return failed();if(mt2mp::sha256(body)!="f55694a3d019d6fec953667a2db29ef47d1c9413bbe721c879d3a4141ca6c684"){error="Actor24 witnessed Boss proxy Detach original body changed before replay";return failed();}uintptr_t proxy=0;if(!at(physical.object,0x28,proxy,error))return failed();receipt.attempted=true;++receipt.attempted_stores;bool returned=false;const auto code=expire_general_proxy(state.image,physical.object,proxy,returned);if(code||!returned){error="Actor24 witnessed Boss wait original proxy expiry failed SEH="+std::to_string(code)+"; no replay";return failed();}++receipt.stores;}
        const auto index=event.effect==life::Effect::OnSpawn?5:event.effect==life::Effect::ClearLivePointers?6:7;if(!route_current(state.routes[index],error,true))return failed();bool returned=false;receipt.attempted=true;++receipt.attempted_stores;state.replaying=true;const auto code=call_void(state.routes[index].original,physical.object,returned);state.replaying=false;
        if(code||!returned){error="Actor24 witnessed original lifecycle failed SEH="+std::to_string(code)+"; no replay";return failed();}++receipt.stores;
        if(!world(error)||!read_bound(found->second,actual,physical,error)||physical!=found->second)return failed();++state.replayed;
    }
    Receipt adoption;const auto adopted=adopt_current(context,binding,*frame.management->intervention_actors,adoption,error);receipt.attempted|=adoption.attempted;receipt.attempted_stores+=adoption.attempted_stores;receipt.stores+=adoption.stores;if(!adopted)return failed();
    State after;if(!read_current(context,binding,after,locals,error))return failed();if(after!=*frame.management->intervention_actors){error="Actor24 complete current Actor values differ after final Host adoption";return failed();}if(!install_cache(std::move(locals),error))return failed();
    if(!state.client.confirm(*state.source,journal,work,error))return failed();
    for(const auto& event:consumed){auto found=state.local_events.find(event.action);if(found==state.local_events.end())continue;
        // The complete native action receipts remain until its last observed
        // ordinal is covered by the confirmed authoritative Frame.
        if(event.ordinal==found->second.size()){for(const auto& value:found->second)state.receipt_bytes-=life::encode(value).dump().size();state.local_events.erase(found);}}
    receipt.confirmed=true;error.clear();return true;
    }catch(const std::exception& exception){error=std::string("Actor24 authoritative lifecycle/adoption exception: ")+exception.what();return failed();}
    catch(...){error="Actor24 authoritative lifecycle/adoption nonstandard exception";return failed();}
}
bool monster_update(uintptr_t pointer,float,bool& handled,std::string& error){
    handled=false;if(!state.bound||state.suspended||state.context.role!=a::Role::Client||management_runtime::intervention_replaying())return true;
    Local* local=nullptr;if(!owner(pointer,local,error)){handled=state.pointers.contains(pointer);return reject(error,error);}if(!local||local->target.family!=Family::Monster)return true;handled=true;return present(*local,error)||reject(error,error);
}
bool adjustment(uintptr_t pointer,bool& handled,std::string& error){
    handled=false;if(!state.bound||state.suspended||state.context.role!=a::Role::Client||management_runtime::intervention_replaying())return true;
    Local* local=nullptr;if(!owner(pointer,local,error)){handled=state.pointers.contains(pointer);return reject(error,error);}handled=local!=nullptr;return true;
}
bool verify_observer(uintptr_t rva,const char* sha,bool& handled,std::string& error){
    handled=false;for(auto& route:state.routes)if(route.rva==rva){if(!route.original)return true;handled=true;if(!sha||std::strcmp(sha,route.sha))return fail(error,"Actor24 shared original observer pin disagrees with existing owner");return route_current(route,error,true);}return true;
}
nlohmann::json status(){return {{"enabled",state.enabled},{"bound",state.bound},{"suspended",state.suspended},{"locked",state.locked},{"cause",state.cause},{"observed",state.observed},{"replayed",state.replayed},{"deduplicated",state.deduplicated},{"presented",state.presented},{"cached_actors",state.locals.size()},{"pending",life::encode(state.host.pending())},{"committed",state.client.committed()},{"unconsumed_original_receipt_bytes",state.receipt_bytes}};}
}
