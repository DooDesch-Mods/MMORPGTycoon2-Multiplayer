#pragma once
#include "job_calls.h"
#include "job_links.h"
#include "developer_staff/job_census.h"
#include <array>
#include <functional>
namespace mt2mp::game::bug_authority {
struct JobLease {
    uintptr_t image=0,queue=0,bug=0,parent=0,region=0;
    uint32_t thread=0;
    std::string snapshot,occurrence_key;
    uint64_t epoch=0,generation=0;
    bool operator==(const JobLease&)const=default;
};
struct JobBirthSnapshot {
    developer_staff::job_census::Census census;
    uintptr_t bug_job=0,parent=0,region=0;
    std::array<float,3> position{};
    // Both complete original DailyCount objects, including local revision and
    // history allocation. Selected birth must not emit a creation/fixed event.
    std::array<unsigned char,0xe0> counters{};
    bool operator==(const JobBirthSnapshot&)const=default;
};
struct JobBirthReceipt {
    JobLease lease;
    JobBirthCall calls;
    subscriber_lifecycle_append::BugPairReceipt pair;
    bool admitted=false,attempted=false,confirmed=false,unknown=false;
    std::string cause,request_rejection;
};
struct JobBirthServices {
    virtual ~JobBirthServices()=default;
    virtual bool authorize(const JobLease&,std::string&)=0;
    virtual bool observe(const JobLease&,JobBirthSnapshot&,std::string&)=0;
    virtual bool inspect(const JobLease&,uintptr_t,developer_staff::job_census::Job&,std::string&)=0;
    virtual bool construct(const JobLease&,JobBirthCall&,std::string&)=0;
    virtual bool configure(const JobLease&,uintptr_t,JobCall&,std::string&)=0;
    virtual bool insert(const JobLease&,uintptr_t,JobCall&,std::string&)=0;
    virtual bool pair(const JobLease&,uintptr_t,subscriber_lifecycle_append::BugPairReceipt&,std::string&)=0;
};
// The frame owner retains this receipt through failure and never retries an
// attempted occurrence. No whole queue load or synthetic completion is used.
bool create_job(JobBirthServices&,const JobLease&,JobBirthReceipt&);
using JobRead=developer_staff::job_census::Read;
bool inspect_job(const JobRead&,const JobLease&,uintptr_t,developer_staff::job_census::Job&,std::string&);
bool observe_job_birth(const JobRead&,const JobLease&,const developer_staff::Capture&,JobBirthSnapshot&,std::string&);
}
