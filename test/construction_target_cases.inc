namespace construction_target_fixture {
struct Capture {uintptr_t image=fixture_image;struct Pin {World origin;} pin;};
namespace job_census=mt2mp::game::developer_staff::job_census;
using mt2mp::game::developer_staff::construction::occurrence;
struct LocalJob:mt2mp::game::developer_staff::construction::LocalJob {
    LocalJob(){building_uid=102;building_key=key(102);}
};
struct Target {LocalTarget binding;uintptr_t region=0,shard=0;uint8_t active=0;int32_t state=0;};
bool session_failure=false;
uint8_t active=1;
int32_t building_state=2;
bool bad_selection=false;
namespace action_session {
struct Context {std::string snapshot_uuid;};
struct Session {bool current(Context& out,std::string& error){if(session_failure){error="original Construction Session failed";return false;}out.snapshot_uuid=fixture_session_snapshot;return true;}};
Session selected_session(){return {};}
}
namespace remove_diagnostics {
bool resolve_identity_destination(const std::string& snapshot,const std::string& canonical,LocalTarget& out,std::string& error){
    return identity_refund_fixture::resolve_identity_destination(snapshot,canonical,out,error);
}
}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
template<class T>bool at(uintptr_t pointer,size_t offset,T& value,std::string& error){
    const auto store=[&]<class V>(const V& source){
        if constexpr(sizeof(T)==sizeof(V)){std::memcpy(&value,&source,sizeof value);return true;}
        else return fail(error,"Construction fixture field width differs");
    };
    if(pointer==fixture_image&&offset==0x13df630){const uintptr_t p=fixture_selection;return store(p);}
    if(pointer==fixture_selection&&offset==0){const uintptr_t ap=fixture_image+(bad_selection?0:0x17205d0);return store(ap);}
    if(pointer==object(102)&&offset==0x7e1)return store(active);
    if(pointer==object(102)&&offset==0x7a8)return store(building_state);
    host::ProcessMemory memory;return memory.copy(pointer+offset,&value,sizeof value,error);
}
const job_census::Read copy=[](uintptr_t pointer,void* out,size_t size,std::string& error){
    host::ProcessMemory memory;return memory.copy(pointer,out,size,error);
};
bool read_jobs(const Capture&,job_census::Census&,std::string& error){
    return fail(error,"Construction target-only fixture has no retained original Job queue");
}
#include "construction_target_actual.inc"
}
void construction_target_checks(){
    namespace fixture=construction_target_fixture;
    std::string error;LocalTarget target;
    const auto setup=[](){
        const auto canonical=natural_setup();auto& natural=registry.at(registry_id(::uuid,canonical));
        natural.scene_manager=0x71000000;natural.scene_region=fixture_saved_region;natural.scene_shard=fixture_root;
        fixture::session_failure=fixture::bad_selection=false;fixture::active=1;fixture::building_state=2;
        return canonical;
    };
    auto canonical=setup();identity_failure=101;
    need(resolve_identity_destination(::uuid,key(102),target,error)&&target.uid==102&&target.object==object(102),
         "canonical target-only resolver lost a baseline Building with no cached object pointer");
    need(registry.at(registry_id(::uuid,key(102))).object==0&&settlement_reads[101]==0&&settlement_reads[103]==0&&settlement_reads[104]==0&&settlement_reads[102]==2&&prop_reads==1,
         "canonical target-only resolver read unrelated metadata or omitted fresh target settlement/ownership");
    canonical=setup();live.at(102).prop.y=12;
    need(resolve_identity_destination(::uuid,key(102),target,error)&&registry.at(registry_id(::uuid,key(102))).metadata.at("y")==12&&logs==1,
         "canonical target-only resolver lost allowed native target height settlement");
    for(unsigned fault=0;fault<14;++fault){
        canonical=setup();auto& saved=registry.at(registry_id(::uuid,key(102)));error.clear();
        if(fault==9||fault==10){
            saved.metadata["rotation"]=live.at(102).rotation;
            saved.metadata["scale"]=live.at(102).scale;
        }
        if(fault==0)saved.revoked=true;
        if(fault==1)saved.unknown=true;
        if(fault==2)++saved.world.epoch;
        if(fault==3)++saved.generation;
        if(fault==4){auto duplicate=saved;duplicate.key+="duplicate";registry.emplace(registry_id(::uuid,duplicate.key),duplicate);}
        if(fault==5)lookup_override=object(103);
        if(fault==6)saved.object=object(103);
        if(fault==7)saved.class_addresspoint=fixture_image+0x16c1390;
        if(fault==8)live.at(102).prop.x+=1;
        if(fault==9)live.at(102).rotation[0]=0.5f;
        if(fault==10)live.at(102).scale[0]=2;
        if(fault==11)registry.at(registry_id(::uuid,canonical)).unknown=true;
        if(fault==12)unknown_locked=true;
        if(fault==13)fixture_saves=1;
        need(!resolve_identity_destination(::uuid,key(102),target,error)&&!error.empty()&&!target.object,
             (std::string("canonical target-only resolver accepted invalid identity or metadata; fault=")+std::to_string(fault)).c_str());
    }
    canonical=setup();need(!resolve_identity_destination("foreign",key(102),target,error)&&!target.object,
         "canonical target-only resolver accepted another snapshot");
    canonical=setup();host::batch_final_failure="original Construction target final World failure";error.clear();
    need(!resolve_identity_destination(::uuid,key(102),target,error)&&error==host::batch_final_failure&&!target.object,
         "canonical target-only resolver published before final World proof");
    host::batch_final_failure.clear();
    canonical=setup();fixture::Capture capture;capture.pin.origin=live_world;fixture::LocalJob job;fixture::Target actual;
    identity_failure=101;
    need(fixture::building(capture,job,actual,error)&&actual.binding.uid==102&&settlement_reads[101]==0&&settlement_reads[102]==2,
         "actual Construction building body still hydrates unrelated natural Scenery or lost its exact target");
    for(unsigned fault=0;fault<6;++fault){
        canonical=setup();capture.pin.origin=live_world;job={};error.clear();
        if(fault==0)job.building_uid=103;
        if(fault==1)++capture.pin.origin.world_epoch;
        if(fault==2)fixture::bad_selection=true;
        if(fault==3)fixture::active=2;
        if(fault==4)fixture::building_state=0;
        if(fault==5)fixture::session_failure=true;
        need(!fixture::building(capture,job,actual,error)&&!error.empty(),
             "actual Construction building body lost UID/Origin/SelectionManager/online/Boolean or Session guard");
    }
    canonical=setup();identity_failure=101;json report;
    need(!identity_bindings(report,error)&&error=="fixture live building identity failed",
         "target-only Construction path changed complete default identity admission");
    reset();
}
