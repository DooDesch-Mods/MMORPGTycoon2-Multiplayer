#include "native_calls.h"
#include "initial_release/release_entry.h"
#include <Windows.h>
#include <array>
#include <charconv>
#include <exception>
namespace mt2mp::game::developer_staff::bug_creation {
namespace {
struct Scope {const Anchor* callback=nullptr;std::string* cause=nullptr;bool failed=false;};
thread_local Scope* current=nullptr;
bool add(uintptr_t image,uintptr_t collection,int type,float reproducibility,uintptr_t& result,uint32_t& exception){
    __try{result=reinterpret_cast<uintptr_t(*)(uintptr_t,int,float)>(image+0x59a00)(collection,type,reproducibility);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();return false;}
}
bool set_anchor(SetAnchor original,uintptr_t bug,uintptr_t parent,uint32_t& exception){
    __try{original(bug,parent);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();return false;}
}
bool attach_collection(const Lease* lease,uintptr_t& result,uint32_t& exception){
    __try{
        result=reinterpret_cast<uintptr_t(*)(size_t)>(lease->image+0xa02670)(0x50);
        reinterpret_cast<void(*)(uintptr_t)>(lease->image+0x59c40)(result);
        reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(lease->image+(lease->kind==Parent::Building?0x406400:0x3c4790))(lease->parent,result);
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();return false;}
}
bool property(uintptr_t image,uintptr_t object,uintptr_t prop,uintptr_t load,const char* text,uint32_t& exception){
    initial_release::Record record{};bool constructed=false,ok=false;
    __try{
        reinterpret_cast<initial_release::RecordCtor>(image+0x967370)(&record,text);constructed=true;
        if(reinterpret_cast<initial_release::RecordEquals>(image+0x1113d80)(&record,text))
            ok=reinterpret_cast<bool(*)(uintptr_t,uintptr_t,void*,void*)>(image+load)(image+prop,object,&record,nullptr);
    }__except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}
    if(constructed){__try{reinterpret_cast<initial_release::RecordDtor>(image+0x9676a0)(&record);}
        __except(EXCEPTION_EXECUTE_HANDLER){if(!exception)exception=GetExceptionCode();ok=false;}}
    return ok&&!exception;
}
}
bool NativeServices::authorize(const Lease& lease,std::string& error){
    if(!original_||!authorize_){error="Original SetAnchor detour/authority binding unavailable";return false;}
    return authorize_(lease,error);
}
bool NativeServices::read(const Lease& lease,Snapshot& out,std::string& error){return observe(read_,lease,out,error);}
bool NativeServices::add_specific(const Lease& lease,const Value& value,const Anchor& callback,uintptr_t& result,std::string& error){
    if(current){error="Nested original Bug insertion is not owned";return false;}
    Scope scope{&callback,&error,false};current=&scope;
    uint32_t exception=0;
    const bool returned=add(lease.image,lease.collection,value.type,value.reproducibility,result,exception);
    current=nullptr;
    if(!returned){
        const auto cause="Original AddSpecificBug SEH "+std::to_string(exception);
        if(error.empty())error=cause;else error+="; "+cause;
        return false;
    }
    return !scope.failed;
}
bool NativeServices::dispatch_anchor(uintptr_t bug,uintptr_t parent,uintptr_t return_address){
    if(!current)return false;
    // A handled failure must not fall through to the RNG-bearing original.
    try {
        if(!(*current->callback)(bug,parent,return_address,*current->cause))current->failed=true;
    }catch(const std::exception& error){current->failed=true;if(current->cause->empty())*current->cause=std::string("Bug anchor callback exception: ")+error.what();}
    catch(...){current->failed=true;if(current->cause->empty())*current->cause="Bug anchor callback unknown exception";}
    return true;
}
bool NativeServices::scalars(const Lease& lease,uintptr_t bug,const Value& value,std::string& error){
    // Original AddRandomBug can raise an existing occurrence's type in place.
    constexpr std::array<const char*,3> type_names={"c","b","a"};
    if(value.type<0||value.type>=static_cast<int>(type_names.size())){error="Bug type is outside original enum labels";return false;}
    std::array<char,64> floating{};
    const auto converted=std::to_chars(floating.data(),floating.data()+floating.size(),value.reproducibility,std::chars_format::general);
    if(converted.ec!=std::errc{}){error="Bug reproducibility text conversion failed";return false;}
    const std::array<std::string,9> records={
        std::string("type ")+type_names[static_cast<size_t>(value.type)],
        "createdFlatDay "+std::to_string(value.created),"discoveredFlatDay "+std::to_string(value.discovered),
        "toonsAffected "+std::to_string(value.toons),"reproducibility "+std::string(floating.data(),converted.ptr),
        "variantId "+std::to_string(value.variant),std::string("known ")+(value.known?"true":"false"),
        std::string("manualJob ")+(value.manual?"true":"false"),std::string("beingDebugged ")+(value.debugged?"true":"false")};
    constexpr std::array<uintptr_t,9> properties={0x13b69e0,0x13b69a0,0x13b6960,0x13b6920,0x13b68e0,0x13b68a0,0x13b6860,0x13b6820,0x13b67e0};
    constexpr std::array<uintptr_t,9> loads={0xd0f770,0xd30470,0xd30470,0xd30470,0xd2a9b0,0xd30470,0xd257f0,0xd257f0,0xd257f0};
    for(size_t index=0;index<records.size();++index){
        uint32_t exception=0;
        if(!property(lease.image,bug,properties[index],loads[index],records[index].c_str(),exception)){
            error="Original Bug property rejected/failed: "+records[index]+" SEH "+std::to_string(exception);return false;
        }
    }
    return true;
}
bool NativeServices::anchor(const Lease& lease,uintptr_t bug,std::string& error){
    uint32_t exception=0;
    if(!set_anchor(original_,bug,lease.parent,exception)){error="Original Bug SetAnchor SEH "+std::to_string(exception);return false;}
    return true;
}
bool NativeServices::ensure_collection(const Lease& requested,CollectionReceipt& receipt){
    if(receipt.admitted){
        if(receipt.requested!=requested){receipt.request_rejection="Retained collection receipt belongs to a different lease";return false;}
        return receipt.confirmed;
    }
    Snapshot before;
    if(!authorize(requested,receipt.cause)||!read(requested,before,receipt.cause))return false;
    receipt.requested=requested;receipt.admitted=true;
    if(requested.collection){receipt.collection=requested.collection;receipt.confirmed=true;return true;}
    try {
        if(!authorize(requested,receipt.cause))return false;
        Snapshot current_before;
        if(!read(requested,current_before,receipt.cause))return false;
        if(current_before!=before){receipt.cause="Bug tracker changed before original collection attachment";return false;}
        receipt.attempted=true;uint32_t exception=0;
        receipt.returned=attach_collection(&requested,receipt.collection,exception);
        if(!receipt.returned){receipt.unknown=true;receipt.cause="Original empty BugCollection attachment SEH "+std::to_string(exception);return false;}
        Lease attached=requested;attached.collection=receipt.collection;
        Snapshot after;
        if(!authorize(attached,receipt.cause)||!read(attached,after,receipt.cause)){receipt.unknown=true;return false;}
        if(after!=before){receipt.unknown=true;receipt.cause="Empty BugCollection attachment changed original Bug/Tracker state";return false;}
        receipt.confirmed=true;return true;
    }catch(const std::exception& error){receipt.unknown=receipt.attempted;if(receipt.cause.empty())receipt.cause=std::string("Bug collection attachment exception: ")+error.what();return false;}
    catch(...){receipt.unknown=receipt.attempted;if(receipt.cause.empty())receipt.cause="Bug collection attachment unknown exception";return false;}
}
}
