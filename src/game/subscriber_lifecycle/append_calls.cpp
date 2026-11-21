#include "append_calls.h"
#include "native_codes.h"
#ifdef MT2MP_FRAMEWORK_PLUGIN
#include "mt2mp/hooks/framework_backend.h"
#endif
#include <array>
#include <exception>
#include <Windows.h>
namespace mt2mp::subscriber_lifecycle::first_append {
namespace {
constexpr std::array<uintptr_t,13> record_rvas={0xa0d230,0xa0be20,0x96f9e0,0x9708c0,0x11cbc00,0x11c8b80,
    0x96db40,0x976590,0x976580,0x96dd70,0x9765a0,0x9765c0,0x9765b0};
DecodeApi record_functions(uintptr_t image){
    DecodeApi a;
    a.allocate=reinterpret_cast<Allocate>(image+0xa0d230);a.release=reinterpret_cast<Release>(image+0xa0be20);
    a.record_construct=reinterpret_cast<Unary>(image+0x96f9e0);a.record_destroy=reinterpret_cast<Unary>(image+0x9708c0);
    a.string_construct=reinterpret_cast<StringConstruct>(image+0x11cbc00);a.string_destroy=reinterpret_cast<Unary>(image+0x11c8b80);
    a.add_child=reinterpret_cast<Binary>(image+0x96db40);a.set_label=reinterpret_cast<Binary>(image+0x976590);
    a.set_string=reinterpret_cast<Binary>(image+0x976580);a.token_count=reinterpret_cast<Integer>(image+0x96dd70);
    a.set_integer=reinterpret_cast<Integer>(image+0x9765a0);a.set_type=reinterpret_cast<Integer>(image+0x9765c0);a.set_float=reinterpret_cast<Float>(image+0x9765b0);
    return a;
}
uint32_t invoke(Unary f,void* p){__try{f(p);}__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;}
uint32_t invoke_link(QuestResolve f,void* p,void* context){__try{f(p,context);}__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;}
uint32_t invoke_allocate(Allocate f,uintptr_t* out){__try{*out=reinterpret_cast<uintptr_t>(f(0xb0));}__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;}
uint32_t invoke_shift(ProgressShift f,void* to,const void* from,size_t n){__try{f(to,from,n);}__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;}
uint32_t registry(uintptr_t image,uint64_t uid,uintptr_t* out){__try{*out=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x481f60)(uid);}__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;}
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
        if(actual!=c.bytes){
            const auto mismatch="Subscriber append native code mismatch RVA="+std::to_string(c.rva);
#ifdef MT2MP_FRAMEWORK_PLUGIN
            // Context.Lookup is also the owned private-link observer during normal gameplay.
            if(c.rva==lookup_rva){
                std::string route_error;
                if(hooks::framework::verify_code(image+c.rva,c.bytes.data(),c.bytes.size(),route_error))continue;
                e=mismatch+"; "+route_error;return false;
            }
#endif
            e=mismatch;return false;
        }
    }
    out.records=record_functions(image);
    out.context_construct=reinterpret_cast<Unary>(image+0x8a7a60);out.context_destroy=reinterpret_cast<Unary>(image+0x8a75a0);out.context_abort=reinterpret_cast<Unary>(image+0x8a70d0);
    out.progress_construct=reinterpret_cast<Unary>(image+0xc15630);out.progress_destroy=reinterpret_cast<Unary>(image+0xc156c0);
    out.progress_npc_assign=reinterpret_cast<QuestResolve>(image+0x3e2d70);out.progress_add=reinterpret_cast<QuestResolve>(image+0xb61670);
    out.progress_shift=reinterpret_cast<ProgressShift>(image+0xa9c7b0);
    out.quest_resolve=reinterpret_cast<QuestResolve>(image+0xbfa800);
    out.calls.array_load=reinterpret_cast<ArrayLoad>(image+array_load_rva);
    out.calls.login=reinterpret_cast<Notify>(image+notify_login_rva);out.calls.logout=reinterpret_cast<Notify>(image+notify_logout_rva);
    out.calls.int_load=reinterpret_cast<PropertyLoad>(image+manager_int_load_rva);
    // D1 performs the one Resolve pass. calls.resolve intentionally remains null.
    e.clear();return true;
}
}
