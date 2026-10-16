#include "bug_authority/job_links.h"
#include "bug_authority/job_native.h"
#include "subscriber_lifecycle/append_calls.h"
#include <Windows.h>
#include <atomic>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
namespace fixture {
std::map<uintptr_t,uintptr_t> words;
int constructs=0,closes=0,properties=0,fresh_calls=0,fail_fresh=0,fail_property=0;
bool fail_close=false;
int code_calls=0;bool fail_code=false;
void construct(void* slot){++constructs;*static_cast<uintptr_t*>(slot)=0x7000;words[0x7010]=0;words[0x7018]=0;}
void close(void*){++closes;if(fail_close)throw std::runtime_error("original close failure");}
bool fresh(void*,std::string& e){if(++fresh_calls==fail_fresh){e="original current owner changed";return false;}return true;}
void* original(void*,int32_t){return nullptr;}
}
namespace mt2mp::subscriber_lifecycle::first_append {
bool context_call(Unary fn,void* context,CallReceipt& receipt,std::string& e){receipt.attempted=true;
    try{fn(context);receipt.returned=true;return true;}catch(const std::exception& cause){e=cause.what();return false;}}
}
namespace mt2mp::game::subscriber_lifecycle_append {
namespace {
namespace local=mt2mp::subscriber_lifecycle;
namespace stage=local::first_append;
std::atomic_flag busy=ATOMIC_FLAG_INIT;
bool hook_ready=true;uintptr_t image_base=0x10000000;stage::AppendApi api;
local::LookupScope lookup_scope;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& e){
    const auto found=fixture::words.find(p+offset);
    if(found==fixture::words.end())return fail(e,"original local read failed");out=static_cast<T>(found->second);return true;
}
}
#include "bug_authority/job_links.inc"
}
namespace mt2mp::game::bug_authority {
using developer_staff::construction::Call;
uint32_t job_link_property(uintptr_t,uintptr_t property,uintptr_t receiver,bool bug,bool attach,void* context,Call* out){
    using namespace subscriber_lifecycle_append;
    if(property!=(bug?0x4000u:0x5000u))throw std::runtime_error("wrong resolved property receiver");
    ++fixture::properties;out->record_confirmed=true;out->attempted=true;
    if(fixture::properties==fixture::fail_property){out->exception=17;return 17;}
    const int32_t id=attach?(bug?1:2):0;
    fixture::words[receiver+(bug?0x48:0x38)]=reinterpret_cast<uintptr_t>(lookup_scope.lookup(GetCurrentThreadId(),context,id,fixture::original));
    out->accepted=true;out->returned=true;out->cleanup_confirmed=true;return 0;
}
}

namespace mt2mp::game::bug_authority {
namespace {
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
// The unchanged full-image verifier is the boundary here. The actual wrapper
// must invoke it before passing the property receivers to the real pair binder.
bool code(const JobRead&,uintptr_t,uintptr_t& bug,uintptr_t& job,std::string& error){
    ++fixture::code_calls;
    if(fixture::fail_code)return fail(error,"original property verifier failure");
    bug=0x4000;job=0x5000;return true;
}
}
#include "bug_job_native_pair_actual.inc"
bool NativeJobBirth::observe(const JobLease&,JobBirthSnapshot&,std::string&){throw std::runtime_error("unexpected observe");}
bool NativeJobBirth::inspect(const JobLease&,uintptr_t,developer_staff::job_census::Job&,std::string&){throw std::runtime_error("unexpected inspect");}
bool NativeJobBirth::construct(const JobLease&,JobBirthCall&,std::string&){throw std::runtime_error("unexpected construct");}
bool NativeJobBirth::configure(const JobLease&,uintptr_t,JobCall&,std::string&){throw std::runtime_error("unexpected configure");}
bool NativeJobBirth::insert(const JobLease&,uintptr_t,JobCall&,std::string&){throw std::runtime_error("unexpected insert");}
bool NativeJobBirth::pair(const JobLease&,uintptr_t,subscriber_lifecycle_append::BugPairReceipt&,std::string&){throw std::runtime_error("unexpected pair");}
bool NativeJobBirth::erase(const JobLease&,const JobBirthSnapshot&,size_t,JobCall&,std::string&){throw std::runtime_error("unexpected erase");}
bool NativeJobBirth::destroy(const JobLease&,uintptr_t,JobCall&,std::string&){throw std::runtime_error("unexpected destroy");}
}

namespace {
using namespace mt2mp::game::subscriber_lifecycle_append;
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
BugPairRequest reset(bool attach){
    fixture::words={{0x2000+0x48,attach?0:0x3000},{0x3000+0x38,0x2000}};
    fixture::constructs=fixture::closes=fixture::properties=fixture::fresh_calls=fixture::fail_fresh=fixture::fail_property=0;
    fixture::fail_close=false;lookup_scope.clear();
    api.context_construct=fixture::construct;api.context_destroy=fixture::close;
    return {image_base,0x2000,0x3000,0x4000,0x5000,GetCurrentThreadId(),attach,"action:example#bug:1#job:1",fixture::fresh,nullptr};
}
}
int main(){try{size_t cases=0;BugPairReceipt receipt;std::string error;
    auto request=reset(true);
    require(bind_bug_pair(request,receipt,error)&&receipt.confirmed&&receipt.context_closed&&receipt.lookup.original_calls==2&&receipt.lookup.external_hits==2,"actual attach/unique borrowed keys failed");
    require(fixture::words.at(0x2048)==0x3000&&fixture::words.at(0x3038)==0x2000&&fixture::closes==1,"actual reciprocal attach differs");++cases;
    require(bind_bug_pair(request,receipt,error)&&!receipt.attempted&&fixture::constructs==1,"same reciprocal links replayed");++cases;
    request=reset(false);require(bind_bug_pair(request,receipt,error)&&receipt.lookup.original_calls==2&&receipt.lookup.external_hits==0&&fixture::words.at(0x2048)==0&&fixture::words.at(0x3038)==0,"actual null Resolve detach failed");++cases;
    request=reset(true);fixture::words[0x2048]=0x9999;
    require(!bind_bug_pair(request,receipt,error)&&!receipt.attempted&&fixture::constructs==0,"foreign reciprocal graph mutated");++cases;
    request=reset(true);fixture::fail_property=1;
    require(!bind_bug_pair(request,receipt,error)&&receipt.unknown&&receipt.attempted&&fixture::closes==0,"first property failure was cleaned or accepted");++cases;
    request=reset(true);fixture::fail_property=2;
    require(!bind_bug_pair(request,receipt,error)&&receipt.unknown&&fixture::words.at(0x2048)==0x3000&&fixture::closes==0,"second property failure lost partial effect");++cases;
    request=reset(true);fixture::fail_fresh=5;
    require(!bind_bug_pair(request,receipt,error)&&receipt.unknown&&error.find("original current owner changed")!=std::string::npos,"borrowed lookup owner failure cause lost");++cases;
    request=reset(true);fixture::fail_close=true;
    require(!bind_bug_pair(request,receipt,error)&&receipt.unknown&&error.find("original close failure")!=std::string::npos,"Context cleanup failure hidden");++cases;
    request=reset(true);fixture::fail_fresh=1;
    require(!bind_bug_pair(request,receipt,error)&&!receipt.attempted&&!receipt.unknown&&fixture::constructs==0,"pre-effect owner rejection became effect");++cases;
    request=reset(true);fixture::words[0x3038]=0;
    require(!bind_bug_pair(request,receipt,error)&&!receipt.attempted&&fixture::constructs==0,"missing original SetBugfix edge accepted");++cases;
    request=reset(true);fixture::words[0x3038]=0x8888;
    require(!bind_bug_pair(request,receipt,error)&&!receipt.attempted&&fixture::constructs==0,"foreign SetBugfix Bug accepted");++cases;
    request=reset(false);fixture::words[0x3038]=0;
    require(!bind_bug_pair(request,receipt,error)&&!receipt.attempted,"partial cleared pair detached");++cases;

    using namespace mt2mp::game::bug_authority;
    image_base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const JobLease lease{image_base,0x8000,0x2000,0x9000,0xa000,GetCurrentThreadId(),"snapshot","occurrence",3,4};
    for(int scenario=0;scenario<8;++scenario){
        reset(false);fixture::code_calls=0;fixture::fail_code=scenario==2;
        int authorizations=0;
        JobAuthorize authority=[&](const JobLease& actual,std::string& e){
            require(actual==lease,"detach changed exact lease");++authorizations;
            if(scenario==1||(scenario==5&&authorizations==2)){e="original bound owner rejected";return false;}return true;};
        JobRead reader=[](uintptr_t,void*,size_t,std::string&)->bool{throw std::runtime_error("unexpected verifier read");};
        JobCapture capture=[](mt2mp::game::developer_staff::Capture&,std::string&)->bool{throw std::runtime_error("unexpected capture");};
        NativeJobBirth services(scenario==7?JobRead{}:reader,authority,capture);
        auto selected=lease;if(scenario==3)++selected.thread;if(scenario==4)++selected.image;
        if(scenario==6)fixture::fail_property=2;
        receipt.confirmed=true;receipt.attempted=true;error.clear();
        const bool ok=services.detach(selected,0x3000,receipt,error);
        if(scenario==0){
            require(ok&&receipt.confirmed&&receipt.attempted&&fixture::properties==2&&fixture::closes==1,"fresh adapter detach failed");
            require(authorizations>=3&&fixture::code_calls==authorizations,"detach skipped initial or fresh authorization");
            require(fixture::words.at(0x2048)==0&&fixture::words.at(0x3038)==0,"fresh adapter left reciprocal links");
        }else if(scenario==6){
            require(!ok&&receipt.attempted&&receipt.unknown&&!receipt.confirmed&&fixture::closes==0,"wrapper lost partial detach failure");
        }else{
            require(!ok&&!receipt.attempted&&!receipt.confirmed&&!receipt.unknown&&fixture::constructs==0&&fixture::properties==0,"wrapper admission failure mutated or retained stale receipt");
            require(fixture::words.at(0x2048)==0x3000&&fixture::words.at(0x3038)==0x2000,"rejected wrapper changed links");
            if(scenario==1||scenario==5)require(error=="original bound owner rejected","owner cause overwritten");
            if(scenario==2)require(error=="original property verifier failure","verifier cause overwritten");
            if(scenario==3||scenario==4||scenario==7)require(authorizations==0&&fixture::code_calls==0,"invalid local owner reached verifier");
        }
        ++cases;
    }
    std::cout<<"PASS "<<cases<<" actual borrowed pair cases\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}}
