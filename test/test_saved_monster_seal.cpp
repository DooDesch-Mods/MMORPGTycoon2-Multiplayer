#include "monster_saved_identity.h"
#include "direct_monster_identity.h"
#include "monster_snapshot_companion.h"
#include "dungeon_scene_identity_runtime.h"
#include <optional>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <mutex>
#include <Windows.h>
template<class... Args> void fixture_log(const Args&...) {}
#define LOG_INFO(...) fixture_log(__VA_ARGS__)
#define LOG_ERROR(...) fixture_log(__VA_ARGS__)
namespace fixture {
namespace q=mt2mp::game::monster_saved_identity;
namespace direct_monster_identity=mt2mp::game::direct_monster_identity;
namespace attachment=mt2mp::game::monster_snapshot_companion;
namespace snapshot_monster_key=mt2mp::game::snapshot_monster_key;
namespace region_saved_identity=mt2mp::game::region_saved_identity;
namespace opened {
struct CurrentWorld{std::string uuid;uint64_t world_epoch=1,generation=2;void* map=nullptr;std::string sha256;bool operator==(const CurrentWorld&)const=default;};
struct Status{std::string state="pinned",uuid,sha256;bool held=true;uint64_t generation=0,bytes=0;};
Status pinned;
Status status(){return pinned;}
bool read_completed_world(uint64_t generation,CurrentWorld&,std::string&);
}
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    template<class T>void put(uintptr_t address,T value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=p[i];}
    bool read(uintptr_t address,void* output,size_t size,std::string& e){auto* p=static_cast<unsigned char*>(output);for(size_t i=0;i<size;++i){const auto found=bytes.find(address+i);if(found==bytes.end()){e="fixture read absent";return false;}p[i]=found->second;}return true;}
}memory;
constexpr uintptr_t image=0x140000000,map_address=0x100000,fixture_region=0x200000,fixture_manager=0x300000,zone_actor=0x400000,direct_actor=0x500000;
const std::string fixture_digest(64,'a'),birth="11111111-1111-4111-8111-111111111111";
std::string sha=fixture_digest,failure,uuid;
bool host_active=false,prepared_present=false,world_changed=false;
uintptr_t bound_context=9;
std::map<int,int> saves,loads,load_stack;
std::map<uintptr_t,q::Pool> observed;
std::map<uintptr_t,q::SavedPool> pools;
std::optional<mt2mp::game::dungeon_scene_identity_runtime::Census> scene_census;
bool export_failed=false;unsigned gizmo_seals=0;size_t assertions=0;
namespace dungeon_scene_identity_runtime {
using Census=mt2mp::game::dungeon_scene_identity_runtime::Census;
using Manifest=mt2mp::game::dungeon_gizmo_snapshot_alias::Manifest;
mt2mp::game::dungeon_gizmo_identity::Registry gizmos;
auto scope(const Census& c){return mt2mp::game::dungeon_gizmo_identity::Scope{c.world.uuid,c.world.sha256,c.world.world_epoch,c.world.generation,uintptr_t(c.world.map)+0x1c0};}
bool seal_snapshot(const Census& c,const Manifest* aliases,std::string& e){++gizmo_seals;
    return gizmos.seal(scope(c),c.saved,aliases?&aliases->rows:nullptr,e,aliases?aliases->retired:std::set<std::string>{});}
bool capture_aliases(const Census& c,const std::string& target_sha,Manifest& out,std::string& e){
    out={};if(export_failed){e="original Gizmo alias export failure";return false;}out.present=true;
    return gizmos.capture(scope(c),target_sha,c.saved,out.rows,e)&&gizmos.tombstones(scope(c),out.retired,e);}
}
namespace alias_ports {
std::string failure;
unsigned party_capture=0,party_prepare=0,boss_prepare=0,scenery_capture=0,scenery_prepare=0;
mt2mp::game::party_snapshot_alias::Manifest party_source,party_prepared;
mt2mp::game::boss_snapshot_alias::Manifest boss_prepared;
mt2mp::game::scenery_scene_snapshot_alias::Manifest scenery_prepared;
bool identity(const std::string& id,const std::string& digest,std::string& error){
    if(id!=birth||digest!=fixture_digest){error="fixture alias exact snapshot UUID/SHA differs";return false;}return true;
}
bool entered(const char* operation,std::string& error){
    if(failure==operation){error=std::string("original ")+operation+" failure";return false;}return true;
}
void clear(){failure.clear();party_capture=party_prepare=boss_prepare=scenery_capture=scenery_prepare=0;
    party_source={};party_source.present=true;const auto record=fixture_digest+":world:party:1";party_source.rows.push_back({record,record});
    party_prepared={};boss_prepared={};scenery_prepared={};
}
}
namespace party_saved_identity_runtime {
using Manifest=mt2mp::game::party_snapshot_alias::Manifest;
bool aliases(const std::string& id,const std::string& digest,Manifest& out,std::string& error){
    ++alias_ports::party_capture;out={};if(!alias_ports::identity(id,digest,error)||!alias_ports::entered("Party capture",error))return false;
    const auto& expected=alias_ports::party_source;
    if(!mt2mp::game::party_snapshot_alias::valid(expected,digest,error))return false;
    if(expected.present){if(!mt2mp::game::party_snapshot_alias::decode(mt2mp::game::party_snapshot_alias::encode(expected),digest,out,error))return false;}
    if(out!=expected){error="fixture Party capture complete alias readback differs";return false;}return true;
}
bool prepare_aliases(const std::string& id,const std::string& digest,uint64_t bytes,const Manifest& input,std::string& error){
    ++alias_ports::party_prepare;if(!alias_ports::identity(id,digest,error)||!alias_ports::entered("Party admission",error))return false;
    if(bytes!=77){error="fixture Party admission exact file length differs";return false;}
    if(!mt2mp::game::party_snapshot_alias::valid(input,digest,error))return false;
    Manifest decoded;if(input.present){if(!mt2mp::game::party_snapshot_alias::decode(mt2mp::game::party_snapshot_alias::encode(input),digest,decoded,error))return false;
        if(decoded!=input){error="fixture Party admission complete alias readback differs";return false;}}
    alias_ports::party_prepared=input;return true;
}
}
namespace boss_saved_identity_runtime {
using Manifest=mt2mp::game::boss_snapshot_alias::Manifest;
bool prepare_aliases(const std::string& id,const std::string& digest,uint64_t bytes,const Manifest& input,std::string& error){
    ++alias_ports::boss_prepare;if(!alias_ports::identity(id,digest,error)||!alias_ports::entered("Boss admission",error))return false;
    if(bytes!=77){error="fixture Boss admission exact file length differs";return false;}
    if(!mt2mp::game::boss_snapshot_alias::valid(input,digest,error))return false;
    Manifest decoded;if(input.present){if(!mt2mp::game::boss_snapshot_alias::decode(mt2mp::game::boss_snapshot_alias::encode(input),digest,decoded,error))return false;
        if(decoded!=input){error="fixture Boss admission complete alias readback differs";return false;}}
    alias_ports::boss_prepared=input;return true;
}
}
namespace remove_diagnostics::scene_scenery {
using Manifest=mt2mp::game::scenery_scene_snapshot_alias::Manifest;
using Census=mt2mp::game::dungeon_scene_identity_runtime::Census;
bool census_matches(const Census& census,std::string& error){
    if(!scene_census||&census!=&*scene_census||!census.snapshot_census||census.world.uuid!=birth||census.world.sha256!=fixture_digest){error="fixture Scenery exact original census/file differs";return false;}return true;
}
bool capture_saved(const Census& census,Manifest& out,std::string& error){
    ++alias_ports::scenery_capture;out={};if(!census_matches(census,error)||!alias_ports::entered("Scenery capture",error))return false;
    Manifest next;next.present=true;next.root_present=true;
    for(const auto& row:census.scenery_saved)next.rows.push_back({row.record,row.record});
    std::sort(next.rows.begin(),next.rows.end(),[](const auto& a,const auto& b){return a.record<b.record;});
    if(!mt2mp::game::scenery_scene_snapshot_alias::valid(next,census.world.sha256,error))return false;
    Manifest decoded;if(!mt2mp::game::scenery_scene_snapshot_alias::decode(mt2mp::game::scenery_scene_snapshot_alias::encode(next),census.world.sha256,decoded,error))return false;
    if(decoded!=next){error="fixture Scenery capture complete alias readback differs";return false;}out=std::move(decoded);return true;
}
bool prepare_saved(const Census& census,const Manifest& input,std::string& error){
    ++alias_ports::scenery_prepare;if(!census_matches(census,error)||!alias_ports::entered("Scenery admission",error))return false;
    if(!input.present){error="fixture Scenery admission has no complete alias manifest";return false;}
    if(!mt2mp::game::scenery_scene_snapshot_alias::valid(input,census.world.sha256,error))return false;
    std::set<std::string> expected,actual;
    for(const auto& row:census.scenery_saved)if(!expected.insert(row.record).second){error="fixture Scenery original census repeats a saved member";return false;}
    for(const auto& row:input.rows)actual.insert(row.record);
    if(expected!=actual){error="fixture Scenery admission omits or adds an original member";return false;}
    alias_ports::scenery_prepared=input;return true;
}
}
attachment::Manifest prepared,captured;
opened::CurrentWorld origin,current;
std::map<uint64_t,uintptr_t> uid_registry;
bool healthy(std::string& e){e=failure;return failure.empty();}
bool reject(std::string& e,const std::string& cause){failure=cause;e=cause;return false;}
template<class Left,class Right>bool same_world(const Left& a,const Right& b){return a.uuid==b.uuid&&a.world_epoch==b.world_epoch&&a.generation==b.generation&&a.map==b.map;}
unsigned long exact_uid(uint64_t uid,uintptr_t& out){out=uid_registry[uid];return 0;}
namespace native_open_runtime {bool read_current_world(opened::CurrentWorld& out,std::string&){out=current;if(world_changed)++out.generation;return true;}}
#include "saved_monster_seal_actual.inc"
void need(bool value,const std::string& cause){++assertions;if(!value)throw std::runtime_error(cause);}
void setup(){
    bound_context=9;uuid=birth;alias_ports::clear();
    memory={};failure.clear();observed.clear();pools.clear();prepared={};captured={};prepared_present=false;host_active=false;world_changed=false;scene_census.reset();gizmo_seals=0;export_failed=false;dungeon_scene_identity_runtime::gizmos={};
    direct_monster_identity::registry={};current={"local-native-origin",7,9,reinterpret_cast<void*>(map_address)};
    const auto root=map_address+0x1c0;
    memory.put(map_address,image+0x1742860);memory.put(root,image+0x174ad20);memory.put(root+0x48,int32_t(0));
    memory.put(map_address+0x230,image+0x16f04e0);memory.put(map_address+0x238,uintptr_t(0x600000));
    memory.put(map_address+0x240,int32_t(1));memory.put(map_address+0x244,int32_t(1));memory.put(0x600000,fixture_region);
    memory.put(map_address+0x358,uintptr_t(0));memory.put(map_address+0x360,int32_t(0));memory.put(map_address+0x364,int32_t(0));
    memory.put(fixture_region,image+0x1787930);memory.put(fixture_region+0x70,root);memory.put(fixture_region+0x834,int32_t(67));memory.put(fixture_region+0x988,fixture_manager);
    memory.put(fixture_manager,image+0x1714100);memory.put(fixture_manager+0x80,fixture_region);memory.put(fixture_manager+0x38,image+0x16e8b60);
    memory.put(fixture_manager+0x40,uintptr_t(0x700000));memory.put(fixture_manager+0x48,int32_t(2));memory.put(fixture_manager+0x4c,int32_t(2));
    memory.put(0x700000,zone_actor);memory.put(0x700008,direct_actor);
    for(const auto actor:{zone_actor,direct_actor}){memory.put(actor,image+0x16c0770);memory.put(actor+0x18,uint64_t(actor)+0x200000000);memory.put(actor+0x170,root);memory.put(actor+0x178,fixture_region);memory.put(actor+0x8d0,uintptr_t(actor==zone_actor?0x800000:0));memory.put(actor+0x8d8,uintptr_t(actor==zone_actor?0x900000:0));uid_registry[uint64_t(actor)+0x200000000]=actor;}
    q::Pool pool;std::string e;auto links=[](uintptr_t actor,int32_t& id,std::string&){id=actor==zone_actor?17:0;return true;};
    need(q::observe(memory,image,fixture_manager,links,pool,e),e);observed.emplace(fixture_manager,pool);
}
std::string record(){return fixture_digest+":world:monster:inline:region{67}/monsterManager/monster[1]";}
direct_monster_identity::Scope scope(){return {current.uuid,current.world_epoch,current.generation,map_address+0x1c0,fixture_region,fixture_manager};}
constexpr uintptr_t scene_region=0xa00000,scene_shard=0xb00000,scene_manager=0xc00000,scene_actor=0xd00000;
const std::string scene_record=fixture_digest+":world:monster:"+snapshot_monster_key::scene_suffix(67,3,9,0);
void add_scene(){
    current.uuid=birth;uuid=birth;scene_census.emplace();auto& c=*scene_census;c.snapshot_census=true;
    c.world.uuid=current.uuid;c.world.sha256=fixture_digest;c.world.generation=current.generation;c.world.world_epoch=current.world_epoch;c.world.map=current.map;
    mt2mp::game::dungeon_scene_identity_runtime::Pool pool;pool.managers[2]=scene_manager;
    pool.scene.exterior_region=67;pool.scene.exterior_slot=3;pool.scene.owner.instance_index=9;
    pool.scene.owner.key=fixture_digest+":world:building:17/instance/9";pool.scene.owner.shard=scene_shard;pool.scene.owner.region=scene_region;
    c.pools.push_back(pool);memory.put(scene_region+0x70,scene_shard);memory.put(scene_manager,image+0x1714100);memory.put(scene_manager+0x80,scene_region);
    memory.put(scene_manager+0x38,image+0x16e8b60);memory.put(scene_manager+0x40,uintptr_t(0xe00000));memory.put(scene_manager+0x48,int32_t(1));memory.put(scene_manager+0x4c,int32_t(1));memory.put(0xe00000,scene_actor);
    memory.put(scene_actor,image+0x16c0770);memory.put(scene_actor+0x18,uint64_t(0x400000012));memory.put(scene_actor+0x170,scene_shard);memory.put(scene_actor+0x178,scene_region);
    memory.put(scene_actor+0x8d0,uintptr_t(0));memory.put(scene_actor+0x8d8,uintptr_t(0));uid_registry[0x400000012]=scene_actor;
    q::Pool observed_scene;std::string e;auto links=[](uintptr_t,int32_t& id,std::string&){id=0;return true;};
    need(q::observe(memory,image,scene_manager,links,observed_scene,e),e);observed.emplace(scene_manager,observed_scene);
    need(dungeon_scene_identity_runtime::gizmos.seal(dungeon_scene_identity_runtime::scope(c),{},nullptr,e),e);
}
void scene_tests(){
    std::string e,key;direct_monster_identity::Entry entry;
    setup();add_scene();const direct_monster_identity::Scope local{current.uuid,current.world_epoch,current.generation,map_address+0x1c0,scene_region,scene_manager,scene_census->pools.at(0).scene.owner.key};
    need(seal_world(current,false,e),e);need(pools.size()==2&&captured.rows.size()==2&&gizmo_seals==1,"Complete root/Instance native pools or Gizmo seal were lost");
    need(direct_monster_identity::registry.resolve(local,scene_actor,0x400000012,key,e)&&key==scene_record,"Dungeon original saved record did not bind full UID/scene owner");
    setup();add_scene();prepared_present=true;prepared={fixture_digest,77,{{67,scene_record,"action:22222222-2222-4222-8222-222222222222"},{67,record(),"action:"+birth}}};
    need(seal_world(current,false,e),e);need(direct_monster_identity::registry.resolve(local,scene_actor,0x400000012,key,e)&&key=="action:22222222-2222-4222-8222-222222222222","Dungeon confirmed action identity lost on native load");
    for(int mode=0;mode<8;++mode){setup();add_scene();
        if(mode==0)observed.erase(scene_manager);if(mode==1)scene_census->world.uuid="foreign";
        if(mode==2)memory.put(scene_actor+0x170,map_address+0x1c0);if(mode==3)uid_registry[0x400000012]=direct_actor;
        if(mode==4)memory.put(scene_manager+0x80,fixture_region);if(mode==5)memory.put(scene_manager+0x48,int32_t(0));
        if(mode==6)scene_census->pools.push_back(scene_census->pools[0]);if(mode==7)world_changed=true;
        need(!seal_world(current,false,e),"Invalid Dungeon native pool accepted mode="+std::to_string(mode));
        need(!direct_monster_identity::registry.resolve(local,scene_actor,0x400000012,key,e),"Failed Dungeon seal committed a live target");
    }
    setup();add_scene();host_active=true;export_failed=true;need(!seal_world(current,false,e)&&e=="original Gizmo alias export failure","Gizmo alias failure was hidden by Monster seal");
    setup();add_scene();scene_census.reset();need(!seal_world(current,false,e),"Unclassified actual Dungeon Monster pool silently dropped");
    setup();add_scene();memory.put(scene_actor+0x8d0,uintptr_t(0xf00000));need(seal_world(current,false,e)&&captured.rows.size()==1,"Dungeon Zone-owned Monster became a direct target");
}
void tests(){
    std::string e,key;direct_monster_identity::Entry entry;
    setup();need(seal_world(current,false,e),e);need(captured.rows.size()==1&&captured.rows[0].record==record(),"Zone row lost original ordinal or became direct");
    need(direct_monster_identity::registry.resolve(scope(),direct_actor,uint64_t(direct_actor)+0x200000000,key,e)&&key==record(),"legacy native record binding absent");
    setup();prepared_present=true;prepared={fixture_digest,77,{{67,record(),"action:"+birth}}};need(seal_world(current,false,e),e);
    need(direct_monster_identity::registry.lookup(birth,scope(),entry,e)&&entry.uid==uint64_t(direct_actor)+0x200000000,"action birth not restored to full local UID");
    setup();host_active=true;need(direct_monster_identity::registry.stage(birth,scope(),direct_actor,uint64_t(direct_actor)+0x200000000,e)&&direct_monster_identity::registry.confirm(birth,e),e);
    need(seal_world(current,false,e)&&captured.rows[0].key=="action:"+birth,"Host capture changed action key");
    for(int mode=0;mode<7;++mode){setup();prepared_present=true;prepared={fixture_digest,77,{{67,record(),"action:"+birth}}};
        if(mode==0)prepared.rows.clear();if(mode==1)prepared.rows[0].region=68;if(mode==2)prepared.rows.push_back({67,fixture_digest+":world:monster:19",fixture_digest+":world:monster:19"});
        if(mode==3)uid_registry[uint64_t(direct_actor)+0x200000000]=zone_actor;if(mode==4)memory.put(direct_actor+0x8d8,uintptr_t(1));
        if(mode==5)world_changed=true;if(mode==6)memory.put(fixture_manager+0x80,fixture_region+1);
        need(!seal_world(current,false,e),"invalid seal accepted mode="+std::to_string(mode));
        need(!direct_monster_identity::registry.lookup(birth,scope(),entry,e),"failed seal partially committed registry");
    }
    setup();memory.put(direct_actor+0x8d0,uintptr_t(0x800000));need(seal_world(current,false,e)&&captured.rows.empty(),"zero direct roster rejected");
}
}
namespace fixture {
bool opened::read_completed_world(uint64_t expected,CurrentWorld& out,std::string& error){if(expected!=current.generation){error="original completed generation differs";return false;}out=current;return true;}
}
namespace network_fixture {
using fixture::current;
namespace opened=fixture::opened;
namespace aliases=mt2mp::game::network_snapshot_alias;
struct Roots{void* map=nullptr;};
struct WriterBinding{};
struct Pending{std::vector<int> nodes;WriterBinding file_binding;void* property=nullptr;void* manager=nullptr;void* writer=nullptr;void* context=nullptr;};
struct Shared{std::mutex mutex;std::string state="idle",role,uuid,sha,error,invalidation_reason,keys_digest;
    bool original_load=false;uint32_t expected=0;size_t pending_nodes=0;void* client_load_mode=nullptr;Roots roots;
    std::map<int,int> context_owners,bindings,pipe_bindings,shards,contexts;std::map<int,Pending> pending;};
Shared shared_value;Shared& shared(){return shared_value;}
bool capture_active=false,client_active=false,gate_failure=false;uint64_t client_generation=0;constexpr uint32_t maximum_nodes=65536;
std::string prepared_alias_uuid,prepared_alias_sha;aliases::Manifest prepared_aliases;
mt2mp::game::network_object_identity::Registry object_registry;
bool valid_uuid(const std::string& value){return mt2mp::game::network_object_identity::uuid(value);}
bool valid_sha(const std::string& value){return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string::npos;}
bool fail_locked(Shared& value,const std::string& cause,std::string* error){value.state="failed";value.error=cause;client_active=false;if(error)*error=cause;return false;}
void reset(Shared& value,const std::string& id,const std::string& expected_sha,uint32_t expected,const std::string& role){value.uuid=id;value.sha=expected_sha;value.expected=expected;value.role=role;value.error.clear();value.original_load=false;value.bindings.clear();}
bool main_gate(Shared& value,bool,std::string& error,bool=false,bool=false){return !gate_failure||fail_locked(value,"original Network Mainthread gate failed",&error);}
namespace building_mixed_identity::opened_file{using CurrentWorld=fixture::opened::CurrentWorld;}
namespace native_open_runtime{bool read_current_world(opened::CurrentWorld& out,std::string& error){return fixture::native_open_runtime::read_current_world(out,error);}}
bool roots_read(Roots& roots,std::string&){roots.map=current.map;return true;}
bool publish(Shared& value,const Roots& roots,std::string& error){value.roots=roots;value.state="client_ready";client_active=false;error.clear();return true;}
#include "saved_network_begin_load_actual.inc"
#include "saved_network_original_load_actual.inc"
#include "saved_network_invalidate_actual.inc"
void clear(){capture_active=client_active=gate_failure=false;shared_value.state="idle";shared_value.role.clear();shared_value.uuid.clear();shared_value.sha.clear();shared_value.error.clear();shared_value.pending.clear();shared_value.original_load=false;prepared_alias_uuid.clear();prepared_alias_sha.clear();prepared_aliases={};}
}
namespace fixture {
namespace network_identity=::network_fixture;
std::mutex gate;bool hooks=true,client_active=false,emergency=false;uint32_t main_thread=17;uint64_t generation=0,prepared_bytes=0;
std::string prepared_uuid,prepared_sha;
unsigned GetCurrentThreadId(){return 17;}
namespace net {
struct SnapshotFileHandle{bool open(const std::filesystem::path&,DWORD,DWORD,std::string& error){error="unexpected companion file in load lifecycle fixture";return false;}HANDLE get()const{return INVALID_HANDLE_VALUE;}bool close(std::string*){return true;}};
bool snapshot_file_length(HANDLE,uint64_t&,std::string& error){error="unexpected companion length in load lifecycle fixture";return false;}
}
#include "saved_monster_reset_actual.inc"
#include "saved_monster_prepare_actual.inc"
#include "saved_monster_load_finish_actual.inc"
void load_lifecycle_tests(){
    const auto absent=std::filesystem::path(__FILE__).parent_path()/"load-lifecycle-absent/native-snapshot.dat";
    std::string error;const auto initial=[&]{setup();network_identity::clear();host_active=client_active=false;prepared_uuid.clear();prepared_sha.clear();prepared_bytes=0;
        current.uuid=birth;current.sha256=fixture_digest;opened::pinned={"pinned",birth,fixture_digest,true,current.generation,77};};
    initial();need(prepare_load(absent,birth,fixture_digest,77,error),error);
    need(!network_identity::client_active&&network_identity::prepared_alias_uuid==birth,"Preparation started original Network load before verified-local-load invalidation");
    network_identity::invalidate_world("verified-local-load");
    need(arm_client(birth,fixture_digest,error),error);
    need(client_active&&network_identity::client_active&&network_identity::shared().original_load,"Exact arm did not start Network original load after invalidation");
    // The existing actual native property fixture supplies complete root and
    // Instance observations after the original NativeOpen serialization.
    setup();current.uuid=birth;current.sha256=fixture_digest;add_scene();
    need(seal_client(error),error);
    need(!client_active&&!network_identity::client_active&&network_identity::shared().state=="client_ready"&&pools.size()==2&&gizmo_seals==1,
        "Original completed load failed to publish Network and complete Dungeon scene bindings");
    for(int mode=0;mode<4;++mode){initial();need(prepare_load(absent,birth,fixture_digest,77,error),error);network_identity::invalidate_world("verified-local-load");
        if(mode==0)prepared_uuid.clear();if(mode==1)prepared_uuid="22222222-2222-4222-8222-222222222222";if(mode==2)prepared_sha=std::string(64,'b');if(mode==3)prepared_bytes=78;
        need(!arm_client(birth,fixture_digest,error)&&error=="Monster load lacks exact prepared companion decision"&&!client_active&&!network_identity::client_active,
            "Missing/wrong exact prepared UUID/SHA/bytes started an original load mode="+std::to_string(mode));
    }
    initial();need(prepare_load(absent,birth,fixture_digest,77,error),error);network_identity::invalidate_world("verified-local-load");network_identity::gate_failure=true;
    need(!arm_client(birth,fixture_digest,error)&&error=="original Network Mainthread gate failed"&&!client_active,"Network begin failure was hidden or Monster observer armed anyway");
    initial();need(prepare_load(absent,birth,fixture_digest,77,error),error);network_identity::invalidate_world("verified-local-load");opened::pinned.bytes=78;
    need(!arm_client(birth,fixture_digest,error)&&!client_active&&!network_identity::client_active,"NativeOpen pinned byte count did not match exact prepared file");
}
void alias_port_tests(){
    std::string error;direct_monster_identity::Entry entry;
    setup();need(seal_world(current,false,error),error);
    need(alias_ports::party_capture==1&&captured.parties==alias_ports::party_source&&captured.parties.rows.size()==1,"actual seal lost complete Party aliases");
    for(const std::string operation:{"Party capture","Scenery capture","Scenery admission"}){
        setup();add_scene();host_active=true;alias_ports::failure=operation;
        need(!seal_world(current,false,error)&&error=="original "+operation+" failure"&&failure==error&&pools.empty()&&captured.rows.empty(),"actual seal hid original alias failure or published success");
        need(!direct_monster_identity::registry.lookup(birth,scope(),entry,error),"failed alias capture/admission committed direct Monster bindings");
    }
    setup();add_scene();const auto root_record=mt2mp::game::scenery_scene_key::root_record(fixture_digest,67,8,0);
    scene_census->scenery_saved.push_back({root_record,"world",0x100,0x200,0x300,0x400,0x500});
    need(seal_world(current,false,error)&&alias_ports::scenery_prepare==1&&captured.scene_scenery==alias_ports::scenery_prepared&&captured.scene_scenery.rows.size()==1,"actual seal dropped populated original Root Scenery aliases");
    setup();add_scene();prepared_present=true;prepared={fixture_digest,77,{{67,scene_record,scene_record},{67,record(),record()}}};
    prepared.scene_scenery.present=true;prepared.scene_scenery.root_present=true;
    scene_census->scenery_saved.push_back({root_record,"world",0x100,0x200,0x300,0x400,0x500});
    need(!seal_world(current,false,error)&&error=="fixture Scenery admission omits or adds an original member"&&pools.empty(),"actual seal accepted incomplete prepared Scenery aliases");
    const auto absent=std::filesystem::path(__FILE__).parent_path()/"load-lifecycle-absent/native-snapshot.dat";
    for(const std::string operation:{"Party admission","Boss admission"}){
        setup();network_identity::clear();prepared_uuid.clear();prepared_sha.clear();prepared_bytes=0;alias_ports::failure=operation;
        need(!prepare_load(absent,birth,fixture_digest,77,error)&&error=="original "+operation+" failure"&&prepared_uuid.empty()&&prepared_bytes==0&&!client_active,"actual prepare hid alias admission failure or armed a load");
        if(operation=="Party admission")need(alias_ports::boss_prepare==0,"failed Party admission continued to Boss admission");
    }
}
}

int main()try{fixture::tests();fixture::scene_tests();fixture::load_lifecycle_tests();fixture::alias_port_tests();std::cout<<fixture::assertions<<" actual Monster root/Dungeon seal, ownership, companion and atomic world comparisons passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
