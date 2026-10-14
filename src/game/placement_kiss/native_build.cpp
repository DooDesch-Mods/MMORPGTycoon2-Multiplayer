#include "native_build.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstring>
#include "native_codeguards.h"
#include "native_family.h"
#include "family_codeguards.h"
#include "effect_codeguards.h"

namespace placement_kiss {
namespace {
bool read(const void* p, void* out, std::size_t bytes, std::uint32_t& exception) noexcept {
    __try { if(!p) return false; std::memcpy(out,p,bytes); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { exception=GetExceptionCode(); return false; }
}
bool guards(std::uintptr_t image, std::uint32_t& exception) noexcept {
    __try { for(const auto& g:native_guards) if(std::memcmp(reinterpret_cast<const void*>(image+g.rva),g.bytes,g.size)!=0) return false; for(const auto& g:effect_guards) if(std::memcmp(reinterpret_cast<const void*>(image+g.rva),g.bytes,g.size)!=0) return false; for(const auto& g:family_guards) if(std::memcmp(reinterpret_cast<const void*>(image+g.rva),g.bytes,g.size)!=0) return false; return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { exception=GetExceptionCode(); return false; }
}
bool string_read(const void* native, std::string& output, std::uint32_t& exception) {
    struct Gnu { const char* data; std::size_t size; std::byte rest[16]; } value{};
    if(!read(native,&value,sizeof(value),exception)||!value.data||!value.size||value.size>256) return false;
    char text[256]{};
    if(!read(value.data,text,value.size,exception)) return false;
    output.assign(text,value.size);
    return output.find('\0')==std::string::npos;
}
struct GnuString { alignas(8) std::byte bytes[32]{}; };
bool read_definition(std::uintptr_t image,const Request& r,void*& value,std::uint32_t& exception) noexcept {
    struct Borrowed { const char* data; std::size_t size; std::byte rest[16]{}; } type{r.type.data(),r.type.size(),{}};
    __try {
        void* manager=nullptr;
        if(!read(reinterpret_cast<void*>(image+0x13df780),&manager,sizeof(manager),exception)||!manager)return false;
        value=reinterpret_cast<void*(*)(void*,const void*)>(image+0x251500)(manager,&type);
        return value!=nullptr;
    } __except(EXCEPTION_EXECUTE_HANDLER) {exception=GetExceptionCode();return false;}
}
bool normal_definition(std::uintptr_t image,const Request& r,void*& value,std::string& cause,std::uint32_t& exception) {
    if(!read_definition(image,r,value,exception)){cause="error: actual BuildingDefinition lookup failed, exception="+std::to_string(exception);return false;}
    void* variants=nullptr;std::int32_t count=0;
    if(!family::definition(value,image,r,variants,count,cause,exception)){cause+=", exception="+std::to_string(exception);return false;}
    return true;
}
bool matching_identifier(void* identifier,const Request& request,std::uint32_t& exception) {
    std::string type,variant;
    return string_read(identifier,type,exception)&&string_read(static_cast<std::byte*>(identifier)+32,variant,exception)&&type==request.type&&variant==request.variant;
}
struct Preparation { std::uintptr_t image; const Request* request; void* shard; PrivateBuild* output; void* definition; std::string* cause; };
bool construct(const Preparation& p) noexcept {
    auto& out=*p.output;
    out.shard=p.shard;
    GnuString type,variant;
    __try {
        const auto b=p.image;
        out.stage=1;
        reinterpret_cast<void(*)(void*,const char*)>(b+0x49e090)(&type,p.request->type.c_str());
        out.stage=2;
        reinterpret_cast<void(*)(void*,const char*)>(b+0x49e090)(&variant,p.request->variant.c_str());
        out.stage=3;
        out.receiver=reinterpret_cast<void*(*)(std::size_t)>(b+0xa02670)(0x770);
        if(!out.receiver) return false;
        auto* receiver=static_cast<std::byte*>(out.receiver);
        out.stage=4;
        reinterpret_cast<void(*)(void*,const void*)>(b+0x5949f0)(receiver,&type);
        out.stage=5;
        void* identifier=nullptr;std::int32_t selected_index=-1;
        const auto getter=reinterpret_cast<family::IdentifierAt>(b+0xe43780);
        const family::ArrayCalls calls{reinterpret_cast<family::VariantAt>(b+0x2413a0),reinterpret_cast<family::Clear>(b+0xc4b230),reinterpret_cast<family::Add>(b+0xc4b240)};
        if(!family::populate_identifiers(calls,receiver+0x188,p.definition,b,*p.request,*p.cause,out.exception))return false;
        if(!family::select_identifier(getter,receiver+0x188,*p.request,selected_index,identifier,*p.cause,out.exception))return false;
        out.stage=19;
        reinterpret_cast<void(*)(void*)>(b+0x592450)(receiver);
        out.stage=20;
        if(!family::select_identifier(getter,receiver+0x1a0,*p.request,selected_index,identifier,*p.cause,out.exception)||!matching_identifier(identifier,*p.request,out.exception))return false;
        out.stage=6;
        void* blueprint=reinterpret_cast<void*(*)(std::size_t)>(b+0xa02670)(0x260);
        if(!blueprint) return false;
        // Publish owned children immediately, so the retained private receiver describes partial construction.
        std::memcpy(receiver+0x6c0,&blueprint,sizeof(blueprint));
        out.stage=7;
        reinterpret_cast<void(*)(void*,void*)>(b+0x400200)(blueprint,p.shard);
        out.stage=8;
        reinterpret_cast<void(*)(void*,const void*,const void*,bool,bool)>(b+0x400ad0)(blueprint,&type,&variant,false,true);
        out.stage=9;
        void* prop=reinterpret_cast<void*(*)(std::size_t)>(b+0xa02670)(0x220);
        if(!prop) return false;
        std::memcpy(receiver+0x6b8,&prop,sizeof(prop));
        out.stage=10;
        reinterpret_cast<void(*)(void*)>(b+0x4cacc0)(prop);
        out.stage=11;
        reinterpret_cast<void(*)(void*,void*)>(b+0x4cad10)(prop,blueprint);
        out.stage=12;
        reinterpret_cast<void(*)(void*,const float*,const float*,const float*)>(b+0x9ff370)(static_cast<std::byte*>(prop)+0x100,p.request->pose.quaternion.data(),p.request->pose.position.data(),p.request->pose.scale.data());
        // Only this private receiver is armed. The live UI cursor is never assigned this behaviour.
        if(!family::set_private_selection(receiver,selected_index,*p.cause,out.exception))return false;
        receiver[0x660]=std::byte{0}; receiver[0x661]=std::byte{1}; receiver[0x662]=std::byte{0};
        out.stage=13;
        reinterpret_cast<void(*)(void*)>(b+0x11bd1e0)(&variant);
        out.stage=14;
        reinterpret_cast<void(*)(void*)>(b+0x11bd1e0)(&type);
        out.stage=15;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { out.exception=GetExceptionCode(); return false; }
}
bool invoke(Update fn,void* receiver,std::uint32_t& exception) noexcept {
    __try { fn(receiver,0.0f); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { exception=GetExceptionCode(); return false; }
}
bool destruct(std::uintptr_t image,void* receiver,std::uint32_t& exception) noexcept {
    __try { reinterpret_cast<void(*)(void*)>(image+0x5950c0)(receiver); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { exception=GetExceptionCode(); return false; }
}
struct EffectPort {
    std::uintptr_t image;std::uint32_t& exception;
    bool read(std::uintptr_t p,void* out,std::size_t size) { return placement_kiss::read(reinterpret_cast<const void*>(p),out,size,exception); }
    bool manager(std::uintptr_t shard,std::uintptr_t& value) noexcept {
        __try { value=reinterpret_cast<std::uintptr_t(*)(std::uintptr_t)>(image+0x5086a0)(shard);return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {exception=GetExceptionCode();return false;}
    }
    bool destroy(std::uintptr_t owner,std::uintptr_t instance) noexcept {
        __try { reinterpret_cast<void(*)(std::uintptr_t,std::uintptr_t)>(image+0x188230)(owner,instance);return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {exception=GetExceptionCode();return false;}
    }
    bool clear(std::uintptr_t slot,std::uintptr_t expected) noexcept {
        __try {
            auto* p=reinterpret_cast<std::uintptr_t*>(slot);
            if(*p!=expected)return false;
            *p=0;return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) {exception=GetExceptionCode();return false;}
    }
};
bool consume(std::uintptr_t image,void* receiver,std::uint32_t& exception) noexcept {
    __try { reinterpret_cast<void(*)(void*)>(image+0x5f8c20)(static_cast<std::byte*>(receiver)+0x3c8); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { exception=GetExceptionCode(); return false; }
}
}
bool NativeBuild::bind_before_hooks(std::uintptr_t image,std::uint32_t root_main_thread,std::string& cause) {
    if(image_||!image||image!=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))||!root_main_thread||GetCurrentThreadId()!=root_main_thread) { cause="error: exact process image and actual Root mainthread required before binding"; return false; }
    std::uint32_t exception=0;
    if(!guards(image,exception)) { cause="error: original native full-body placement guard differs, exception="+std::to_string(exception); return false; }
    image_=image; thread_=root_main_thread; return true;
}
bool NativeBuild::publish_typed_original(Update fn,std::string& cause) {
    if(!image_||original_||!fn||GetCurrentThreadId()!=thread_) { cause="error: guarded original publication requires actual mainthread and one typed original"; return false; }
    original_=fn; return true;
}
Capture NativeBuild::capture_at_update_entry(void* receiver,Request& r,std::string& cause) const {
    if(!image_||GetCurrentThreadId()!=thread_) { cause="error: placement capture requires guarded actual mainthread"; return Capture::BlockedKnownCommit; }
    auto* p=static_cast<std::byte*>(receiver);
    std::uint32_t exception=0;
    std::uintptr_t ap=0;
    if(!read(p,&ap,8,exception)||ap!=image_+0x17327d0) { cause="error: Build receiver class is not the guarded native class"; return Capture::BlockedKnownCommit; }
    unsigned char flags[3]{};
    if(!read(p+0x660,flags,3,exception)) { cause="error: native pending placement flags are unreadable"; return Capture::BlockedKnownCommit; }
    if(flags[0]==1||flags[1]==0) return Capture::PassOriginal;
    if(flags[0]!=0||flags[1]!=1||flags[2]!=0) { cause="error: pending placement flags are invalid or cancellation is armed"; return Capture::BlockedKnownCommit; }
    void* prop=nullptr; void* blueprint=nullptr; void* data=nullptr;
    std::int32_t count=0,index=0; unsigned char palette=0;
    if(!read(p+0x6b8,&prop,8,exception)||!read(p+0x6c0,&blueprint,8,exception)||!prop||!blueprint||!read(p+0x1a8,&data,8,exception)||!read(p+0x1b0,&count,4,exception)||!read(p+0x6d0,&palette,1,exception)||palette>1||(palette&&!read(p+0x6cc,&index,4,exception))||!data||index<0||index>=count) { cause="error: pending native building identifiers or Blueprint children unavailable"; return Capture::BlockedKnownCommit; }
    std::uintptr_t prop_ap=0,bp_ap=0;
    if(!read(prop,&prop_ap,8,exception)||!read(blueprint,&bp_ap,8,exception)||prop_ap!=image_+0x1705230||bp_ap!=image_+0x16dd870) { cause="error: native Blueprint child class differs"; return Capture::BlockedKnownCommit; }
    auto* identifier=static_cast<std::byte*>(data)+std::size_t(index)*64;
    if(!string_read(identifier,r.type,exception)||!string_read(identifier+32,r.variant,exception)||!read(static_cast<std::byte*>(prop)+0x100,r.pose.quaternion.data(),16,exception)||!read(static_cast<std::byte*>(prop)+0x110,r.pose.position.data(),12,exception)||!read(static_cast<std::byte*>(prop)+0x11c,r.pose.scale.data(),12,exception)) { cause="error: pending native building scalar capture failed, exception="+std::to_string(exception); return Capture::BlockedKnownCommit; }
    if(!valid_scalars(r,cause)) return Capture::BlockedKnownCommit;
    void* definition=nullptr;
    if(!normal_definition(image_,r,definition,cause,exception))return Capture::BlockedKnownCommit;
    return Capture::Captured;
}
bool NativeBuild::prepare(const Request& r,void* shard,PrivateBuild& out,std::string& cause) const {
    if(!image_||!original_||GetCurrentThreadId()!=thread_||!shard||out.receiver||out.stage||!valid_request(r,cause)) { if(cause.empty()) cause="error: private native construction binding unavailable"; return false; }
    void* definition=nullptr;std::uint32_t exception=0;
    if(!normal_definition(image_,r,definition,cause,exception))return false;
    if(!construct({image_,&r,shard,&out,definition,&cause})) { cause=(cause.empty()?std::string{}:cause+"; ")+"error: private native construction failed at stage="+std::to_string(out.stage)+", exception="+std::to_string(out.exception); if(out.receiver&&(out.stage==5||out.stage==19||out.stage==20)){std::int32_t count=-1;std::uint32_t read_exception=0;const std::size_t offset=out.stage==5?0x198:0x1b0;const bool readable=read(static_cast<std::byte*>(out.receiver)+offset,&count,sizeof(count),read_exception);cause+=", identifier_header="+std::to_string(offset)+", count_read="+std::to_string(readable)+", actual_count="+std::to_string(count)+", required_count>=1 and exact requested filtered identifier, count_read_exception="+std::to_string(read_exception);} return false; }
    return true;
}
bool NativeBuild::consume_captured_intent(void* receiver,const Request& captured,std::uint32_t& exception,std::string& cause) const {
    Request fresh;
    if(capture_at_update_entry(receiver,fresh,cause)!=Capture::Captured||fresh.type!=captured.type||fresh.variant!=captured.variant||fresh.pose!=captured.pose) { if(cause.empty()) cause="error: captured native intent changed before its cleanup"; return false; }
    if(!consume(image_,receiver,exception)) { cause="error: native captured-intent Clear failed, exception="+std::to_string(exception)+"; cleanup cannot replay"; return false; }
    unsigned char flags[3]{};
    if(!read(static_cast<std::byte*>(receiver)+0x660,flags,3,exception)||flags[0]||flags[1]||flags[2]) { cause="error: native captured-intent flags did not clear; cleanup cannot replay"; return false; }
    return true;
}
bool NativeBuild::update_once(PrivateBuild& build,std::uint32_t& exception) const {
    if(!original_||GetCurrentThreadId()!=thread_||!build.receiver||build.stage!=15) return false;
    build.stage=16;
    return invoke(original_,build.receiver,exception);
}
bool NativeBuild::destroy_once(PrivateBuild& build,std::uint32_t& exception) const {
    if(!image_||GetCurrentThreadId()!=thread_||!build.receiver||(build.stage!=15&&build.stage!=16)) return false;
    build.stage=17;
    EffectPort effects{image_,exception};
    if(!effect_cleanup::release(effects,reinterpret_cast<std::uintptr_t>(build.receiver),reinterpret_cast<std::uintptr_t>(build.shard),build.preview_effect)) return false;
    if(!destruct(image_,build.receiver,exception)) return false;
    build.receiver=nullptr; build.stage=18; return true;
}
}
