namespace duplicate_construction_native_fixture {
namespace action_session=mt2mp::action_session;
namespace staff=mt2mp::game::developer_staff;
namespace job_census=staff::job_census;
using Capture=staff::Capture;using Origin=staff::Origin;
using LocalJob=staff::construction::LocalJob;using PlacementBefore=staff::construction::PlacementBefore;
using staff::construction::occurrence;
Capture fixture_current;job_census::Census fixture_queue;
std::map<std::string,mt2mp::game::remove_diagnostics::LocalTarget> fixture_targets;
std::map<uintptr_t,uintptr_t> fixture_words;
bool fixture_read_ok=true,fixture_resolve_ok=true,fixture_queue_change=false,fixture_pool_change=false;
unsigned fixture_reads=0,fixture_queue_reads=0;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool fixture_duplicate_read_process(Capture& out,std::string& error){++fixture_reads;
    if(!fixture_read_ok)return fail(error,"original Duplicate process read failed");
    out=fixture_current;if(fixture_pool_change&&fixture_reads==2)++out.state.business_count;return true;
}
bool read_jobs(const Capture&,job_census::Census& out,std::string&){++fixture_queue_reads;out=fixture_queue;
    if(fixture_queue_change&&fixture_queue_reads==2)++out.jobs.back().raw_id;return true;}
template<class T>bool at(uintptr_t pointer,size_t offset,T& out,std::string& error){
    const auto found=fixture_words.find(pointer+offset);
    if(found==fixture_words.end()||sizeof(T)!=sizeof(uintptr_t))return fail(error,"original Duplicate native field unavailable");
    std::memcpy(&out,&found->second,sizeof out);return true;
}
const job_census::Read copy=[](uintptr_t pointer,void* out,size_t size,std::string& error){
    const auto found=fixture_words.find(pointer);
    if(found==fixture_words.end()||size!=sizeof(uintptr_t))return fail(error,"original Duplicate proxy read failed");
    std::memcpy(out,&found->second,size);return true;
};
namespace remove_diagnostics {
using LocalTarget=mt2mp::game::remove_diagnostics::LocalTarget;
bool resolve_record_target(const std::string& snapshot,const std::string& key,LocalTarget& out,std::string& error){
    if(!fixture_resolve_ok)return fail(error,"original Duplicate target resolver failed");
    const auto found=fixture_targets.find(key);
    if(found==fixture_targets.end()||snapshot!="snapshot")return fail(error,"foreign Duplicate target");
    out=found->second;return true;
}
}
#include "construction_duplicate_origin.inc"
#include "construction_duplicate_owners.inc"
#define read_process fixture_duplicate_read_process
#include "developer_staff/construction_duplicate.inc"
#undef read_process
void reset(){
    fixture_current={};fixture_current.image=0x140000000;fixture_current.thread=17;
    fixture_current.pin.origin.world_epoch=7;fixture_current.pin.origin.generation=9;
    fixture_queue={};fixture_queue.queue=0x1000;fixture_targets.clear();fixture_words.clear();
    fixture_read_ok=fixture_resolve_ok=true;fixture_queue_change=fixture_pool_change=false;fixture_reads=fixture_queue_reads=0;
}
void add(uint64_t uid,const std::string& key,uintptr_t pointer){
    mt2mp::game::remove_diagnostics::LocalTarget target;target.object=pointer;target.uid=uid;
    target.normal_building=true;target.snapshot_id="snapshot";target.record_key=key;target.world_epoch=7;target.world_generation=9;
    fixture_targets[key]=target;fixture_words[pointer+0x178]=0x2000;fixture_words[pointer+0x1000]=pointer+0x20;
    job_census::Job job;job.pointer=pointer+0x2000;job.building=pointer;job.building_proxy=pointer+0x1000;
    job.type=1;job.valid=1;job.region=0x2000;fixture_queue.jobs.push_back(job);
}
void run(){
    std::string error;std::vector<LocalJob> out;
    const auto need=[](bool value,const std::string& cause){fixture::need(value,cause);};
    for(bool host:{false,true}){
        reset();fixture_current.pin.time_scale=host?1.0f:0.0f;
        PlacementBefore before{fixture_current,fixture_queue};
        add(500,"first",0x3000);add(600,"second",0x6000);
        need(duplicate_construction_native_fixture::finish_duplicate(before,"snapshot",{{500,"first"},{600,"second"}},out,error)&&out.size()==2&&
            out[0].building_key=="first"&&out[1].building_key=="second"&&out[0].building_uid==500&&out[1].building_uid==600&&
            fixture_reads==2&&fixture_queue_reads==2,"actual multi-Building Host/Client Duplicate Jobs lacked complete repeated admission: "+error);
    }
    for(int wrong=0;wrong<10;++wrong){reset();PlacementBefore before{fixture_current,fixture_queue};add(500,"first",0x3000);
        if(wrong==0)fixture_queue.jobs[0].building=0xdead;
        if(wrong==1)fixture_queue.jobs[0].developer=0xbeef;
        if(wrong==2)fixture_targets.at("first").uid++;
        if(wrong==3)fixture_targets.at("first").world_generation++;
        if(wrong==4)fixture_words[0x4000]=0;
        if(wrong==5){before.queue.jobs=fixture_queue.jobs;before.queue.jobs[0].raw_id++;}
        if(wrong==6)fixture_read_ok=false;
        if(wrong==7)fixture_resolve_ok=false;
        if(wrong==8)fixture_queue_change=true;
        if(wrong==9)fixture_pool_change=true;
        out.push_back({"stale",1,1});
        need(!duplicate_construction_native_fixture::finish_duplicate(before,"snapshot",{{500,"first"}},out,error)&&out.empty()&&!error.empty(),
            "foreign/changed/unreadable Duplicate Job published a partial occurrence set case="+std::to_string(wrong));
    }
    reset();PlacementBefore before{fixture_current,fixture_queue};add(500,"first",0x3000);fixture_queue.jobs.clear();
    need(duplicate_construction_native_fixture::finish_duplicate(before,"snapshot",{{500,"first"}},out,error)&&out.empty(),"original online Duplicate without new Job must remain valid: "+error);
    reset();before={fixture_current,fixture_queue};add(500,"first",0x3000);fixture_queue.jobs.push_back(fixture_queue.jobs[0]);
    need(!duplicate_construction_native_fixture::finish_duplicate(before,"snapshot",{{500,"first"}},out,error)&&out.empty(),"extra unknown Duplicate Job weakened complete queue coverage");
}
}
