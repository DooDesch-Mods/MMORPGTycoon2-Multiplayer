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
        {0xa0d230,80,"e3400b04edc50cf40ee2588351ceb97aa7fce3cdbb30b720dbdfe7a1b05dfb35"},
        {0x284380,208,"26508ff85204012c520a34af5cf7fea16c6b658ed3ad41739e6bc7cfd5080e33"},
        {0x284310,112,"d8e83c99b624c2cd66d55ae10b9021efb534860323fd01dadaa016d9a854c691"},
        {0x284970,64,"5ed42afc114ca4de0bea0e08119a69d4c50e2af22d45939851362cb6e08214d1"},
        {0xb673b0,416,"f65ab23d735730eb8fde8cd05eef9ba71efb8ceee03ce4fdeb34cc940924fe45"},
        {0xc3b0c0,176,"c5e73e7399872d8d858a358992b94e179c6f770d20cf791ea9193e22ad912988"},
        {0xa9c7b0,8,"0120f2ac7097c33adb611d188e7ecbd01622b154e7f666290fc938cdadc4d5c5"},
        {0xe0e7a0,64,"1d77c51fe3bb8558fa51a2e3ceac6fd897d7576f0e6fb0683d1f0146935245c6"},
        {0xe0e8b0,64,"d1ee32c47b795b614220e7a63b5353cbfdadff8b4ffc5486177f3455ae0d8350"},
        {0xe0e070,64,"f714444f5fd1dd1d4cd9691159974cbfaf5d2e60a844ca221ac9bfb3ab89d398"},
        {0xe0e230,64,"427e86f6018b8049f0a466687c658f57649edeb62f4470907f8f0c7d79891f2a"},
        {0x970590,416,"708dc9f67619c74b41ea34856237e98209b298b0cc72613a4cf3b729999a5976"},
        {0x9708c0,160,"182327238cd9e4972b16d4d21ec53c523fce5ef393eda38318e12cc9eb727642"},
        {0x111f720,272,"646ae2ceb1adbb4b88d9e72b93f1c5cb83b4eda2a7e81c23c411c2aec65ee3d2"}};
    for(const auto& body:bodies){if(body.rva==0xc3b0c0){if(!verify_job_destructor(error))return false;continue;}std::vector<unsigned char> bytes(body.size);
        if(!read(image+body.rva,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=body.sha)return fail(error,"Bug Job original complete body differs RVA="+std::to_string(body.rva));}
    if(!read(image+0x1a334e0,&bug_property,8,error)||!read(image+0x1a37a30,&job_property,8,error))return false;
    struct Property{uintptr_t object,ap,offset;};
    const Property properties[]={{bug_property,0x172f370,0x48},{job_property,0x172f290,0x38}};
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
