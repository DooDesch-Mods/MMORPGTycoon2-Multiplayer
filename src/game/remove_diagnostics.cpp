#include "modular_identity.h"
#include "service_building_census.h"
#include "remove_diagnostics.h"
#include "building_scene_identity.h"
#include "scenery_scene_identity.h"
#include "character_boss_retirement_read.h"
#include "typed_action/action_session.h"
#include "typed_action_runtime.h"
#include "move_contract.h"
#include "dungeon_scene_identity_runtime.h"
#include "region_saved_identity.h"
#include "native_refund/native_refund.h"
#include "scenery_kiss/native_catalog.h"
#include "scenery_kiss/lifecycle_contract.h"
#include <cmath>
#include "object_identity/remove_runtime/remove_runtime.h"
#include "building_capture_identity.h"
#include "snapshot_building_key.h"
#include "mt2mp/game/normal_load_identity.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/net/snapshot_sync.h"
#include <regex>
#include <set>
#include <Windows.h>

namespace mt2mp::game::native_open_runtime {
bool read_current_world(building_mixed_identity::opened_file::CurrentWorld&,std::string&);
}
namespace mt2mp::game::remove_diagnostics {
namespace {
using json=nlohmann::json;
namespace rt=mt2mp::remove_runtime;
namespace host=rt::host;
struct Bound {
    std::string snapshot,key,sha;
    uint64_t uid=0,entry=0,generation=0,sequence=0;
    prop_lease::World world;
    bool revoked=false,unknown=false;
    json metadata;
    uintptr_t object=0,class_addresspoint=0;std::string scene_owner,scene_record;uintptr_t scene_manager=0,scene_shard=0,scene_region=0;
    std::string revocation_key;
    bool operator==(const Bound&)const=default;
};
struct Attempt {json arguments,receipt;};
std::map<std::string,Bound> registry;
std::map<std::string,Attempt> attempts;
std::map<std::string,Attempt> retirements;
uint64_t registry_generation=1,next_entry=0,next_sequence=0;
bool unknown_locked=false;
prop_lease::World admitted_world;
std::atomic<uint32_t> main_thread{0};
bool positive(const json& value,uint64_t& number) {
    if(!value.is_number_unsigned()&&!value.is_number_integer())return false;
    if(!value.is_number_unsigned()&&value.get<int64_t>()<=0)return false;
    number=value.get<uint64_t>();return number!=0;
}
std::string binding_id(const std::string& snapshot,const std::string& key) {
    return snapshot+":"+key;
}
std::string registry_id(const std::string& snapshot,const std::string& key) {
    return std::to_string(registry_generation)+":"+binding_id(snapshot,key);
}
class RegistryRetirement final:public rt::RootBindingRetirement {
public:
    bool read_actual_revocation(const rt::PrivateRequest& request,rt::BindingRevocation& out,std::string& error)override {
        out={};
        if(!main_thread.load()||main_thread.load()!=GetCurrentThreadId()) {error="actual remove revocation reader requires bound Mainthread";return false;}
        const Bound* match=nullptr;
        for(const auto& [id,entry]:registry) {
            if(entry.sequence!=request.sequence)continue;
            if(match){error="actual remove revocation sequence is ambiguous";return false;}
            match=&entry;
        }
        if(!match||!match->revoked||match->unknown||!match->entry||!match->generation||
           match->uid!=request.full_uid||match->world!=request.world||match->object!=request.object||
           binding_id(match->snapshot,match->key)!=request.snapshot_binding||!match->class_addresspoint||
           match->metadata.empty()||match->revocation_key.empty()) {
            error="actual remove revocation immutable binding differs";return false;
        }
        out={match->generation,match->entry,match->sequence,match->uid,match->world,
             binding_id(match->snapshot,match->key),match->revocation_key};return true;
    }
    bool verify_actual_revocation(const rt::PrivateRequest& request,const rt::BindingRevocation& saved,std::string& error)override {
        rt::BindingRevocation actual;
        if(!read_actual_revocation(request,actual,error))return false;
        if(actual!=saved){error="actual remove revocation changed after journal seal";return false;}
        return true;
    }
};
RegistryRetirement retirement_provider;
bool history_retirement(const Bound& saved,std::string& error) {
    if(!saved.sequence)return true;
    const auto* journal=rt::Runtime::instance().private_retirement_status(saved.sequence);
    const auto* record=journal?journal->record():nullptr;
    if(!record||!journal->effects().confirmed||journal->effects().unknown||record->request.full_uid!=saved.uid||
       record->request.world!=saved.world||record->request.object!=saved.object||record->root.entry_id!=saved.entry||
       record->root.registry_generation!=saved.generation||record->request.snapshot_binding!=binding_id(saved.snapshot,saved.key)) {
        error="building history original native holder retirement unconfirmed";return false;
    }
    error.clear();return true;
}
bool classify_normal_building(host::ProcessMemory& memory,uintptr_t image,const prop_lease::Prop& prop,
    const native::BuildingState& building,bool& supported,bool& scenery,std::string& cause,std::string& error) {
    supported=false;scenery=false;cause.clear();
    if(prop.class_addresspoint==image+0x16c1390)return scenery_kiss::classify_scenery(image,prop.class_addresspoint,prop.uid,building,scenery,error);
    if(prop.class_addresspoint!=image+0x16d93c0){error="native UI target actual Building class differs";return false;}
    if(building.dungeon_membership||building.dungeon_instance_membership){cause="native UI target belongs to a separate dungeon world";return true;}
    const auto object=uintptr_t(prop.registry_object);uintptr_t definition=0,table=0,data=0;uint64_t length=0;uint32_t flags=0;
    if(!object||object>UINTPTR_MAX-0x398){error="native Building definition owner address overflows";return false;}
    if(!memory.copy(object+0x390,&definition,sizeof definition,error))return false;
    if(!definition||definition>UINTPTR_MAX-0x170){error="native Building definition pointer unavailable or overflows";return false;}
    if(!memory.copy(definition,&table,sizeof table,error)||!memory.copy(definition+0x18,&data,sizeof data,error)||
       !memory.copy(definition+0x20,&length,sizeof length,error)||!memory.copy(definition+0x16c,&flags,sizeof flags,error))return false;
    if(table!=image+0x172ae50){error="native Building definition class differs";return false;}
    if(!data||!length||length>256){error="native Building definition identifier span invalid";return false;}
    char name[256]{};if(!memory.copy(data,name,size_t(length),error))return false;
    if(std::string(name,size_t(length))!=building.type){error="native Building definition type differs from actual object";return false;}
    if(building.type=="Scenery"||(flags&(1u<<13))){cause="native UI target is a separate Scenery definition";return true;}
    if(flags&((1u<<16)|(1u<<17))){cause="native UI target is a separate dungeon definition";return true;}
    if(((flags&(1u<<2))!=0)!=(building.type=="StartingPoint")){cause="native UI target StartingPoint type/feature differs";return true;}
    supported=true;return true;
}
bool current(host::opened::CurrentWorld& world,std::string& error) {
    if(!host::RootSceneryReadBatch::read_current(world,error))return false;
    const auto thread=GetCurrentThreadId();
    const auto existing=main_thread.load(std::memory_order_acquire);
    if(existing&&existing!=thread){error="remove diagnostics require bound Mainthread";return false;}
    main_thread.store(thread,std::memory_order_release);
    return true;
}
json result_json(const rt::OperationStatus& value) {
    const auto& r=value.removal;
    return {{"sequence",value.sequence},{"phase",int(value.phase)},
        {"attempted",r.attempted},{"returned",r.returned},{"confirmed",r.confirmed},
        {"unknown_locked",r.unknown_locked},{"target_retired",r.target_retired},
        {"undo_confirmed",r.undo_confirmed},{"selection_confirmed",r.selection_confirmed},
        {"retained",r.retained},{"retired",r.retired},{"seh_code",r.seh_code},{"error",r.error},
        {"pool",{{"leases",value.pool.leases},{"held",value.pool.held},{"tombstones",value.pool.tombstones},
                 {"released",value.pool.released},{"unknown_lock",value.pool.unknown_lock}}},
        {"retirement_confirmed",value.retirement_confirmed},{"retirement_cause",value.retirement_cause}};
}
__declspec(noinline) bool lookup(host::LookupApi api,uint64_t uid,void*& object,uint32_t& seh) {
    __try{object=api.find(uid);return true;}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:(seh=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER)){return false;}
}
template<class Binding>bool import_bindings(const std::vector<Binding>& values,const std::string& snapshot,
                                  const std::string& sha,const prop_lease::World& world,json& out,std::string& error) {
    for(const auto& value:values) {
        if(value.record_key.empty()||!value.local_uid){error="native snapshot binding lacks key or full UID";return false;}
        const auto id=registry_id(snapshot,value.record_key);
        const json metadata={{"type",value.building.type},{"variant",value.building.variant},
            {"x",value.building.prop.x},{"y",value.building.prop.y},{"z",value.building.prop.z}};
        const auto found=registry.find(id);
        if(found!=registry.end()) {
            const auto& old=found->second;
            if(old.uid!=value.local_uid||old.sha!=sha||old.world!=world||old.metadata!=metadata){error="retained native snapshot binding changed";return false;}
        } else {
            if(next_entry==UINT64_MAX){error="native snapshot registry entry exhausted";return false;}
            Bound fresh;fresh.snapshot=snapshot;fresh.key=value.record_key;fresh.sha=sha;fresh.uid=value.local_uid;
            fresh.entry=++next_entry;fresh.generation=registry_generation;fresh.world=world;fresh.metadata=metadata;
            registry.emplace(id,std::move(fresh));
        }
        const auto& saved=registry.at(id);
        out.push_back({{"snapshot_id",snapshot},{"record_key",saved.key},{"local_uid",saved.uid},
            {"entry_id",saved.entry},{"registry_generation",saved.generation},{"revoked",saved.revoked},
            {"unknown",saved.unknown},{"type",value.building.type},{"variant",value.building.variant},
            {"x",value.building.prop.x},{"y",value.building.prop.y},{"z",value.building.prop.z}});
    }
    return true;
}
}
json status() {
    if(!main_thread.load()||main_thread.load()!=GetCurrentThreadId())return {{"initialized",false}};
    const auto value=rt::Runtime::instance().status();
    json saved=json::object();for(const auto& [token,attempt]:attempts)saved[token]=attempt.receipt;
    json retired=json::object();for(const auto& [token,attempt]:retirements)retired[token]=attempt.receipt;
    return {{"initialized",true},{"phase",int(value.phase)},{"hook_mask",value.hook_mask},
        {"created_mask",value.created_mask},{"originals_published",value.originals_published},
        {"inflight",value.inflight},{"cause",value.cause},{"unknown_locked",unknown_locked},
        {"registry_entries",registry.size()},{"registry_generation",registry_generation},{"receipts",saved},{"retirements",retired}};
}
namespace {bool attach_refunds(const host::opened::CurrentWorld&,json&,std::string&);}
namespace {
enum class BindingDetails {Identity,Refund,BuildingTargets};
bool collect_bindings_body(json& out,std::string& error,BindingDetails details,const std::set<uint64_t>* omit_metadata=nullptr) {
    out=json::object();error.clear();host::opened::CurrentWorld world;
    if(!current(world,error))return false;
    if(unknown_locked){error="remove diagnostics are Unknown; no new binding admission";return false;}
    const prop_lease::World actual{world.world_epoch,world.generation,world.world,world.map};
    if(admitted_world.epoch&&admitted_world!=actual) {
        if(registry_generation==UINT64_MAX){error="native snapshot registry generation exhausted";return false;}
        for(auto& [id,entry]:registry)entry.revoked=true;
        ++registry_generation;
    }
    admitted_world=actual;
    const auto session_snapshot=net::current_snapshot_id();
    json retained=json::array();bool retained_present=false;
    for(auto& [id,saved]:registry) {
        if(saved.generation!=registry_generation||saved.world!=actual||saved.snapshot!=session_snapshot||saved.scene_owner=="retired")continue;
        retained_present=true;
        if(!saved.revoked&&!saved.unknown&&(!omit_metadata||!omit_metadata->contains(saved.uid))) {
            native::BuildingState settled;
            if(!scenery_kiss::read_action_prop(saved.uid,settled,error))return false;
            if(settled.prop.uid!=saved.uid||saved.metadata.at("type")!=settled.type||
               saved.metadata.at("variant")!=settled.variant||saved.metadata.at("x")!=settled.prop.x||
               saved.metadata.at("z")!=settled.prop.z||!std::isfinite(settled.prop.y)||
               (saved.metadata.contains("rotation")&&saved.metadata.at("rotation")!=settled.rotation)||
               (saved.metadata.contains("scale")&&saved.metadata.at("scale")!=settled.scale)) {
                error="shared building identity changed during native height readback";return false;
            }
            if(saved.metadata.at("y")!=settled.prop.y) {
                LOG_INFO("[Action] Native terrain height settled key={} before={} after={}",saved.key,saved.metadata.at("y").dump(),settled.prop.y);
                saved.metadata["y"]=settled.prop.y;
            }
        }
        if(details==BindingDetails::BuildingTargets&&omit_metadata&&omit_metadata->contains(saved.uid)&&!saved.revoked&&!saved.unknown){
            // The Building census already excludes this admitted saved Scenery.
            // Preserve the stored field guards without allocating its unused row.
            for(const char* field:{"type","variant","x","y","z"})(void)saved.metadata.at(field);
            if(saved.metadata.contains("rotation"))(void)saved.metadata.at("rotation");
            if(saved.metadata.contains("scale"))(void)saved.metadata.at("scale");
            continue;
        }
        retained.push_back({{"snapshot_id",saved.snapshot},{"record_key",saved.key},{"local_uid",saved.uid},
            {"entry_id",saved.entry},{"registry_generation",saved.generation},{"revoked",saved.revoked},
            {"unknown",saved.unknown},{"type",saved.metadata.at("type")},{"variant",saved.metadata.at("variant")},
            {"x",saved.metadata.at("x")},{"y",saved.metadata.at("y")},{"z",saved.metadata.at("z")}});
        if(!saved.scene_record.empty())retained.back()["native_record_key"]=saved.scene_record;
        if(saved.metadata.contains("rotation"))retained.back()["rotation"]=saved.metadata.at("rotation");
        if(saved.metadata.contains("scale"))retained.back()["scale"]=saved.metadata.at("scale");
    }
    // The session baseline remains the identity source after confirmed local edits.
    // Target resolution validates each live UID against the current native world.
    if(retained_present) {
        if(!scene_scenery::append_saved(retained,error))return false;
        if(details==BindingDetails::Refund&&!attach_refunds(world,retained,error)){out={{"world_epoch",actual.epoch},{"world_generation",actual.origin_generation},{"bindings",retained}};return false;}
        out={{"world_epoch",actual.epoch},{"world_generation",actual.origin_generation},{"bindings",std::move(retained)}};
        return true;
    }
    const auto capture=building_capture_identity::status();
    const auto loaded=normal_load_identity::status();
    json list=json::array();
    std::string baseline_snapshot,baseline_sha;
    if(capture.bindings_confirmed&&capture.world_witness_current&&!capture.capture_uuid.empty()&&capture.capture_uuid==session_snapshot) {
        building_capture_identity::Status fresh;
        if(building_capture_identity::seal(capture.capture_uuid,capture.snapshot_sha256,capture.snapshot_bytes,
            capture.session_epoch,fresh,error)!=building_capture_identity::SealResult::Confirmed) {
            if(error.empty())error="fresh native capture world binding unavailable";
            return false;
        }
        if(!fresh.bindings_confirmed||!fresh.world_witness_current||fresh.capture_uuid!=capture.capture_uuid||
           fresh.snapshot_sha256!=capture.snapshot_sha256||fresh.snapshot_bytes!=capture.snapshot_bytes) {
            error="fresh native capture identity differs";return false;
        }
        baseline_snapshot=fresh.capture_uuid;baseline_sha=fresh.snapshot_sha256;
        if(!import_bindings(fresh.buildings,fresh.capture_uuid,fresh.snapshot_sha256,actual,list,error))return false;
    } else if(loaded.state=="confirmed"&&!loaded.load_pending&&loaded.snapshot_uuid==world.uuid&&loaded.snapshot_sha256==world.sha256) {
        auto native_bindings=loaded.buildings;
        for(auto& binding:native_bindings) {
            if(binding.link_id<0){error="loaded native building linkID is negative";return false;}
            if(binding.link_id>0) {
                const auto canonical=loaded.snapshot_sha256+":world:building:"+std::to_string(binding.link_id);
                if(!binding.record_key.empty()&&binding.record_key!=canonical){error="loaded native building record key differs from native linkID";return false;}
                binding.record_key=canonical;
            } else if(!snapshot_building_key::valid(binding.record_key)||
                      !binding.record_key.starts_with(loaded.snapshot_sha256+":world:building:inline:")) {
                error="loaded inline native building record key lacks confirmed saved root path";return false;
            }
        }
        baseline_snapshot=loaded.snapshot_uuid;baseline_sha=loaded.snapshot_sha256;
        if(!import_bindings(native_bindings,loaded.snapshot_uuid,loaded.snapshot_sha256,actual,list,error))return false;
    } else {
        error="current complete NativeSnapshot building binding unavailable";return false;
    }
    if(!scene_scenery::append_saved(list,error))return false;
    if(details==BindingDetails::Refund&&!attach_refunds(world,list,error)){out={{"world_epoch",actual.epoch},{"world_generation",actual.origin_generation},{"bindings",list}};return false;}
    out={{"world_epoch",actual.epoch},{"world_generation",actual.origin_generation},
        {"baseline_snapshot_id",baseline_snapshot},{"baseline_sha256",baseline_sha},{"bindings",std::move(list)}};
    return true;
}
bool collect_bindings(json& out,std::string& error,BindingDetails details,const std::set<uint64_t>* omit_metadata=nullptr){
    host::RootSceneryReadBatch batch;json pending;
    if(!collect_bindings_body(pending,error,details,omit_metadata)){out=std::move(pending);return false;}
    if(!batch.finish(error)){out=json::object();return false;}
    out=std::move(pending);return true;
}

}
bool bindings(json& out,std::string& error) {
    return collect_bindings(out,error,BindingDetails::Refund);
}
bool identity_bindings(json& out,std::string& error) {
    return collect_bindings(out,error,BindingDetails::Identity);
}
namespace {
#include "dungeon_building_identity.inc"
#include "root_scenery_metadata.inc"
bool read_registered_building(uint64_t uid,host::opened::CurrentWorld& opened,native::BuildingState& building,
                              uintptr_t& object,uintptr_t& addresspoint,std::string& error,bool readonly=false,bool modular=false,const std::string& dungeon_scene={}) {
    object=0;addresspoint=0;
    if(!uid||!current(opened,error)) {
        if(error.empty())error="shared building requires a current world and positive local UID";
        return false;
    }
    native_free_runtime::Receipt heap;
    if(readonly) {
        std::string hash;
        if(!native::read_resolver_receipt(heap.image,heap.main_thread,hash,error))return false;
        if(hash!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"){error="native refund resolver executable differs";return false;}
        std::copy(hash.begin(),hash.end(),heap.executable.sha256.begin());
    } else {
        auto& free=native_free_runtime::Runtime::instance();
        if(free.status().phase==native_free_runtime::Phase::Idle&&!free.prepare_and_enable(error))return false;
        if(!free.read_actual_receipt(heap,error))return false;
    }
    host::ProcessMemory memory;host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(memory,heap.image,heap.executable.sha256.data(),api,error))return false;
    host::Reader reader(memory,origin,heap.image,heap.main_thread,api);host::WorldState before,after;
    if(!reader.current(before,true,error)||!host::same_world(opened,before.origin)) {
        if(error.empty())error="shared building current world differs";return false;
    }
    void* actual=nullptr;uint32_t seh=0;
    if(!lookup(api,uid,actual,seh)){error="shared building UID lookup SEH="+std::to_string(seh);return false;}
    if(!actual){error="shared building UID lookup returned no object";return false;}
    prop_lease::Prop prop;
    if(!(readonly?read_metadata_prop(memory,reader,heap.image,before,actual,uid,prop,error):reader.prop(actual,uid,prop,error))||
       !scenery_kiss::read_action_prop(uid,building,error))return false;
    if(!reader.current(after,true,error)||!host::same_world(opened,after.origin)||building.prop.uid!=uid) {
        if(error.empty())error="shared building world or UID changed during readback";return false;
    }
    bool normal_building=false,normal_scenery=false;std::string cause;
    if(!classify_normal_building(memory,heap.image,prop,building,normal_building,normal_scenery,cause,error))return false;
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& why){if(base>UINTPTR_MAX-offset){why="Modular identity address overflow";return false;}return memory.copy(base+offset,&value,sizeof value,why);};
    if(!dungeon_scene.empty()){
        if(modular||prop.class_addresspoint!=heap.image+0x16d93c0||
           !dungeon_building_target(memory,heap.image,before.root,uintptr_t(actual),building,dungeon_scene,error))return false;
    }else if(modular){if(!modular_identity::read(at,heap.image,uintptr_t(actual),building,error))return false;}
    else if(!normal_building&&!normal_scenery){error=cause.empty()?"shared created target has no supported native family":cause;return false;}
    object=uintptr_t(actual);addresspoint=prop.class_addresspoint;
    return true;
}
bool attach_refunds(const host::opened::CurrentWorld& origin,json& rows,std::string& error) {
    for(auto& row:rows) {
        row["native_refund"]={{"confirmed",false},{"attempted",false},{"returned",false}};
        if(row.at("revoked")==true||row.at("unknown")==true){row["native_refund"]["cause"]="retired or Unknown binding; refund getter skipped";continue;}
        const auto uid=row.at("local_uid").get<uint64_t>();native::BuildingState before,after;host::opened::CurrentWorld world,after_world;
        uintptr_t object=0,ap=0,after_object=0,after_ap=0;
        if(!read_registered_building(uid,world,before,object,ap,error,true)) {
            row["native_refund"]["cause"]=error;
            if(error=="native UI target belongs to a separate dungeon world"||error=="native UI target is a separate Scenery definition"||error=="native UI target is a separate dungeon definition"){error.clear();continue;}
            return false;
        }
        if(!host::same_world(origin,world)||row.at("type")!=before.type||row.at("variant")!=before.variant||
           row.at("x")!=before.prop.x||row.at("y")!=before.prop.y||row.at("z")!=before.prop.z){error="native refund complete World or registry metadata differs";return false;}
        uintptr_t image=0;uint32_t thread=0;std::string hash;
        if(!native::read_resolver_receipt(image,thread,hash,error))return false;
        if(hash!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"){error="native refund resolver executable differs";return false;}
        host::ProcessMemory memory;native_refund::Receipt receipt;
        const bool okay=native_refund::read(memory,image,thread,object,uid,ap,receipt,error);
        auto& result=row["native_refund"];result={{"confirmed",false},{"attempted",receipt.attempted},{"returned",receipt.returned},
            {"value",receipt.value},{"seh",receipt.seh},{"getter_rva",receipt.getter?receipt.getter-image:0},
            {"local_uid",uid},{"snapshot_id",row.at("snapshot_id")},{"record_key",row.at("record_key")},
            {"world_epoch",origin.world_epoch},{"world_generation",origin.generation}};
        if(!okay){result["cause"]=error;LOG_ERROR("[Remove] Native refund read failed UID={} cause={}",uid,error);return false;}
        if(!read_registered_building(uid,after_world,after,after_object,after_ap,error,true)){result["cause"]=error;return false;}
        if(!host::same_world(origin,after_world)||after_object!=object||after_ap!=ap||
           before.type!=after.type||before.variant!=after.variant||before.prop.uid!=after.prop.uid||
           before.prop.x!=after.prop.x||before.prop.y!=after.prop.y||before.prop.z!=after.prop.z||
           before.rotation!=after.rotation||before.scale!=after.scale){error="native refund complete World or Prop changed after getter";result["cause"]=error;return false;}
        result["confirmed"]=true;result["cause"]="";
    }
    return true;
}
json building_metadata(const native::BuildingState& building) {
    return {{"type",building.type},{"variant",building.variant},
        {"x",building.prop.x},{"y",building.prop.y},{"z",building.prop.z},{"rotation",building.rotation},{"scale",building.scale}};
}
}
static bool admit_created_building_impl(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error,bool modular,const std::string& dungeon_scene={}) {
    error.clear();
    static const std::regex uuid("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    if(!std::regex_match(snapshot,uuid)||key.size()!=43||key.compare(0,7,"action:")||
       !std::regex_match(key.substr(7),uuid)){error="created building shared identity invalid";return false;}
    if(snapshot!=net::current_snapshot_id()){error="created building snapshot differs from current session";return false;}
    json baseline;if(!bindings(baseline,error))return false;
    host::opened::CurrentWorld opened;native::BuildingState building;uintptr_t object=0,addresspoint=0;
    if(!read_registered_building(uid,opened,building,object,addresspoint,error,false,modular,dungeon_scene))return false;
    const prop_lease::World world{opened.world_epoch,opened.generation,opened.world,opened.map};
    const auto id=registry_id(snapshot,key);
    const auto metadata=building_metadata(building);
    const auto previous=registry.find(id);
    if(previous!=registry.end()) {
        const auto& saved=previous->second;
        if(saved.revoked||saved.unknown||saved.uid!=uid||saved.world!=world||saved.metadata!=metadata) {
            error="created building shared identity already binds a different state";return false;
        }
        return true;
    }
    for(const auto& [unused,saved]:registry) {
        if(saved.generation==registry_generation&&saved.world==world&&saved.uid==uid&&!saved.revoked) {
            error="created building local UID already has a shared identity";return false;
        }
    }
    if(next_entry==UINT64_MAX){error="shared building registry entry exhausted";return false;}
    Bound fresh;fresh.snapshot=snapshot;fresh.key=key;fresh.sha=opened.sha256;fresh.uid=uid;
    fresh.entry=++next_entry;fresh.generation=registry_generation;fresh.world=world;
    fresh.metadata=metadata;fresh.object=object;fresh.class_addresspoint=addresspoint;
    registry.emplace(id,std::move(fresh));
    LOG_INFO("[Action] Created building binding admitted key={} local_uid={}",key,uid);
    return true;
}
bool admit_created_building(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error){return admit_created_building_impl(snapshot,key,uid,error,false);}
bool admit_created_modular(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error){return admit_created_building_impl(snapshot,key,uid,error,true);}
bool admit_created_dungeon_building(const std::string& snapshot,const std::string& key,uint64_t uid,const std::string& scene,std::string& error){
    if(!action_session::valid_dungeon_building_scene(scene)){error="Dungeon Building admission scene invalid";return false;}
    return admit_created_building_impl(snapshot,key,uid,error,false,scene);
}

bool retire_undone_building(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error) {
    if(!main_thread.load()||main_thread.load()!=GetCurrentThreadId()||snapshot!=net::current_snapshot_id()||
       (!snapshot_building_key::valid(key)&&!scenery_scene_key::saved(key)&&!(key.size()==43&&key.compare(0,7,"action:")==0))) {error="building undo current owner or key invalid";return false;}
    const auto found=registry.find(registry_id(snapshot,key));
    if(found==registry.end()||found->second.revoked||found->second.unknown||found->second.uid!=uid) {
        error="building undo exact live binding missing";return false;
    }
    host::opened::CurrentWorld opened;uintptr_t image=0;uint32_t thread=0;std::string hash;
    if(!current(opened,error)||!native::read_resolver_receipt(image,thread,hash,error)||
       thread!=GetCurrentThreadId()||hash!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"||
       found->second.world!=prop_lease::World{opened.world_epoch,opened.generation,opened.world,opened.map}) {
        if(error.empty())error="building undo current world differs";return false;
    }
    host::ProcessMemory memory;host::LookupApi api;void* object=nullptr;uint32_t seh=0;
    if(!host::resolve_lookup(memory,image,hash,api,error)||!lookup(api,uid,object,seh)) {
        if(error.empty())error="building undo UID lookup SEH="+std::to_string(seh);return false;
    }
    if(object){error="building undo original UID still exists";return false;}
    found->second.revoked=true;error.clear();return true;
}
static bool restore_redone_building_impl(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error,bool modular,const std::string& dungeon_scene={}) {
    if(!main_thread.load()||main_thread.load()!=GetCurrentThreadId()||snapshot!=net::current_snapshot_id()||
       (!snapshot_building_key::valid(key)&&!scenery_scene_key::saved(key)&&!(key.size()==43&&key.compare(0,7,"action:")==0))) {error="building redo current owner or key invalid";return false;}
    const auto found=registry.find(registry_id(snapshot,key));
    if(found==registry.end()||!found->second.revoked||found->second.unknown||found->second.uid!=uid) {
        error="building redo exact tombstone binding missing";return false;
    }
    if(!history_retirement(found->second,error))return false;
    host::opened::CurrentWorld opened;native::BuildingState building;uintptr_t object=0,ap=0;
    if(!read_registered_building(uid,opened,building,object,ap,error,false,modular,dungeon_scene))return false;
    auto& saved=found->second;
    if(saved.world!=prop_lease::World{opened.world_epoch,opened.generation,opened.world,opened.map}||
       saved.metadata!=building_metadata(building)) {error="building redo UID/world/full native state differs";return false;}
    saved.object=object;saved.class_addresspoint=ap;saved.sequence=0;saved.revocation_key.clear();saved.revoked=false;error.clear();return true;
}
bool restore_redone_building(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error){return restore_redone_building_impl(snapshot,key,uid,error,false);}
bool restore_redone_modular(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error){return restore_redone_building_impl(snapshot,key,uid,error,true);}
bool restore_redone_dungeon_building(const std::string& snapshot,const std::string& key,uint64_t uid,const std::string& scene,std::string& error){
    if(!action_session::valid_dungeon_building_scene(scene)){error="Dungeon Building admission scene invalid";return false;}
    return restore_redone_building_impl(snapshot,key,uid,error,false,scene);
}

bool history_target_absent(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error) {
    if(!main_thread.load()||main_thread.load()!=GetCurrentThreadId()||snapshot!=net::current_snapshot_id()) {
        error="building history absence requires current snapshot/Mainthread";return false;
    }
    const auto found=registry.find(registry_id(snapshot,key));
    if(found==registry.end()||!found->second.revoked||found->second.unknown||found->second.uid!=uid) {
        error="building history absence exact tombstone binding missing";return false;
    }
    host::opened::CurrentWorld opened;uintptr_t image=0;uint32_t thread=0;std::string hash;
    if(!current(opened,error)||!native::read_resolver_receipt(image,thread,hash,error))return false;
    if(thread!=GetCurrentThreadId()||hash!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"||
       found->second.world!=prop_lease::World{opened.world_epoch,opened.generation,opened.world,opened.map}) {
        error="building history absence actual world/resolver differs";return false;
    }
    if(!history_retirement(found->second,error))return false;
    host::ProcessMemory memory;host::LookupApi api;void* object=nullptr;uint32_t seh=0;
    if(!host::resolve_lookup(memory,image,hash,api,error)||!lookup(api,uid,object,seh)) {
        if(error.empty())error="building history absence UID lookup SEH="+std::to_string(seh);return false;
    }
    if(object){error="building history restoration UID already live";return false;}
    error.clear();return true;
}
bool confirmed_remove_graph(const std::string& token,const std::string& snapshot,const std::string& key,
                            uint64_t uid,uintptr_t& graph,std::string& error) {
    graph=0;
    if(!history_target_absent(snapshot,key,uid,error))return false;
    const auto attempt=attempts.find(token),retired=retirements.find(token);
    if(attempt==attempts.end()||retired==retirements.end()||!attempt->second.receipt.value("confirmed",false)||
       !retired->second.receipt.value("confirmed",false)||attempt->second.arguments.at("snapshot_id")!=snapshot||
       attempt->second.arguments.at("record_key")!=key) {error="building history confirmed Remove/retirement receipt missing";return false;}
    const auto sequence=attempt->second.receipt.at("sequence").get<uint64_t>();
    const auto* journal=rt::Runtime::instance().private_retirement_status(sequence);
    const auto* record=journal?journal->record():nullptr;
    const auto found=registry.find(registry_id(snapshot,key));
    if(!record||!journal->effects().confirmed||journal->effects().unknown||!record->committed_graph||
       found==registry.end()||found->second.sequence!=sequence||record->request.full_uid!=uid||
       record->request.snapshot_binding!=binding_id(snapshot,key)||!record->commit.confirmed||record->commit.unknown_locked) {
        error="building history actual committed Remove graph/ownership differs";return false;
    }
    if(!retirement_provider.verify_actual_revocation(record->request,record->root,error))return false;
    graph=record->committed_graph;error.clear();return true;
}

bool is_dungeon_building_target(const LocalTarget& target,const std::string& scene,std::string& error){
    host::opened::CurrentWorld opened;native::BuildingState value;uintptr_t object=0,ap=0;
    if(target.snapshot_id!=net::current_snapshot_id()||
       !read_registered_building(target.uid,opened,value,object,ap,error,true,false,scene))return false;
    if(object!=target.object||opened.world_epoch!=target.world_epoch||opened.generation!=target.world_generation||
       value.type!=target.type||value.variant!=target.variant){
        error="Dungeon Building exact current target identity differs";return false;
    }
    error.clear();return true;
}
bool dungeon_building_scene(const LocalTarget& target,std::string& scene,bool& matched,std::string& error){
    scene.clear();matched=false;error.clear();
    if(target.normal_building||target.normal_scenery)return true;
    native::BuildingState value;
    if(!scenery_kiss::read_action_prop(target.uid,value,error))return false;
    if(value.type!="DungeonExterior"&&!value.dungeon_membership&&!value.dungeon_instance_membership)return true;
    matched=true;
    if(value.type=="DungeonExterior"&&!value.dungeon_membership&&!value.dungeon_instance_membership)scene="world";
    else{
        host::ProcessMemory memory;uintptr_t shard=0,region=0;
        if(!memory.copy(target.object+0x170,&shard,8,error)||!memory.copy(target.object+0x178,&region,8,error))return false;
        dungeon_scene_identity_runtime::World world;std::vector<dungeon_scene_roster::Scene> scenes;
        if(!dungeon_scene_identity_runtime::read_scene_roster(world,scenes,error))return false;
        for(const auto& row:scenes)if(row.owner.shard==shard&&row.owner.region==region&&row.owner.type==1){
            if(!scene.empty()){error="Dungeon Building current scene owner is ambiguous";return false;}
            scene=row.owner.key;
        }
        if(scene.empty()){error="Dungeon Building target has no current template owner";return false;}
    }
    if(!is_dungeon_building_target(target,scene,error)){scene.clear();return false;}
    return true;
}
bool refresh_moved_building(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error,const std::string& dungeon_scene) {
    error.clear();host::opened::CurrentWorld opened;native::BuildingState building;uintptr_t object=0,addresspoint=0;
    if(snapshot!=net::current_snapshot_id()){error="moved building snapshot differs from current session";return false;}
    const auto found=registry.find(registry_id(snapshot,key));
    if(found==registry.end()||found->second.revoked||found->second.unknown||found->second.uid!=uid) {
        error="moved building has no current shared binding";return false;
    }
    if(!read_registered_building(uid,opened,building,object,addresspoint,error,false,false,dungeon_scene))return false;
    auto& saved=found->second;
    if(saved.world!=prop_lease::World{opened.world_epoch,opened.generation,opened.world,opened.map}||
       saved.metadata.at("type")!=building.type||saved.metadata.at("variant")!=building.variant||
       (saved.object&&saved.object!=object)||(saved.class_addresspoint&&saved.class_addresspoint!=addresspoint)) {error="moved building identity changed";return false;}
    saved.metadata=building_metadata(building);saved.object=object;saved.class_addresspoint=addresspoint;
    LOG_INFO("[Action] Moved building binding refreshed key={} local_uid={}",key,uid);
    return true;
}
namespace {
bool resolve_local_target_impl(uintptr_t manager,uintptr_t object,LocalTarget& out,std::string& error,
                               const host::opened::CurrentWorld* admitted,bool metadata_only=false,const Bound* known=nullptr) {
    out={};error.clear();
    host::opened::CurrentWorld opened;
    if(!current(opened,error))return false;
    if(unknown_locked||!manager||!object||object>UINTPTR_MAX-0x20) {
        error="native UI target authority or pointer binding unavailable";return false;
    }
    if(admitted) {
        if(!host::same_world(*admitted,opened)){error="native UI admitted world changed before target observation";return false;}
    } else {
        json sources;if(!collect_bindings(sources,error,BindingDetails::Identity))return false;
    }
    auto& free=native_free_runtime::Runtime::instance();
    native_free_runtime::Receipt heap;
    if(!free.read_actual_receipt(heap,error))return false;
    host::ProcessMemory memory;host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(memory,heap.image,heap.executable.sha256.data(),api,error))return false;
    host::Reader reader(memory,origin,heap.image,heap.main_thread,api);host::WorldState world;
    if(!reader.current(world,true,error)||!host::same_world(opened,world.origin)||world.selection!=manager) {
        if(error.empty())error="native UI selection manager or complete world origin differs";return false;
    }
    uint64_t uid=0;
    if(!memory.copy(object+0x18,&uid,sizeof uid,error))return false;
    if(!uid){error="native UI target full UID is zero";return false;}
    prop_lease::Prop prop;
    if(!(metadata_only?read_metadata_prop(memory,reader,heap.image,world,reinterpret_cast<void*>(object),uid,prop,error):
         reader.prop(reinterpret_cast<void*>(object),uid,prop,error)))return false;
    native::BuildingState building;
    if(!scenery_kiss::read_action_prop(uid,building,error))return false;
    bool normal_building=false,normal_scenery=false;std::string kind_cause;
    if(!classify_normal_building(memory,heap.image,prop,building,normal_building,normal_scenery,kind_cause,error))return false;
    host::WorldState after;
    if(!reader.current(after,true,error)||!host::same_world(opened,after.origin)||after.selection!=manager) {
        if(error.empty())error="native UI complete world origin changed during target observation";return false;
    }
    const json metadata={{"type",building.type},{"variant",building.variant},
        {"x",building.prop.x},{"y",building.prop.y},{"z",building.prop.z}};
    const prop_lease::World actual{opened.world_epoch,opened.generation,opened.world,opened.map};
    const Bound* matched=known;
    if(known){
        if(!metadata_only||!admitted||known->generation!=registry_generation||known->world!=actual||known->uid!=uid||known->revoked||known->unknown){
            error="native readonly target supplied binding is no longer current";return false;
        }
    }else for(const auto& [id,binding]:registry) {
        if(binding.generation!=registry_generation||binding.world!=actual||binding.uid!=uid||binding.revoked||binding.unknown)continue;
        if(matched){error="native UI target snapshot binding is ambiguous";return false;}
        matched=&binding;
    }
    if(!matched||!scenery_kiss::metadata_matches(matched->metadata,building)||building.prop.uid!=uid||
       (matched->object&&matched->object!=object)||(matched->class_addresspoint&&matched->class_addresspoint!=prop.class_addresspoint)) {
        error="native UI target type/coordinates or snapshot registry differs";return false;
    }
    if(normal_scenery&&!matched->scene_owner.empty()){
        scene_scenery::Row scene;dungeon_scene_identity_runtime::World observed;
        if(!scene_scenery::owner(object,uid,scene,observed,error)||scene.scene!=matched->scene_owner||observed.game!=opened.game||
           observed.mode!=opened.mode||observed.state!=opened.state||observed.world!=opened.world||observed.map!=opened.map||
           observed.world_epoch!=opened.world_epoch||observed.generation!=opened.generation){if(error.empty())error="Scene Scenery target changed its confirmed current Scene owner";return false;}
    }
    out={manager,object,uid,opened.world_epoch,opened.generation,matched->snapshot,matched->key,building.type,building.variant,normal_building,normal_scenery,std::move(kind_cause)};
    return true;
}
}
bool resolve_current_building_key(const std::string& snapshot,uintptr_t object,uint64_t uid,std::string& key,std::string& error){
    key.clear();error.clear();
    if(!uid||!object||unknown_locked||!main_thread.load()||main_thread.load()!=GetCurrentThreadId()||snapshot!=net::current_snapshot_id()){
        error="current Building identity requires bound Mainthread, active snapshot and nonzero full UID/pointer";return false;}
    host::opened::CurrentWorld opened;if(!current(opened,error))return false;
    const prop_lease::World expected{opened.world_epoch,opened.generation,opened.world,opened.map};
    if(admitted_world!=expected){error="current Building identity has no admitted NativeOpen world";return false;}
    auto& free=native_free_runtime::Runtime::instance();native_free_runtime::Receipt heap;
    if(!free.read_actual_receipt(heap,error))return false;
    host::ProcessMemory memory;host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(memory,heap.image,heap.executable.sha256.data(),api,error))return false;
    host::Reader reader(memory,origin,heap.image,heap.main_thread,api);host::WorldState before;prop_lease::Prop prop;
    if(!reader.current(before,true,error)||!host::same_world(before.origin,opened)||
       !reader.prop(reinterpret_cast<void*>(object),uid,prop,error))return false;
    if(!prop.typed_prop||!prop.complete_ownership||prop.uid!=uid||prop.registry_object!=reinterpret_cast<void*>(object)||
       (prop.class_addresspoint!=heap.image+0x16d93c0&&prop.class_addresspoint!=heap.image+0x16c1390)){error="current Building identity exact root/manager/RTTI/full UID proof differs";return false;}
    const Bound* matched=nullptr;
    for(const auto& [id,entry]:registry){(void)id;
        if(entry.generation!=registry_generation||entry.world!=expected||entry.uid!=uid||entry.revoked||entry.unknown)continue;
        if(matched){error="current Building identity native full UID has multiple canonical registry entries";return false;}
        matched=&entry;}
    if(!matched||matched->snapshot!=snapshot||matched->key.empty()||
       (matched->object&&matched->object!=object)||(matched->class_addresspoint&&matched->class_addresspoint!=prop.class_addresspoint)){
        error="current Building identity has no exact unrevoked canonical registry entry";return false;}
    if(prop.class_addresspoint==heap.image+0x16c1390){scene_scenery::Row row;dungeon_scene_identity_runtime::World world;
        if(matched->scene_owner.empty()||!scene_scenery::owner(object,uid,row,world,error)||row.scene!=matched->scene_owner||
           world.world_epoch!=opened.world_epoch||world.generation!=opened.generation||world.world!=opened.world||world.map!=opened.map)
            {if(error.empty())error="Current Scene Scenery identity lacks its exact confirmed reciprocal Scene";return false;}}
    host::WorldState after;
    if(!reader.current(after,true,error)||!host::same_world(after.origin,opened)||after.root!=before.root||admitted_world!=expected){
        if(error.empty())error="current Building identity NativeOpen changed during read";return false;}
    key=matched->key;return true;
}

bool resolve_local_target(uintptr_t manager,uintptr_t object,LocalTarget& out,std::string& error) {
    return resolve_local_target_impl(manager,object,out,error,nullptr);
}
bool resolve_current_building_identity(const std::string& snapshot,const std::string& key,uintptr_t& object,uint64_t& uid,std::string& error){
    object=0;uid=0;const auto found=registry.find(registry_id(snapshot,key));
    if(found==registry.end()||found->second.revoked||found->second.unknown||found->second.generation!=registry_generation||found->second.snapshot!=snapshot){error="current Building key has no unrevoked current canonical identity";return false;}
    const auto full_uid=found->second.uid;auto& free=native_free_runtime::Runtime::instance();native_free_runtime::Receipt heap;
    if(!free.read_actual_receipt(heap,error))return false;host::ProcessMemory memory;host::LookupApi api;
    if(!host::resolve_lookup(memory,heap.image,heap.executable.sha256.data(),api,error))return false;
    void* actual=nullptr;uint32_t seh=0;if(!lookup(api,full_uid,actual,seh)){error="current Building full UID lookup SEH="+std::to_string(seh);return false;}
    if(!actual){error="current Building full UID lookup returned no actual object";return false;}
    std::string reverse;if(!resolve_current_building_key(snapshot,uintptr_t(actual),full_uid,reverse,error))return false;
    if(reverse!=key){error="current Building exact full UID reverse mapping differs";return false;}object=uintptr_t(actual);uid=full_uid;return true;
}
bool identity_targets(std::vector<LocalTarget>& out,std::string& error) {
    host::RootSceneryReadBatch batch;out.clear();json sources;if(!collect_bindings(sources,error,BindingDetails::Identity))return false;
    auto& free=native_free_runtime::Runtime::instance();native_free_runtime::Receipt heap;
    if(!free.read_actual_receipt(heap,error))return false;
    host::ProcessMemory memory;host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(memory,heap.image,heap.executable.sha256.data(),api,error))return false;
    host::Reader reader(memory,origin,heap.image,heap.main_thread,api);host::WorldState world;
    if(!reader.current(world,true,error))return false;
    const prop_lease::World expected{world.origin.world_epoch,world.origin.generation,world.origin.world,world.origin.map};
    std::map<uint64_t,const Bound*> by_uid;
    for(const auto& [id,binding]:registry){(void)id;
        if(binding.generation!=registry_generation||binding.world!=expected||binding.revoked||binding.unknown)continue;
        if(!by_uid.emplace(binding.uid,&binding).second){error="native UI target snapshot binding is ambiguous";return false;}
    }
    const auto resolve=[&](const json& row,LocalTarget& target,std::string& cause){
        const auto snapshot=row.at("snapshot_id").get<std::string>(),key=row.at("record_key").get<std::string>();
        const auto found=registry.find(registry_id(snapshot,key));
        if(found==registry.end()||found->second.revoked||found->second.unknown||found->second.world!=prop_lease::World{world.origin.world_epoch,world.origin.generation,world.origin.world,world.origin.map}){
            cause="Service census exact current unrevoked NativeOrigin binding missing";return false;
        }
        const auto unique=by_uid.find(found->second.uid);
        if(unique==by_uid.end()||unique->second!=&found->second){cause="Service census supplied binding differs from complete UID index";return false;}
        void* object=nullptr;uint32_t seh=0;
        if(!lookup(api,found->second.uid,object,seh)){cause="Service census full UID lookup SEH="+std::to_string(seh);return false;}
        if(!object){cause="Service census full UID no longer resolves";return false;}
        return resolve_local_target_impl(world.selection,uintptr_t(object),target,cause,&world.origin,true,&found->second);
    };
    std::vector<LocalTarget> pending;
    if(!service_building_census::read(sources,resolve,pending,error)||!batch.finish(error))return false;
    out=std::move(pending);return true;
}

#include "building_identity_census.inc"

bool is_modular_target(const LocalTarget& target,std::string& error){
    host::opened::CurrentWorld opened;native::BuildingState value;uintptr_t object=0,ap=0;
    if(target.snapshot_id!=net::current_snapshot_id()||!read_registered_building(target.uid,opened,value,object,ap,error,true,true))return false;
    if(target.object!=object||target.type!=value.type||target.variant!=value.variant||target.world_epoch!=opened.world_epoch||target.world_generation!=opened.generation){error="Modular target full canonical World binding differs";return false;}error.clear();return true;
}
bool resolve_record_target(const std::string& snapshot,const std::string& key,LocalTarget& out,std::string& error) {
    out={};json sources;if(!collect_bindings(sources,error,BindingDetails::Identity))return false;
    const auto found=registry.find(registry_id(snapshot,key));
    if(found==registry.end()||found->second.revoked||found->second.unknown) {
        error="native UI record has no current unrevoked binding";return false;
    }
    auto& free=native_free_runtime::Runtime::instance();native_free_runtime::Receipt heap;
    if(!free.read_actual_receipt(heap,error))return false;
    host::ProcessMemory memory;host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(memory,heap.image,heap.executable.sha256.data(),api,error))return false;
    host::Reader reader(memory,origin,heap.image,heap.main_thread,api);host::WorldState world;
    if(!reader.current(world,true,error))return false;
    const prop_lease::World expected=found->second.world;
    if(expected!=prop_lease::World{world.origin.world_epoch,world.origin.generation,world.origin.world,world.origin.map}) {
        error="native UI record registry and complete current world differ";return false;
    }
    void* object=nullptr;uint32_t seh=0;
    if(!lookup(api,found->second.uid,object,seh)) {error="native UI record lookup SEH="+std::to_string(seh);return false;}
    if(!object){error="native UI record lookup returned no target";return false;}
    if(!resolve_local_target_impl(world.selection,uintptr_t(object),out,error,&world.origin))return false;
    if(out.snapshot_id!=snapshot||out.record_key!=key||out.world_epoch!=expected.epoch||out.world_generation!=expected.origin_generation||
       found->second.revoked||found->second.unknown) {
        error="native UI record lookup resolved a different snapshot binding";out={};return false;
    }
    return true;
}
bool retire_region_scenery(const std::string& snapshot,const std::string& token,
    const std::vector<uint64_t>& removed,std::vector<std::string>& keys,std::string& error) {
    keys.clear();host::opened::CurrentWorld world;if(!current(world,error))return false;
    static const std::regex uuid("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    const prop_lease::World actual{world.world_epoch,world.generation,world.world,world.map};
    if(unknown_locked||admitted_world!=actual||snapshot!=net::current_snapshot_id()||!std::regex_match(token,uuid)) {
        error="region scenery retirement requires admitted current snapshot and action";return false;
    }
    std::set<uint64_t> ids(removed.begin(),removed.end());
    if(ids.size()!=removed.size()||ids.contains(0)){error="region removed scenery UIDs invalid or duplicate";return false;}
    native_free_runtime::Receipt heap;
    if(!native_free_runtime::Runtime::instance().read_actual_receipt(heap,error))return false;
    host::ProcessMemory memory;host::LookupApi api;
    if(!host::resolve_lookup(memory,heap.image,heap.executable.sha256.data(),api,error))return false;
    for(const auto uid:ids) {
        void* object=nullptr;uint32_t seh=0;
        if(!lookup(api,uid,object,seh)){error="region removed scenery lookup SEH="+std::to_string(seh);return false;}
        if(object){error="region removed scenery still registered uid="+std::to_string(uid);return false;}
    }
    std::vector<Bound*> matching;std::set<std::string> unique;
    for(auto& [id,entry]:registry) {
        if(entry.generation!=registry_generation||entry.world!=actual||entry.snapshot!=snapshot||!ids.contains(entry.uid))continue;
        if(entry.revoked||entry.unknown||!unique.insert(entry.key).second) {
            error="region removed scenery shared binding revoked, unknown or ambiguous";return false;
        }
        matching.push_back(&entry);
    }
    // Only the exact UIDs absent after the native region census lose their shared bindings.
    for(auto* entry:matching){entry->revoked=true;entry->revocation_key="native-region:"+token;}
    keys.assign(unique.begin(),unique.end());error.clear();return true;
}

bool remove(const json& args,json& out,std::string& error) {
    out=json::object();error.clear();
    static const std::regex uuid("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    const std::array<const char*,5> keys{"token","snapshot_id","record_key","expected_world_epoch","expected_world_generation"};
    if(!args.is_object()||args.size()!=keys.size()) {error="snapshot remove requires exact token, snapshot, record and world binding";return false;}
    for(const auto* key:keys)if(!args.contains(key)){error="snapshot remove argument missing";return false;}
    if(!args.at("token").is_string()||!args.at("snapshot_id").is_string()||!args.at("record_key").is_string()) {
        error="snapshot remove token, snapshot and record require strings";return false;
    }
    const auto token=args.at("token").get<std::string>(),snapshot=args.at("snapshot_id").get<std::string>(),key=args.at("record_key").get<std::string>();
    uint64_t epoch=0,generation=0;
    if(!std::regex_match(token,uuid)||!std::regex_match(snapshot,uuid)||key.empty()||key.size()>4096||
       !positive(args.at("expected_world_epoch"),epoch)||!positive(args.at("expected_world_generation"),generation)) {
        error="snapshot remove arguments invalid";return false;
    }
    host::opened::CurrentWorld world;if(!current(world,error))return false;
    if(const auto previous=attempts.find(token);previous!=attempts.end()) {
        out=previous->second.receipt;
        error=previous->second.arguments==args?"snapshot remove token already attempted; no replay":"snapshot remove conflicting token binding";
        return false;
    }
    if(unknown_locked){error="snapshot remove Unknown lock; no replay";return false;}
    if(world.world_epoch!=epoch||world.generation!=generation){error="snapshot remove fresh world differs";return false;}
    json sources;if(!bindings(sources,error))return false;
    const auto found=registry.find(registry_id(snapshot,key));
    const prop_lease::World actual{world.world_epoch,world.generation,world.world,world.map};
    if(found==registry.end()||found->second.world!=actual||found->second.revoked||found->second.unknown) {
        error="snapshot remove current unrevoked native binding unavailable";return false;
    }
    if(next_sequence==UINT64_MAX){error="snapshot remove sequence exhausted";return false;}
    const auto sequence=++next_sequence;
    auto [position,inserted]=attempts.emplace(token,Attempt{args,json{{"token",token},{"sequence",sequence},{"confirmed",false}}});
    if(!inserted){error="snapshot remove attempt reservation failed";return false;}
    auto& receipt=position->second.receipt;auto& binding=found->second;
    bool okay=false;
    try {
        auto& free=native_free_runtime::Runtime::instance();
        if(free.status().phase==native_free_runtime::Phase::Idle&&!free.prepare_and_enable(error)) {
            receipt["stage"]="free_installation";
        } else {
            auto& runtime=rt::Runtime::instance();
            if(runtime.status().phase==rt::RuntimePhase::Idle&&!runtime.prepare_and_enable(error)) {
                receipt["stage"]="remove_installation";
            } else {
                native_free_runtime::Receipt heap;host::ProcessMemory memory;host::NativeOrigin origin;host::LookupApi api;
                if(free.read_actual_receipt(heap,error)&&host::resolve_lookup(memory,heap.image,heap.executable.sha256.data(),api,error)) {
                    void* object=nullptr;uint32_t seh=0;
                    if(!lookup(api,binding.uid,object,seh))error="snapshot remove native UID lookup SEH="+std::to_string(seh);
                    else if(!object)error="snapshot remove native UID lookup empty";
                    else {
                        host::Reader reader(memory,origin,heap.image,heap.main_thread,api);prop_lease::Prop prop;
                        if(reader.prop(object,binding.uid,prop,error)) {
                            native::BuildingState actual_building;
                            if(!scenery_kiss::read_action_prop(binding.uid,actual_building,error))throw std::runtime_error(error);
                            const json metadata={{"type",actual_building.type},{"variant",actual_building.variant},
                                {"x",actual_building.prop.x},{"y",actual_building.prop.y},{"z",actual_building.prop.z}};
                            if(actual_building.prop.uid!=binding.uid||!scenery_kiss::metadata_matches(binding.metadata,actual_building)||
                               (binding.object&&binding.object!=uintptr_t(object))||(binding.class_addresspoint&&binding.class_addresspoint!=prop.class_addresspoint))
                                throw std::runtime_error("snapshot remove current type/coordinates differ from immutable binding");
                            binding.object=reinterpret_cast<uintptr_t>(object);binding.class_addresspoint=prop.class_addresspoint;
                            rt::PrivateRequest request{sequence,reinterpret_cast<uintptr_t>(object),binding.uid,actual,binding_id(snapshot,key)};
                            const auto result=runtime.execute(request);receipt["result"]=result_json(result);
                            receipt["stage"]="native_remove";okay=result.phase==rt::OperationPhase::Confirmed&&result.removal.confirmed;
                            error=result.removal.error;
                            if(okay){binding.revoked=true;binding.sequence=sequence;
                                binding.revocation_key="remove:"+std::to_string(binding.generation)+":"+std::to_string(binding.entry)+":"+std::to_string(sequence)+":"+token;
                                receipt["binding_revoked"]=true;}
                            if(result.phase==rt::OperationPhase::Unknown||result.pool.unknown_lock){binding.unknown=true;unknown_locked=true;}
                        }
                    }
                }
            }
        }
    } catch(const std::exception& failure) {error=failure.what();binding.unknown=true;unknown_locked=true;}
    catch(...) {error="snapshot remove diagnostic nonstandard exception";binding.unknown=true;unknown_locked=true;}
    if(!okay&&error.empty())error="snapshot remove failed without cause";
    receipt["confirmed"]=okay;receipt["error"]=error;receipt["snapshot_id"]=snapshot;receipt["record_key"]=key;
    receipt["world_epoch"]=epoch;receipt["world_generation"]=generation;out=receipt;
    if(okay)LOG_INFO("[Remove] Native snapshot target removed token={} sequence={} key={}",token,sequence,key);
    else LOG_ERROR("[Remove] Native snapshot target failed token={} sequence={} key={} cause={}",token,sequence,key,error);
    return okay;
}
bool retire(const json& args,json& out,std::string& error) {
    out=json::object();error.clear();
    static const std::regex uuid("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    if(!args.is_object()||args.size()!=1||!args.contains("token")||!args.at("token").is_string()||
       !std::regex_match(args.at("token").get<std::string>(),uuid)){error="remove retirement requires exact canonical operation token";return false;}
    const auto token=args.at("token").get<std::string>();
    if(!main_thread.load()||main_thread.load()!=GetCurrentThreadId()){error="remove retirement requires bound Mainthread";return false;}
    if(const auto previous=retirements.find(token);previous!=retirements.end()) {
        out=previous->second.receipt;error="remove retirement token already attempted; no replay";return false;
    }
    const auto previous=attempts.find(token);
    if(previous==attempts.end()||!previous->second.receipt.value("confirmed",false)||unknown_locked) {
        error="remove retirement confirmed nonUnknown operation unavailable";return false;
    }
    const auto sequence=previous->second.receipt.at("sequence").get<uint64_t>();
    auto& receipt=retirements.emplace(token,Attempt{args,json{{"token",token},{"sequence",sequence},{"confirmed",false}}}).first->second.receipt;
    bool okay=false;
    try {
        auto& runtime=rt::Runtime::instance();
        if(runtime.bind_root_retirement_service(retirement_provider,error))okay=runtime.retire_completed(sequence,error);
        if(const auto* state=runtime.operation_status(sequence)) {
            receipt["result"]=result_json(*state);
            if(state->phase==rt::OperationPhase::Unknown||state->pool.unknown_lock)unknown_locked=true;
        }
        if(const auto* journal=runtime.private_retirement_status(sequence)) {
            const auto& effects=journal->effects();json frees=json::array();
            for(const auto& value:effects.frees)frees.push_back({{"phase",int(value.phase)},
                {"allocation_generation",value.permit.allocation_generation},{"world_generation",value.permit.world_generation},
                {"retirement_receipt",value.permit.retirement_receipt},{"call",value.permit.call},{"bytes",value.permit.bytes},
                {"delete_attempted",value.delete_attempted},{"delete_returned",value.delete_returned},
                {"sized_attempted",value.sized_attempted},{"sized_returned",value.sized_returned},
                {"free_attempted",value.free_attempted},{"free_returned",value.free_returned},{"seh_code",value.seh_code}});
            receipt["effects"]={{"attempted",effects.attempted},{"confirmed",effects.confirmed},
                {"unknown",effects.unknown},{"cause",effects.cause},{"releases",effects.releases.size()},{"frees",std::move(frees)}};
        }
    }catch(const std::exception& failure){error=failure.what();unknown_locked=true;}
    catch(...){error="remove retirement nonstandard exception";unknown_locked=true;}
    if(!okay&&error.empty())error="remove retirement failed without cause";
    receipt["confirmed"]=okay;receipt["error"]=error;out=receipt;
    if(okay)LOG_INFO("[Remove] Native holders retired token={} sequence={}",token,sequence);
    else LOG_ERROR("[Remove] Native retirement failed token={} sequence={} cause={}",token,sequence,error);
    return okay;
}
#include "character_boss_retirement_runtime.inc"

#include "scenery_scene_identity.inc"
#include "building_scene_identity.inc"
}
