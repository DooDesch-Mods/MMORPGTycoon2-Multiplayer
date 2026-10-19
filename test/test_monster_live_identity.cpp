#include "typed_action/action_session.h"
#include "live_host_state/monster_state.h"
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
namespace {
constexpr uintptr_t image_base=0x140000000,world_root=0x100000,map_pointer=0x200000,region_pointer=0x300000,manager_pointer=0x400000,object_pointer=0x500000,regions_data=0x600000,members_data=0x700000;
constexpr uint64_t full_uid=0x100001d01;
const std::string shared_snapshot="11111111-1111-4111-8111-111111111111",local_origin="22222222-2222-4222-8222-222222222222",birth_id="33333333-3333-4333-8333-333333333333",other_id="44444444-4444-4444-8444-444444444444";
std::map<uintptr_t,unsigned char> bytes;
unsigned checks=0,world_reads=0;
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
struct {uint32_t thread=7;uintptr_t image=image_base;}state;
uint32_t GetCurrentThreadId(){return 7;}
const char* exe_sha="exact-owned-binary";
namespace host{
struct WorldState{
    struct Origin{std::string uuid=local_origin;uint64_t world_epoch=8,generation=9,map=map_pointer;bool operator==(const Origin&)const=default;}origin;
    int32_t saves=0,edits=0;uintptr_t active_graph=0;
    uintptr_t root=world_root;bool operator==(const WorldState&)const=default;
};
struct LookupApi{uintptr_t find=0x1234;};struct NativeOrigin{};
WorldState current_world;
bool resolve_lookup(int,uintptr_t,const char*,LookupApi&,std::string&){return true;}
struct Reader{
    Reader(int,NativeOrigin&,uintptr_t,uint32_t,LookupApi&){}
    bool current(WorldState& out,bool,std::string& e){++world_reads;if(!owner_available)return fail(e,"original native owner absent");out=current_world;return true;}
};
}
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
    std::atomic<bool> wrong_thread_{false};std::string cause_;std::map<std::string,LiveLocal> locals_;
    bool reject(std::string& e,const std::string& why,bool lock=false){locked_=lock;return fail(e,why);}
    bool row(Memory& m,const LiveWorld& p,uintptr_t object,ml::Row& out,LiveLocal& local,std::string& e){
        if(!full(m,p,object,local.entry,out.birth,e))return false;
        out.name="Gnashbone";out.rank=1;out.slot=1;out.type=0;out.level=1;out.pose.region=67;out.pose.orientation={0,0,0,1};out.pose.scale={1,1,1};out.on_map=true;return true;
    }
#include "monster_live_identity_bodies.inc"
};
void array(uintptr_t address,uintptr_t ap,uintptr_t data,int32_t count){put(address,image_base+ap);put(address+8,data);put(address+0x10,count);put(address+0x14,count);}
identity::Scope local_scope(){return {host::current_world.origin.uuid,8,9,world_root,region_pointer,manager_pointer};}
void reset(MonsterLivePort& port,a::Role role=a::Role::Host){
    bytes.clear();identity::registry={};host::current_world={};world_reads=0;owner_available=true;uid_result=object_pointer;lookup_fault=0;actual_scale=0;after_lookup={};network_snapshot=shared_snapshot;
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
int main(){try{
    MonsterLivePort port;std::string e;
    for(auto role:{a::Role::Host,a::Role::Client}){
        reset(port,role);host::current_world.origin.uuid=role==a::Role::Host?local_origin:"55555555-5555-4555-8555-555555555555";stage();
        Memory m;LiveWorld p;identity::Entry entry;std::string birth;
        need(port.world(m,p,e),e);need(port.full(m,p,object_pointer,entry,birth,e)&&birth==birth_id&&entry.scope.snapshot==host::current_world.origin.uuid,"actual full rejected distinct local origin: "+e);
        ml::State out;need(port.read(out,e)&&out.rows.size()==1&&out.rows[0].birth==birth_id&&port.locals_.size()==1,"actual complete census lost distinct origin: "+e);
        need(world_reads==3&&entry.scope.snapshot!=port.context_.snapshot_uuid,"world binding or distinct IDs fixture bypassed");
    }
    reset(port);auto wrong=local_scope();wrong.snapshot=shared_snapshot;stage(true,birth_id,wrong);reject_read(port,"no confirmed full-UID/current-world action binding");
    reset(port);stage(false);reject_read(port,"unconfirmed birth");
    reset(port);reject_read(port,"no confirmed full-UID/current-world action binding");
    reset(port);stage();identity::Entry retired;need(identity::registry.lookup(birth_id,local_scope(),retired,e),e);need(identity::registry.retire(birth_id,retired,e),e);
    array(manager_pointer+0x38,0x16e8b60,members_data,0);ml::State out;need(port.read(out,e)&&out.rows.empty()&&out.retired==std::vector<std::string>{birth_id},"local-origin tombstone absent: "+e);
    reset(port);stage();array(manager_pointer+0x38,0x16e8b60,members_data,0);reject_read(port,"confirmed birth absent");
    reset(port);stage();stage(true,other_id,local_scope(),object_pointer+0x1000,full_uid+1);reject_read(port,"confirmed birth absent");
    reset(port);stage();auto foreign=local_scope();foreign.snapshot="foreign-origin";stage(true,other_id,foreign,object_pointer+0x1000,full_uid+1);need(port.read(out,e)&&out.rows.size()==1,"foreign origin polluted local census: "+e);
    reset(port);stage();host::current_world.origin.generation=10;reject_read(port,"native World or snapshot differs");
    reset(port);stage();host::current_world.origin.world_epoch=10;reject_read(port,"native World or snapshot differs");
    reset(port);stage();network_snapshot="changed-shared";reject_read(port,"native World or snapshot differs");
    reset(port);stage();session.c.snapshot_uuid="changed-session";reject_read(port,"current Session differs");
    reset(port);stage();owner_available=false;reject_read(port,"original native owner absent");
    reset(port,a::Role::Client);stage();actual_scale=1;reject_read(port,"actual Clock is not zero");
    reset(port);stage();put(object_pointer+0x8d8,uintptr_t(1));reject_read(port,"null-Zone row retains proxy");
    reset(port);stage();put(manager_pointer+0x80,region_pointer+8);reject_read(port,"Region/manager census differs");
    reset(port);stage();uid_result=object_pointer+8;reject_read(port,"full UID lookup differs");
    reset(port);stage();after_lookup=[]{put(object_pointer+0x18,full_uid+1);};reject_read(port,"full UID changed after lookup");
    reset(port);stage();array(manager_pointer+0x38,0x16e8b60,members_data,2);put(members_data+8,object_pointer);reject_read(port,"owning membership differs");
    std::cout<<"PASS "<<checks<<" actual world/full/read and Registry assertions with distinct local/shared UUIDs\n";return 0;
}catch(const std::exception& x){std::cerr<<"FAIL "<<x.what()<<'\n';return 1;}}
