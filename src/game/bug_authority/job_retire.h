#pragma once
#include "job_birth.h"
namespace mt2mp::game::bug_authority {
struct JobRetireReceipt {
    JobLease lease;
    uintptr_t job=0;
    subscriber_lifecycle_append::BugPairReceipt pair;
    JobCall erase,destroy;
    bool admitted=false,attempted=false,confirmed=false,unknown=false;
    std::string cause,request_rejection;
};
struct JobRetireServices {
    virtual ~JobRetireServices()=default;
    virtual bool authorize(const JobLease&,std::string&)=0;
    virtual bool observe(const JobLease&,JobBirthSnapshot&,std::string&)=0;
    virtual bool detach(const JobLease&,uintptr_t,subscriber_lifecycle_append::BugPairReceipt&,std::string&)=0;
    virtual bool erase(const JobLease&,const JobBirthSnapshot&,size_t,JobCall&,std::string&)=0;
    virtual bool destroy(const JobLease&,uintptr_t,JobCall&,std::string&)=0;
};
// Developer/Job references are detached by the existing actor owner first.
// This operation closes Bug/Job references before retiring the selected Job.
bool retire_job(JobRetireServices&,const JobLease&,uintptr_t job,JobRetireReceipt&);
}
