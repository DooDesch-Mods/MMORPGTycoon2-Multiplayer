#include "bug_authority/job_retire.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game;
using namespace bug_authority;
namespace {
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
struct Native final:JobRetireServices {
    JobLease lease{0x10000000,0x1000,0x2000,0x3000,0x4000,1,"snapshot","bug-birth#job-birth",3,4};
    JobBirthSnapshot world;
    std::string fault;
    int detached=0,erased=0,destroyed=0,authorized=0;
    Native(){
        world.census.queue=lease.queue;world.census.data=0x5000;world.census.capacity=4;
        world.parent=lease.parent;world.region=lease.region;world.position={5,6,7};world.bug_job=0x7000;
        developer_staff::job_census::Job selected;selected.pointer=0x7000;selected.type=7;selected.valid=1;selected.region=lease.region;
        selected.bug.pointer=lease.bug;selected.bug.anchor=lease.parent;selected.bug.job=selected.pointer;
        developer_staff::job_census::Job other;other.pointer=0x8000;other.type=1;other.valid=1;
        world.census.jobs={selected,other};world.census.developers.push_back({0x9000,0,0,9,-1,false});
    }
    bool authorize(const JobLease&,std::string& error)override{
        ++authorized;if((fault=="guard"&&authorized==1)||(fault=="late_guard"&&authorized==4)){error="original retirement owner changed";return false;}return true;
    }
    bool observe(const JobLease&,JobBirthSnapshot& out,std::string&)override{out=world;return true;}
    bool detach(const JobLease&,uintptr_t,subscriber_lifecycle_append::BugPairReceipt& out,std::string& error)override{
        ++detached;out.attempted=true;world.bug_job=0;
        if(fault=="detach_throw")throw std::runtime_error("original detach exception");
        if(fault=="detach"){out.unknown=true;error="original detach fault";return false;}
        world.census.jobs[0].bug={};out.confirmed=true;
        if(fault=="detach_shape")world.census.jobs[1].type=2;
        return true;
    }
    bool erase(const JobLease&,const JobBirthSnapshot&,size_t index,JobCall& out,std::string& error)override{
        ++erased;out.attempted=true;world.census.jobs.erase(world.census.jobs.begin()+static_cast<ptrdiff_t>(index));
        if(fault=="erase"){error="original erase fault";return false;}
        out.returned=true;
        if(fault=="counter")++world.counters[0x50];
        if(fault=="other_job")world.census.jobs[0].pointer=0xdead;
        if(fault=="developer")world.census.developers[0].job=0x8000;
        return true;
    }
    bool destroy(const JobLease&,uintptr_t job,JobCall& out,std::string& error)override{
        ++destroyed;out.attempted=true;
        require(job==0x7000&&world.bug_job==0&&world.census.jobs.size()==1&&world.census.jobs[0].pointer==0x8000,"deleted before selected detach/erase");
        if(fault=="destroy"){error="original destructor fault";return false;}
        out.returned=true;if(fault=="destroy_shape")world.census.jobs.clear();return true;
    }
};
}
int main(){try{
    size_t cases=0;Native native;const auto original=native.world;JobRetireReceipt receipt;
    require(retire_job(native,native.lease,0x7000,receipt)&&receipt.confirmed&&!receipt.unknown,"actual selected retirement failed");++cases;
    require(native.detached==1&&native.erased==1&&native.destroyed==1&&native.world.counters==original.counters&&
        native.world.census.jobs[0]==original.census.jobs[1],"selected retirement changed event counters/survivor");++cases;
    require(retire_job(native,native.lease,0x7000,receipt)&&native.destroyed==1,"selected retirement replayed destructor");++cases;
    auto other=native.lease;other.occurrence_key+="other";
    require(!retire_job(native,other,0x7000,receipt)&&receipt.confirmed&&!receipt.request_rejection.empty(),"foreign retry replaced retained receipt");++cases;
    for(const auto fault:{"detach_throw","detach","detach_shape","erase","counter","other_job","developer","destroy","destroy_shape","late_guard"}){
        Native f;f.fault=fault;JobRetireReceipt failed;
        require(!retire_job(f,f.lease,0x7000,failed)&&failed.attempted&&failed.unknown&&!failed.confirmed&&!failed.cause.empty(),"partial retirement became success");++cases;
        const auto cause=failed.cause;const auto calls=f.detached;
        require(!retire_job(f,f.lease,0x7000,failed)&&f.detached==calls&&failed.cause==cause,"partial retirement replayed or lost cause");++cases;
    }
    for(int shape=0;shape<5;++shape){Native f;JobRetireReceipt rejected;
        if(shape==0)f.world.census.jobs[0].developer=0x9000;
        if(shape==1)f.world.census.developers[0].job=0x7000;
        if(shape==2)f.world.bug_job=0;
        if(shape==3)f.world.census.jobs[0].bug.pointer=0xdead;
        if(shape==4)f.world.census.jobs[0].type=1;
        require(!retire_job(f,f.lease,0x7000,rejected)&&!rejected.admitted&&!rejected.attempted&&f.detached==0,"foreign or Developer-linked Job was retired");++cases;
    }
    Native guard;guard.fault="guard";JobRetireReceipt rejected;
    require(!retire_job(guard,guard.lease,0x7000,rejected)&&!rejected.attempted&&!rejected.unknown&&rejected.cause=="original retirement owner changed","original pre-effect cause lost");++cases;
    std::cout<<"PASS "<<cases<<" actual selected Bug Job retirement checks\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}}
