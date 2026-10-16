#include "append_calls.h"
#include "native_codes.h"
#include <array>
#include <exception>
#include <Windows.h>
namespace mt2mp::subscriber_lifecycle::first_append {
namespace {
constexpr std::array<uintptr_t,13> record_rvas={0xa02670,0xa01260,0x9667c0,0x9676a0,0x11c0260,0x11bd1e0,
    0x964920,0x96d370,0x96d360,0x964b50,0x96d380,0x96d3a0,0x96d390};
DecodeApi record_functions(uintptr_t image){
    DecodeApi a;
    a.allocate=reinterpret_cast<Allocate>(image+0xa02670);a.release=reinterpret_cast<Release>(image+0xa01260);
    a.record_construct=reinterpret_cast<Unary>(image+0x9667c0);a.record_destroy=reinterpret_cast<Unary>(image+0x9676a0);
    a.string_construct=reinterpret_cast<StringConstruct>(image+0x11c0260);a.string_destroy=reinterpret_cast<Unary>(image+0x11bd1e0);
    a.add_child=reinterpret_cast<Binary>(image+0x964920);a.set_label=reinterpret_cast<Binary>(image+0x96d370);
    a.set_string=reinterpret_cast<Binary>(image+0x96d360);a.token_count=reinterpret_cast<Integer>(image+0x964b50);
    a.set_integer=reinterpret_cast<Integer>(image+0x96d380);a.set_type=reinterpret_cast<Integer>(image+0x96d3a0);a.set_float=reinterpret_cast<Float>(image+0x96d390);
    return a;
}
uint32_t invoke(Unary f,void* p){__try{f(p);}__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;}
uint32_t invoke_link(QuestResolve f,void* p,void* context){__try{f(p,context);}__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;}
uint32_t invoke_allocate(Allocate f,uintptr_t* out){__try{*out=reinterpret_cast<uintptr_t>(f(0xb0));}__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;}
uint32_t invoke_shift(ProgressShift f,void* to,const void* from,size_t n){__try{f(to,from,n);}__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;}
uint32_t registry(uintptr_t image,uint64_t uid,uintptr_t* out){__try{*out=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x47ce20)(uid);}__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;}
}
bool context_call(Unary f,void* p,CallReceipt& r,std::string& e){
    r={};if(!f||!p){e="native Context call input missing";return false;}
    r.attempted=true;
    try{r.seh=invoke(f,p);if(r.seh){e="native Context SEH="+std::to_string(r.seh);return false;}}
    catch(const std::exception& ex){e="native Context C++: "+std::string(ex.what());return false;}
    catch(...){e="native Context unknown C++ exception";return false;}
    r.returned=true;e.clear();return true;
}
bool quest_link_call(QuestResolve f,void* link,void* context,CallReceipt& r,std::string& e){
    r={};if(!f||!link||!context){e="native Quest link call input missing";return false;}
    r.attempted=true;
    try{r.seh=invoke_link(f,link,context);if(r.seh){e="native Quest link SEH="+std::to_string(r.seh);return false;}}
    catch(const std::exception& ex){e="native Quest link C++: "+std::string(ex.what());return false;}
    catch(...){e="native Quest link unknown C++ exception";return false;}
    r.returned=true;e.clear();return true;
}
bool allocate_progress(Allocate f,uintptr_t& out,CallReceipt& r,std::string& e){
    r={};out=0;if(!f){e="Progress original allocator missing";return false;}r.attempted=true;
    try{r.seh=invoke_allocate(f,&out);if(r.seh){e="Progress allocate SEH="+std::to_string(r.seh);return false;}}
    catch(const std::exception& ex){e="Progress allocate C++: "+std::string(ex.what());return false;}
    catch(...){e="Progress allocate unknown C++ exception";return false;}
    r.returned=true;if(!out){e="Progress original allocation returned null";return false;}e.clear();return true;
}
bool shift_progress(ProgressShift f,void* to,const void* from,size_t size,CallReceipt& r,std::string& e){
    r={};if(!f||!to||!from||!size){e="Progress original shift input missing";return false;}r.attempted=true;
    try{r.seh=invoke_shift(f,to,from,size);if(r.seh){e="Progress shift SEH="+std::to_string(r.seh);return false;}}
    catch(const std::exception& ex){e="Progress shift C++: "+std::string(ex.what());return false;}
    catch(...){e="Progress shift unknown C++ exception";return false;}
    r.returned=true;e.clear();return true;
}

bool registry_lookup(uintptr_t image,uint64_t uid,uintptr_t& out,std::string& e){
    out=0;try{const auto code=registry(image,uid,&out);if(code){e="Subscriber Toon Registry SEH="+std::to_string(code);return false;}}
    catch(const std::exception& ex){e="Subscriber Toon Registry C++: "+std::string(ex.what());return false;}
    catch(...){e="Subscriber Toon Registry unknown exception";return false;}
    return true;
}
bool resolve_record_api(Memory& memory,uintptr_t image,DecodeApi& out,std::string& e){
    out={};
    if(!image){e="Subscriber Record API image missing";return false;}
    for(const auto rva:record_rvas){
        const NativeCode* expected=nullptr;
        for(const auto& code:native_codes)if(code.rva==rva){expected=&code;break;}
        if(!expected){e="Subscriber Record API fingerprint missing RVA="+std::to_string(rva);return false;}
        std::array<unsigned char,16> actual{};
        if(image>UINTPTR_MAX-rva||image+rva>UINTPTR_MAX-(actual.size()-1)){
            e="Subscriber Record API address overflow RVA="+std::to_string(rva);return false;
        }
        if(!memory.read(image+rva,actual.data(),actual.size(),e))return false;
        if(actual!=expected->bytes){e="Subscriber Record API native code mismatch RVA="+std::to_string(rva);return false;}
    }
    out=record_functions(image);e.clear();return true;
}
bool resolve_append_api(Memory& memory,uintptr_t image,AppendApi& out,std::string& e){
    out={};
    for(const auto& c:native_codes){std::array<unsigned char,16> actual{};
        if(!image||image>UINTPTR_MAX-c.rva||!memory.read(image+c.rva,actual.data(),actual.size(),e))return false;
        if(actual!=c.bytes){e="Subscriber append native code mismatch RVA="+std::to_string(c.rva);return false;}}
    out.records=record_functions(image);
    out.context_construct=reinterpret_cast<Unary>(image+0x89e940);out.context_destroy=reinterpret_cast<Unary>(image+0x89e480);out.context_abort=reinterpret_cast<Unary>(image+0x89dfb0);
    out.progress_construct=reinterpret_cast<Unary>(image+0xc08f20);out.progress_destroy=reinterpret_cast<Unary>(image+0xc08fb0);
    out.progress_npc_assign=reinterpret_cast<QuestResolve>(image+0x3de260);out.progress_add=reinterpret_cast<QuestResolve>(image+0xb54e10);
    out.progress_shift=reinterpret_cast<ProgressShift>(image+0xa91670);
    out.quest_resolve=reinterpret_cast<QuestResolve>(image+0xbee180);
    out.calls.array_load=reinterpret_cast<ArrayLoad>(image+array_load_rva);
    out.calls.login=reinterpret_cast<Notify>(image+notify_login_rva);out.calls.logout=reinterpret_cast<Notify>(image+notify_logout_rva);
    out.calls.int_load=reinterpret_cast<PropertyLoad>(image+manager_int_load_rva);
    // D1 performs the one Resolve pass. calls.resolve intentionally remains null.
    e.clear();return true;
}
}
