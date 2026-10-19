#include "dungeon_instance_native.h"
#include "dungeon_instance_native_guard.h"
#include "dungeon_instance_native_codes.h"
#include "dungeon_scene_identity_runtime.h"
#include "native_open_runtime.h"
#include "session_provider.h"
#include "typed_action_runtime.h"
#include "subscriber_lifecycle/client_world.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/hooks/minhook_backend.h"
#include <Windows.h>
#include <intrin.h>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>

namespace mt2mp::game::dungeon_instance_native {
namespace {
namespace life=dungeon_instance_lifecycle;
constexpr const char* executable="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
struct Memory final:subscriber_lifecycle::Memory {
    bool read(uintptr_t p,void* out,size_t bytes,std::string& error)override{
        if(!p||bytes>UINTPTR_MAX-p)return life::fail(error,"native read span invalid");
        SIZE_T got=0;if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(p),out,bytes,&got)||got!=bytes)
            return life::fail(error,"native read failed error="+std::to_string(GetLastError()));return true;
    }
    template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& error){
        if(!p||offset>UINTPTR_MAX-p)return life::fail(error,"native field span invalid");return read(p+offset,&out,sizeof out,error);
    }
} memory;
uintptr_t image=0;uint32_t mainthread=0;
struct Owner {life::Pin pin;uint64_t dungeon_uid=0;};
struct Bound {
    action_session::Context context;action_session::Binding binding;life::journal::Origin origin;
    dungeon_scene_identity_runtime::World world;std::vector<Owner> owners;
};
struct Observation {
    uint64_t sequence=0;life::Effect effect=life::Effect::Make;guard::Pool before,after;
    uintptr_t target=0,result=0;std::string operation,key;uint32_t ordinal=0;bool action=false;
};
std::mutex gate;
std::shared_ptr<const Bound> bound;
std::vector<Observation> pending;
struct ActionReceipt {std::vector<life::Event> events;std::vector<Owner> before;};
std::map<std::string,ActionReceipt> action_receipts;
std::string failure;
uint64_t sequence=0;
std::atomic<uint32_t> inflight=0;
thread_local unsigned structural_depth=0,replay_depth=0;
struct Action {std::string operation;uint32_t ordinal=0;std::vector<Owner> owners;};
thread_local Action action;
struct ReplayMake {uintptr_t dungeon=0,manager=0,active_shard=0,displayed_shard=0;bool dip_suppressed=false;};
thread_local ReplayMake* replay_make=nullptr;
using Make=uintptr_t(*)(uintptr_t);
using Unary=void(*)(uintptr_t);
using Destroy=void(*)(uintptr_t,uintptr_t);
using Update=void(*)(uintptr_t,float);
using Dip=void(*)(uintptr_t,uintptr_t,const void*,const void*,float);
Make original_make=nullptr;Destroy original_destroy=nullptr;
Unary original_reset=nullptr,original_force=nullptr,original_process=nullptr,original_kick=nullptr;
Update original_desired=nullptr,original_update=nullptr;Dip original_dip=nullptr;
struct Route {uintptr_t rva=0,detour=0;void** original=nullptr;size_t bytes=0;std::string sha;
    std::vector<unsigned char> pristine;std::array<unsigned char,16> installed{};std::array<unsigned char,64> relocated{};};
std::vector<Route> routes;
bool attempted=false,installed=false;std::mutex installation;

void fault_locked(const std::string& cause){
    if(failure.empty()){failure=cause.empty()?"Dungeon Instance native operation failed without cause":cause;
        LOG_ERROR("Dungeon Instance native: {}",failure);}
}
void fault(const std::string& cause){std::lock_guard lock(gate);fault_locked(cause);}
std::shared_ptr<const Bound> binding(){std::lock_guard lock(gate);return bound;}
bool same_world(const dungeon_scene_identity_runtime::World& a,const building_mixed_identity::opened_file::CurrentWorld& b){
    return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&
        a.uuid==b.uuid&&a.sha256==b.sha256&&a.world_epoch==b.world_epoch&&a.generation==b.generation;
}
bool fresh(const Bound& b,std::string& error){
    if(GetCurrentThreadId()!=mainthread)return life::fail(error,"native control requires Mainthread");
    if(!healthy(error))return false;
    action_session::Context context;action_session::Binding actual;building_mixed_identity::opened_file::CurrentWorld world;
    uintptr_t base=0;uint32_t thread=0;std::string sha;
    if(!action_session::selected_session().current(context,error)||!typed_action_runtime::read_binding(b.context.snapshot_uuid,actual,error)||
       !native_open_runtime::read_current_world(world,error)||!native::read_resolver_receipt(base,thread,sha,error))return false;
    if(context!=b.context||actual!=b.binding||!same_world(b.world,world)||base!=image||thread!=mainthread||sha!=executable)
        return life::fail(error,"fresh actual Session/Frame/world/image owner differs");
    return true;
}
bool typed(uintptr_t p,uintptr_t ap,std::string& error){uintptr_t actual=0;
    return memory.at(p,0,actual,error)&&(actual==image+ap||life::fail(error,"native owner addresspoint differs"));}
bool owns(const Bound& current,uintptr_t dungeon,std::string& error){
    uintptr_t exterior=0,root=0,reverse=0;uint64_t uid=0;
    if(!typed(dungeon,0x16c0560,error)||!memory.at(dungeon,0x6f8,exterior,error)||
       !memory.at(exterior,0x170,root,error))return false;
    if(root!=uintptr_t(current.world.map)+0x1c0){error.clear();return false;}
    if(!typed(exterior,0x16d93c0,error)||!memory.at(exterior,0xa38,reverse,error)||!memory.at(exterior,0x18,uid,error))return false;
    return (reverse==dungeon&&uid)||life::fail(error,"bound raw receiver lost its reciprocal exterior/full UID");
}
bool pool(uintptr_t dungeon,guard::Pool& out,std::string& error){
    out={};out.dungeon=dungeon;
    if(!typed(dungeon,0x16c0560,error)||!memory.at(dungeon,0x18,out.dungeon_uid,error)||
       !memory.at(dungeon,0x6f8,out.exterior,error)||!typed(out.exterior,0x16d93c0,error)||
       !memory.at(out.exterior,0x18,out.exterior_uid,error)||!memory.at(out.exterior,0x170,out.root,error)||
       !memory.at(dungeon,0x414,out.counter,error))return false;
    uintptr_t reverse=0;if(!memory.at(out.exterior,0xa38,reverse,error))return false;
    if(!out.dungeon_uid||!out.exterior_uid||!out.root||reverse!=dungeon||out.counter<0)
        return life::fail(error,"raw actual Dungeon/exterior UID/next-index owner invalid");
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& e){
        if(!memory.at(p,offset,value,e))return false;
        if(p==dungeon+0x4b8&&offset==0x10&&(int64_t(value)<0||uint64_t(value)>1000000))
            return life::fail(e,"raw actual Instance count exceeds saved roster bound");return true;
    };
    dungeon_scene_roster::Array array;if(!dungeon_scene_roster::array(at,dungeon+0x4b8,array,error))return false;
    if(array.count>1000000)return life::fail(error,"raw actual Instance array exceeds saved roster bound");
    std::set<int32_t> ids;
    for(uintptr_t instance:array.members){life::Member row;row.instance=instance;uintptr_t owner=0;
        if(!typed(instance,0x1719450,error)||!memory.at(instance,0x18,row.index,error)||!memory.at(instance,0x30,owner,error)||
           !memory.at(instance,0x38,row.shard,error)||!memory.at(instance,0x40,row.region,error))return false;
        if(owner!=dungeon||row.index<0||!ids.insert(row.index).second||!row.shard||!row.region)
            return life::fail(error,"raw actual Instance index/owner differs");
        out.members.push_back(row);
    }
    uint64_t uid=0,exterior_uid=0;int32_t counter=-1;uintptr_t exterior=0;
    if(!dungeon_scene_roster::unchanged(at,dungeon+0x4b8,array,error)||!memory.at(dungeon,0x18,uid,error)||
       !memory.at(dungeon,0x414,counter,error)||!memory.at(dungeon,0x6f8,exterior,error)||!memory.at(out.exterior,0x18,exterior_uid,error))return false;
    return (uid==out.dungeon_uid&&exterior_uid==out.exterior_uid&&exterior==out.exterior&&counter==out.counter)||
        life::fail(error,"raw actual Instance owner/counter changed during copy");
}
bool census(const Bound& b,std::vector<Owner>& out,std::string& error){
    out.clear();if(!fresh(b,error))return false;
    dungeon_scene_identity_runtime::World world;std::vector<dungeon_scene_roster::Scene> scenes;
    if(!dungeon_scene_identity_runtime::read_scene_roster(world,scenes,error))return false;
    if(world.uuid!=b.world.uuid||world.world_epoch!=b.world.world_epoch||world.generation!=b.world.generation||world.map!=b.world.map)
        return life::fail(error,"current raw Scene roster world changed");
    for(const auto& scene:scenes)if(scene.owner.type==1){guard::Pool actual;
        if(!pool(scene.owner.dungeon,actual,error))return false;
        if(actual.exterior!=scene.owner.exterior||actual.exterior_uid!=scene.owner.exterior_uid||actual.dungeon_uid!=scene.dungeon_uid)
            return life::fail(error,"current raw pool differs from canonical Scene owner");
        Owner owner;owner.dungeon_uid=actual.dungeon_uid;
        owner.pin={b.origin,b.binding.native_epoch,b.binding.native_generation,actual.exterior_uid,actual.root,
                   actual.exterior,actual.dungeon,scene.owner.exterior_key,actual.counter,actual.members};
        if(!life::valid(owner.pin,error))return false;out.push_back(std::move(owner));
    }
    return fresh(b,error);
}
std::string identify(const std::vector<Owner>& owners,const guard::Pool& raw){
    for(const auto& owner:owners)if(owner.pin.dungeon==raw.dungeon&&owner.dungeon_uid==raw.dungeon_uid&&
       owner.pin.exterior==raw.exterior&&owner.pin.exterior_uid==raw.exterior_uid&&owner.pin.root==raw.root)return owner.pin.key;
    return {};
}
bool event(const Observation& raw,const std::vector<Owner>& current,const std::string& natural,uint32_t ordinal,life::Event& out,std::string& error){
    out={};out.exterior=raw.key.empty()?identify(current,raw.before):raw.key;
    if(out.exterior.empty())return life::fail(error,"retained original observation lacks an actual canonical exterior witness");
    out.operation=raw.action?raw.operation:natural;out.ordinal=raw.action?raw.ordinal:ordinal;out.action=raw.action;
    return guard::transition(raw.before,raw.after,raw.effect,raw.target,raw.result,out,error);
}
struct Running {Running(){++inflight;}~Running(){--inflight;}};
struct Depth {unsigned& value;explicit Depth(unsigned& v):value(v){++value;}~Depth(){--value;}};
DWORD store_native(uintptr_t address,const void* before,const void* after,size_t bytes,bool& matched){
    __try{matched=std::memcmp(reinterpret_cast<const void*>(address),before,bytes)==0;
        if(matched)std::memcpy(reinterpret_cast<void*>(address),after,bytes);return 0;}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool store(uintptr_t p,size_t offset,const void* before,const void* after,size_t bytes,std::string& error){
    if(!p||offset>UINTPTR_MAX-p||bytes>UINTPTR_MAX-p-offset)return life::fail(error,"guarded native store span invalid");
    bool matched=false;const auto code=store_native(p+offset,before,after,bytes,matched);
    return (!code&&matched)||life::fail(error,"guarded native store failed SEH="+std::to_string(code));
}
bool camera_before(uintptr_t dungeon,ReplayMake& camera,std::string& error){
    camera.dungeon=dungeon;
    if(!memory.at(image,0x13df440,camera.manager,error)||!typed(camera.manager,0x1702b00,error)||
       !memory.at(camera.manager,0x60,camera.active_shard,error)||!memory.at(dungeon,0x818,camera.displayed_shard,error))return false;
    return camera.active_shard!=0||life::fail(error,"replay Make has no original active camera Shard");
}
bool camera_after(const ReplayMake& camera,uintptr_t born,std::string& error){
    uintptr_t manager=0,active=0,displayed=0,shard=0;
    if(!memory.at(image,0x13df440,manager,error)||!memory.at(camera.manager,0x60,active,error)||
       !memory.at(camera.dungeon,0x818,displayed,error)||!memory.at(born,0x38,shard,error))return false;
    if(manager!=camera.manager||active!=camera.active_shard)return life::fail(error,"replay Make changed original ShardManager camera");
    bool restore=false;
    if(!guard::camera_restore(camera.displayed_shard,displayed,shard,camera.dip_suppressed,restore,error))return false;
    if(!restore)return true;
    if(!store(camera.dungeon,0x818,&displayed,&camera.displayed_shard,sizeof displayed,error)||
       !memory.at(camera.dungeon,0x818,displayed,error))return false;
    return displayed==camera.displayed_shard||life::fail(error,"replay Make displayed Shard restore readback differs");
}
template<class Call>uintptr_t observe(life::Effect effect,uintptr_t dungeon,uintptr_t target,Call&& call){
    const auto current=binding();if(!current||structural_depth)return call();
    std::string ownership;if(!owns(*current,dungeon,ownership)){if(!ownership.empty())fault(ownership);return call();}
    Running running;Depth nested(structural_depth);
    Observation raw;raw.effect=effect;raw.target=target;std::string error;
    const bool captured=pool(dungeon,raw.before,error);
    if(!captured)fault(error);
    if(captured&&!replay_depth){
        raw.action=!action.operation.empty();raw.operation=action.operation;
        if(raw.action){if(action.ordinal==UINT32_MAX){fault("Dungeon Instance action ordinal overflow");}
            else raw.ordinal=++action.ordinal;raw.key=identify(action.owners,raw.before);}
        if(raw.key.empty())raw.key=identify(current->owners,raw.before);
        std::lock_guard lock(gate);if(sequence==UINT64_MAX)fault_locked("Dungeon Instance observation sequence overflow");else raw.sequence=++sequence;
    }
    uintptr_t result=0;
    try{result=call();}catch(const std::exception& e){fault(std::string("Dungeon Instance original exception: ")+e.what());throw;}
    catch(...){fault("Dungeon Instance original unknown C++ exception");throw;}
    if(!captured||replay_depth)return result;
    raw.result=result;
    if(!pool(dungeon,raw.after,error)){fault(error);return result;}
    if(current->context.role==action_session::Role::Client&&!raw.action){fault("Unowned original Client Instance lifecycle effect executed");return result;}
    {
        std::lock_guard lock(gate);
        size_t bytes=sizeof raw+(raw.before.members.size()+raw.after.members.size())*sizeof(life::Member);
        for(const auto& prior:pending)bytes+=sizeof prior+(prior.before.members.size()+prior.after.members.size())*sizeof(life::Member);
        if(!bound||bound->origin!=current->origin){fault_locked("Dungeon Instance native observation crossed bound world");}
        else if(bytes>live_host_state::max_logical_frame_payload){fault_locked("Dungeon Instance retained raw observations exceed Frame resource bound");}
        else{
            if(raw.key.empty())raw.key=identify(bound->owners,raw.before);
            if(raw.key.empty())for(const auto& receipt:action_receipts){raw.key=identify(receipt.second.before,raw.before);if(!raw.key.empty())break;}
            pending.push_back(std::move(raw));
        }
    }
    return result;
}
uintptr_t make_hook(uintptr_t dungeon){
    ReplayMake camera;std::string error;const bool replay=replay_depth!=0;
    if(replay&&!camera_before(dungeon,camera,error)){fault(error);return 0;}
    struct CameraScope {ReplayMake* before;explicit CameraScope(ReplayMake* value):before(replay_make){replay_make=value;}~CameraScope(){replay_make=before;}} scope(replay?&camera:replay_make);
    const auto result=observe(life::Effect::Make,dungeon,0,[&]{return original_make(dungeon);});
    if(replay&&(!result||!camera_after(camera,result,error)))fault(error.empty()?"Original replay Make returned no Instance":error);
    return result;
}
void destroy_hook(uintptr_t dungeon,uintptr_t instance){(void)observe(life::Effect::Destroy,dungeon,instance,[&]{original_destroy(dungeon,instance);return uintptr_t(0);});}
void reset_hook(uintptr_t instance){uintptr_t dungeon=0;std::string error;
    if(binding()&&!memory.at(instance,0x30,dungeon,error))fault(error);
    (void)observe(life::Effect::Reset,dungeon,instance,[&]{original_reset(instance);return uintptr_t(0);});
}
bool independent_client(uintptr_t receiver,bool instance=false){
    const auto current=binding();if(!current||current->context.role!=action_session::Role::Client||!action.operation.empty()||replay_depth)return false;
    uintptr_t dungeon=receiver;std::string error;
    if(instance&&!memory.at(receiver,0x30,dungeon,error)){fault(error);return false;}
    const bool own=owns(*current,dungeon,error);if(!own&&!error.empty())fault(error);return own;
}
void force_hook(uintptr_t instance){const auto caller=uintptr_t(_ReturnAddress())-image;const auto current=binding();
    if(guard::suppress_force(independent_client(instance,true),false,false,caller))return;
    uintptr_t dungeon=0;std::string error;if(current&&!memory.at(instance,0x30,dungeon,error))fault(error);
    (void)observe(life::Effect::ForcePlayersOut,dungeon,instance,[&]{original_force(instance);return uintptr_t(0);});
}
void desired_hook(uintptr_t dungeon,float dt){if(independent_client(dungeon))return;Running running;original_desired(dungeon,dt);}
void process_hook(uintptr_t dungeon){if(independent_client(dungeon))return;Running running;original_process(dungeon);}
void kick_hook(uintptr_t dungeon){if(independent_client(dungeon))return;Running running;original_kick(dungeon);}
void update_hook(uintptr_t instance,float dt){
    if(!independent_client(instance,true)){Running running;original_update(instance,dt);return;}
    Running running;guard::Update before;std::string error;
    auto at=[](uintptr_t p,size_t offset,auto& value,std::string& e){return memory.at(p,offset,value,e);};
    if(!guard::update(at,image,instance,before,error)){fault(error);return;}
    try{original_update(instance,dt);}catch(const std::exception& e){fault(std::string("Dungeon Instance original Update exception: ")+e.what());throw;}
    catch(...){fault("Dungeon Instance original Update unknown C++ exception");throw;}
    auto write=[](uintptr_t p,size_t offset,const void* old_value,const void* value,size_t bytes,std::string& e){return store(p,offset,old_value,value,bytes,e);};
    if(!guard::restore_update(at,write,image,before,error))fault(error);
}
void dip_hook(uintptr_t manager,uintptr_t shard,const void* color,const void* transform,float duration){
    const auto caller=uintptr_t(_ReturnAddress())-image;
    if(replay_make&&guard::suppress_dip(true,caller,manager,replay_make->manager)){replay_make->dip_suppressed=true;return;}
    original_dip(manager,shard,color,transform,duration);
}
bool verify_route(const Route& route,std::string& error){
    std::vector<unsigned char> body(route.bytes);if(!memory.read(image+route.rva,body.data(),body.size(),error))return false;
    if(!installed)return mt2mp::sha256(body)==route.sha||life::fail(error,"original hook body differs RVA="+std::to_string(route.rva));
    std::array<unsigned char,64> trampoline{};
    if(!*route.original||!memory.read(uintptr_t(*route.original),trampoline.data(),trampoline.size(),error))return false;
    if(!std::equal(route.installed.begin(),route.installed.end(),body.begin())||trampoline!=route.relocated||body[0]!=0xe9||
       !std::equal(body.begin()+16,body.end(),route.pristine.begin()+16))return life::fail(error,"owned Instance hook body/trampoline differs");
    int32_t displacement=0;std::memcpy(&displacement,body.data()+1,4);std::array<unsigned char,14> relay{};
    if(!memory.read(image+route.rva+5+intptr_t(displacement),relay.data(),relay.size(),error))return false;
    uintptr_t target=0;std::memcpy(&target,relay.data()+6,sizeof target);
    return (relay[0]==0xff&&relay[1]==0x25&&relay[2]==0&&relay[3]==0&&relay[4]==0&&relay[5]==0&&target==route.detour)||
        life::fail(error,"owned Instance hook relay differs");
}
DWORD call_original(life::Effect effect,uintptr_t dungeon,uintptr_t target,uintptr_t& made){
    __try{
        if(effect==life::Effect::Make)made=make_hook(dungeon);
        else if(effect==life::Effect::Destroy)destroy_hook(dungeon,target);
        else if(effect==life::Effect::Reset)reset_hook(target);
        else force_hook(target);
        return 0;
    }__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool initialize(uintptr_t base,std::string& error){
    std::lock_guard lock(installation);
    if(attempted){if(!installed||base!=image)return life::fail(error,"hook installation incomplete or image differs");
        for(const auto& route:routes)if(!verify_route(route,error))return false;return healthy(error);}
    uintptr_t actual=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(actual,thread,sha,error)||actual!=base||thread!=GetCurrentThreadId()||sha!=executable)
        return life::fail(error,error.empty()?"hook installation requires exact image/Mainthread":error);
    image=base;mainthread=thread;
    const std::array<uintptr_t,9> detours{uintptr_t(&make_hook),uintptr_t(&destroy_hook),uintptr_t(&reset_hook),uintptr_t(&force_hook),
        uintptr_t(&desired_hook),uintptr_t(&process_hook),uintptr_t(&kick_hook),uintptr_t(&update_hook),uintptr_t(&dip_hook)};
    const std::array<void**,9> originals{reinterpret_cast<void**>(&original_make),reinterpret_cast<void**>(&original_destroy),
        reinterpret_cast<void**>(&original_reset),reinterpret_cast<void**>(&original_force),reinterpret_cast<void**>(&original_desired),
        reinterpret_cast<void**>(&original_process),reinterpret_cast<void**>(&original_kick),reinterpret_cast<void**>(&original_update),reinterpret_cast<void**>(&original_dip)};
    if(routes.empty())for(size_t i=0;i<detours.size();++i){const auto& code=codes::routes[i];Route route;route.rva=code.rva;route.detour=detours[i];
        route.original=originals[i];route.bytes=code.bytes;route.sha=code.sha;routes.push_back(std::move(route));}
    for(auto& route:routes){if(!verify_route(route,error))return false;
        route.pristine.resize(route.bytes);if(!memory.read(image+route.rva,route.pristine.data(),route.pristine.size(),error))return false;}
    attempted=true;
    for(auto& route:routes){auto status=MH_CreateHook(reinterpret_cast<void*>(image+route.rva),reinterpret_cast<void*>(route.detour),route.original);
        if(status!=MH_OK){fault("Instance MH_CreateHook status="+std::to_string(status));return healthy(error);}
        if(!memory.read(uintptr_t(*route.original),route.relocated.data(),route.relocated.size(),error)){fault(error);return false;}
        status=MH_EnableHook(reinterpret_cast<void*>(image+route.rva));
        if(status!=MH_OK){fault("Instance MH_EnableHook status="+std::to_string(status));return healthy(error);}
        if(!memory.read(image+route.rva,route.installed.data(),route.installed.size(),error)){fault(error);return false;}
    }
    installed=true;for(const auto& route:routes)if(!verify_route(route,error)){fault(error);return false;}return true;
}
bool healthy(std::string& error){std::lock_guard lock(gate);if(!failure.empty()){error=failure;return false;}
    if(!installed)return life::fail(error,"native hooks not installed");error.clear();return true;}
bool bind(const action_session::Context& context,const action_session::Binding& frame,const life::journal::Origin& origin,std::string& error){
    if(!healthy(error)||GetCurrentThreadId()!=mainthread||!live_host_state::vip::valid_origin(origin)||
       origin.session!=context.session||origin.snapshot!=context.snapshot_uuid||
       (context.role!=action_session::Role::Host&&context.role!=action_session::Role::Client))return life::fail(error,error.empty()?"bound Session/Origin invalid":error);
    const auto previous=binding();if(previous){if(previous->context==context&&previous->binding==frame&&previous->origin==origin)return fresh(*previous,error);
        return life::fail(error,"previous native observation binding requires explicit reset");}
    auto next=std::make_shared<Bound>();next->context=context;next->binding=frame;next->origin=origin;
    building_mixed_identity::opened_file::CurrentWorld world;if(!native_open_runtime::read_current_world(world,error))return false;
    next->world={world.game,world.mode,world.state,world.world,world.map,world.generation,world.world_epoch,world.uuid,world.sha256};
    if(world.uuid.empty()||world.world_epoch!=frame.native_epoch||world.generation!=frame.native_generation)
        return life::fail(error,"binding completed native world differs: session_snapshot="+context.snapshot_uuid+" native_snapshot="+world.uuid+
            " native_epoch="+std::to_string(world.world_epoch)+" binding_epoch="+std::to_string(frame.native_epoch)+
            " native_generation="+std::to_string(world.generation)+" binding_generation="+std::to_string(frame.native_generation));
    if(!census(*next,next->owners,error))return life::fail(error,error.empty()?"binding completed native world differs":error);
    std::lock_guard lock(gate);if(bound)return life::fail(error,"native observation binding became occupied");bound=next;return true;
}
bool reset(std::string& error){
    if(GetCurrentThreadId()!=mainthread||inflight.load()||!action.operation.empty()||replay_depth)return life::fail(error,"reset requires idle Mainthread original-call boundary");
    std::lock_guard lock(gate);bound.reset();pending.clear();action_receipts.clear();sequence=0;
    if(!installed)return life::fail(error,"cannot reset incomplete hook installation");failure.clear();error.clear();return true;
}
bool begin_action(const std::string& operation,std::string& error){
    const auto current=binding();if(!current||!party_identity::uuid(operation)||!action.operation.empty()||!fresh(*current,error))
        return life::fail(error,error.empty()?"action requires idle bound actual operation UUID":error);
    {std::lock_guard lock(gate);if(action_receipts.contains(operation))return life::fail(error,"actual original action receipt already retained");}
    std::vector<Owner> owners;if(!census(*current,owners,error))return false;
    auto next=std::make_shared<Bound>(*current);next->owners=owners;
    {std::lock_guard lock(gate);for(auto& row:pending)if(row.key.empty())row.key=identify(owners,row.before);bound=next;}
    action={operation,0,std::move(owners)};return true;
}
bool end_action(const std::string& operation,bool returned,std::string& error){
    if(operation!=action.operation||operation.empty()||GetCurrentThreadId()!=mainthread)return life::fail(error,"actual action end has another operation/thread");
    const auto current=binding();auto before=std::move(action.owners);const auto calls=action.ordinal;action={};
    if(!returned){if(calls)fault("Dungeon Instance actual Engine action failed after original lifecycle effects");return healthy(error);}
    if(!current||!fresh(*current,error))return false;
    std::vector<Owner> owners;if(!census(*current,owners,error))return false;
    std::vector<Observation> rows;{std::lock_guard lock(gate);for(auto& row:pending){
        if(row.key.empty())row.key=identify(before,row.before);
        if(row.action&&row.operation==operation)rows.push_back(row);}}
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.sequence<b.sequence;});
    std::vector<life::Event> events;
    for(const auto& row:rows){life::Event value;if(!event(row,owners,operation,0,value,error)){fault(error);return false;}events.push_back(std::move(value));}
    for(auto& owner:before)owner.pin.members.clear();
    {std::lock_guard lock(gate);action_receipts.emplace(operation,ActionReceipt{std::move(events),std::move(before)});
        if(current->context.role==action_session::Role::Client)std::erase_if(pending,[&](const auto& row){return row.action&&row.operation==operation;});}
    return true;
}
bool drain(const std::string& operation,std::vector<life::Event>& out,std::vector<life::Pin>& pools,std::string& error){
    out.clear();pools.clear();const auto current=binding();
    if(!current||!party_identity::uuid(operation)||!action.operation.empty()||!fresh(*current,error))return life::fail(error,error.empty()?"drain requires idle bound Mainthread":error);
    std::vector<Owner> owners;if(!census(*current,owners,error))return false;
    std::vector<Observation> rows;{std::lock_guard lock(gate);if(inflight.load())return life::fail(error,"original Instance calls are still running during drain");rows=pending;}
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.sequence<b.sequence;});
    uint32_t ordinal=0;
    for(const auto& row:rows){life::Event value;if(!row.action&&ordinal==UINT32_MAX)return life::fail(error,"natural observation ordinal overflow");
        if(!event(row,owners,operation,row.action?0:++ordinal,value,error))return false;out.push_back(std::move(value));}
    for(const auto& owner:owners)pools.push_back(owner.pin);
    if(!fresh(*current,error))return false;
    auto next=std::make_shared<Bound>(*current);next->owners=std::move(owners);
    {std::lock_guard lock(gate);if(inflight.load()||pending.size()!=rows.size())return life::fail(error,"Instance observations changed during drain");
        pending.clear();bound=next;}
    return true;
}
bool release_action(const std::string& operation,std::string& error){
    const auto current=binding();if(!current||!fresh(*current,error)||!party_identity::uuid(operation))return false;
    std::lock_guard lock(gate);if(std::any_of(pending.begin(),pending.end(),[&](const auto& row){return row.operation==operation;}))
        return life::fail(error,"action observations still await existing Frame journal ownership");
    action_receipts.erase(operation);return true;
}
bool adopt_counters(const std::vector<std::pair<std::string,int32_t>>& target,life::Receipt& receipt,std::string& error)try{
    receipt={};const auto current=binding();
    if(!current||current->context.role!=action_session::Role::Client||!fresh(*current,error))return life::fail(error,error.empty()?"counter adoption requires bound Client":error);
    subscriber_lifecycle::first_append::ClientWorld held;
    if(!subscriber_lifecycle::first_append::read_client_world_roots(memory,current->world.sha256,current->world.uuid,true,held,error,&current->context))return false;
    std::vector<Owner> before;if(!census(*current,before,error))return false;
    std::map<std::string,int32_t> wanted;
    for(const auto& row:target)if(row.second<0||!dungeon_scene_key::building_key(row.first)||!wanted.emplace(row).second)
        return life::fail(error,"final pool counters contain invalid or repeated canonical owner");
    if(wanted.size()!=before.size())return life::fail(error,"final counter adoption requires complete current Dungeon pool coverage");
    for(const auto& owner:before)if(!wanted.contains(owner.pin.key))return life::fail(error,"final counter adoption omits current Dungeon pool");
    const auto reject=[&](){receipt.unknown=receipt.attempted;receipt.cause=error;if(receipt.unknown)fault(error);return false;};
    for(const auto& owner:before){const auto value=wanted.at(owner.pin.key);if(value==owner.pin.counter)continue;
        receipt.attempted=true;++receipt.counter_stores;
        if(!store(owner.pin.dungeon,0x414,&owner.pin.counter,&value,sizeof value,error))return reject();}
    std::vector<Owner> after;if(!census(*current,after,error)||before.size()!=after.size())return reject();
    for(size_t i=0;i<before.size();++i)if(!life::same_owner(before[i].pin,after[i].pin)||before[i].pin.members!=after[i].pin.members||
       after[i].pin.counter!=wanted.at(before[i].pin.key)){life::fail(error,"final pool counter/actual ordered membership readback differs");return reject();}
    subscriber_lifecycle::first_append::ClientWorld final;
    if(!subscriber_lifecycle::first_append::read_client_world_roots(memory,current->world.sha256,current->world.uuid,true,final,error,&current->context)||
       !subscriber_lifecycle::first_append::same_client_owner(held,final)){if(error.empty())life::fail(error,"counter adoption lost held native owner");return reject();}
    receipt.returned=true;receipt.confirmed=true;return true;
}catch(const std::exception& e){receipt.unknown=receipt.attempted;receipt.cause=std::string("Dungeon Instance counter adoption C++: ")+e.what();
    if(receipt.unknown)fault(receipt.cause);return life::fail(error,receipt.cause);}
bool NativePort::current(const std::string& key,life::Pin& out,std::string& error){
    out={};const auto current=binding();if(!current||!fresh(*current,error))return life::fail(error,error.empty()?"native Port is unbound":error);
    std::vector<Owner> owners;if(!census(*current,owners,error))return false;
    const auto found=std::find_if(owners.begin(),owners.end(),[&](const auto& owner){return owner.pin.key==key;});
    if(found==owners.end())return life::fail(error,"current canonical exterior is absent");out=found->pin;return true;
}
bool NativePort::observed(const life::Event& expected,bool& out,std::string& error){
    out=false;const auto current=binding();if(!current||!fresh(*current,error))return life::fail(error,error.empty()?"observed action has no fresh bound world":error);
    if(!expected.action||!life::valid(expected,error))return false;
    std::lock_guard lock(gate);const auto found=action_receipts.find(expected.operation);
    if(found==action_receipts.end())return true;
    const auto row=std::find_if(found->second.events.begin(),found->second.events.end(),[&](const auto& value){return value.ordinal==expected.ordinal;});
    if(row==found->second.events.end())return life::fail(error,"retained actual action lacks requested original ordinal");
    if(*row!=expected)return life::fail(error,"retained actual original action effect differs from Host");
    out=true;return true;
}
bool NativePort::invoke(const life::Pin& before,const life::Event& expected,uintptr_t target,life::Receipt& receipt,std::string& error)try{
    const auto current=binding();if(!current||current->context.role!=action_session::Role::Client||!fresh(*current,error)||
       !life::valid(expected,error)||!action.operation.empty()||replay_depth)return life::fail(error,error.empty()?"native replay requires idle bound Client":error);
    subscriber_lifecycle::first_append::ClientWorld held;
    if(!subscriber_lifecycle::first_append::read_client_world_roots(memory,current->world.sha256,current->world.uuid,true,held,error,&current->context))return false;
    life::Pin actual;if(!this->current(before.key,actual,error)||!life::same_owner(before,actual)||actual.counter!=before.counter||actual.members!=before.members)
        return life::fail(error,error.empty()?"native original invocation owner changed":error);
    if(expected.effect!=life::Effect::Make){const auto found=std::find_if(actual.members.begin(),actual.members.end(),[&](const auto& row){return row.index==expected.index&&row.instance==target;});
        if(found==actual.members.end())return life::fail(error,"native target is not its actual saved index member");}
    receipt.attempted=true;
    const auto reject=[&](){receipt.unknown=true;receipt.cause=error.empty()?"Dungeon Instance original invocation outcome unknown":error;fault(receipt.cause);return false;};
    if(actual.counter!=expected.counter_before){++receipt.counter_stores;
        if(!store(actual.dungeon,0x414,&actual.counter,&expected.counter_before,sizeof actual.counter,error))return reject();
        int32_t counter=-1;if(!memory.at(actual.dungeon,0x414,counter,error)||counter!=expected.counter_before)
            {if(error.empty())life::fail(error,"original next-index adoption readback differs");return reject();}}
    Depth replay(replay_depth);uintptr_t made=0;
    const auto code=call_original(expected.effect,actual.dungeon,target,made);
    if(code){receipt.unknown=true;receipt.cause="Dungeon Instance original replay SEH="+std::to_string(code);fault(receipt.cause);return life::fail(error,receipt.cause);}
    receipt.made_instance=made;receipt.returned=true;
    if(!healthy(error)||!fresh(*current,error))return reject();
    subscriber_lifecycle::first_append::ClientWorld after;
    if(!subscriber_lifecycle::first_append::read_client_world_roots(memory,current->world.sha256,current->world.uuid,true,after,error,&current->context)||
       !subscriber_lifecycle::first_append::same_client_owner(held,after)){if(error.empty())life::fail(error,"original replay lost held native owner");return reject();}
    return true;
}catch(const std::exception& e){receipt.unknown=receipt.attempted;receipt.cause=std::string("Dungeon Instance original replay C++: ")+e.what();fault(receipt.cause);return life::fail(error,receipt.cause);}
catch(...){receipt.unknown=receipt.attempted;receipt.cause="Dungeon Instance original replay unknown C++ exception";fault(receipt.cause);return life::fail(error,receipt.cause);}
}
