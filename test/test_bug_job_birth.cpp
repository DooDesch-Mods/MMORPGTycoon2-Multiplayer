#include "bug_authority/job_birth.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game;
using namespace bug_authority;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Native final:JobBirthServices {
    JobLease lease{0x10000000,0x1000,0x2000,0x3000,0x4000,1,"snapshot","bug-birth#job-birth",3,4};
    JobBirthSnapshot world;
    developer_staff::job_census::Job selected;
    int allocated=0,configured=0,inserted=0,paired=0,authorized=0;
    std::string fault;
    Native(){
        world.census.queue=lease.queue;world.census.data=0x5000;world.census.capacity=4;
        developer_staff::job_census::Job old;old.pointer=0x6000;old.type=1;old.valid=1;world.census.jobs.push_back(old);
        world.parent=lease.parent;world.region=lease.region;world.position={5,6,7};
        world.census.developers.push_back({0x9000,0,0,9,-1,false});
    }
    bool authorize(const JobLease&,std::string& error)override{
        ++authorized;if((fault=="guard"&&authorized==1)||(fault=="late_guard"&&authorized==5)){error="original owner changed";return false;}return true;
    }
    bool observe(const JobLease&,JobBirthSnapshot& out,std::string&)override{out=world;return true;}
    bool inspect(const JobLease&,uintptr_t,developer_staff::job_census::Job& out,std::string&)override{out=selected;return true;}
    bool construct(const JobLease&,JobBirthCall& out,std::string& error)override{
        ++allocated;out.allocate.attempted=true;
        if(fault=="allocate_throw")throw std::runtime_error("original allocation exception");
        out.allocate.returned=true;out.job=0x7000;out.construct.attempted=true;
        if(fault=="constructor"){error="original constructor fault";out.construct.exception=17;return false;}
        out.construct.returned=true;selected.pointer=out.job;selected.valid=1;selected.region=0xdead;
        if(fault=="constructor_shape")selected.building=0xdead;
        return true;
    }
    bool configure(const JobLease& owner,uintptr_t,JobCall& out,std::string& error)override{
        ++configured;out.attempted=true;
        if(fault=="configure"){error="original SetBugfix fault";return false;}
        out.returned=true;selected.type=7;selected.position=world.position;selected.region=owner.region;
        selected.bug.pointer=owner.bug;selected.bug.anchor=owner.parent;selected.bug.known=1;
        if(fault=="region")selected.region=0xdead;
        if(fault=="counter")++world.counters[0x50];
        return true;
    }
    bool insert(const JobLease&,uintptr_t,JobCall& out,std::string& error)override{
        ++inserted;out.attempted=true;world.census.jobs.push_back(selected);
        if(fault=="insert"){error="original AddItem fault";return false;}
        out.returned=true;
        if(fault=="reorder")std::swap(world.census.jobs.front(),world.census.jobs.back());
        if(fault=="duplicate")world.census.jobs.push_back(selected);
        if(fault=="developer")world.census.developers[0].job=selected.pointer;
        return true;
    }
    bool pair(const JobLease&,uintptr_t,subscriber_lifecycle_append::BugPairReceipt& out,std::string& error)override{
        ++paired;out.attempted=true;world.bug_job=selected.pointer;
        if(fault=="pair"){error="original reciprocal Resolve fault";out.unknown=true;return false;}
        world.census.jobs.back().bug.job=selected.pointer;out.confirmed=true;
        if(fault=="pair_readback")world.census.jobs.back().bug.anchor=0xdead;
        return true;
    }
};
}
int main(){try{
    size_t count=0;JobBirthReceipt receipt;
    Native native;const auto before=native.world;
    require(create_job(native,native.lease,receipt)&&receipt.confirmed&&!receipt.unknown,"actual selected birth failed");++count;
    require(native.allocated==1&&native.configured==1&&native.inserted==1&&native.paired==1&&native.world.counters==before.counters,
            "selected birth emitted or duplicated an event");++count;
    require(create_job(native,native.lease,receipt)&&native.allocated==1,"confirmed selected birth replayed");++count;
    auto foreign=native.lease;foreign.occurrence_key+="other";
    require(!create_job(native,foreign,receipt)&&receipt.confirmed&&!receipt.request_rejection.empty()&&native.allocated==1,
            "foreign retry replaced original receipt");++count;
    for(const auto fault:{"allocate_throw","constructor","constructor_shape","configure","region","counter","insert","reorder","duplicate","developer","pair","pair_readback","late_guard"}){
        Native f;f.fault=fault;JobBirthReceipt failed;
        require(!create_job(f,f.lease,failed)&&failed.attempted&&failed.unknown&&!failed.confirmed&&!failed.cause.empty(),"partial original failure became success");++count;
        const auto cause=failed.cause;const auto attempts=f.allocated;
        require(!create_job(f,f.lease,failed)&&failed.cause==cause&&f.allocated==attempts,"failed selected birth replayed or lost cause");++count;
    }
    Native guard;guard.fault="guard";JobBirthReceipt rejected;
    require(!create_job(guard,guard.lease,rejected)&&!rejected.admitted&&!rejected.attempted&&!rejected.unknown&&guard.allocated==0&&rejected.cause=="original owner changed",
            "pre-effect original owner cause lost");++count;
    Native linked;linked.world.bug_job=0x9999;rejected={};
    require(!create_job(linked,linked.lease,rejected)&&!rejected.admitted&&linked.allocated==0,"existing Bug reciprocal overwritten");++count;
    Native reverse;reverse.world.census.jobs.front().bug.pointer=reverse.lease.bug;rejected={};
    require(!create_job(reverse,reverse.lease,rejected)&&!rejected.admitted&&reverse.allocated==0,"existing queue Bug backlink duplicated");++count;
    std::cout<<"PASS "<<count<<" actual selected Bug Job birth checks\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}}
