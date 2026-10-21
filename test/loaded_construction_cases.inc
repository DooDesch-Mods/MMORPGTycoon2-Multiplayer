namespace loaded_construction_fixture {
namespace action_session=mt2mp::action_session;
namespace staff=mt2mp::game::developer_staff;
namespace job_census=staff::job_census;
using Capture=staff::Capture;using LocalJob=staff::construction::LocalJob;
using staff::construction::occurrence;
struct Target {struct Binding {uintptr_t object=100;std::string snapshot_id="snapshot",type="SmallInn",variant="default";} binding;
    uintptr_t region=300;uint8_t active=0;int32_t state=2;};
Capture current;job_census::Census jobs;Target fixture_target;
const std::string key=std::string(64,'a')+":world:building:inline:region{60}/buildingManager/building[0]";
uint64_t uid=500;uintptr_t weak=100+0x20;unsigned reads=0,queues=0,fixture_target_reads=0;
bool read_ok=true,key_ok=true,target_ok=true,queue_change=false,pool_change=false,target_change=false,proxy_ok=true;
bool fail(std::string& e,const std::string& why){e=why;return false;}
namespace native_free_runtime {
enum class Phase {Idle,Enabled,Failed,Unknown};
struct Receipt {uintptr_t image=0;uint32_t main_thread=0;};
struct Runtime {
    Phase fixture_phase=Phase::Idle;
    unsigned fixture_prepare_calls=0,fixture_receipt_reads=0;
    bool fixture_prepare_ok=true,fixture_receipt_ok=true,fixture_wrong_image=false,fixture_wrong_thread=false;
    static Runtime& instance(){static Runtime fixture_instance;return fixture_instance;}
    struct Status {Phase phase;};
    Status status()const{return {fixture_phase};}
    bool prepare_and_enable(std::string& e){
        ++fixture_prepare_calls;
        if(reads!=1||queues!=1)return fail(e,"Free installation preceded actual current Mainthread/Origin and queue");
        if(!fixture_prepare_ok){fixture_phase=Phase::Failed;return fail(e,"original Free installer failed");}
        fixture_phase=Phase::Enabled;return true;
    }
    bool read_actual_receipt(Receipt& out,std::string& e){
        ++fixture_receipt_reads;
        if(fixture_phase!=Phase::Enabled)return fail(e,"original sticky Free runtime failure");
        if(!fixture_receipt_ok)return fail(e,"original Free receipt failed");
        out={current.image+(fixture_wrong_image?1u:0u),current.thread+(fixture_wrong_thread?1u:0u)};return true;
    }
};
}
void reset(){native_free_runtime::Runtime::instance()={};current={};current.image=0x140000000;current.thread=17;current.pin.origin.world_epoch=7;current.pin.origin.generation=9;current.state.rows.resize(50);
    jobs={};job_census::Job job;job.pointer=400;job.building=100;job.building_proxy=200;job.region=300;job.type=1;job.valid=1;
    job.position={1,2,3};job.destination={4,5,6};jobs.jobs={job};fixture_target={};uid=500;weak=100+0x20;reads=queues=fixture_target_reads=0;
    read_ok=key_ok=target_ok=proxy_ok=true;queue_change=pool_change=target_change=false;}
bool fixture_loaded_read_process(Capture& out,std::string& e){++reads;if(!read_ok)return fail(e,"loaded original read failure");out=current;
    if(pool_change&&reads==2)++out.state.rows[0].state;return true;}
bool owners(const Capture& a,const Capture& b,std::string& e){return (a.pin.origin.world_epoch==b.pin.origin.world_epoch&&a.pin.origin.generation==b.pin.origin.generation&&a.pin.time_scale==b.pin.time_scale)||fail(e,"loaded owner changed");}
bool read_jobs(const Capture&,job_census::Census& out,std::string&){++queues;out=jobs;if(queue_change&&queues==2)out.jobs[0].position[0]+=1;return true;}
template<class T>bool at(uintptr_t object,size_t offset,T& out,std::string& e){if(object!=100||offset!=0x18||sizeof(T)!=sizeof uid)return fail(e,"unexpected loaded target field");std::memcpy(&out,&uid,sizeof out);return true;}
const job_census::Read copy=[](uintptr_t pointer,void* out,size_t size,std::string& e){
    if(!proxy_ok)return fail(e,"loaded original proxy read failure");
    if(pointer!=200||size!=sizeof weak)return fail(e,"unexpected loaded proxy field");std::memcpy(out,&weak,size);return true;};
namespace remove_diagnostics {
bool resolve_current_building_key(const std::string& snapshot,uintptr_t object,uint64_t actual,std::string& out,std::string& e){
    const auto& fixture_free=native_free_runtime::Runtime::instance();
    if(fixture_free.fixture_phase!=native_free_runtime::Phase::Enabled||fixture_free.fixture_receipt_reads!=1)
        return fail(e,"loaded canonical target preceded actual Free receipt");
    if(!key_ok||snapshot!="snapshot"||object!=100||actual!=500)return fail(e,"loaded exact canonical owner failure");out=key;return true;}
}
bool building(const Capture&,const LocalJob& link,Target& out,std::string& e){++fixture_target_reads;
    if(!target_ok||link.building_key!=key||link.building_uid!=500)return fail(e,"loaded original target failure");out=fixture_target;
    if(target_change&&fixture_target_reads==2)out.binding.variant="changed";return true;}
#define read_process fixture_loaded_read_process
#include "developer_staff/loaded_construction.inc"
#undef read_process
}
void loaded_construction_cases(){
    using namespace loaded_construction_fixture;std::string error;std::vector<LocalJob> out;const action_session::Binding binding{7,9};
    for(bool host:{false,true}){reset();current.pin.time_scale=host?1.0f:0.0f;
        jobs.jobs[0].developer=700;jobs.jobs[0].developer_id=4;jobs.jobs[0].reciprocal=true;
        current.state.rows[4].job_link=true;current.state.rows[4].position={2,3,4};
        const auto before=jobs;const auto pool=current.state;
        const bool capture_ok=loaded_construction_fixture::capture_loaded("snapshot",binding,out,error);
        need(capture_ok&&out.size()==1&&out[0].building_key==key&&out[0].building_uid==500&&
             out[0].job==400&&out[0].building==100&&out[0].building_proxy==200&&out[0].region==300,
             "loaded Host/Client partially progressed Job did not retain exact canonical occurrence: "+error);
        need(jobs==before&&current.state==pool&&queues==2&&fixture_target_reads==2,"loaded admission changed progress or omitted repeated queue/target proof");
    }
    reset();native_free_runtime::Runtime::instance().fixture_phase=native_free_runtime::Phase::Enabled;
    need(loaded_construction_fixture::capture_loaded("snapshot",binding,out,error)&&
         native_free_runtime::Runtime::instance().fixture_prepare_calls==0&&
         native_free_runtime::Runtime::instance().fixture_receipt_reads==1,"already enabled Free runtime reinstalled or skipped its actual receipt");
    for(int fixture_failure=0;fixture_failure<6;++fixture_failure){
        reset();auto& fixture_free=native_free_runtime::Runtime::instance();
        if(fixture_failure==0)fixture_free.fixture_prepare_ok=false;
        if(fixture_failure==1)fixture_free.fixture_phase=native_free_runtime::Phase::Failed;
        if(fixture_failure==2)fixture_free.fixture_receipt_ok=false;
        if(fixture_failure==3)fixture_free.fixture_wrong_image=true;
        if(fixture_failure==4)fixture_free.fixture_wrong_thread=true;
        if(fixture_failure==5)fixture_free.fixture_phase=native_free_runtime::Phase::Unknown;
        const bool fixture_captured=loaded_construction_fixture::capture_loaded("snapshot",binding,out,error);
        need(!fixture_captured&&out.empty()&&fixture_target_reads==0,"failed or foreign Free receipt reached loaded target resolution");
        need(fixture_free.fixture_prepare_calls==((fixture_failure==1||fixture_failure==5)?0u:1u)&&
             fixture_free.fixture_receipt_reads==(fixture_failure==0?0u:1u),"Free sticky installer/receipt failure ordering changed");
        if(fixture_failure<3||fixture_failure==5)need(error==(fixture_failure==0?"original Free installer failed":(fixture_failure==1||fixture_failure==5)?
            "original sticky Free runtime failure":"original Free receipt failed"),"original Free failure was replaced");
    }
    reset();read_ok=false;
    need(!loaded_construction_fixture::capture_loaded("snapshot",binding,out,error)&&
         native_free_runtime::Runtime::instance().fixture_prepare_calls==0,"Free installer ran before proven current Mainthread");
    reset();jobs.jobs.clear();need(loaded_construction_fixture::capture_loaded("snapshot",binding,out,error)&&out.empty(),"new-world empty construction changed");
    reset();jobs.jobs[0].type=2;need(loaded_construction_fixture::capture_loaded("snapshot",binding,out,error)&&out.empty()&&fixture_target_reads==0,"another Job family was claimed as construction");
    for(int bad=0;bad<18;++bad){reset();out={{"stale",99,99}};
        switch(bad){case 0:current.pin.origin.world_epoch++;break;case 1:current.pin.origin.generation++;break;
        case 2:jobs.jobs[0].building=0;break;case 3:jobs.jobs[0].building_proxy=0;break;case 4:jobs.jobs[0].region=0;break;
        case 5:uid=0;break;case 6:key_ok=false;break;case 7:target_ok=false;break;case 8:jobs.jobs[0].valid=0;break;
        case 9:jobs.jobs[0].monster=1;break;case 10:jobs.jobs[0].bug.pointer=1;break;
        case 11:jobs.jobs[0].developer=700;break;case 12:weak=0;break;case 13:fixture_target.region=301;break;
        case 14:fixture_target.binding.snapshot_id="foreign";break;case 15:jobs.jobs.push_back(jobs.jobs[0]);break;
        case 16:read_ok=false;break;case 17:proxy_ok=false;break;}
        need(!loaded_construction_fixture::capture_loaded("snapshot",binding,out,error)&&out.empty()&&!error.empty(),"invalid loaded occurrence admitted or stale output retained");
    }
    reset();queue_change=true;need(!loaded_construction_fixture::capture_loaded("snapshot",binding,out,error)&&out.empty()&&error.find("graph changed")!=std::string::npos,"loaded queue progress changed before publication");
    reset();pool_change=true;need(!loaded_construction_fixture::capture_loaded("snapshot",binding,out,error)&&out.empty(),"loaded actor progress changed before publication");
    reset();target_change=true;need(!loaded_construction_fixture::capture_loaded("snapshot",binding,out,error)&&out.empty(),"loaded target metadata changed before publication");
}
