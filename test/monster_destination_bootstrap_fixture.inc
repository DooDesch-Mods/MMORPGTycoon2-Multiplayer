#include "direct_monster_identity.h"
#include "dungeon_monster_reference.h"
#include "monster_zone_read.h"
#include "region_saved_identity.h"

namespace direct_bootstrap_fixture {
namespace q=mt2mp::game::npc_quest_targets;
namespace monster_zone_read=mt2mp::game::monster_zone_read;
namespace region_saved_identity=mt2mp::game::region_saved_identity;
namespace dungeon_monster_reference=mt2mp::game::dungeon_monster_reference;
namespace identity {
using Scope=mt2mp::game::direct_monster_identity::Scope;
using Entry=mt2mp::game::direct_monster_identity::Entry;
mt2mp::game::direct_monster_identity::Registry registry;
}
constexpr uintptr_t image_value=0x10000000,map_value=0x20000000,root_value=map_value+0x1c0;
constexpr uintptr_t region_value=0x21000000,manager_value=0x22000000,monster=0x23000000,array_value=0x24000000;
constexpr uint64_t monster_uid=0x100000025;
const std::string exe_sha(64,'a'),snapshot="11111111-1111-4111-8111-111111111111";
const std::string action="bfb4fd31-401f-4234-a7e3-8d2a82b23dce",expected_key="action:"+action;
fixture::Memory memory;
uintptr_t receipt_image=image_value,lookup_pointer=monster;
uint32_t receipt_thread=7,current_thread=7;
std::string receipt_sha=exe_sha;
bool receipt_ok=true,lookup_ok=true,lookup_empty_cause=false,world_ok=true,sample_ok=true;
unsigned lookup_calls=0,world_reads=0;
struct Runtime {bool ready=false;uintptr_t image=0;uint32_t thread=0;bool operator==(const Runtime&)const=default;}state;
unsigned GetCurrentThreadId(){return current_thread;}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
struct Memory {uintptr_t process=1;};
template<class T>bool field(Memory&,uintptr_t pointer,size_t offset,T& value,std::string& error){return memory.read(pointer+offset,&value,sizeof value,error);}
namespace native {
bool read_resolver_receipt(uintptr_t& image,uint32_t& thread,std::string& sha,std::string& error){
    if(!receipt_ok)return fail(error,"original resolver receipt failure");
    image=receipt_image;thread=receipt_thread;sha=receipt_sha;return true;
}
}
namespace host {
struct LookupApi {uintptr_t find=0;};
struct NativeOrigin {};
struct WorldState {mt2mp::game::dungeon_scene_identity_runtime::World origin;uintptr_t root=0;};
WorldState world;
bool resolve_lookup(uintptr_t process,uintptr_t image,const std::string& sha,LookupApi& out,std::string& error){
    ++lookup_calls;if(!lookup_ok){if(!lookup_empty_cause)error="original pinned lookup body differs";return false;}
    if(process!=1||image!=image_value||sha!=exe_sha)return fail(error,"pinned lookup received foreign context");
    out.find=1;return true;
}
struct Reader {
    uintptr_t image;uint32_t thread;
    Reader(uintptr_t,NativeOrigin&,uintptr_t base,uint32_t owner,const LookupApi&):image(base),thread(owner){}
    bool current(WorldState& out,bool active,std::string& error){
        ++world_reads;if(!world_ok)return fail(error,"original current native world failure");
        if(!active||image!=image_value||thread!=current_thread)return fail(error,"native world context differs");
        out=world;return true;
    }
};
}
namespace action_runtime {
bool same_world(const host::WorldState& a,const host::WorldState& b){return a.root==b.root&&a.origin.uuid==b.origin.uuid&&a.origin.world_epoch==b.origin.world_epoch&&a.origin.generation==b.origin.generation&&a.origin.map==b.origin.map;}
}
namespace d {
struct Sample {monster_zone_read::State state;};
Sample current;
bool sample(Memory&,const q::World& region,Sample& out,std::string& error){
    if(!sample_ok)return fail(error,"original current Monster manager read failure");
    if(region.image!=image_value||region.region!=region_value||region.root!=root_value)return fail(error,"current Monster manager owner differs");
    out=current;return true;
}
}
struct Pin {host::WorldState world;q::World region;host::LookupApi api;};
identity::Scope scope(const Pin& p,const d::Sample& sample){return {p.world.origin.uuid,p.world.origin.world_epoch,p.world.origin.generation,p.world.root,p.region.region,sample.state.monster_manager,"world"};}
unsigned lookup(uintptr_t function,uint64_t uid,uintptr_t& pointer){pointer=lookup_pointer;return function!=1||uid!=monster_uid?1u:0u;}
namespace typed_action_runtime {
bool resolve_held_dungeon_destination(uintptr_t,std::string&,bool& held,std::string&){held=false;return true;}
}
namespace dungeon_scene_identity_runtime {
using Census=mt2mp::game::dungeon_scene_identity_runtime::Census;
bool read_current(Census&,std::string& error){return fail(error,"unexpected Dungeon census in root fixture");}
}
#include "monster_destination_bootstrap_actual.inc"
size_t checks=0;
void need(bool okay,const std::string& cause){++checks;if(!okay)throw std::runtime_error(cause+" bootstrap comparison="+std::to_string(checks));}
void setup(){
    memory={};state={};receipt_image=image_value;receipt_thread=current_thread=7;receipt_sha=exe_sha;
    receipt_ok=lookup_ok=world_ok=sample_ok=true;lookup_empty_cause=false;lookup_calls=world_reads=0;lookup_pointer=monster;
    host::world={};auto& world=host::world;world.root=root_value;world.origin.uuid=snapshot;world.origin.world_epoch=1;world.origin.generation=2;world.origin.map=reinterpret_cast<void*>(map_value);
    memory.put(monster,uintptr_t(image_value+0x16c0770));memory.put(monster+0x18,monster_uid);memory.put(monster+0x170,root_value);
    memory.put(monster+0x178,region_value);memory.put(monster+0x8d8,uintptr_t(0));
    memory.put(map_value,uintptr_t(image_value+0x1742860));memory.put(root_value,uintptr_t(image_value+0x174ad20));memory.put(root_value+0x48,int32_t(0));
    memory.put(map_value+0x230,uintptr_t(image_value+0x16f04e0));memory.put(map_value+0x238,array_value);
    memory.put(map_value+0x240,int32_t(1));memory.put(map_value+0x244,int32_t(1));
    memory.put(map_value+0x358,uintptr_t(0));memory.put(map_value+0x360,int32_t(0));memory.put(map_value+0x364,int32_t(0));
    memory.put(array_value,region_value);memory.put(region_value,uintptr_t(image_value+0x1787930));memory.put(region_value+0x70,root_value);memory.put(region_value+0x834,int32_t(67));
    d::current={};d::current.state.monster_manager=manager_value;monster_zone_read::Monster row;row.pointer=monster;row.uid=monster_uid;row.tag=-11752269;d::current.state.monsters.push_back(row);
    identity::registry={};std::string error;const identity::Scope current{snapshot,1,2,root_value,region_value,manager_value,"world"};
    if(!identity::registry.restore(current,{{expected_key,identity::Entry{current,monster,monster_uid,true,false}}},error))throw std::runtime_error(error);
}
void tests(){
    std::string key,error;setup();const auto inactive=state;const auto bindings=identity::registry;
    need(direct_destination(monster,key,error),"Valid bootstrap read failed: "+error);
    need(key==expected_key&&lookup_calls==1&&world_reads==1,"Bootstrap read did not resolve exact saved Monster binding");
    need(state==inactive&&!state.ready&&state.image==0&&state.thread==0&&identity::registry==bindings,"Read enabled or rewrote mutation runtime/identity state");
    setup();state={true,image_value,7};need(direct_destination(monster,key,error)&&key==expected_key,"Active action runtime changed read-only destination result");
    for(int mode=0;mode<15;++mode){
        setup();error.clear();key="stale";
        if(mode==0)current_thread=8;if(mode==1)receipt_thread=0;if(mode==2)receipt_image=0;if(mode==3)receipt_sha[0]='b';if(mode==4)receipt_ok=false;
        if(mode==5)lookup_ok=false;if(mode==6){lookup_ok=false;lookup_empty_cause=true;}if(mode==7)world_ok=false;
        if(mode==8)memory.put(region_value+0x70,root_value+8);if(mode==9)d::current.state.monsters.clear();
        if(mode==10)memory.put(monster,uintptr_t(image_value+0x1742980));if(mode==11)memory.put(monster+0x8d8,uintptr_t(0x40000000));
        if(mode==12)lookup_pointer=monster+8;if(mode==13)memory.put(monster+0x18,monster_uid^(uint64_t(1)<<32));if(mode==14)d::current.state.monster_manager+=8;
        need(!direct_destination(monster,key,error)&&!error.empty()&&key.empty(),"Invalid bootstrap context/identity accepted mode="+std::to_string(mode));
        need(state==Runtime{},"Failed bootstrap read changed action runtime");
        if(mode<5)need(lookup_calls==0&&world_reads==0,"Invalid native resolver reached lookup/world reads");
        if(mode==5)need(error=="original pinned lookup body differs","Pinned lookup cause replaced");
        if(mode==6)need(error=="Quest direct Monster pinned full UID lookup unavailable","Empty lookup cause remained invisible");
        if(mode==7)need(error=="original current native world failure","Current world cause replaced");
    }
}
}
