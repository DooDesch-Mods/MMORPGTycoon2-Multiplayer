#include "character_import_native.h"
#include "character_import_codes.h"
#include "mt2mp/hooks/original_body.h"
#include <Windows.h>
#include <cstring>
namespace mt2mp::game::character_import_native {
namespace {
namespace payload=character_import_payload;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
enum class Step {Decode,DestroyStore,Borrow,DestroyBorrow,ImageConstruct,ImageClear,Encode,ImageDestroy,ZBegin,ZInflate,ZEnd};
using Init=int(*)(payload::Stream*,const char*,int);
using Inflate=int(*)(payload::Stream*,int);
using End=int(*)(payload::Stream*);
using Version=const char*(*)();
struct Invocation {
    uintptr_t image=0,object=0,store=0;Step step=Step::Decode;
    void* storage=nullptr;const uint8_t* bytes=nullptr;size_t size=0;
    Init init=nullptr;Inflate inflate=nullptr;End end=nullptr;Version version=nullptr;payload::Stream* stream=nullptr;int code=0;
};
uint32_t invoke(Invocation& call)noexcept{
    __try{switch(call.step){
        case Step::Decode:call.store=reinterpret_cast<uintptr_t(*)(const void*)>(call.image+0x880c90)(reinterpret_cast<const void*>(call.object));break;
        case Step::DestroyStore:reinterpret_cast<void(*)(void*)>(call.image+0xa06600)(reinterpret_cast<void*>(call.store));break;
        case Step::Borrow:reinterpret_cast<void(*)(void*,const char*,int32_t)>(call.image+0xa066d0)(call.storage,reinterpret_cast<const char*>(call.bytes),int32_t(call.size));break;
        case Step::DestroyBorrow:reinterpret_cast<void(*)(void*)>(call.image+0xa065d0)(call.storage);break;
        case Step::ImageConstruct:reinterpret_cast<void(*)(void*,uint32_t,uint32_t)>(call.image+0xa10c40)(call.storage,512,512);break;
        case Step::ImageClear:{const std::array<float,4> opaque{0,0,0,1};reinterpret_cast<void(*)(void*,const void*)>(call.image+0xa11640)(call.storage,opaque.data());break;}
        case Step::Encode:reinterpret_cast<void(*)(void*,void*)>(call.image+0x8814e0)(call.storage,reinterpret_cast<void*>(call.store));break;
        case Step::ImageDestroy:reinterpret_cast<void(*)(void*)>(call.image+0xa113d0)(call.storage);break;
        case Step::ZBegin:call.code=call.init(call.stream,call.version(),sizeof(payload::Stream));break;
        case Step::ZInflate:call.code=call.inflate(call.stream,4);break;
        case Step::ZEnd:call.code=call.end(call.stream);break;
    }return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool codes(uintptr_t image,subscriber_lifecycle::Memory& memory,std::string& error){
    for(const auto& pin:character_import_codes::native){std::vector<unsigned char> value(pin.bytes);
        if(!memory.read(image+pin.rva,value.data(),value.size(),error))return false;
        if(!hooks::framework::verify_original_hash(image+pin.rva,value.data(),value.size(),pin.sha,error))
            return fail(error,"Character import original native body differs RVA="+std::to_string(pin.rva)+" original="+error);}
    return true;
}
struct Zlib {
    Invocation call;Receipt& receipt;
    bool resolve(uintptr_t image,subscriber_lifecycle::Memory& memory,std::string& error){
        const auto module=GetModuleHandleW(L"zlib1.dll");
        if(!module)return fail(error,"Character import original loaded zlib1.dll is absent");
        call.init=reinterpret_cast<Init>(GetProcAddress(module,"inflateInit_"));call.inflate=reinterpret_cast<Inflate>(GetProcAddress(module,"inflate"));
        call.end=reinterpret_cast<End>(GetProcAddress(module,"inflateEnd"));call.version=reinterpret_cast<Version>(GetProcAddress(module,"zlibVersion"));
        if(!call.init||!call.inflate||!call.end||!call.version)return fail(error,"Character import original zlib public API is incomplete");
        const std::array<std::pair<uintptr_t,uintptr_t>,3> imports{{{0xa7f588,uintptr_t(call.init)},{0xa7f598,uintptr_t(call.inflate)},{0xa7f590,uintptr_t(call.end)}}};
        for(const auto& [rva,expected]:imports){std::array<unsigned char,6> thunk{};int32_t relative=0;uintptr_t entry=0;
            if(!memory.read(image+rva,thunk.data(),thunk.size(),error))return false;
            if(thunk[0]!=0xff||thunk[1]!=0x25)return fail(error,"Character import original zlib IAT thunk changed RVA="+std::to_string(rva));
            std::memcpy(&relative,thunk.data()+2,4);const auto address=uintptr_t(int64_t(image+rva+6)+relative);
            if(!memory.read(address,&entry,sizeof entry,error))return false;
            if(entry!=expected)return fail(error,"Character import current original zlib import receiver differs RVA="+std::to_string(rva));}
        return true;
    }
    bool operation(Step step,payload::Stream& stream,int expected,std::string& error){
        call.step=step;call.stream=&stream;const auto seh=invoke(call);
        if(seh){receipt.unknown=true;receipt.exception=seh;return fail(error,"Character import zlib operation SEH="+std::to_string(seh)+" step="+std::to_string(unsigned(step)));}
        if(call.code!=expected)return fail(error,"Character import zlib status="+std::to_string(call.code)+" required="+std::to_string(expected)+" step="+std::to_string(unsigned(step))+
            " consumed="+std::to_string(stream.total_in)+" remaining="+std::to_string(stream.avail_in)+" produced="+std::to_string(stream.total_out));return true;
    }
    bool begin(payload::Stream& stream,std::string& error){return operation(Step::ZBegin,stream,0,error);}
    bool inflate(payload::Stream& stream,std::string& error){return operation(Step::ZInflate,stream,1,error);}
    bool end(payload::Stream& stream,std::string& error){return operation(Step::ZEnd,stream,0,error);}
};
bool checked(uintptr_t image,subscriber_lifecycle::Memory& memory,const std::vector<uint8_t>& compressed,Receipt& receipt,std::string& error){
    Zlib zlib{{},receipt};if(!zlib.resolve(image,memory,error))return false;std::vector<uint8_t> expanded;
    if(!payload::expand(zlib,compressed,expanded,error))return false;
    std::vector<subscriber_lifecycle::Record> records;if(!payload::record(expanded,records,error))return false;
    receipt.payload_complete=true;return true;
}
bool read_bytes(uintptr_t image,subscriber_lifecycle::Memory& memory,uintptr_t original_image,std::vector<uint8_t>& compressed,Receipt& receipt,std::string& error){
    if(!original_image)return fail(error,"Character import original image receiver is null");
    Invocation call;call.image=image;call.object=original_image;receipt.decoder_attempted=true;const auto seh=invoke(call);
    if(seh){receipt.unknown=true;receipt.exception=seh;return fail(error,"Character import original image decoder SEH="+std::to_string(seh));}
    receipt.decoder_returned=true;if(!call.store)return fail(error,"Character import original image contains no embedded data");
    auto read=[&](uintptr_t base,size_t offset,auto& out,std::string& cause){if(!base||base>UINTPTR_MAX-offset)return fail(cause,"Character import Store address overflow/null");return memory.read(base+offset,&out,sizeof out,cause);};
    payload::Store value;bool ok=payload::store(read,call.store,image+0x1749500,value,error);
    if(ok){compressed.resize(value.write-value.buffer);ok=memory.read(value.buffer,compressed.data(),compressed.size(),error);}
    const auto cause=error;call.step=Step::DestroyStore;const auto cleanup=invoke(call);receipt.decoder_cleaned=cleanup==0;
    if(cleanup){receipt.unknown=true;receipt.exception=cleanup;return fail(error,(cause.empty()?std::string{}:cause+"; ")+"Character import decoded Store cleanup SEH="+std::to_string(cleanup));}
    return ok;
}
}
bool verify(uintptr_t image,subscriber_lifecycle::Memory& memory,std::string& error){return codes(image,memory,error);}
bool read(uintptr_t image,subscriber_lifecycle::Memory& memory,uintptr_t original_image,std::string& encoded,Receipt& receipt,std::string& error)try{
    encoded.clear();receipt={};if(!codes(image,memory,error)){receipt.cause=error;return false;}
    std::vector<uint8_t> compressed;
    if(!read_bytes(image,memory,original_image,compressed,receipt,error)||!checked(image,memory,compressed,receipt,error)){receipt.cause=error;return false;}
    encoded=payload::hex(compressed);return true;
}catch(const std::exception& exception){receipt.cause=error=std::string("Character import embedded payload capture: ")+exception.what();return false;}
bool validate(uintptr_t image,subscriber_lifecycle::Memory& memory,const std::string& encoded,Receipt& receipt,std::string& error)try{
    receipt={};if(!codes(image,memory,error)){receipt.cause=error;return false;}std::vector<uint8_t> compressed;
    if(!payload::bytes(encoded,compressed,error)||!checked(image,memory,compressed,receipt,error)){receipt.cause=error;return false;}return true;
}catch(const std::exception& exception){receipt.cause=error=std::string("Character import embedded payload validation: ")+exception.what();return false;}
bool create(uintptr_t image,subscriber_lifecycle::Memory& memory,const std::string& encoded,Image& owner,Receipt& receipt,std::string& error)try{
    if(owner.constructed||owner.unknown)return fail(error,"Character import private image owner is not empty");
    if(!validate(image,memory,encoded,receipt,error))return false;std::vector<uint8_t> compressed;if(!payload::bytes(encoded,compressed,error))return false;
    Invocation call;call.image=image;call.storage=owner.storage.data();call.step=Step::ImageConstruct;auto seh=invoke(call);
    if(seh){owner.unknown=receipt.unknown=true;receipt.exception=seh;return fail(error,"Character import private image constructor SEH="+std::to_string(seh));}
    owner.constructed=receipt.image_constructed=true;call.step=Step::ImageClear;seh=invoke(call);
    if(seh){owner.unknown=receipt.unknown=true;receipt.exception=seh;return fail(error,"Character import private opaque image Clear SEH="+std::to_string(seh));}
    alignas(8) std::array<unsigned char,0x38> borrowed{};call.step=Step::Borrow;call.storage=borrowed.data();call.bytes=compressed.data();call.size=compressed.size();seh=invoke(call);
    if(seh){receipt.unknown=true;receipt.exception=seh;return fail(error,"Character import private borrowed Store constructor SEH="+std::to_string(seh));}
    receipt.borrowed_constructed=true;
    call.step=Step::Encode;call.storage=owner.storage.data();call.store=uintptr_t(borrowed.data());seh=invoke(call);
    if(seh){owner.unknown=receipt.unknown=true;receipt.exception=seh;error="Character import original EncodeDataInImage SEH="+std::to_string(seh);}
    call.step=Step::DestroyBorrow;call.storage=borrowed.data();const auto cleanup=invoke(call);receipt.borrowed_cleaned=cleanup==0;
    if(cleanup){receipt.unknown=true;if(!error.empty())error+="; ";error+="Character import borrowed Store destructor SEH="+std::to_string(cleanup);return false;}
    if(seh)return false;
    receipt.image_encoded=true;std::vector<uint8_t> actual;
    if(!read_bytes(image,memory,uintptr_t(owner.storage.data()),actual,receipt,error))return false;
    if(actual!=compressed)return fail(error,"Character import private original image roundtrip changed embedded data");
    receipt.image_roundtrip=true;return true;
}catch(const std::exception& exception){receipt.cause=error=std::string("Character import private original image: ")+exception.what();return false;}
bool discard(uintptr_t image,Image& owner,Receipt& receipt,std::string& error){
    if(!owner.constructed)return true;
    if(owner.unknown)return fail(error,"Character import private image cleanup cannot replay an Unknown constructor/encoder");
    Invocation call;call.image=image;call.storage=owner.storage.data();call.step=Step::ImageDestroy;const auto seh=invoke(call);
    if(seh){owner.unknown=receipt.unknown=true;receipt.exception=seh;return fail(error,"Character import private image destructor SEH="+std::to_string(seh));}
    owner.constructed=false;receipt.image_cleaned=true;return true;
}
}
