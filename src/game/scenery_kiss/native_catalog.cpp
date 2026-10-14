#include "native_catalog.h"
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "scenery_codeguards.h"
#include "catalog_sample.h"
#include <Windows.h>
#include <cstring>

namespace mt2mp::scenery_kiss {
namespace host=native::host;
namespace {
constexpr const char* exe="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
bool code(host::ProcessMemory& memory,uintptr_t image,std::string& e){
    for(const auto& guard:catalog_guards){std::vector<unsigned char> bytes(guard.size);if(!memory.copy(image+guard.rva,bytes.data(),bytes.size(),e))return false;
        if(std::memcmp(bytes.data(),guard.bytes,bytes.size())){e="scenery_catalog_code_differs RVA="+std::to_string(guard.rva);return false;}}
    return true;
}
bool text(host::ProcessMemory& memory,uintptr_t pointer,std::string& out,std::string& e){
    struct String {uintptr_t data;size_t size;unsigned char rest[16];} value{};
    if(!pointer||!memory.copy(pointer,&value,sizeof value,e)||!value.data||!value.size||value.size>256){if(e.empty())e="scenery_catalog_GNUstring_invalid";return false;}
    std::array<char,256> bytes{};if(!memory.copy(value.data,bytes.data(),value.size,e))return false;
    out.assign(bytes.data(),value.size);if(out.find('\0')!=std::string::npos){e="scenery_catalog_GNUstring_NUL";return false;}return true;
}
uint32_t lookup(uintptr_t image,uintptr_t manager,int32_t category,int32_t variant,uintptr_t& directory,uintptr_t& filename,uint64_t& roundtrip){
    __try{
        directory=uintptr_t(reinterpret_cast<const void*(*)(int32_t)>(image+0x448660)(category));
        if(!reinterpret_cast<bool(*)(void*,const void*,int32_t)>(image+0x2cead0)(reinterpret_cast<void*>(manager),reinterpret_cast<void*>(directory),variant))return 0xE0010002;
        filename=uintptr_t(reinterpret_cast<const void*(*)(void*,const void*,int32_t)>(image+0x2ceb40)(reinterpret_cast<void*>(manager),reinterpret_cast<void*>(directory),variant));
        roundtrip=reinterpret_cast<uint64_t(*)(void*,const void*)>(image+0x2cedb0)(reinterpret_cast<void*>(manager),reinterpret_cast<void*>(filename));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t directory_call(uintptr_t image,int32_t category,uintptr_t& result){
    __try{result=uintptr_t(reinterpret_cast<const void*(*)(int32_t)>(image+0x448660)(category));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t from_file(uintptr_t image,uintptr_t manager,const void* filename,uint64_t& out){
    __try{out=reinterpret_cast<uint64_t(*)(void*,const void*)>(image+0x2cedb0)(reinterpret_cast<void*>(manager),filename);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t find(host::FindUid fn,uint64_t uid,uintptr_t& object){
    __try{object=uintptr_t(fn(uid));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool manager(host::ProcessMemory& memory,uintptr_t image,uintptr_t& out,std::string& e){
    if(!memory.copy(image+0x13df730,&out,sizeof out,e))return false;
    if(!out){e="scenery_catalog_actual_manager_missing";return false;}return true;
}
struct ActualRoot final:Root {
    host::ProcessMemory& memory;uintptr_t image;uint32_t thread;host::LookupApi api;
    ActualRoot(host::ProcessMemory& m,uintptr_t i,uint32_t t,host::LookupApi a):memory(m),image(i),thread(t),api(a){}
    bool bind(uint64_t uid,Binding& b,std::string& e)override{
        b={};if(GetCurrentThreadId()!=thread){e="scenery_action_prop_wrong_thread";return false;}
        host::NativeOrigin origin;host::Reader reader(memory,origin,image,thread,api);host::WorldState world;prop_lease::Prop prop;
        uintptr_t object=0;const auto seh=find(api.find,uid,object);if(seh||!object){e="scenery_action_prop_registry_failed SEH="+std::to_string(seh);return false;}
        if(!reader.current(world,true,e)||!reader.prop(reinterpret_cast<void*>(object),uid,prop,e))return false;
        uintptr_t shard=0,region=0;
        if(!memory.copy(object+0x170,&shard,8,e)||!memory.copy(object+0x178,&region,8,e))return false;
        if(shard!=world.root){e="scenery_action_prop_requires_root_world";return false;}
        b={object,uid,world.origin.world_epoch,world.origin.generation,uintptr_t(world.origin.world),shard,region};return true;
    }
};
}
bool NativeCatalog::identify(int32_t category,int32_t variant,std::string& directory_text,std::string& filename_text,std::string& e){
    directory_text.clear();filename_text.clear();if(GetCurrentThreadId()!=thread_||category<0||category>=47){e="scenery_catalog_actual_thread_or_category_invalid";return false;}
    if(!code(memory_,image_,e))return false;
    uintptr_t actual_manager=0,after_manager=0,directory=0,filename=0;uint64_t roundtrip=0;
    if(!manager(memory_,image_,actual_manager,e))return false;
    const auto seh=lookup(image_,actual_manager,category,variant,directory,filename,roundtrip);
    if(seh){e="scenery_catalog_native_lookup_failed SEH="+std::to_string(seh);return false;}
    const uint64_t expected=uint64_t(uint32_t(category))|(uint64_t(uint32_t(variant))<<32);
    if(roundtrip!=expected){e="scenery_catalog_native_roundtrip_differs";return false;}
    if(!text(memory_,directory,directory_text,e)||!text(memory_,filename,filename_text,e)||!manager(memory_,image_,after_manager,e))return false;
    if(actual_manager!=after_manager){e="scenery_catalog_actual_manager_changed";return false;}return true;
}
bool NativeCatalog::classify_directory(const std::string& type,bool& scenery,std::string& e){
    scenery=false;if(GetCurrentThreadId()!=thread_){e="scenery family lookup wrong thread";return false;}
    if(type.empty()||type.size()>256||type.find('\0')!=std::string::npos){e="scenery family type invalid";return false;}
    if(!code(memory_,image_,e))return false;
    for(int32_t category=0;category<47;++category){uintptr_t directory=0;std::string actual;const auto seh=directory_call(image_,category,directory);
        if(seh){e="scenery family native directory failed SEH="+std::to_string(seh);return false;}
        if(!text(memory_,directory,actual,e))return false;
        if(actual==type){scenery=true;return true;}}
    return true;
}
bool NativeCatalog::resolve(const State& state,int32_t& category,int32_t& variant,std::string& e){
    category=-1;variant=0;
    if(GetCurrentThreadId()!=thread_||!code(memory_,image_,e)){if(e.empty())e="scenery_catalog_actual_thread_invalid";return false;}
    if(state.variant.empty()||state.variant.size()>256||state.variant.find('\0')!=std::string::npos){e="scenery_catalog_request_filename_invalid";return false;}
    struct String {const char* data;size_t size;unsigned char rest[16];} view{state.variant.data(),state.variant.size(),{}};
    uintptr_t actual_manager=0;if(!manager(memory_,image_,actual_manager,e))return false;
    uint64_t packed=0;const auto seh=from_file(image_,actual_manager,&view,packed);if(seh){e="scenery_catalog_native_resolve_failed SEH="+std::to_string(seh);return false;}
    const auto unsigned_category=uint32_t(packed);if(unsigned_category>=47){e="scenery_catalog_request_category_unknown";return false;}
    const int32_t wanted_category=int32_t(unsigned_category),wanted_variant=int32_t(uint32_t(packed>>32));
    std::string directory,filename;if(!identify(wanted_category,wanted_variant,directory,filename,e))return false;
    if(directory!=state.type||filename!=state.variant){e="scenery_catalog_request_identity_differs";return false;}
    category=wanted_category;variant=wanted_variant;return true;
}
bool resolve_ui_scenery_catalog(uintptr_t image,uint32_t thread,const std::string& type,const std::string& variant,bool& scenery,int32_t& category,int32_t& hash,std::string& e){
    host::ProcessMemory memory;NativeCatalog catalog(memory,image,thread);scenery=false;category=-1;hash=0;
    if(!catalog.classify_directory(type,scenery,e))return false;
    if(!scenery)return true;
    State state;state.type=type;state.variant=variant;state.alive=true;
    return catalog.resolve(state,category,hash,e);
}
bool read_native_scenery_samples(nlohmann::json& out,std::string& e){
    out=nlohmann::json::object();uintptr_t image=0;uint32_t thread=0;std::string hash;
    if(!game::native::read_resolver_receipt(image,thread,hash,e))return false;
    if(hash!=exe||thread!=GetCurrentThreadId()){e="scenery catalog sample actual EXE/thread differs";return false;}
    host::ProcessMemory memory;host::LookupApi api;if(!host::resolve_lookup(memory,image,hash,api,e))return false;
    host::NativeOrigin origin;host::Reader reader(memory,origin,image,thread,api);host::WorldState before,after;
    if(!reader.current(before,true,e))return false;
    std::vector<game::native::WorldPropState> props;if(!game::native::list_world_props(128,props,e))return false;
    nlohmann::json samples=nlohmann::json::array();NativeCatalog catalog(memory,image,thread);
    for(const auto& prop:props){uintptr_t object=0,ap=0,shard=0;const auto seh=find(api.find,prop.prop.uid,object);
        if(seh||!object){e="scenery catalog sample actual UID lookup failed SEH="+std::to_string(seh);return false;}
        if(!memory.copy(object,&ap,sizeof ap,e))return false;if(ap!=image+0x16c1390)continue;
        if(!memory.copy(object+0x170,&shard,sizeof shard,e))return false;if(shard!=before.root)continue;
        CatalogSample sample;if(!read_catalog_sample(memory,catalog,image,object,prop.prop.uid,before.root,sample,e))return false;
        uintptr_t registered_after=0;const auto after_seh=find(api.find,sample.uid,registered_after);
        if(after_seh||registered_after!=object){e="scenery catalog sample UID registry changed SEH="+std::to_string(after_seh);return false;}
        const auto& actual=sample.state;
        samples.push_back({{"local_uid",sample.uid},{"type",actual.type},{"variant",actual.variant},{"category",sample.category},{"variant_hash",sample.variant_hash},{"x",actual.x},{"y",actual.y},{"z",actual.z},{"rotation",actual.rotation},{"scale",actual.scale},{"action_admission","not_evaluated"}});
    }
    if(!reader.current(after,true,e)||!host::same_world(before.origin,after.origin)){if(e.empty())e="scenery catalog sample actual world changed";return false;}
    out={{"observed",true},{"scope","actual_root_scenery_from_first_128_props"},{"prop_count",props.size()},{"sample_count",samples.size()},{"prop_limit",128},{"possibly_truncated",props.size()==128},{"snapshot_uuid",before.origin.uuid},{"world_epoch",before.origin.world_epoch},{"generation",before.origin.generation},{"samples",std::move(samples)}};e.clear();return true;
}
bool read_action_prop(uint64_t uid,game::native::BuildingState& out,std::string& e){
    out={};uintptr_t image=0;uint32_t thread=0;std::string hash;
    if(!game::native::read_resolver_receipt(image,thread,hash,e))return false;
    if(hash!=exe||thread!=GetCurrentThreadId()){e="scenery_action_prop_exe_or_thread_differs";return false;}
    host::ProcessMemory memory;host::LookupApi api;if(!host::resolve_lookup(memory,image,hash,api,e))return false;
    uintptr_t object=0,ap=0;const auto seh=find(api.find,uid,object);
    if(seh||!object){e="scenery_action_prop_registry_failed SEH="+std::to_string(seh);return false;}
    if(!memory.copy(object,&ap,sizeof ap,e))return false;
    if(ap!=image+0x16c1390)return game::native::read_building(uid,out,e);
    ActualRoot root(memory,image,thread,api);NativeCatalog catalog(memory,image,thread);State state;
    if(!read_state(memory,catalog,root,image,uid,state,e))return false;
    out.prop={uid,state.x,state.y,state.z};out.type=state.type;out.variant=state.variant;out.rotation=state.rotation;out.scale=state.scale;return true;
}
}
