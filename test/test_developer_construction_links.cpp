#include "developer_staff/construction_links.h"
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
#include "developer_staff/construction_links.inc"
}
namespace mt2mp::game::developer_staff::construction {
uint32_t link_property(uintptr_t,uintptr_t,uintptr_t receiver,bool developer,bool attach,void* context,Call* out){
    using namespace subscriber_lifecycle_append;++fixture::properties;out->record_confirmed=true;out->attempted=true;
    if(fixture::properties==fixture::fail_property){out->exception=17;return 17;}
    const int32_t id=attach?(developer?1:2):0;
    fixture::words[receiver+(developer?0x4c8:0xb8)]=reinterpret_cast<uintptr_t>(lookup_scope.lookup(GetCurrentThreadId(),context,id,fixture::original));
    out->accepted=true;out->returned=true;out->cleanup_confirmed=true;return 0;
}
}
namespace {
using namespace mt2mp::game::subscriber_lifecycle_append;
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
DeveloperPairRequest reset(bool attach){
    fixture::words={{0x2000+0x4c8,attach?0:0x3000},{0x3000+0xb8,attach?0:0x2000}};
    fixture::constructs=fixture::closes=fixture::properties=fixture::fresh_calls=fixture::fail_fresh=fixture::fail_property=0;
    fixture::fail_close=false;lookup_scope.clear();
    api.context_construct=fixture::construct;api.context_destroy=fixture::close;
    return {image_base,0x2000,0x3000,0x4000,0x5000,GetCurrentThreadId(),attach,"action:example",fixture::fresh,nullptr};
}
}
int main(){try{size_t cases=0;DeveloperPairReceipt receipt;std::string error;
    auto request=reset(true);
    require(bind_developer_pair(request,receipt,error)&&receipt.confirmed&&receipt.context_closed&&receipt.lookup.original_calls==2&&receipt.lookup.external_hits==2,"actual attach/unique borrowed keys failed");
    require(fixture::words.at(0x24c8)==0x3000&&fixture::words.at(0x30b8)==0x2000&&fixture::closes==1,"actual reciprocal attach differs");++cases;
    require(bind_developer_pair(request,receipt,error)&&!receipt.attempted&&fixture::constructs==1,"same reciprocal links replayed");++cases;
    request=reset(false);require(bind_developer_pair(request,receipt,error)&&receipt.lookup.original_calls==2&&receipt.lookup.external_hits==0&&fixture::words.at(0x24c8)==0&&fixture::words.at(0x30b8)==0,"actual null Resolve detach failed");++cases;
    request=reset(true);fixture::words[0x24c8]=0x9999;
    require(!bind_developer_pair(request,receipt,error)&&!receipt.attempted&&fixture::constructs==0,"foreign reciprocal graph mutated");++cases;
    request=reset(true);fixture::fail_property=1;
    require(!bind_developer_pair(request,receipt,error)&&receipt.unknown&&receipt.attempted&&fixture::closes==0,"first property failure was cleaned or accepted");++cases;
    request=reset(true);fixture::fail_property=2;
    require(!bind_developer_pair(request,receipt,error)&&receipt.unknown&&fixture::words.at(0x24c8)==0x3000&&fixture::closes==0,"second property failure lost partial effect");++cases;
    request=reset(true);fixture::fail_fresh=5;
    require(!bind_developer_pair(request,receipt,error)&&receipt.unknown&&error.find("original current owner changed")!=std::string::npos,"borrowed lookup owner failure cause lost");++cases;
    request=reset(true);fixture::fail_close=true;
    require(!bind_developer_pair(request,receipt,error)&&receipt.unknown&&error.find("original close failure")!=std::string::npos,"Context cleanup failure hidden");++cases;
    request=reset(true);fixture::fail_fresh=1;
    require(!bind_developer_pair(request,receipt,error)&&!receipt.attempted&&!receipt.unknown&&fixture::constructs==0,"pre-effect owner rejection became effect");++cases;
    std::cout<<"PASS "<<cases<<" actual borrowed pair cases\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}}
