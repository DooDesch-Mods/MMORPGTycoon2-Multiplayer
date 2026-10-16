#include "job_retire.h"
#include <algorithm>
#include <stdexcept>
namespace mt2mp::game::bug_authority {
bool retire_job(JobRetireServices& services,const JobLease& lease,uintptr_t job,JobRetireReceipt& receipt){
    if(receipt.admitted){
        if(receipt.lease!=lease||receipt.job!=job){receipt.request_rejection="Bug Job retirement receipt belongs to another occurrence";return false;}
        return receipt.confirmed;
    }
    std::string error;JobBirthSnapshot before;
    if(!job||!services.authorize(lease,error)||!services.observe(lease,before,error)){
        receipt.cause=error.empty()?"Bug Job selected retirement receiver absent":error;return false;}
    const auto found=std::find_if(before.census.jobs.begin(),before.census.jobs.end(),[&](const auto& item){return item.pointer==job;});
    if(found==before.census.jobs.end()||before.census.queue!=lease.queue||before.parent!=lease.parent||before.region!=lease.region||
       before.bug_job!=job||found->type!=7||found->valid!=1||found->bug.pointer!=lease.bug||found->bug.job!=job||found->bug.anchor!=lease.parent||
       found->region!=lease.region||found->developer||found->building||found->building_proxy||found->monster||found->monster_proxy||
       std::any_of(before.census.developers.begin(),before.census.developers.end(),[&](const auto& dev){return dev.job==job;})){
        receipt.cause="Bug Job selected retirement owner/type/reciprocal/no-Developer shape differs";return false;}
    const auto index=static_cast<size_t>(found-before.census.jobs.begin());
    receipt.admitted=true;receipt.lease=lease;receipt.job=job;
    try{
        auto check=[&](bool ok,const char* cause){if(!ok){if(error.empty())error=cause;throw std::runtime_error(error);}};
        auto fresh=[&](){check(services.authorize(lease,error),"Bug Job selected retirement owner changed");};
        fresh();const bool detached=services.detach(lease,job,receipt.pair,error);
        receipt.attempted=receipt.pair.attempted||receipt.pair.unknown;
        check(detached&&receipt.pair.confirmed&&!receipt.pair.unknown,"Bug Job original reciprocal detach unconfirmed");
        auto expected=before;expected.bug_job=0;expected.census.jobs[index].bug={};
        JobBirthSnapshot actual;check(services.observe(lease,actual,error)&&actual==expected,"Bug Job reciprocal detach changed another queue field");
        fresh();const bool erased=services.erase(lease,actual,index,receipt.erase,error);receipt.attempted=receipt.attempted||receipt.erase.attempted;
        check(erased&&receipt.erase.returned&&!receipt.erase.exception,"Bug Job selected queue erase unconfirmed");
        expected.census.jobs.erase(expected.census.jobs.begin()+static_cast<ptrdiff_t>(index));
        for(auto& dev:expected.census.developers){dev.job_index=-1;
            for(size_t n=0;n<expected.census.jobs.size();++n)if(expected.census.jobs[n].pointer==dev.job)dev.job_index=static_cast<int32_t>(n);}
        check(services.observe(lease,actual,error)&&actual==expected,"Bug Job selected queue erase changed a surviving receiver/counter");
        fresh();const bool destroyed=services.destroy(lease,job,receipt.destroy,error);receipt.attempted=receipt.attempted||receipt.destroy.attempted;
        check(destroyed&&receipt.destroy.returned&&!receipt.destroy.exception,"Bug Job original selected destructor unconfirmed");
        // Only surviving objects are observed after the deleting destructor.
        check(services.observe(lease,actual,error)&&actual==expected,"Bug Job selected destructor changed surviving graph/counters");
        fresh();receipt.confirmed=true;return true;
    }catch(const std::exception& cause){if(error.empty())error=cause.what();}
    catch(...){if(error.empty())error="Bug Job selected retirement nonstandard exception";}
    receipt.attempted=receipt.attempted||receipt.pair.attempted||receipt.pair.unknown||receipt.erase.attempted||receipt.destroy.attempted;
    receipt.unknown=receipt.attempted;receipt.cause=error;return false;
}
}
