#include "dungeon_scene_identity_runtime.h"
#include "scene_clone/raw.h"
#include "dungeon_template_owner.h"
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/net/snapshot_buffer.h"
#include "mt2mp/net/snapshot_sync.h"
#include "remove_diagnostics.h"
#include "scenery_scene_key.h"
#include "region_saved_identity.h"
#include "building_capture_identity.h"
#include "dungeon_scene_records.h"
#include "dungeon_scene_record_codes.h"
#include "subscriber_lifecycle/capture.h"
#include "live_host_state/memory_query_timing.h"
#include "typed_action_runtime.h"
#include <Windows.h>
#include <mutex>

namespace mt2mp::game::native_open_runtime {
bool read_current_world(building_mixed_identity::opened_file::CurrentWorld&,std::string&);
}
namespace mt2mp::game::dungeon_scene_identity_runtime {
namespace {
namespace host=mt2mp::build_actions::private_native::host;
namespace identity=dungeon_gizmo_identity;
identity::Registry registry;identity::Scope bound;std::mutex gate;
constexpr const char* executable="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
uint32_t original_find(host::FindUid call,uint64_t uid,uintptr_t& found)noexcept{
    __try{found=uintptr_t(call(uid));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t original_cast(host::DynamicCast call,uintptr_t object,uintptr_t source,uintptr_t target,uintptr_t& found)noexcept{
    __try{found=uintptr_t(call(reinterpret_cast<void*>(object),reinterpret_cast<void*>(source),reinterpret_cast<void*>(target),0));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
identity::Scope scope(const World& w){return {w.uuid,w.sha256,w.world_epoch,w.generation,uintptr_t(w.map)+0x1c0};}
World copy_world(const building_mixed_identity::opened_file::CurrentWorld& w){
    return {w.game,w.mode,w.state,w.world,w.map,w.generation,w.world_epoch,w.uuid,w.sha256};
}
struct SceneSceneryHost final:mt2mp::build_actions::private_native::place::SceneryHost {
    const World& expected;const Pool& pool;host::LookupApi api;uint32_t thread;bool snapshot=false;
    SceneSceneryHost(const World& w,const Pool& p,host::LookupApi lookup,uint32_t owner,bool capture=false):expected(w),pool(p),api(lookup),thread(owner),snapshot(capture){}
    bool current(mt2mp::build_actions::private_native::place::SceneryContext& out,std::string& error)override{
        out={};building_mixed_identity::opened_file::CurrentWorld actual;
        if(snapshot&&!expected.generation&&!expected.world_epoch){
            if(thread!=GetCurrentThreadId()||!building_capture_identity::confirm_current_receivers(expected.game,expected.mode,expected.state,expected.world,expected.map,error))return false;
            out={0,pool.scene.owner.region,pool.scene.owner.shard,pool.managers[0],true,true,true,true};return true;
        }
        if(!native_open_runtime::read_current_world(actual,error))return false;
        if(actual.game!=expected.game||actual.mode!=expected.mode||actual.state!=expected.state||actual.world!=expected.world||
           actual.map!=expected.map||actual.generation!=expected.generation||actual.world_epoch!=expected.world_epoch)
            return fail(error,"Dungeon Scenery current NativeOpen world lifetime differs");
        out={expected.world_epoch,pool.scene.owner.region,pool.scene.owner.shard,pool.managers[0],true,thread==GetCurrentThreadId(),true};return true;
    }
    bool registry_object(uint64_t uid,uintptr_t& found,std::string& error)override{
        const auto exception=original_find(api.find,uid,found);
        return !exception||fail(error,"Dungeon Scenery original full UID lookup SEH="+std::to_string(exception));
    }
};
bool active(identity::Scope& out,std::string& e){
    building_mixed_identity::opened_file::CurrentWorld world;if(!native_open_runtime::read_current_world(world,e))return false;out=scope(copy_world(world));
    return registry.current(out,e);
}
}
#include "root_scenery_snapshot.inc"
static bool read_census(const World& world,const std::vector<Exterior>& exteriors,bool snapshot,Census& out,std::string& error,const identity::Registry* preview=nullptr)try{
    live_host_state::apply_timing::PageReadScope census_pages(!live_host_state::apply_timing::PageReadScope::reader_active());
    out={};uintptr_t image=0;uint32_t thread=0;std::string digest;
    if(!native::read_resolver_receipt(image,thread,digest,error))return false;
    if(digest!=executable||image!=uintptr_t(GetModuleHandleW(nullptr))||thread!=GetCurrentThreadId()||!world.map||!world.world||!identity::uuid(world.uuid)||!identity::digest(world.sha256))
        return fail(error,"Dungeon scene snapshot classification requires exact native image/Mainthread/owned file world");
    host::ProcessMemory memory;host::LookupApi api;if(!host::resolve_lookup(memory,image,digest,api,error))return false;
    struct Pin{uintptr_t rva;size_t bytes;const char* sha;};
    constexpr std::array<Pin,7> stream_contract{{
        {0xe39220,880,"a774f62245dc31d1e2750a00fa09d6c3edd8a4b42eb0113c81a7a4c73d000973"},
        {0xe39170,176,"dcf90d69d55472cbbed118382fc2cff02015f0990e868dcdbbd06d8e3f9b28b2"},

        {0xdde8e0,337,"d0361f2626f6af644b4f2f349adfcdf15f698f63e1c80a6234b63eb966ef4c5f"},
        {0xdde8b0,37,"1130ec4f456035af71b381da6e5d47daf5cfd2df532c22d3a23c3d5f86eeb1e3"},
        {0xe14190,268,"f9ce3891249b849fcd54b435745454f39ad8afab8c88589125e0630227798b4e"},
        {0xc11fd0,1107,"14d2cff6a8c3cb290f055d6e790123133f30a38ce99fdb485c8f0363fd97a53f"},
        {0x12903c0,868,"8d316c27bf91648d0df640a3921176b9f0ded232c238863d5148f47c5bae4eeb"}}};
    if(snapshot)for(const auto& pin:stream_contract){std::vector<uint8_t> bytes(pin.bytes);if(!memory.copy(image+pin.rva,bytes.data(),bytes.size(),error))return false;
        net::SnapshotSha256 hash;hash.add(bytes.data(),bytes.size());if(hash.finish()!=pin.sha)return fail(error,"Dungeon scene original Gizmo stream/property body differs RVA="+std::to_string(pin.rva));}
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& e){
        if(!p||offset>UINTPTR_MAX-p||p+offset>UINTPTR_MAX-(sizeof value-1))return fail(e,"Dungeon scene identity field span invalid");
        return memory.copy(p+offset,&value,sizeof value,e);
    };
    auto find=[&](uint64_t uid,uintptr_t& found,std::string& e){live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);
        if(!uid)return fail(e,"Dungeon scene identity requires full native UID");const auto exception=original_find(api.find,uid,found);
        return !exception||fail(e,"Dungeon scene original full UID lookup SEH="+std::to_string(exception));
    };
    auto typed=[&](uintptr_t object,uintptr_t source,uintptr_t target,std::string& e){live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);uintptr_t found=0;
        const auto exception=original_cast(api.cast,object,image+source,image+target,found);
        return (!exception&&found==object)||fail(e,"Dungeon scene original RTTI receiver differs SEH="+std::to_string(exception));
    };
    std::map<uint64_t,std::string> exterior_records;std::set<std::string> records;
    for(const auto& row:exteriors)if(!row.uid||!(snapshot?(snapshot_building_key::valid(row.record)&&row.record.substr(0,64)==world.sha256):dungeon_scene_key::building_key(row.record))||
        !exterior_records.emplace(row.uid,row.record).second||!records.insert(row.record).second)return fail(error,"Dungeon scene exact native root Building records invalid or duplicated");
    auto identify=[&](uintptr_t exterior,uint64_t uid,std::string& key,std::string& e){uintptr_t found=0;const auto row=exterior_records.find(uid);
        if(row==exterior_records.end())return fail(e,"Dungeon scene exterior absent from exact sealed root Building records");
        if(!find(uid,found,e))return false;if(found!=exterior)return fail(e,"Dungeon scene sealed exterior full UID registry differs");key=row->second;return true;
    };
    Census result;result.world=world;result.snapshot_census=snapshot;std::vector<dungeon_scene_roster::Scene> scenes;
    if(snapshot&&!root_scenery_rows(world,image,thread,api,true,result.scenery_saved,error))return false;
    const auto root=uintptr_t(world.map)+0x1c0;
    if(!([&]{live_host_state::apply_timing::PageReadScope roster_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return dungeon_scene_roster::read(at,typed,find,identify,image,uintptr_t(world.map),root,scenes,error);}()))return false;
    std::map<std::pair<uintptr_t,uint64_t>,std::string> parent_records;
    std::set<uintptr_t> managers,objects;std::set<uint64_t> uids;
    // Resolve every actual scene roster first, so Instance -> template prototype
    // references work regardless of traversal order and never fall back to names.
    for(const auto& scene:scenes){Pool pool;pool.scene=scene;
        for(size_t i=0;i<pool.managers.size();++i){uintptr_t ap=0,region=0,shard=0;
            if(!at(scene.owner.region,dungeon_template_owner::region_offsets[i],pool.managers[i],error)||
               !at(pool.managers[i],0,ap,error)||!at(pool.managers[i],dungeon_template_owner::reverse_region[i],region,error))return false;
            if(!managers.insert(pool.managers[i]).second||ap!=image+dungeon_template_owner::addresspoints[i]||region!=scene.owner.region)
                return fail(error,"Dungeon scene classification manager class/unique reciprocal Region differs");
            if(i==0||i==3){if(!at(pool.managers[i],i==0?0x908:0x68,shard,error))return false;
                if(shard!=scene.owner.shard)return fail(error,"Dungeon scene classification manager reciprocal Shard differs");}
        }
        if(!dungeon_zone_roster::read(at,find,image,pool.managers[1],scene.owner.shard,scene.owner.region,pool.zones,error))return false;
        SceneSceneryHost scenery_host(world,pool,api,thread,snapshot);
        if(!mt2mp::build_actions::private_native::place::read_scenery(memory,scenery_host,image,pool.scenery,error))return false;
        const auto manager=pool.managers[3];dungeon_scene_roster::Array array;if(!dungeon_scene_roster::array(at,manager+0x18,array,error))return false;
        for(size_t i=0;i<array.members.size();++i){const auto object=array.members[i];uint64_t uid=0;uintptr_t found=0;
            if(!at(object,0x18,uid,error)||!find(uid,found,error))return false;
            if(found!=object||!objects.insert(object).second||!uids.insert(uid).second)return fail(error,"Dungeon scene classification Gizmo full UID/registry duplicated or foreign");
            const auto record=identity::record(world.sha256,scene.exterior_region,scene.exterior_slot,scene.owner.instance_index,int32_t(i));
            if(!identity::saved(record))return fail(error,"Dungeon scene original Gizmo child record path invalid");
            identity::Member member{scene.owner.key,object,manager,scene.owner.shard,scene.owner.region,uid};
            std::string canonical=record;
            if(!snapshot){identity::Member known;
                if(preview){if(!preview->resolve(scope(world),object,uid,canonical,error)||!preview->lookup(scope(world),canonical,known,error))return false;}
                else if(!resolve(object,uid,canonical,error)||!lookup(canonical,known,error))return false;
                if(known!=member)return fail(error,"Dungeon Gizmo live canonical scene/full owner differs from confirmed identity");}
            result.saved.push_back({snapshot?record:std::string{},member});result.keys.push_back(canonical);parent_records.emplace(std::pair{object,uid},canonical);
        }
        if(snapshot)for(size_t child=0;child<pool.scenery.members.size();++child){const auto& member=pool.scenery.members[child];
            const auto record=scenery_scene_key::record(world.sha256,scene.exterior_region,scene.exterior_slot,scene.owner.instance_index,int32_t(child));
            if(!scenery_scene_key::saved(record))return fail(error,"Scene Scenery original saved child record path invalid");
            result.scenery_saved.push_back({record,scene.owner.key,member.object,pool.managers[0],scene.owner.shard,scene.owner.region,member.uid});
        }
        if(!dungeon_scene_roster::unchanged(at,manager+0x18,array,error))return false;result.pools.push_back(std::move(pool));
    }
    auto text=[&](uintptr_t base,size_t offset,std::string& value,std::string& e){uintptr_t data=0;uint64_t size=0;
        if(!at(base,offset,data,e)||!at(base,offset+8,size,e))return false;
        if(size>SIZE_MAX||size>UINTPTR_MAX-data||(size&&!data))return fail(e,"Dungeon scene native Gizmo GNU string span invalid");
        value.resize(size_t(size));return value.empty()||memory.copy(data,value.data(),value.size(),e);
    };
    auto parent=[&](uintptr_t object,uint64_t uid,std::string& key,std::string& e){const auto found=parent_records.find({object,uid});
        if(found==parent_records.end())return fail(e,"Dungeon scene Gizmo prototype parent lacks an exact saved scene record");key=found->second;return true;
    };
    for(auto& pool:result.pools)if(!dungeon_gizmo_reader::read(at,text,find,parent,image,pool.managers[3],pool.scene.owner.shard,pool.scene.owner.region,pool.gizmos,pool.local,error))return false;
    std::vector<dungeon_scene_roster::Scene> after;
    if(!([&]{live_host_state::apply_timing::PageReadScope roster_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return dungeon_scene_roster::read(at,typed,find,identify,image,uintptr_t(world.map),root,after,error);}()))return false;
    if(after.size()!=scenes.size())return fail(error,"Dungeon scene roster count changed during complete census");
    for(size_t i=0;i<after.size();++i)if(after[i].owner.key!=scenes[i].owner.key||after[i].owner.shard!=scenes[i].owner.shard||
        after[i].owner.region!=scenes[i].owner.region||after[i].owner.dungeon!=scenes[i].owner.dungeon||after[i].dungeon_uid!=scenes[i].dungeon_uid||
        after[i].owner.exterior_uid!=scenes[i].owner.exterior_uid||after[i].exterior_region!=scenes[i].exterior_region||after[i].exterior_slot!=scenes[i].exterior_slot)
        return fail(error,"Dungeon scene full owner identity/order changed during complete census");
    out=std::move(result);error.clear();return true;
}catch(const std::exception& e){out={};return fail(error,std::string("Dungeon scene snapshot classification: ")+e.what());}
bool read_scenery_pool(const World& world,const dungeon_scene_roster::Scene& scene,const std::array<uintptr_t,4>& managers,
    mt2mp::build_actions::private_native::place::SceneryWitness& out,std::string& error){
    out={};uintptr_t image=0;uint32_t thread=0;std::string digest;
    if(!native::read_resolver_receipt(image,thread,digest,error))return false;
    if(digest!=executable||image!=uintptr_t(GetModuleHandleW(nullptr))||thread!=GetCurrentThreadId()||!world.world||!world.map||
       !identity::uuid(world.uuid)||!identity::digest(world.sha256)||!identity::scene(scene.owner.key)||!managers[0])
        return fail(error,"Scene Scenery pool requires exact Mainthread/world/Scene owner");
    host::ProcessMemory memory;host::LookupApi api;if(!host::resolve_lookup(memory,image,digest,api,error))return false;
    Pool pool;pool.scene=scene;pool.managers=managers;SceneSceneryHost owner(world,pool,api,thread);
    return mt2mp::build_actions::private_native::place::read_scenery(memory,owner,image,out,error);
}
bool classify(const World& world,const std::vector<Exterior>& exteriors,Census& out,std::string& error){return read_census(world,exteriors,true,out,error);}
bool classify_capture(const std::string& id,const std::string& sha,const CaptureWorld& capture,const std::vector<Exterior>& exteriors,Census& out,std::string& error){
    out={};World world;world.game=capture.game;world.mode=capture.mode;world.state=capture.state;world.world=capture.world;world.map=capture.map;
    if(building_mixed_identity::opened_file::status().current_origin_available){building_mixed_identity::opened_file::CurrentWorld current;
        if(!native_open_runtime::read_current_world(current,error))return false;
        if(current.game!=world.game||current.mode!=world.mode||current.state!=world.state||current.world!=world.world||current.map!=world.map)
            return fail(error,"Dungeon scene Host capture actual native world differs from loaded origin");world=copy_world(current);
    }
    world.uuid=id;world.sha256=sha;return classify(world,exteriors,out,error);
}
bool classify_loaded(const std::string& id,const std::string& sha,const std::vector<Exterior>& exteriors,Census& out,std::string& error){
    out={};const auto status=building_mixed_identity::opened_file::status();building_mixed_identity::opened_file::CurrentWorld world;
    if(!building_mixed_identity::opened_file::read_completed_world(status.generation,world,error))return false;
    if(world.uuid!=id||world.sha256!=sha)return fail(error,"Dungeon scene Client completed native snapshot UUID/SHA differs");
    return classify(copy_world(world),exteriors,out,error);
}
static bool current_exteriors(World& out,std::vector<Exterior>& exteriors,std::string& error){
    live_host_state::apply_timing::PageReadScope census_pages(!live_host_state::apply_timing::PageReadScope::reader_active());
    out={};exteriors.clear();const auto snapshot=net::current_snapshot_id();identity::Scope before;if(!current(before,error))return false;
    building_mixed_identity::opened_file::CurrentWorld world;if(!native_open_runtime::read_current_world(world,error))return false;
    uintptr_t image=0;uint32_t thread=0;std::string sha;if(!native::read_resolver_receipt(image,thread,sha,error))return false;
    if(sha!=executable||thread!=GetCurrentThreadId())return fail(error,"Dungeon current census requires pinned native Mainthread");
    host::ProcessMemory memory;const auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& e){
        if(!p||offset>UINTPTR_MAX-p||p+offset>UINTPTR_MAX-(sizeof value-1))return fail(e,"Dungeon current census field span invalid");
        return memory.copy(p+offset,&value,sizeof value,e);
    };
    {live_host_state::apply_timing::PageReadScope exterior_pages(!live_host_state::apply_timing::PageReadScope::reader_active());
    dungeon_scene_roster::Array regions;const auto map=uintptr_t(world.map);
    if(!dungeon_scene_roster::array(at,map+0x230,regions,error))return false;
    for(const auto region:regions.members){uintptr_t manager=0;if(!at(region,0x950,manager,error))return false;
        dungeon_scene_roster::Array buildings;if(!dungeon_scene_roster::array(at,manager+0x408,buildings,error))return false;
        for(const auto building:buildings.members){uintptr_t dungeon=0;uint64_t uid=0;if(!at(building,0xa38,dungeon,error))return false;
            if(!dungeon)continue;Exterior row;if(!at(building,0x18,uid,error)||!([&]{live_host_state::apply_timing::PageReadScope original_key(live_host_state::apply_timing::PageBoundary::OriginalCall);return remove_diagnostics::resolve_current_building_key(snapshot,building,uid,row.record,error);}()))return false;
            row.uid=uid;exteriors.push_back(std::move(row));}
        if(!dungeon_scene_roster::unchanged(at,manager+0x408,buildings,error))return false;
    }
    if(!dungeon_scene_roster::unchanged(at,map+0x230,regions,error))return false;
    }
    identity::Scope after;if(net::current_snapshot_id()!=snapshot||!current(after,error)||after!=before)
        return fail(error,error.empty()?"Dungeon current exterior shared snapshot or NativeOpen lifetime changed":error);
    if(scope(copy_world(world))!=after)return fail(error,"Dungeon current exterior world differs from confirmed origin");
    out=copy_world(world);error.clear();return true;
}
bool read_scene_roster(World& out,std::vector<dungeon_scene_roster::Scene>& scenes,std::string& error)try{
    live_host_state::apply_timing::PageReadScope census_pages(!live_host_state::apply_timing::PageReadScope::reader_active());
    out={};scenes.clear();World world;std::vector<Exterior> exteriors;
    if(!current_exteriors(world,exteriors,error))return false;
    uintptr_t image=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(image,thread,sha,error))return false;
    if(sha!=executable||image!=uintptr_t(GetModuleHandleW(nullptr))||thread!=GetCurrentThreadId())return fail(error,"Dungeon current roster requires pinned native image/Mainthread");
    host::ProcessMemory memory;
    const auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& e){
        if(!p||offset>UINTPTR_MAX-p||p+offset>UINTPTR_MAX-(sizeof value-1))return fail(e,"Dungeon current roster field span invalid");
        return memory.copy(p+offset,&value,sizeof value,e);
    };
    const auto map=uintptr_t(world.map);
    host::LookupApi api;if(!host::resolve_lookup(memory,image,sha,api,error))return false;
    auto find=[&](uint64_t uid,uintptr_t& found,std::string& e){live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);
        if(!uid)return fail(e,"Dungeon current scene requires full native UID");
        const auto exception=original_find(api.find,uid,found);
        return !exception||fail(e,"Dungeon current scene full UID lookup SEH="+std::to_string(exception));
    };
    auto typed=[&](uintptr_t object,uintptr_t from,uintptr_t to,std::string& e){live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);uintptr_t found=0;
        const auto exception=original_cast(api.cast,object,image+from,image+to,found);
        return (!exception&&found==object)||fail(e,"Dungeon current scene original RTTI differs SEH="+std::to_string(exception));
    };
    std::map<uint64_t,std::string> records;
    for(const auto& exterior:exteriors)if(!records.emplace(exterior.uid,exterior.record).second)
        return fail(error,"Dungeon current scene exterior full UID repeats");
    auto identify=[&](uintptr_t object,uint64_t uid,std::string& key,std::string& e){
        const auto row=records.find(uid);uintptr_t registered=0;
        if(row==records.end())return fail(e,"Dungeon current scene exterior lacks confirmed root Building identity");
        if(!find(uid,registered,e))return false;
        if(registered!=object)return fail(e,"Dungeon current scene exterior original UID binding changed");
        key=row->second;return true;
    };
    std::vector<dungeon_scene_roster::Scene> ready;
    if(!([&]{live_host_state::apply_timing::PageReadScope roster_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return dungeon_scene_roster::read(at,typed,find,identify,image,map,map+0x1c0,ready,error);}()))return false;
    identity::Scope after;if(!current(after,error)||after!=scope(world))
        return fail(error,error.empty()?"Dungeon current roster NativeOpen lifetime changed":error);
    out=world;scenes=std::move(ready);error.clear();return true;
}catch(const std::exception& e){out={};scenes.clear();return fail(error,std::string("Dungeon current scene roster: ")+e.what());}
static bool current_census(Census& out,bool verify,std::string& error,const identity::Registry* preview=nullptr){
    out={};World world;std::vector<Exterior> exteriors;
    if(!current_exteriors(world,exteriors,error)||!read_census(world,exteriors,false,out,error,preview))return false;
    identity::Scope after;if(!current(after,error)||after!=scope(world)){out={};return fail(error,error.empty()?"Dungeon current census NativeOpen lifetime changed":error);}
    std::vector<identity::Member> members;for(const auto& row:out.saved)members.push_back(row.member);
    if(verify){if(preview){if(!preview->verify_roster(after,members,error)){out={};return false;}}
        else {std::lock_guard lock(gate);if(!registry.verify_roster(after,members,error)){out={};return false;}}}return true;
}
bool read_current(Census& out,std::string& error){return current_census(out,true,error);}
bool confirm_cell_cascade(const action_session::Binding& binding,const action_session::Request& request,const Census& before,Census& after,std::string& error){
    after={};const auto controller=typed_action_runtime::controller_status();
    const bool stroke=request.action==action_session::Action::SetDungeonPalette&&request.dungeon_palette.edit==action_session::DungeonPaletteEdit::Cells&&request.dungeon_palette.stroke.mode==action_session::DungeonCellMode::Close;
    const bool history=(request.action==action_session::Action::UndoBuilding||request.action==action_session::Action::RedoBuilding)&&request.canonical_record_key=="dungeon-tiles:"+request.target_action_uuid;
    if((!stroke&&!history)||request.snapshot_uuid!=net::current_snapshot_id()||
       binding.native_epoch!=before.world.world_epoch||binding.native_generation!=before.world.generation||
       !controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid)
        return fail(error,"Dungeon cell cascade retirement requires its exact pending Close stroke lease");
    Census actual;if(!current_census(actual,false,error))return false;
    if(scope(before.world)!=scope(actual.world)||before.pools.size()!=actual.pools.size()||before.saved.size()!=before.keys.size())
        return fail(error,"Dungeon cell cascade changed the sealed world lifetime or complete scene roster");
    for(size_t i=0;i<before.pools.size();++i){const auto& x=before.pools[i];const auto& y=actual.pools[i];
        if(x.scene.owner.key!=y.scene.owner.key||x.scene.owner.exterior_key!=y.scene.owner.exterior_key||x.scene.owner.dungeon!=y.scene.owner.dungeon||
           x.scene.owner.shard!=y.scene.owner.shard||x.scene.owner.region!=y.scene.owner.region||x.managers!=y.managers)
            return fail(error,"Dungeon cell cascade changed an original scene owner or manager");
    }
    std::vector<identity::Member> members;std::set<std::pair<uintptr_t,uint64_t>> surviving;
    for(const auto& row:actual.saved){const auto found=std::find_if(before.saved.begin(),before.saved.end(),[&](const auto& prior){return prior.member==row.member;});
        if(found==before.saved.end()||!surviving.emplace(row.member.object,row.member.uid).second)return fail(error,"Dungeon cell Close cascade contains a new or changed Gizmo identity");members.push_back(row.member);}
    uintptr_t image=0;uint32_t thread=0;std::string digest;host::ProcessMemory memory;host::LookupApi api;
    if(!native::read_resolver_receipt(image,thread,digest,error)||thread!=GetCurrentThreadId()||digest!=executable||
       !host::resolve_lookup(memory,image,executable,api,error))return fail(error,error.empty()?"Dungeon cell cascade retirement lacks pinned Mainthread lookup":error);
    std::vector<size_t> retired;
    for(size_t i=0;i<before.saved.size();++i){const auto& member=before.saved[i].member;if(surviving.contains({member.object,member.uid}))continue;
        uintptr_t found=0;const auto seh=original_find(api.find,member.uid,found);
        if(seh||found)return fail(error,"Dungeon cell queued Gizmo retirement is not complete SEH="+std::to_string(seh)+" registered="+std::to_string(bool(found)));
        retired.push_back(i);
    }
    std::lock_guard lock(gate);identity::Scope current_scope;if(!active(current_scope,error)||current_scope!=scope(before.world))return fail(error,error.empty()?"Dungeon cell cascade changed identity lifetime before retirement":error);
    auto next=registry;for(const auto i:retired)if(!next.retire(current_scope,before.keys[i],before.saved[i].member,error))return false;
    if(!next.verify_roster(current_scope,members,error))return false;registry=std::move(next);after=std::move(actual);error.clear();return true;
}
bool confirm_cell_restoration(const action_session::Binding& binding,const action_session::Request& request,const Census& before,
                             const std::vector<GizmoRestore>& restored,const std::vector<std::pair<std::string,identity::Member>>& retired,Census& after,std::string& error){
    after={};const auto controller=typed_action_runtime::controller_status();
    if((request.action!=action_session::Action::UndoBuilding&&request.action!=action_session::Action::RedoBuilding)||
       (request.canonical_record_key!="dungeon-tiles:"+request.target_action_uuid&&request.canonical_record_key!="dungeon-input:"+request.target_action_uuid)||request.snapshot_uuid!=net::current_snapshot_id()||
       binding.native_epoch!=before.world.world_epoch||binding.native_generation!=before.world.generation||
       !controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid)
        return fail(error,"Dungeon original Gizmo restoration requires exact pending cell history lease");
    identity::Registry next;identity::Scope expected=scope(before.world);
    {std::lock_guard lock(gate);identity::Scope actual;if(!active(actual,error)||actual!=expected)return fail(error,error.empty()?"Dungeon cell history changed its current scene lifetime":error);next=registry;}
    host::ProcessMemory memory;host::LookupApi api;uintptr_t image=0;uint32_t thread=0;std::string digest;
    if(!native::read_resolver_receipt(image,thread,digest,error)||thread!=GetCurrentThreadId()||digest!=executable||!host::resolve_lookup(memory,image,executable,api,error))
        return fail(error,error.empty()?"Dungeon cell history Gizmo retirement lacks exact Mainthread lookup":error);
    std::set<std::string> keys;
    for(const auto& [key,member]:retired){
        const auto prior=std::find_if(before.saved.begin(),before.saved.end(),[&](const auto& row){return row.member==member;});
        if(prior==before.saved.end()||before.saved.size()!=before.keys.size()||before.keys[size_t(prior-before.saved.begin())]!=key||!keys.insert(key).second)
            return fail(error,"Dungeon cell history Gizmo retirement lacks its complete before member/key");
        uintptr_t found=0;const auto seh=original_find(api.find,member.uid,found);
        if(seh||found)return fail(error,"Dungeon cell history original Gizmo deletion remains registered SEH="+std::to_string(seh));
        if(!next.retire(expected,key,member,error))return false;
    }
    keys.clear();
    for(const auto& row:restored){
        const auto scene=std::find_if(before.pools.begin(),before.pools.end(),[&](const auto& value){return value.scene.owner.key==row.before.scene;});
        if(scene==before.pools.end()||scene->managers[3]!=row.before.manager||scene->scene.owner.shard!=row.before.shard||scene->scene.owner.region!=row.before.region||
           !keys.insert(row.key).second||!next.restore_retired(expected,row.key,row.before,row.after,error))
            return fail(error,error.empty()?"Dungeon native Gizmo return lacks its unique exact original scene owner":error);
    }
    Census actual;if(!current_census(actual,true,error,&next))return false;
    if(scope(actual.world)!=expected||actual.pools.size()!=before.pools.size())return fail(error,"Dungeon cell original restoration changed the complete scene roster");
    {std::lock_guard lock(gate);identity::Scope final_scope;if(!active(final_scope,error)||final_scope!=expected)return fail(error,error.empty()?"Dungeon cell original restoration changed lifetime before binding":error);registry=std::move(next);}
    after=std::move(actual);error.clear();return true;
}
bool seal_snapshot(const Census& census,const dungeon_gizmo_snapshot_alias::Manifest* aliases,std::string& error){
    std::lock_guard lock(gate);const auto next=scope(census.world);
    if(!census.snapshot_census)return fail(error,"Dungeon Gizmo identity seed requires its exact owned snapshot census");
    if(aliases&&(!aliases->present||!dungeon_gizmo_snapshot_alias::valid(*aliases,census.world.sha256,error)))return false;
    if(!registry.seal(next,census.saved,aliases?&aliases->rows:nullptr,error,aliases?aliases->retired:std::set<std::string>{}))return false;bound=next;return true;
}
bool capture_aliases(const Census& census,const std::string& next_sha,dungeon_gizmo_snapshot_alias::Manifest& out,std::string& error){
    std::lock_guard lock(gate);out={};identity::Scope actual;if(!active(actual,error))return false;
    if(!census.snapshot_census)return fail(error,"Dungeon Gizmo alias export requires its exact owned snapshot census");
    if(uintptr_t(census.world.map)+0x1c0!=actual.root||census.world.generation!=actual.generation||census.world.world_epoch!=actual.epoch)
        return fail(error,"Dungeon Gizmo capture actual NativeOpen world lifetime differs");
    dungeon_gizmo_snapshot_alias::Manifest next;next.present=true;
    if(!registry.capture(actual,next_sha,census.saved,next.rows,error)||!registry.tombstones(actual,next.retired,error)||!dungeon_gizmo_snapshot_alias::valid(next,next_sha,error))return false;
    out=std::move(next);return true;
}
bool current(identity::Scope& out,std::string& error){std::lock_guard lock(gate);return active(out,error);}
bool resolve(uintptr_t object,uint64_t uid,std::string& key,std::string& error){std::lock_guard lock(gate);identity::Scope actual;
    return active(actual,error)&&registry.resolve(actual,object,uid,key,error);
}
bool lookup(const std::string& key,identity::Member& out,std::string& error){std::lock_guard lock(gate);identity::Scope actual;
    return active(actual,error)&&registry.lookup(actual,key,out,error);
}
bool confirmed_birth(const std::string& action_uuid,const std::vector<identity::Member>& members,std::string& error){
    std::lock_guard lock(gate);identity::Scope actual;if(!active(actual,error))return false;
    if(!identity::uuid(action_uuid)||members.empty()||members.size()>INT32_MAX)return fail(error,"Dungeon Gizmo confirmed original birth UUID/count invalid");
    auto next=registry;for(size_t i=0;i<members.size();++i)if(!next.append(actual,identity::birth(action_uuid,uint32_t(i)),members[i],error))return false;
    registry=std::move(next);return true;
}
bool confirmed_retirement(const std::string& key,const identity::Member& member,std::string& error){
    std::lock_guard lock(gate);identity::Scope actual;return active(actual,error)&&registry.retire(actual,key,member,error);
}
bool preview_clones(const World& world,const std::vector<std::pair<std::string,identity::Member>>& births,
    const std::vector<std::pair<std::string,identity::Member>>& retired,ClonePreview& out,Census& census,std::string& error,const std::vector<GizmoRestore>& restored,const std::string& template_exterior,const scene_clone::Raw* held){
    out={};census={};ClonePreview next;next.scope=scope(world);
    {std::lock_guard lock(gate);identity::Scope current_scope;if(!active(current_scope,error)||current_scope!=next.scope)return fail(error,error.empty()?"Scene clone Gizmo current world differs":error);next.before=registry;next.after=registry;}
    host::ProcessMemory memory;host::LookupApi api;uintptr_t image{};uint32_t thread{};std::string digest;
    if(!native::read_resolver_receipt(image,thread,digest,error)||thread!=GetCurrentThreadId()||digest!=executable||!host::resolve_lookup(memory,image,digest,api,error))return fail(error,error.empty()?"Scene clone Gizmo exact Mainthread lookup absent":error);
    for(const auto& [key,member]:retired){uintptr_t found{};const auto exception=original_find(api.find,member.uid,found);
        if(exception||(scene_clone::held_member(held,member.scene,member.object,member.uid)?found!=member.object:found!=0))return fail(error,"Scene clone Gizmo original retirement is incomplete SEH="+std::to_string(exception));
        if(!next.after.retire(next.scope,key,member,error))return false;}
    for(const auto& [key,member]:births)if(!next.after.append(next.scope,key,member,error))return false;
    for(const auto& row:restored)if(!next.after.restore_scene(next.scope,row.key,row.before,row.after,template_exterior,error))return false;
    if(!current_census(census,true,error,&next.after)||scope(census.world)!=next.scope)return fail(error,error.empty()?"Scene clone complete Gizmo preview census differs":error);
    next.prepared=true;out=std::move(next);error.clear();return true;
}
bool check_clones(const ClonePreview& preview,std::string& error){std::lock_guard lock(gate);identity::Scope current_scope;
    return preview.prepared&&active(current_scope,error)&&current_scope==preview.scope&&registry==preview.before||fail(error,error.empty()?"Scene clone Gizmo preview changed or was already published":error);}
bool publish_clones(ClonePreview& preview,std::string& error){std::lock_guard lock(gate);identity::Scope current_scope;
    if(!preview.prepared||!active(current_scope,error)||current_scope!=preview.scope||registry!=preview.before)return fail(error,error.empty()?"Scene clone Gizmo publication no longer matches preview":error);
    registry=std::move(preview.after);preview.prepared=false;error.clear();return true;
}
void invalidate(){std::lock_guard lock(gate);registry.invalidate();bound={};}
#include "dungeon_scene_record_runtime.inc"
}
