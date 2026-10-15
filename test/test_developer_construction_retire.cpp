#include "developer_staff/construction_native.h"
#include "developer_staff/construction_calls.h"
#include "developer_staff/construction_occurrence.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
using namespace mt2mp::game;
using namespace mt2mp::game::developer_staff;
namespace fixture {
struct Model {
    Capture capture;job_census::Census original;
    std::vector<uintptr_t> slots;int32_t count=0;
    std::map<uintptr_t,job_census::Job> jobs;
    std::set<uintptr_t> destroyed;
    bool fail_shift=false,fail_count=false,fail_destroy=false,corrupt_selected=false,corrupt_survivor=false;
    bool shifted=false,expired=false,fail_proxy=false;int reads_after_free=0,destroy_calls=0,building_calls=0,cached_reads=0;
    uintptr_t fresh_building=0;uint8_t fresh_active=1;
} model;
}
namespace mt2mp::game::developer_staff {
bool read_process(Capture& out,std::string&){out=fixture::model.capture;return true;}
namespace job_census {
bool read(const Read&,const Capture&,Census& out,std::string&){auto& m=fixture::model;out=m.original;out.jobs.clear();
    for(int32_t i=0;i<m.count;++i)out.jobs.push_back(m.jobs.at(m.slots[size_t(i)]));
    for(auto& d:out.developers){d.job_index=-1;for(size_t i=0;i<out.jobs.size();++i)if(out.jobs[i].pointer==d.job)d.job_index=int32_t(i);}
    return true;
}
}
}
namespace mt2mp::game::live_host_state {
bool compare_fixed_i32(uintptr_t address,int32_t expected,int32_t target,std::string& e){auto& m=fixture::model;
    if(m.fail_count||address!=m.original.queue+0x28||m.count!=expected){e="original count compare failed";return false;}
    m.count=target;return true;
}
}
namespace mt2mp::game::developer_staff::construction {
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
struct Memory {bool copy(uintptr_t p,void* output,size_t n,std::string& e){auto& m=fixture::model;
    if(m.destroyed.contains(p)){++m.reads_after_free;return fail(e,"freed receiver read");}
    for(const auto& entry:m.jobs){const auto& j=entry.second;
        if(p==j.building){++m.cached_reads;return fail(e,"cached Building read");}
        if(p==j.building_proxy&&n==sizeof(uintptr_t)){
            if(m.fail_proxy)return fail(e,"actual proxy read failed");
            *static_cast<uintptr_t*>(output)=m.expired?0:j.building+0x20;return true;}
    }
    if(!m.jobs.contains(p)||n!=0xc8)return fail(e,"unexpected memory extent");
    std::fill_n(static_cast<unsigned char*>(output),n,static_cast<unsigned char>(p));
    if(m.shifted&&m.corrupt_selected)static_cast<unsigned char*>(output)[0]^=1;
    return true;
}} memory;
const job_census::Read copy=[](auto p,auto out,auto n,auto& e){return memory.copy(p,out,n,e);};
struct Work {Capture before;bool attempted=false;bool fresh(std::string&){return true;}};
struct Target {struct Binding{uintptr_t object=0;} binding;uintptr_t region=0;uint8_t active=1;};
bool owners(const Capture&,const Capture&,std::string&){return true;}
bool building(const Capture&,const LocalJob& link,Target& out,std::string&){auto& m=fixture::model;++m.building_calls;
    out.binding.object=m.fresh_building?m.fresh_building:link.building_uid;out.active=m.fresh_active;out.region=m.fresh_building?78:77;return true;}
bool call(Work& w,const Call& r,const char*,std::string& e){w.attempted=w.attempted||r.attempted;
    return (r.returned&&!r.exception)||fail(e,"original call failed");}
uint32_t shift(uintptr_t,uintptr_t dest,uintptr_t src,size_t bytes,Call* out){auto& m=fixture::model;out->attempted=true;
    if(m.fail_shift){out->exception=1;return 1;}
    const auto base=m.original.data;for(size_t i=0;i<bytes/8;++i)m.slots[(dest-base)/8+i]=m.slots[(src-base)/8+i];
    m.shifted=true;out->returned=true;return 0;
}
uint32_t unary(uintptr_t,Unary command,uintptr_t object,Call* out){auto& m=fixture::model;out->attempted=true;
    if(command!=Unary::DestroyJob){out->exception=2;return 2;}++m.destroy_calls;
    if(m.fail_destroy){out->exception=3;return 3;}
    m.destroyed.insert(object);out->returned=true;
    if(m.corrupt_survivor&&!m.slots.empty())m.jobs[m.slots[0]].valid=0;
    return 0;
}
#include "construction_retire_body.inc"
}
namespace {
void require(bool v,const char* e){if(!v)throw std::runtime_error(e);}
construction::LocalJob link(uintptr_t job,uint64_t uid,bool alive=true){
    const auto& j=fixture::model.jobs.at(job);return {"action:construction",job,uid,alive,false,j.building,j.building_proxy,j.region};
}
void reset(){fixture::model={};auto& m=fixture::model;m.original.queue=1000;m.original.data=2000;m.original.capacity=8;m.count=3;
    for(uintptr_t i=0;i<3;++i){job_census::Job j;j.pointer=3000+i*256;j.type=1;j.valid=1;j.building=5000+i;j.region=77;j.building_proxy=6000+i;
        m.jobs.emplace(j.pointer,j);m.slots.push_back(j.pointer);}
    job_census::Developer d;d.id=2;d.pointer=7000;d.job=m.slots[2];d.job_index=2;d.reciprocal=true;
    m.original.developers.push_back(d);m.jobs.at(d.job).developer=d.pointer;
}
}
int main(){try{using namespace mt2mp::game::developer_staff::construction;size_t cases=0;
    for(size_t selected:{size_t(0),size_t(1),size_t(2)}){reset();auto& m=fixture::model;m.original.developers.clear();m.jobs.at(m.slots[2]).developer=0;
        const auto pointer=m.slots[selected];Work w;std::string e;
        require(retire(w,link(pointer,5000+selected),e),"selected native retirement failed");
        require(m.destroy_calls==1&&m.destroyed.contains(pointer)&&m.reads_after_free==0&&m.count==2,"selected lifetime readback differs");++cases;}
    reset();{Work w;std::string e;require(retire(w,link(3000,5000),e),"surviving reciprocal index was not rebased");++cases;}
    reset();{Work w;std::string e;require(!retire(w,link(3512,5002),e)&&!w.attempted&&fixture::model.destroy_calls==0,"linked Job retired");++cases;}
    reset();fixture::model.fail_shift=true;{Work w;std::string e;require(!retire(w,link(3000,5000),e)&&w.attempted&&fixture::model.destroy_calls==0,"memmove failure lost uncertainty");++cases;}
    reset();fixture::model.fail_count=true;{Work w;std::string e;require(!retire(w,link(3000,5000),e)&&w.attempted&&fixture::model.destroy_calls==0,"count failure reached destructor");++cases;}
    reset();fixture::model.corrupt_selected=true;{Work w;std::string e;require(!retire(w,link(3000,5000),e)&&w.attempted&&fixture::model.destroy_calls==0,"changed detached Job destroyed");++cases;}
    reset();fixture::model.fail_destroy=true;{Work w;std::string e;require(!retire(w,link(3000,5000),e)&&w.attempted&&fixture::model.destroy_calls==1,"destructor failure hidden");++cases;}
    reset();fixture::model.corrupt_survivor=true;{Work w;std::string e;require(!retire(w,link(3000,5000),e)&&w.attempted&&fixture::model.reads_after_free==0,"surviving graph mutation hidden");++cases;}
    reset();{Work w;std::string e;require(!retire(w,{"action:a",9999,5000},e)&&!w.attempted&&!e.empty(),"missing selected Job silently accepted");++cases;}
    reset();fixture::model.expired=true;{Work w;std::string e;
        require(retire(w,link(3000,5000,false),e)&&fixture::model.building_calls==0&&fixture::model.cached_reads==0,"expired target retirement dereferenced Building");++cases;}
    reset();{Work w;std::string e;require(!retire(w,link(3000,5000,false),e)&&!w.attempted&&fixture::model.building_calls==0,"tombstone with live weak target accepted");++cases;}
    reset();fixture::model.expired=true;fixture::model.fail_proxy=true;{Work w;std::string e;
        require(!retire(w,link(3000,5000,false),e)&&!w.attempted&&e=="actual proxy read failed","expired proxy read cause replaced");++cases;}
    reset();fixture::model.expired=true;{Work w;std::string e;auto old=link(3000,5000,false);old.building_proxy+=8;
        require(!retire(w,old,e)&&!w.attempted,"changed expired occurrence proxy accepted");++cases;}
    reset();{Work w;std::string e;auto old=link(3000,5000);old.retired=true;
        require(!retire(w,old,e)&&!w.attempted,"previously retired occurrence revived");++cases;}
    reset();fixture::model.expired=true;fixture::model.fresh_building=9000;fixture::model.fresh_active=0;
    {Work w;std::string e;auto old=link(3000,5000);old.target_restored=true;
        require(retire(w,old,e)&&fixture::model.destroy_calls==1&&fixture::model.cached_reads==0&&fixture::model.fresh_active==0,"restored inactive Building was activated or rebound on old Job retirement");++cases;}
    reset();fixture::model.expired=true;fixture::model.fresh_building=9000;fixture::model.fresh_active=0;
    {Work w;std::string e;require(!retire(w,link(3000,5000),e)&&!w.attempted,"fresh receiver accepted without confirmed Redo");++cases;}
    reset();{Work w;std::string e;auto old=link(3000,5000);old.target_restored=true;
        require(!retire(w,old,e)&&!w.attempted,"restored target silently bound to old live Job proxy");++cases;}
    std::cout<<"PASS "<<cases<<" actual selected retirement cases\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
