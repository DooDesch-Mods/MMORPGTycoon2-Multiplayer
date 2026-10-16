#pragma once
#include "job_birth.h"
#include "job_retire.h"
namespace mt2mp::game::bug_authority {
using JobAuthorize=std::function<bool(const JobLease&,std::string&)>;
using JobCapture=std::function<bool(developer_staff::Capture&,std::string&)>;
bool verify_job_calls(const JobRead&,uintptr_t image,std::string&);
class NativeJobBirth final:public JobBirthServices,public JobRetireServices {
    JobRead read_;
    JobAuthorize authorize_;
    JobCapture capture_;
    uintptr_t bug_property_=0,job_property_=0;
    uintptr_t detached_job_=0;
    std::array<unsigned char,0xc8> detached_bytes_{};
public:
    NativeJobBirth(JobRead read,JobAuthorize authorize,JobCapture capture):
        read_(std::move(read)),authorize_(std::move(authorize)),capture_(std::move(capture)){}
    bool authorize(const JobLease&,std::string&)override;
    bool observe(const JobLease&,JobBirthSnapshot&,std::string&)override;
    bool inspect(const JobLease&,uintptr_t,developer_staff::job_census::Job&,std::string&)override;
    bool construct(const JobLease&,JobBirthCall&,std::string&)override;
    bool configure(const JobLease&,uintptr_t,JobCall&,std::string&)override;
    bool insert(const JobLease&,uintptr_t,JobCall&,std::string&)override;
    bool pair(const JobLease&,uintptr_t,subscriber_lifecycle_append::BugPairReceipt&,std::string&)override;
    bool detach(const JobLease&,uintptr_t,subscriber_lifecycle_append::BugPairReceipt&,std::string&)override;
    bool erase(const JobLease&,const JobBirthSnapshot&,size_t,JobCall&,std::string&)override;
    bool destroy(const JobLease&,uintptr_t,JobCall&,std::string&)override;
};
}
