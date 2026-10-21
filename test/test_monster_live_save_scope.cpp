#include "typed_action/action_session.h"
#include "live_host_state/monster_state.inc"
#include "subscriber_lifecycle/save_scope.h"
#include "live_host_state/apply_timing.h"
#include "live_host_state/memory_query_timing.h"
#include "monster_target_key.h"
#include "direct_monster_identity.h"
#include "npc_quest_targets.h"
#include <atomic>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
namespace mt2mp::action_session{Session& selected_session();}
namespace a=mt2mp::action_session;
namespace action=a;
namespace identity=mt2mp::game::direct_monster_identity;
namespace ml=mt2mp::game::live_host_state::monsters;
namespace q=mt2mp::game::npc_quest_targets;
namespace monster_target_key=mt2mp::game::monster_target_key;
namespace {
constexpr uintptr_t image_base=0x140000000,map_pointer=0x200000,world_root=map_pointer+0x1c0,region_pointer=0x300000,manager_pointer=0x400000,object_pointer=0x500000,regions_data=0x600000,members_data=0x700000;
constexpr uint64_t full_uid=0x100001d01;
const std::string shared_snapshot="11111111-1111-4111-8111-111111111111",local_origin="22222222-2222-4222-8222-222222222222",birth_id="33333333-3333-4333-8333-333333333333",other_id="44444444-4444-4444-8444-444444444444";
std::map<uintptr_t,unsigned char> bytes;
unsigned checks=0,world_reads=0;uint32_t current_thread=7;
constexpr uintptr_t mode_pointer=0x800000,game_pointer=0x900000,selection_pointer=0xa00000;
bool owner_available=true;uintptr_t uid_result=object_pointer;uint32_t lookup_fault=0;float actual_scale=0;
std::function<void()> after_lookup;
bool fail(std::string& e,const std::string& why){e=why;return false;}
void need(bool okay,const std::string& why){++checks;if(!okay)throw std::runtime_error(why);}
template<class T>void put(uintptr_t address,const T& value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof(T);++i)bytes[address+i]=p[i];}
struct Memory{
    int process=1;
    bool copy(uintptr_t address,void* target,size_t count,std::string& e){
        auto* p=static_cast<unsigned char*>(target);for(size_t i=0;i<count;++i){auto found=bytes.find(address+i);if(found==bytes.end())return fail(e,"fixture unreadable field");p[i]=found->second;}return true;
    }
};
template<class T>bool field(Memory& m,uintptr_t base,size_t offset,T& out,std::string& e){return m.copy(base+offset,&out,sizeof(out),e);}
namespace d{struct ReadMemory{Memory& memory;bool read(uintptr_t address,void* out,size_t count,std::string& e){return memory.copy(address,out,count,e);}};}
struct {uint32_t thread=7;uintptr_t image=image_base;}fixture_state;
uint32_t GetCurrentThreadId(){return current_thread;}
const char* exe_sha="exact-owned-binary";
namespace host{
struct WorldState{
    struct Origin{std::string uuid=local_origin,sha256=std::string(64,'a');uint64_t world_epoch=8,generation=9,bytes=100;
        void* map=reinterpret_cast<void*>(map_pointer);void* mode=reinterpret_cast<void*>(mode_pointer);void* game=reinterpret_cast<void*>(game_pointer);
        bool operator==(const Origin&)const=default;}origin;
    int32_t saves=-1,edits=-1;uintptr_t root=0,selection=0,active_graph=0;bool operator==(const WorldState&)const=default;
};
struct LookupApi{uintptr_t find=0x1234,cast=0x5678;};
WorldState current_world;
struct NativeOrigin{bool current(WorldState::Origin& out,std::string& e){++world_reads;if(!owner_available){e="original native owner absent";return false;}out=current_world.origin;return true;}};
bool resolve_lookup(int,uintptr_t,const char*,LookupApi&,std::string&){return true;}
struct RootSceneryReadBatch{
 static bool reuse_world(WorldState&,bool,uintptr_t,uint32_t,const LookupApi&,bool& reused,std::string& e){reused=false;e.clear();return true;}
};
struct Reader{
    Memory memory_;NativeOrigin& origin_;uintptr_t image_;uint32_t thread_;LookupApi api_;bool world_busy_=false;
    Reader(int,NativeOrigin& origin,uintptr_t image,uint32_t thread,LookupApi api):origin_(origin),image_(image),thread_(thread),api_(api){}
    bool current(WorldState&,bool,std::string&);
};
#include "monster_live_world_reader.inc"
}
#define state fixture_state
struct Session:a::Session{a::Context c;bool current(a::Context& out,std::string&)override{out=c;return true;}}session;
std::string network_snapshot=shared_snapshot;
namespace net{std::string current_snapshot_id(){return network_snapshot;}}
namespace action_runtime{
#include "monster_live_world_binding.inc"
}
namespace native{struct ClockState{float time_scale=0;};bool read_clock(ClockState& out,std::string&){out.time_scale=actual_scale;return true;}}
bool same_world(const host::WorldState& x,const host::WorldState& y){return x==y;}
uint32_t lookup(uintptr_t api,uint64_t uid,uintptr_t& found){need(api==0x1234&&uid==full_uid,"full UID lookup arguments differ");found=uid_result;if(after_lookup)after_lookup();return lookup_fault;}
struct LiveWorld{host::WorldState world;host::LookupApi api;};
struct LiveLocal{identity::Entry entry;};
class MonsterLivePort{
public:
    a::Context context_;a::Binding binding_{8,9};bool bound_=true,suspended_=false,locked_=false;
    bool enabled=true;unsigned presentations=0,failures=0,adjustments=0;
    nlohmann::json callback_scope_;
    std::atomic<bool> wrong_thread_{false};std::string cause_;std::map<std::string,LiveLocal> locals_;
    bool reject(std::string& e,const std::string& why,bool lock=false){locked_=lock;return fail(e,why);}
    bool row(Memory& m,const LiveWorld& p,uintptr_t object,ml::Row& out,LiveLocal& local,std::string& e){
        if(!full(m,p,object,local.entry,out.birth,e))return false;
        out.name="Gnashbone";out.rank=1;out.slot=1;out.type=0;out.level=1;out.pose.region=67;out.pose.orientation={0,0,0,1};out.pose.scale={1,1,1};out.on_map=true;return true;
    }
    bool active()const{return enabled;}
    bool actor(Memory&,uintptr_t,LiveLocal&,std::string&){return true;}
    bool classify(uintptr_t,bool&,LiveLocal&,std::string&);
    void failed(const std::string& e){++failures;locked_=true;cause_=e;}
    bool present(uintptr_t,LiveLocal&,float dt,std::string& e){if(dt!=0)return fail(e,"nonzero callback dt");++presentations;return true;}
    void adjustment(){++adjustments;}
#include "monster_live_identity_bodies.inc"
};
MonsterLivePort monster_live_port;
using LiveUpdate=void(*)(uintptr_t,float);using LiveAdjust=float(*)(uintptr_t,int32_t,float,uintptr_t,bool);
struct Hook{void* original=nullptr;};Hook live_update_hook,live_adjust_hook;
uintptr_t fixture_caller=image_base+0x2945ba;bool hook_okay=true;unsigned original_updates=0,original_adjustments=0;
void* test_return_address(){return reinterpret_cast<void*>(fixture_caller);}
#define _ReturnAddress test_return_address
bool live_hook_current(Memory&,const Hook&,std::string& e){return hook_okay||fail(e,"original hook seal changed");}
void original_update(uintptr_t,float){++original_updates;}
float original_adjust(uintptr_t,int32_t,float,uintptr_t,bool){++original_adjustments;return 123;}
#include "monster_live_classify.inc"
namespace management_runtime {bool intervention_replaying(){return false;}}
namespace interventions::actors::live {
bool monster_update(uintptr_t,float,bool& owned,std::string& e){owned=false;e.clear();return true;}
bool adjustment(uintptr_t,bool& owned,std::string& e){owned=false;e.clear();return true;}
}
#include "monster_live_callbacks.inc"
std::function<void(bool)> scope_native;
void array(uintptr_t address,uintptr_t ap,uintptr_t data,int32_t count){put(address,image_base+ap);put(address+8,data);put(address+0x10,count);put(address+0x14,count);}
identity::Scope local_scope(){return {host::current_world.origin.uuid,8,9,world_root,region_pointer,manager_pointer};}
void reset(MonsterLivePort& port,a::Role role=a::Role::Host){
    bytes.clear();identity::registry={};host::current_world={};world_reads=0;owner_available=true;uid_result=object_pointer;lookup_fault=0;actual_scale=0;after_lookup={};network_snapshot=shared_snapshot;
    current_thread=7;port.bound_=true;port.suspended_=false;port.wrong_thread_=false;port.cause_.clear();port.callback_scope_=nullptr;port.failures=port.presentations=port.adjustments=0;port.enabled=true;hook_okay=true;fixture_caller=image_base+0x2945ba;original_updates=original_adjustments=0;
    put(mode_pointer,image_base+0x16e6730);put(game_pointer,image_base+0x1743740);put(map_pointer,image_base+0x1742860);
    put(game_pointer+0x28,mode_pointer);put(mode_pointer+0x2c4,int32_t(4));put(mode_pointer+0x30c,int32_t(0));put(image_base+0x13beda0,int32_t(0));
    put(image_base+0x13df630,selection_pointer);put(selection_pointer,image_base+0x17205d0);put(image_base+0x1a3aa80,uintptr_t(0));
    put(world_root,image_base+0x174ad20);put(world_root+0x48,int32_t(0));
    session.c={42,role==a::Role::Host?uint64_t(1):uint64_t(2),1,3,role,shared_snapshot};port.context_=session.c;port.locals_.clear();port.locked_=false;
    put(map_pointer+0x3d4,int32_t(7));array(map_pointer+0x230,0x16f04e0,regions_data,1);put(regions_data,region_pointer);
    put(region_pointer,image_base+0x1787930);put(region_pointer+0x70,world_root);put(region_pointer+0x834,int32_t(67));put(region_pointer+0x988,manager_pointer);
    put(manager_pointer,image_base+0x1714100);put(manager_pointer+0x80,region_pointer);array(manager_pointer+0x38,0x16e8b60,members_data,1);put(members_data,object_pointer);
    put(object_pointer,image_base+0x16c0770);put(object_pointer+0x18,full_uid);put(object_pointer+0x170,world_root);put(object_pointer+0x178,region_pointer);put(object_pointer+0x8d0,uintptr_t(0));put(object_pointer+0x8d8,uintptr_t(0));
}
void stage(bool confirm=true,const std::string& birth=birth_id,identity::Scope scope=local_scope(),uintptr_t object=object_pointer,uint64_t uid=full_uid){
    std::string e;need(identity::registry.stage(birth,scope,object,uid,e),e);if(confirm)need(identity::registry.confirm(birth,e),e);
}
void reject_read(MonsterLivePort& port,const std::string& cause){ml::State out;std::string e;need(!port.read(out,e)&&e.find(cause)!=std::string::npos,"expected "+cause+", got "+e);}
}
namespace mt2mp::action_session{Session& selected_session(){return ::session;}}
namespace mt2mp::subscriber_lifecycle{
uint32_t call(SaveScopeCall fn,void*){::scope_native(reinterpret_cast<uintptr_t>(fn)==1);return 0;}
#include "monster_live_save_scope.inc"
}
int main(){try{
    auto& port=monster_live_port;std::string e;Memory memory;
    live_update_hook.original=reinterpret_cast<void*>(&original_update);live_adjust_hook.original=reinterpret_cast<void*>(&original_adjust);
    const auto reset_client=[&]{reset(port,a::Role::Client);stage();};
    for(int mode=0;mode<4;++mode){reset_client();
        const int32_t saves=mode==0?0:mode==1?1:2,edits=mode==2?1:0;const uintptr_t graph=mode==3?0xabc:0;
        put(mode_pointer+0x30c,saves);put(image_base+0x13beda0,edits);put(image_base+0x1a3aa80,graph);
        LiveWorld world;need(port.world(memory,world,e,false),"callback World read rejected valid native scope: "+e);
        need(world.world.saves==saves&&world.world.edits==edits&&world.world.active_graph==graph,"actual Reader lost scope fields");
        e.clear();const bool idle=port.world(memory,world,e);need(idle==(mode==0),"writer World admission changed");
        if(mode){need(e.find("require_idle=1 saves="+std::to_string(saves)+" edits="+std::to_string(edits)+" active_graph="+std::to_string(graph))!=std::string::npos,"idle rejection lost actual scope values");ml::State sample;e.clear();need(!port.read(sample,e),"complete census accepted Save/Edit scope");}
        e.clear();live_update_entry(object_pointer,0);need(!port.failures&&port.presentations==1&&!original_updates,"bound scope callback locked or ran gameplay");
        need(live_adjust_entry(object_pointer,0,-1,0,false)==0&&!original_adjustments&&port.adjustments==1,"scoped attribute callback ran Client gameplay");
        if(mode)need(port.callback_scope_["saves"]==saves&&port.callback_scope_["edits"]==edits&&port.callback_scope_["active_graph"]==graph&&port.callback_scope_["local_uid"]==full_uid,"scoped callback receipt differs");
    }
    reset_client();
    // Model the exact original enter ordering: flush before increment, then
    // the existing SaveScope body/exit/leave. The real C++ scope executes.
    unsigned enters=0,leaves=0;put(image_base+0x13beda0,int32_t(1));put(image_base+0x1a3aa80,uintptr_t(0xabc));
    scope_native=[&](bool enter){if(enter){++enters;live_update_entry(object_pointer,0);put(mode_pointer+0x30c,int32_t(1));}else{++leaves;put(mode_pointer+0x30c,int32_t(0));}};
    const auto work=[](void*,std::string& error){live_update_entry(object_pointer,0);Memory m;LiveWorld world;return monster_live_port.world(m,world,error,false)&&world.world.saves==1&&!monster_live_port.failures;};
    const auto guard=[](void*,std::string& error){Memory m;LiveWorld world;LiveLocal local;bool owned=false;return monster_live_port.world(m,world,error,false)&&monster_live_port.classify(object_pointer,owned,local,error)&&owned;};
    mt2mp::subscriber_lifecycle::SaveScopeApi api{reinterpret_cast<mt2mp::subscriber_lifecycle::SaveScopeCall>(1),reinterpret_cast<mt2mp::subscriber_lifecycle::SaveScopeCall>(2)};
    mt2mp::subscriber_lifecycle::SaveScopeReceipt receipt;
    need(mt2mp::subscriber_lifecycle::with_save_scope(api,reinterpret_cast<void*>(mode_pointer),work,guard,nullptr,receipt,e)&&receipt.enter_returned&&receipt.leave_returned&&enters==1&&leaves==1&&!port.failures&&port.presentations==2,"actual SaveScope callback locked or cleanup changed: "+e);
    put(image_base+0x13beda0,int32_t(0));put(image_base+0x1a3aa80,uintptr_t(0));LiveWorld recovered;need(port.world(memory,recovered,e),"idle World failed after scope ended");
    for(int fault=0;fault<13;++fault){reset_client();put(mode_pointer+0x30c,int32_t(1));
        if(fault==0)++host::current_world.origin.generation;
        if(fault==1)++host::current_world.origin.world_epoch;
        if(fault==2)session.c.snapshot_uuid="foreign";
        if(fault==3)actual_scale=1;
        if(fault==4)uid_result=object_pointer+8;
        if(fault==5)put(object_pointer+0x8d8,uintptr_t(1));
        if(fault==6)put(manager_pointer+0x80,region_pointer+8);
        if(fault==7)current_thread=8;
        if(fault==8)fixture_caller++;
        if(fault==9)hook_okay=false;
        if(fault==10)put(mode_pointer+0x30c,int32_t(-1));
        if(fault==11)put(image_base+0x13beda0,int32_t(-1));
        if(fault==12)put(map_pointer+0x3d4,int32_t(6));
        live_update_entry(object_pointer,0);need(port.failures==1&&!port.cause_.empty()&&!port.presentations&&!original_updates,"scope bypassed exact ownership/caller/clock guard "+std::to_string(fault));
    }
    reset_client();put(mode_pointer+0x30c,int32_t(1));live_update_entry(object_pointer,0.1f);need(port.failures==1&&!port.presentations&&!original_updates,"scoped callback accepted nonzero dt");
    std::cout<<"PASS "<<checks<<" actual Reader/world/full/classify/callback/SaveScope assertions\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
