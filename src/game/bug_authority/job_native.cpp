#include "job_native.h"
#include "runtime.h"
#include "mt2mp/core/hash.h"
#include "live_host_state/scalar_store.h"
#include <Windows.h>
namespace mt2mp::game::bug_authority {
namespace {
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool code(const JobRead& read,uintptr_t image,uintptr_t& bug_property,uintptr_t& job_property,std::string& error){
    struct Body{uintptr_t rva;size_t size;const char* sha;};
    static constexpr Body bodies[]={
        {0xa02670,80,"75d24a585d71fb3cf549ddf13f41901b6747711875a65117b5d369802ed7e9ae"},
        {0x281020,208,"0820768383104f64f770ca7cee85683920ff514477865c0bc8116f7d9f417ebf"},
        {0x280fb0,112,"300d68a6f08e92f813f444499efbda30ec470e4be71523bf4e5dfaf288fb1176"},
        {0x281610,64,"5ed42afc114ca4de0bea0e08119a69d4c50e2af22d45939851362cb6e08214d1"},
        {0xb5ab50,416,"fa2b4911d321b08e26a95950e64aec2ff5748970561f6f81927f78a9744361b6"},
        {0xc2e810,176,"9aa893e96fc125d33f182e51dd1ff11ec5164ed30cbd1cd62c2765da01cb20e2"},
        {0xa91670,8,"a2cd879c472cf43a9bb3204f150fa4150c59a714d744434fb224bfb9ececb038"},
        {0xe02e90,64,"62fa9aaa8849cf0dcaa6e149b8bbebb2727db5b6cc8acf808ef80e1e20490f39"},
        {0xe02fa0,64,"1afb9692621ddb3a35c56e78fff475f503b28ac814924e63e14e8facc269f3ee"},
        {0xe02760,64,"ef71c974c887f5242c2be9efa26f4aff8b09b44145257d36f4f4926e8db37a1c"},
        {0xe02920,64,"51b322812fd3011f3f73238b21b23cff6d128b26eea6799ac5250cac62069983"},
        {0x967370,416,"2a6e4111f91e2c3cecb2aa35b99ed8d71f22a75f8300694f7d9ba6b086c01daa"},
        {0x9676a0,160,"33000088d5ae9e353738b283b76f13a641243aa996c7952714c57d6a07cc170f"},
        {0x1113d80,272,"dee54a7232d3aef27b73436213be00e802ba3cb997d4fb09a5f41f38468fafcc"}};
    for(const auto& body:bodies){if(body.rva==0xc2e810){if(!verify_job_destructor(error))return false;continue;}std::vector<unsigned char> bytes(body.size);
        if(!read(image+body.rva,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=body.sha)return fail(error,"Bug Job original complete body differs RVA="+std::to_string(body.rva));}
    if(!read(image+0x1a2c4e0,&bug_property,8,error)||!read(image+0x1a30a30,&job_property,8,error))return false;
    struct Property{uintptr_t object,ap,offset;};
    const Property properties[]={{bug_property,0x1728fd0,0x48},{job_property,0x1728ef0,0x38}};
    for(const auto& property:properties){uintptr_t ap=0,offset=0;
        if(!property.object||property.object>UINTPTR_MAX-0x40)return fail(error,"Bug Job property pointer absent/overflow");
        if(!read(property.object,&ap,8,error)||!read(property.object+0x38,&offset,8,error))return false;
        if(ap!=image+property.ap||offset!=property.offset)return fail(error,"Bug Job original ObjectLink property class/offset differs");}
    return true;
}
bool returned(const JobCall& receipt,const char* name,std::string& error){
    return (receipt.attempted&&receipt.returned&&!receipt.exception)||
        fail(error,std::string("Bug Job original ")+name+" unconfirmed SEH="+std::to_string(receipt.exception));
}
struct PairOwner{NativeJobBirth* services;const JobLease* lease;};
bool fresh_pair(void* user,std::string& error){auto& owner=*static_cast<PairOwner*>(user);return owner.services->authorize(*owner.lease,error);}
}
bool verify_job_calls(const JobRead& read,uintptr_t image,std::string& error){uintptr_t bug=0,job=0;return code(read,image,bug,job,error);}
bool NativeJobBirth::authorize(const JobLease& lease,std::string& error){
    if(!read_||!authorize_||!capture_||lease.image!=uintptr_t(GetModuleHandleW(nullptr))||lease.thread!=GetCurrentThreadId())
        return fail(error,"Bug Job selected birth requires actual image/Mainthread and authority callbacks");
    return authorize_(lease,error)&&code(read_,lease.image,bug_property_,job_property_,error);
}
bool NativeJobBirth::observe(const JobLease& lease,JobBirthSnapshot& out,std::string& error){
    developer_staff::Capture capture;
    return capture_(capture,error)&&observe_job_birth(read_,lease,capture,out,error);
}
bool NativeJobBirth::inspect(const JobLease& lease,uintptr_t job,developer_staff::job_census::Job& out,std::string& error){return inspect_job(read_,lease,job,out,error);}
bool NativeJobBirth::construct(const JobLease& lease,JobBirthCall& receipt,std::string& error){
    construct_job(lease.image,&receipt);
    return returned(receipt.allocate,"allocation",error)&&returned(receipt.construct,"constructor",error)&&
        (receipt.job||fail(error,"Bug Job original allocator returned null"));
}
bool NativeJobBirth::configure(const JobLease& lease,uintptr_t job,JobCall& receipt,std::string& error){
    configure_job(lease.image,job,lease.bug,&receipt);return returned(receipt,"SetBugfix",error);
}
bool NativeJobBirth::insert(const JobLease& lease,uintptr_t job,JobCall& receipt,std::string& error){
    insert_job(lease.image,lease.queue,job,&receipt);return returned(receipt,"AddItem",error);
}
bool NativeJobBirth::pair(const JobLease& lease,uintptr_t job,subscriber_lifecycle_append::BugPairReceipt& receipt,std::string& error){
    PairOwner owner{this,&lease};
    const subscriber_lifecycle_append::BugPairRequest request{lease.image,lease.bug,job,bug_property_,job_property_,
        lease.thread,true,lease.occurrence_key,fresh_pair,&owner};
    return subscriber_lifecycle_append::bind_bug_pair(request,receipt,error);
}
bool NativeJobBirth::detach(const JobLease& lease,uintptr_t job,subscriber_lifecycle_append::BugPairReceipt& receipt,std::string& error){
    receipt={};
    if(!authorize(lease,error))return false;
    PairOwner owner{this,&lease};
    const subscriber_lifecycle_append::BugPairRequest request{lease.image,lease.bug,job,bug_property_,job_property_,
        lease.thread,false,lease.occurrence_key,fresh_pair,&owner};
    return subscriber_lifecycle_append::bind_bug_pair(request,receipt,error);
}
bool NativeJobBirth::erase(const JobLease& lease,const JobBirthSnapshot& before,size_t index,JobCall& receipt,std::string& error){
    if(index>=before.census.jobs.size()||before.census.queue!=lease.queue)return fail(error,"Bug Job selected erase index/owner differs");
    JobBirthSnapshot current;if(!authorize(lease,error)||!observe(lease,current,error))return false;
    if(current!=before)return fail(error,"Bug Job selected erase changed after preflight");
    if(detached_job_)return fail(error,"Bug Job selected erase already retains a detached receiver");
    const auto selected=before.census.jobs[index].pointer;
    if(!read_(selected,detached_bytes_.data(),detached_bytes_.size(),error))return false;
    detached_job_=selected;
    if(index+1<before.census.jobs.size()){
        developer_staff::construction::shift(lease.image,before.census.data+index*8,before.census.data+(index+1)*8,
            (before.census.jobs.size()-index-1)*8,&receipt);
        if(!returned(receipt,"selected queue memmove",error))return false;
    }
    receipt.attempted=true;
    if(!live_host_state::compare_fixed_i32(lease.queue+0x28,static_cast<int32_t>(before.census.jobs.size()),
                                         static_cast<int32_t>(before.census.jobs.size()-1),error))return false;
    receipt.returned=true;return true;
}
bool NativeJobBirth::destroy(const JobLease& lease,uintptr_t job,JobCall& receipt,std::string& error){
    if(!job||job!=detached_job_)return fail(error,"Bug Job deleting destructor lacks exact selected erase witness");
    std::array<unsigned char,0xc8> current{};
    if(!read_(job,current.data(),current.size(),error))return false;
    if(current!=detached_bytes_)return fail(error,"Bug Job detached receiver changed before deleting destructor");
    developer_staff::construction::unary(lease.image,developer_staff::construction::Unary::DestroyJob,job,&receipt);
    if(!returned(receipt,"selected Job destructor",error))return false;
    detached_job_=0;return true;
}
}
