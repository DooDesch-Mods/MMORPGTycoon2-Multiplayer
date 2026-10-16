#include "job_birth.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace mt2mp::game::bug_authority {
namespace {
using Job=developer_staff::job_census::Job;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool called(const JobCall& c){return c.attempted&&c.returned&&!c.exception&&!c.cleanup_exception;}
bool shape(const JobLease& lease,const JobBirthSnapshot& before,const Job& job,bool configured,std::string& e){
    if(!job.pointer||job.type!=(configured?7:0)||job.valid!=1||job.developer||job.building||job.building_proxy||job.monster||job.monster_proxy||
       job.bug.pointer!=(configured?lease.bug:0)||job.destination!=std::array<float,3>{}||
       job.position!=(configured?before.position:std::array<float,3>{}))
        return fail(e,"Bug Job original constructor/SetBugfix fields differ");
    // Job.Init does not initialize Region. SetBugfix must set the actual parent Region.
    return !configured||job.region==lease.region||fail(e,"Bug Job original SetBugfix Region differs");
}
bool exact_queue(const JobBirthSnapshot& before,const JobBirthSnapshot& after,const Job& selected,bool linked,std::string& e){
    if(before.parent!=after.parent||before.region!=after.region||before.position!=after.position||before.counters!=after.counters||
       before.census.queue!=after.census.queue||before.census.developers!=after.census.developers||
       after.bug_job!=(linked?selected.pointer:0)||after.census.jobs.size()!=before.census.jobs.size()+1)
        return fail(e,"Bug Job birth changed parent/counters/Developer links or queue count");
    if(!std::equal(before.census.jobs.begin(),before.census.jobs.end(),after.census.jobs.begin()))
        return fail(e,"Bug Job birth changed or reordered an existing queue member");
    auto expected=selected;expected.bug.job=linked?selected.pointer:0;
    return after.census.jobs.back()==expected||fail(e,"Bug Job appended selected receiver/readback differs");
}
}
bool create_job(JobBirthServices& services,const JobLease& lease,JobBirthReceipt& receipt){
    if(receipt.admitted){
        if(receipt.lease!=lease){receipt.request_rejection="Bug Job retained receipt belongs to another occurrence";return false;}
        return receipt.confirmed;
    }
    std::string error;
    if(!lease.image||!lease.queue||!lease.bug||!lease.parent||!lease.region||!lease.thread||lease.snapshot.empty()||lease.occurrence_key.empty()||
       !lease.epoch||!lease.generation){receipt.cause="Bug Job selected birth lease incomplete";return false;}
    JobBirthSnapshot before;
    if(!services.authorize(lease,error)||!services.observe(lease,before,error)){receipt.cause=error;return false;}
    if(before.census.queue!=lease.queue||before.bug_job||before.parent!=lease.parent||before.region!=lease.region||
       std::any_of(before.census.jobs.begin(),before.census.jobs.end(),[&](const auto& job){return job.bug.pointer==lease.bug;})){
        receipt.cause="Bug Job selected birth already linked or actual owners differ";return false;}
    receipt.admitted=true;receipt.lease=lease;
    try{
        auto check=[&](bool ok,const char* cause){if(!ok){if(error.empty())error=cause;throw std::runtime_error(error);}};
        auto fresh=[&](){check(services.authorize(lease,error),"Bug Job selected birth owner changed");};
        fresh();
        const bool constructed=services.construct(lease,receipt.calls,error);
        receipt.attempted=receipt.calls.allocate.attempted||receipt.calls.construct.attempted;
        check(constructed&&called(receipt.calls.allocate)&&called(receipt.calls.construct)&&receipt.calls.job,"Bug Job original allocation/constructor unconfirmed");
        Job selected;check(services.inspect(lease,receipt.calls.job,selected,error)&&shape(lease,before,selected,false,error),"Bug Job constructor readback failed");
        JobBirthSnapshot unchanged;check(services.observe(lease,unchanged,error)&&unchanged==before,"Bug Job constructor changed existing queue state");
        fresh();
        const bool configured=services.configure(lease,receipt.calls.job,receipt.calls.configure,error);
        receipt.attempted=receipt.attempted||receipt.calls.configure.attempted;
        check(configured&&called(receipt.calls.configure),"Bug Job original SetBugfix unconfirmed");
        check(services.inspect(lease,receipt.calls.job,selected,error)&&shape(lease,before,selected,true,error),"Bug Job SetBugfix readback failed");
        check(services.observe(lease,unchanged,error)&&unchanged==before,"Bug Job SetBugfix changed existing queue state");
        fresh();
        const bool inserted=services.insert(lease,receipt.calls.job,receipt.calls.insert,error);
        receipt.attempted=receipt.attempted||receipt.calls.insert.attempted;
        check(inserted&&called(receipt.calls.insert),"Bug Job original selected AddItem unconfirmed");
        JobBirthSnapshot appended;check(services.observe(lease,appended,error)&&exact_queue(before,appended,selected,false,error),"Bug Job selected AddItem readback failed");
        fresh();
        const bool paired=services.pair(lease,receipt.calls.job,receipt.pair,error);
        receipt.attempted=receipt.attempted||receipt.pair.attempted||receipt.pair.unknown;
        check(paired&&receipt.pair.confirmed&&!receipt.pair.unknown,"Bug Job original reciprocal pair unconfirmed");
        JobBirthSnapshot after;check(services.observe(lease,after,error)&&exact_queue(before,after,selected,true,error),"Bug Job final selected readback failed");
        fresh();receipt.confirmed=true;return true;
    }catch(const std::exception& cause){if(error.empty())error=cause.what();}
    catch(...){if(error.empty())error="Bug Job selected birth nonstandard exception";}
    receipt.attempted=receipt.attempted||receipt.calls.allocate.attempted||receipt.calls.construct.attempted||
        receipt.calls.configure.attempted||receipt.calls.insert.attempted||receipt.pair.attempted||receipt.pair.unknown;
    receipt.unknown=receipt.attempted;receipt.cause=error;return false;
}
namespace {
template<class T>bool at(const JobRead& read,uintptr_t p,size_t offset,T& out,std::string& e){
    if(!p||p>UINTPTR_MAX-offset||p+offset>UINTPTR_MAX-sizeof(T))return fail(e,"Bug Job read address overflow");
    return read(p+offset,&out,sizeof out,e);
}
}
bool inspect_job(const JobRead& read,const JobLease& lease,uintptr_t pointer,Job& out,std::string& e){
    out={};out.pointer=pointer;uintptr_t ap=0;
    if(!at(read,pointer,0,ap,e)||ap!=lease.image+0x17427a0)return e.empty()?fail(e,"Bug Job original class differs"):false;
    if(!at(read,pointer,0x18,out.type,e)||!at(read,pointer,0x1c,out.position,e)||!at(read,pointer,0x28,out.destination,e)||
       !at(read,pointer,0x38,out.bug.pointer,e)||!at(read,pointer,0x68,out.building,e)||!at(read,pointer,0x70,out.building_proxy,e)||
       !at(read,pointer,0xa0,out.monster,e)||!at(read,pointer,0xa8,out.monster_proxy,e)||!at(read,pointer,0xb0,out.region,e)||
       !at(read,pointer,0xb8,out.developer,e)||!at(read,pointer,0xc0,out.raw_id,e)||!at(read,pointer,0xc4,out.valid,e))return false;
    if(out.bug.pointer){auto& b=out.bug;
        if(out.bug.pointer!=lease.bug)return fail(e,"Bug Job configured another Bug receiver");
        if(!at(read,b.pointer,0,ap,e)||ap!=lease.image+0x1742200)return e.empty()?fail(e,"Bug Job Bug class differs"):false;
        if(!at(read,b.pointer,0x28,b.runtime_id,e)||!at(read,b.pointer,0x2c,b.type,e)||!at(read,b.pointer,0x30,b.created,e)||
           !at(read,b.pointer,0x34,b.discovered,e)||!at(read,b.pointer,0x38,b.toons,e)||!at(read,b.pointer,0x3c,b.reproducibility,e)||
           !at(read,b.pointer,0x40,b.variant,e)||!at(read,b.pointer,0x44,b.known,e)||!at(read,b.pointer,0x45,b.manual,e)||
           !at(read,b.pointer,0x46,b.debugged,e)||!at(read,b.pointer,0x48,b.job,e)||!at(read,b.pointer,0x50,b.anchor,e))return false;
    }
    return true;
}
bool observe_job_birth(const JobRead& read,const JobLease& lease,const developer_staff::Capture& capture,JobBirthSnapshot& out,std::string& e){
    out={};if(capture.image!=lease.image||capture.thread!=lease.thread||capture.pin.origin.world_epoch!=lease.epoch||capture.pin.origin.generation!=lease.generation)
        return fail(e,"Bug Job read current Capture differs from selected lease");
    if(!developer_staff::job_census::read(read,capture,out.census,e)||out.census.queue!=lease.queue)
        return e.empty()?fail(e,"Bug Job read current queue differs"):false;
    if(!at(read,lease.bug,0x48,out.bug_job,e)||!at(read,lease.bug,0x50,out.parent,e)||out.parent!=lease.parent)
        return e.empty()?fail(e,"Bug Job read current parent differs"):false;
    return at(read,out.parent,0x178,out.region,e)&&at(read,out.parent,0x110,out.position,e)&&
        at(read,lease.queue,0x48,out.counters,e);
}
}
