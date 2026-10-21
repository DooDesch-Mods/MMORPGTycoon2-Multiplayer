#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <limits>
#include <functional>
#include "multiplayer_world/world_state.h"

namespace identity_refund_fixture {
constexpr uintptr_t fixture_image=0x10000000,fixture_selection=0x20000000,fixture_addresspoint=fixture_image+0x16d93c0;
constexpr const char* executable="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
struct World {
    void* game=reinterpret_cast<void*>(0x31000000);void* mode=reinterpret_cast<void*>(0x32000000);void* state=reinterpret_cast<void*>(0x33000000);
    void* world=reinterpret_cast<void*>(0x30000000);
    void* map=reinterpret_cast<void*>(0x40000000);
    uint64_t world_epoch=41,generation=7;
    std::string uuid=::uuid,sha256=::fixture_sha;
    bool operator==(const World&)const=default;
};
namespace prop_lease {
struct World {
    uint64_t epoch=0,origin_generation=0;void* world=nullptr;void* map=nullptr;
    bool operator==(const World&)const=default;
};
struct Prop {uint64_t uid=0;void* registry_object=nullptr;uintptr_t class_addresspoint=0;};
}
namespace native {
struct BuildingState {
    struct Position {uint64_t uid=0;float x=0,y=0,z=0;} prop;
    std::string type="SmallInn",variant="base";
    std::array<float,4> rotation{0,0,0,1};std::array<float,3> scale{1,1,1};
};
}
struct LocalTarget {
    uintptr_t manager=0,object=0;uint64_t uid=0,world_epoch=0,world_generation=0;
    std::string snapshot_id,record_key,type,variant;bool normal_building=false,normal_scenery=false;
    std::string kind_cause;
};
#include "identity_refund_split_type.inc"
std::map<std::string,Bound> registry;
uint64_t registry_generation=1,next_entry=0;
bool unknown_locked=false;
prop_lease::World admitted_world;
World live_world;
std::map<uint64_t,native::BuildingState> live;
std::map<uint64_t,unsigned> settlement_reads;
unsigned refund_calls=0,registered_reads=0,prop_reads=0,world_reads=0,logs=0,seals=0;
std::vector<uint64_t> refund_uids;
uint64_t refund_failure=0,identity_failure=0;
bool world_failure=false,heap_failure=false,lookup_failure=false,class_failure=false,prop_failure=false;
bool change_world_on_target=false,change_world_on_lookup=false;
bool scene_target=false,scene_owner_failure=false,scene_owner_changed=false;
bool saved_only=false,saved_owner_failure=false;unsigned saved_prop_reads=0;int fixture_saves=0;
constexpr uintptr_t fixture_root=0x400001c0,fixture_saved_region=0x61000000;
uintptr_t lookup_override=0;
std::string fixture_session_snapshot=::uuid;
constexpr uintptr_t object(uint64_t uid){return 0x50000000+uintptr_t(uid)*0x100;}
std::string key(uint64_t uid){return ::fixture_sha+":world:building:"+std::to_string(uid);}
bool fail(std::string& e,const char* cause){e=cause;return false;}
template<class... Args>void capture_log(Args&&...){++logs;}
#define LOG_INFO(...) capture_log(__VA_ARGS__)
#define LOG_ERROR(...) capture_log(__VA_ARGS__)
bool current(World& out,std::string& e){++world_reads;if(world_failure)return fail(e,"fixture current world failed");out=live_world;return true;}
namespace host {
namespace opened {using CurrentWorld=identity_refund_fixture::World;}
struct WorldState {opened::CurrentWorld origin;uintptr_t selection=identity_refund_fixture::fixture_selection,root=fixture_root;};
struct ProcessMemory {
    bool copy(uintptr_t p,void* to,size_t size,std::string& e){
        for(const auto& [uid,unused]:live){(void)unused;
            if(p==object(uid)+0x18&&size==sizeof(uid)){std::memcpy(to,&uid,size);return true;}
            uintptr_t value=0;bool found=true;
            if(p==object(uid))value=scene_target&&uid==101?fixture_image+0x16c1390:fixture_addresspoint;
            else if(p==object(uid)+0x170)value=fixture_root;
            else if(p==object(uid)+0x178)value=fixture_saved_region;
            else found=false;
            if(found&&size==sizeof value){std::memcpy(to,&value,size);return true;}
        }
        if(p==fixture_saved_region+0x834&&size==sizeof(int32_t)){const int32_t id=59;std::memcpy(to,&id,size);return true;}
        return fail(e,"fixture target UID unreadable");
    }
};
inline std::string batch_final_failure;
struct RootSceneryReadBatch {
    opened::CurrentWorld expected=live_world;
    bool finish(std::string& error){
        if(!batch_final_failure.empty()){error=batch_final_failure;return false;}
        if(expected!=live_world){error="fixture readonly batch complete World changed";return false;}
        return true;
    }
};
struct NativeOrigin{};struct LookupApi{};
bool resolve_lookup(ProcessMemory&,uintptr_t,const char*,LookupApi&,std::string& e){
    return !lookup_failure||fail(e,"fixture lookup guard failed");
}
bool same_world(const opened::CurrentWorld& a,const opened::CurrentWorld& b){return a==b;}
struct Reader {
    Reader(ProcessMemory&,NativeOrigin&,uintptr_t,uint32_t,LookupApi){}
    bool current(WorldState& out,bool idle,std::string& e){if(idle&&fixture_saves)return fail(e,"info: remove_wait_save_edit_or_graph");return identity_refund_fixture::current(out.origin,e);}
    bool prop(void* receiver,uint64_t uid,prop_lease::Prop& out,std::string& e){
        ++prop_reads;
        if(saved_only&&uid==101)return fail(e,"remove_region_absent_root_map");
        if(prop_failure)return fail(e,"fixture current target membership failed");
        if(!live.contains(uid)||receiver!=reinterpret_cast<void*>(object(uid)))return fail(e,"fixture target full UID mismatch");
        out={uid,receiver,scene_target&&uid==101?fixture_image+0x16c1390:fixture_addresspoint};return true;
    }
    bool saved_root_scenery(void* receiver,uint64_t uid,int32_t id,uintptr_t region,prop_lease::Prop& out,std::string& e){
        ++saved_prop_reads;
        if(saved_owner_failure)return fail(e,"original saved Root Scenery owner changed");
        if(prop_failure)return fail(e,"fixture current target membership failed");
        if(!scene_target||uid!=101||!live.contains(uid)||receiver!=reinterpret_cast<void*>(object(uid))||id!=59||region!=fixture_saved_region)
            return fail(e,"fixture saved Root Scenery exact context differs");
        out={uid,receiver,fixture_image+0x16c1390};return true;
    }
};
}
namespace native_free_runtime {
struct Receipt {
    uintptr_t image=identity_refund_fixture::fixture_image;uint32_t main_thread=7;
    struct Executable {std::array<char,65> sha256{};Executable(){std::memcpy(sha256.data(),identity_refund_fixture::executable,64);}} executable;
};
enum class Phase {Idle,Enabled};
struct Runtime {
    struct Status {Phase phase=Phase::Enabled;};
    Status status()const{return {};}
    bool prepare_and_enable(std::string& e){return !heap_failure||fail(e,"fixture heap receipt failed");}
    static Runtime& instance(){static Runtime value;return value;}
    bool read_actual_receipt(Receipt& out,std::string& e){if(heap_failure)return fail(e,"fixture heap receipt failed");out={};return true;}
};
}
namespace native {
bool read_resolver_receipt(uintptr_t& out_image,uint32_t& thread,std::string& hash,std::string&){out_image=fixture_image;thread=7;hash=executable;return true;}
}
namespace scenery_kiss {
bool read_action_prop(uint64_t uid,native::BuildingState& out,std::string& e){
    ++settlement_reads[uid];
    if(uid==identity_failure||!live.contains(uid))return fail(e,"fixture live building identity failed");
    out=live.at(uid);
    if(change_world_on_target&&uid==101)++live_world.world_epoch;
    return true;
}
bool metadata_matches(const json& m,const native::BuildingState& b){
    return m.at("type")==b.type&&m.at("variant")==b.variant&&m.at("x")==b.prop.x&&m.at("y")==b.prop.y&&m.at("z")==b.prop.z&&
        (!m.contains("rotation")||m.at("rotation")==b.rotation)&&(!m.contains("scale")||m.at("scale")==b.scale);
}
}
bool lookup(host::LookupApi,uint64_t uid,void*& out,uint32_t& seh){
    if(change_world_on_lookup)++live_world.generation;
    seh=0;out=live.contains(uid)?reinterpret_cast<void*>(lookup_override?lookup_override:object(uid)):nullptr;return true;
}
bool classify_normal_building(host::ProcessMemory&,uintptr_t,const prop_lease::Prop& prop,const native::BuildingState&,
    bool& normal,bool& scenery,std::string& cause,std::string& e){
    normal=prop.class_addresspoint==fixture_addresspoint;scenery=prop.class_addresspoint==fixture_image+0x16c1390;cause.clear();return !class_failure||fail(e,"fixture target definition failed");
}
namespace modular_identity {template<class Read>bool read(Read&,uintptr_t,uintptr_t,const native::BuildingState&,std::string& e){return fail(e,"fixture Modular path is not configured");}}
bool dungeon_building_target(host::ProcessMemory&,uintptr_t,uintptr_t,uintptr_t,const native::BuildingState&,const std::string&,std::string& e){return fail(e,"fixture Dungeon path is not configured");}
#include "root_metadata_prop_actual.inc"
#include "registered_metadata_actual.inc"
bool read_registered_building(uint64_t uid,host::opened::CurrentWorld& world,native::BuildingState& building,
    uintptr_t& target,uintptr_t& ap,std::string& e,bool readonly){
    ++registered_reads;return registered_metadata_actual(uid,world,building,target,ap,e,readonly);
}
namespace native_refund {
struct Receipt {bool attempted=false,returned=false;int32_t value=0;uint32_t seh=0;uintptr_t getter=0;};
bool read(host::ProcessMemory&,uintptr_t,uint32_t,uintptr_t,uint64_t uid,uintptr_t,Receipt& out,std::string& e){
    ++refund_calls;refund_uids.push_back(uid);out={true,false,0,0,fixture_image+0xd314f0};
    if(uid==refund_failure)return fail(e,"fixture foreign refund getter failed");
    out.returned=true;out.value=20;return true;
}
}
struct Imported {int32_t link_id=0;uint64_t local_uid=0;std::string record_key;native::BuildingState building;};
struct SourceStatus {
    bool bindings_confirmed=true,world_witness_current=true,load_pending=false;
    std::string capture_uuid=::uuid,snapshot_uuid=::uuid,snapshot_sha256=::fixture_sha,state="confirmed";
    uint64_t snapshot_bytes=400,session_epoch=41;
    std::vector<Imported> buildings;
};
SourceStatus captured,fixture_loaded,sealed;
namespace building_capture_identity {
using Status=SourceStatus;
enum class SealResult {Pending,Confirmed,Failed};
SealResult seal_result=SealResult::Confirmed;
Status status(){return captured;}
SealResult seal(const std::string&,const std::string&,uint64_t,uint64_t,Status& out,std::string& e){
    ++seals;out=sealed;if(seal_result==SealResult::Failed)e="fixture source seal failed";return seal_result;
}
}
namespace normal_load_identity {SourceStatus status(){return fixture_loaded;}}
namespace snapshot_building_key {bool valid(const std::string& value){return mt2mp::game::snapshot_building_key::valid(value);}}
namespace net {std::string current_snapshot_id(){return fixture_session_snapshot;}}
namespace dungeon_scene_identity_runtime {using World=identity_refund_fixture::World;}
namespace scene_scenery {
struct Row{std::string scene;};
unsigned append_calls=0;std::function<bool(json&,std::string&)> append_fixture;
bool append_saved(json& rows,std::string& error){++append_calls;return !append_fixture||append_fixture(rows,error);}
bool owner(uintptr_t pointer,uint64_t uid,Row& row,dungeon_scene_identity_runtime::World& world,std::string& error){
    if(scene_owner_failure)return fail(error,"original Scene Scenery reciprocal owner failure");
    if(pointer!=object(uid)||!live.contains(uid))return fail(error,"original Scene Scenery full UID differs");
    row.scene=scene_owner_changed?"other":saved_only?"world":"confirmed-template";world=live_world;return true;
}
}
namespace remove_diagnostics {using LocalTarget=identity_refund_fixture::LocalTarget;}
namespace service_building_census {
#include "service_census_actual.inc"
}
bool attach_refunds(const host::opened::CurrentWorld&,json&,std::string&);
bool resolve_local_target(uintptr_t,uintptr_t,LocalTarget&,std::string&);
#include "identity_refund_split_bodies.inc"
#include "building_identity_census_actual.inc"
namespace remove_diagnostics {
bool resolve_identity_destination(uintptr_t p,LocalTarget& out,std::string& e){return identity_refund_fixture::resolve_identity_destination(p,out,e);}
}
#include "service_destination_actual.inc"
#undef LOG_INFO
#undef LOG_ERROR

void reset(){
    scene_scenery::append_calls=0;scene_scenery::append_fixture={};
    saved_only=saved_owner_failure=false;saved_prop_reads=0;fixture_saves=0;
    registry.clear();live.clear();settlement_reads.clear();registry_generation=1;next_entry=0;unknown_locked=false;
    admitted_world={};live_world={};refund_calls=registered_reads=prop_reads=world_reads=logs=seals=0;refund_uids.clear();
    refund_failure=identity_failure=0;world_failure=heap_failure=lookup_failure=class_failure=prop_failure=false;
    change_world_on_target=change_world_on_lookup=false;scene_target=scene_owner_failure=scene_owner_changed=false;lookup_override=0;fixture_session_snapshot=::uuid;
    captured={};fixture_loaded={};sealed={};building_capture_identity::seal_result=building_capture_identity::SealResult::Confirmed;
    for(uint64_t uid=101;uid<=104;++uid){
        native::BuildingState b;b.prop={uid,float(uid),1,2};live.emplace(uid,b);
        captured.buildings.push_back({int32_t(uid),uid,key(uid),b});
    }
    fixture_loaded=sealed=captured;
    json rows=json::array();std::string e;
    admitted_world={live_world.world_epoch,live_world.generation,live_world.world,live_world.map};
    need(import_bindings(captured.buildings,::uuid,::fixture_sha,admitted_world,rows,e),"fixture actual registry import failed");
}
bool target(LocalTarget& out,std::string& e,bool by_key){
    return by_key?resolve_record_target(::uuid,key(101),out,e):resolve_local_target(fixture_selection,object(101),out,e);
}
#include "root_bind_continuation_cases.inc"
#include "building_identity_cases.inc"
#include "party_owner_cases.inc"
#include "construction_target_cases.inc"

void checks(){
    std::string e;LocalTarget out;json report;
    reset();host::batch_final_failure="original final complete Root census failed";
    need(!bindings(report,e)&&e==host::batch_final_failure&&report==json::object(),"complete collector cannot publish when the batch final census fails");
    host::batch_final_failure.clear();reset();
    std::vector<LocalTarget> pending_targets;host::batch_final_failure="original final complete Root census failed";
    need(!identity_targets(pending_targets,e)&&e==host::batch_final_failure&&pending_targets.empty(),"identity census cannot publish targets before complete batch finalization");
    host::batch_final_failure.clear();

    for(bool by_key:{false,true}){
        reset();refund_failure=104;
        need(target(out,e,by_key)&&out.uid==101&&out.object==object(101)&&out.record_key==key(101),"identity target depended on foreign refund getter");
        need(refund_calls==0&&registered_reads==0&&prop_reads==1,"identity resolver entered refund enrichment or lost direct target membership");
        for(uint64_t uid=101;uid<=104;++uid)need(settlement_reads[uid]>0,"identity admission omitted live settlement");
        need(settlement_reads.at(104)==1&&settlement_reads.at(101)==2,"identity resolver repeated admission or omitted direct target state");
        reset();identity_failure=104;
        need(!target(out,e,by_key)&&e=="fixture live building identity failed"&&refund_calls==0,"foreign live identity failure was hidden");
        for(int mismatch=0;mismatch<5;++mismatch){
            reset();auto& bad=live.at(104);
            if(mismatch==0)bad.type="Tavern";if(mismatch==1)bad.variant="boat";
            if(mismatch==2)bad.prop.x+=1;if(mismatch==3)bad.prop.z+=1;
            if(mismatch==4)bad.prop.y=std::numeric_limits<float>::quiet_NaN();
            need(!target(out,e,by_key)&&e=="shared building identity changed during native height readback"&&refund_calls==0,"foreign live metadata failure was hidden");
        }
        reset();live.at(104).prop.y=12;
        need(target(out,e,by_key)&&registry.at(registry_id(::uuid,key(104))).metadata.at("y")==12&&logs==1,"identity collection lost allowed native height settlement");
        reset();unknown_locked=true;
        const char* unknown_cause=by_key?"remove diagnostics are Unknown; no new binding admission":"native UI target authority or pointer binding unavailable";
        need(!target(out,e,by_key)&&e==unknown_cause&&refund_calls==0,"Unknown admitted an identity target");
        reset();world_failure=true;
        need(!target(out,e,by_key)&&e=="fixture current world failed"&&refund_calls==0,"current-world failure was hidden");
        reset();change_world_on_target=true;
        need(!target(out,e,by_key)&&!e.empty()&&refund_calls==0,"world change during collection/target read admitted");
        for(int failure=0;failure<4;++failure){
            reset();if(failure==0)heap_failure=true;if(failure==1)lookup_failure=true;if(failure==2)class_failure=true;if(failure==3)prop_failure=true;
            const char* causes[]={"fixture heap receipt failed","fixture lookup guard failed","fixture target definition failed","fixture current target membership failed"};
            need(!target(out,e,by_key)&&e==causes[failure]&&refund_calls==0,"direct target guard or original cause was lost");
        }
        reset();registry.at(registry_id(::uuid,key(101))).revoked=true;
        need(!target(out,e,by_key)&&!e.empty(),"revoked target admitted");
        reset();registry.at(registry_id(::uuid,key(101))).unknown=true;
        need(!target(out,e,by_key)&&!e.empty(),"unknown target admitted");
        reset();auto duplicate=registry.at(registry_id(::uuid,key(101)));duplicate.key=key(999);
        registry.emplace(registry_id(::uuid,duplicate.key),duplicate);
        need(!target(out,e,by_key)&&e=="native UI target snapshot binding is ambiguous","ambiguous target admitted");
    }
    for(bool by_key:{false,true})for(unsigned failure=0;failure<3;++failure){reset();scene_target=true;
        auto& entry=registry.at(registry_id(::uuid,key(101)));entry.scene_owner="confirmed-template";entry.object=object(101);entry.class_addresspoint=fixture_image+0x16c1390;
        scene_owner_failure=failure==1;scene_owner_changed=failure==2;
        const bool accepted=target(out,e,by_key);
        if(!failure)need(accepted&&out.normal_scenery&&!out.normal_building&&refund_calls==0,"Actual Scene Scenery alias target resolves through existing identity owner");
        else need(!accepted&&!e.empty()&&refund_calls==0,"Scene Scenery stale/foreign reciprocal Scene rejected before any refund read");
        if(failure==1)need(e=="original Scene Scenery reciprocal owner failure","Original Scene Scenery owner failure cause retained");
    }
    reset();refund_failure=104;
    need(identity_bindings(report,e)&&refund_calls==0&&registered_reads==0&&report.at("bindings").size()==4,"complete public identity report must not execute foreign refund getters");
    for(uint64_t uid=101;uid<=104;++uid)need(settlement_reads[uid]==1,"public identity report retains each current building settlement");
    for(const auto& row:report.at("bindings"))need(!row.contains("native_refund"),"identity report must not fabricate refund receipts");
    reset();identity_failure=104;
    need(!identity_bindings(report,e)&&e=="fixture live building identity failed"&&refund_calls==0,"public identity report preserves failed current settlement");
    reset();unknown_locked=true;
    need(!identity_bindings(report,e)&&e=="remove diagnostics are Unknown; no new binding admission","public identity report preserves Unknown guard");
    reset();need(identity_bindings(report,e),"initial public identity report");live.at(104).prop.x+=1;
    need(!identity_bindings(report,e)&&e=="shared building identity changed during native height readback","public identity report revalidates later calls");
    reset();change_world_on_lookup=true;
    need(!resolve_record_target(::uuid,key(101),out,e)&&e=="native UI admitted world changed before target observation"&&prop_reads==0,"nested world replacement reused admission");
    reset();
    need(resolve_record_target(::uuid,key(101),out,e)&&resolve_record_target(::uuid,key(101),out,e)&&
        settlement_reads.at(104)==2&&prop_reads==2,"separate record lookups reused old admission");
    live.at(104).prop.x+=1;
    need(!resolve_record_target(::uuid,key(101),out,e)&&e=="shared building identity changed during native height readback","separate lookup missed changed foreign metadata");
    reset();lookup_override=object(102);
    need(!resolve_record_target(::uuid,key(101),out,e)&&e=="native UI record lookup resolved a different snapshot binding","record resolver accepted another current UID");
    reset();
    need(bindings(report,e)&&refund_calls==4&&registered_reads==8,"public report lost complete four-building refund enrichment");
    for(const auto& row:report.at("bindings"))need(row.at("native_refund").at("confirmed")==true&&row.at("native_refund").at("value")==20,"public refund result absent");
    reset();refund_failure=104;
    need(!bindings(report,e)&&e=="fixture foreign refund getter failed"&&refund_calls==4,"public report suppressed refund getter failure");
    need(report.at("bindings").back().at("native_refund").at("confirmed")==false,"failed public report fabricated confirmation");
    for(bool full:{false,true}){
        reset();registry.clear();admitted_world={};captured.capture_uuid="old";building_capture_identity::seal_result=building_capture_identity::SealResult::Failed;
        const auto mode=full?BindingDetails::Refund:BindingDetails::Identity;
        need(collect_bindings(report,e,mode)&&seals==0&&report.at("bindings").size()==4&&refund_calls==(full?4u:0u),"old capture blocked current load in complete collector");
        reset();registry.clear();admitted_world={};building_capture_identity::seal_result=building_capture_identity::SealResult::Failed;
        need(!collect_bindings(report,e,mode)&&e=="fixture source seal failed"&&refund_calls==0,"current source seal failure changed");
        reset();registry.clear();admitted_world={};sealed.buildings.front().local_uid=0;
        need(!collect_bindings(report,e,mode)&&e=="native snapshot binding lacks key or full UID"&&refund_calls==0,"actual import failure changed");
        reset();registry.clear();admitted_world={};captured.capture_uuid="old";fixture_loaded.buildings.clear();
        need(collect_bindings(report,e,mode)&&report.at("bindings").empty()&&report.at("baseline_snapshot_id")==::uuid&&refund_calls==0,"empty loaded baseline changed");
    }
    reset();const auto old_id=registry_id(::uuid,key(101));++live_world.world_epoch;
    captured.capture_uuid="old";fixture_loaded.snapshot_uuid=live_world.uuid;fixture_loaded.snapshot_sha256=live_world.sha256;
    need(collect_bindings(report,e,BindingDetails::Identity)&&registry_generation==2&&registry.at(old_id).revoked&&
        admitted_world.epoch==live_world.world_epoch&&refund_calls==0,"world transition lost generation/revocation/new import");
    reset();registry_generation=UINT64_MAX;++live_world.world_epoch;
    need(!collect_bindings(report,e,BindingDetails::Identity)&&e=="native snapshot registry generation exhausted","generation overflow guard changed");
    continuation_checks();
    building_identity_checks();
    party_owner_checks();
    construction_target_checks();
}
}
