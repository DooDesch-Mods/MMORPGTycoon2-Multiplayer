#include "native_scenery.h"
#include "native_catalog.h"
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "scenery_codeguards.h"
#include <Windows.h>
#include <cstring>

namespace mt2mp::scenery_kiss {
namespace {
bool guards(uintptr_t image,uint32_t& exception)noexcept{
    __try{for(const auto& guard:scenery_guards)if(std::memcmp(reinterpret_cast<const void*>(image+guard.rva),guard.bytes,guard.size))return false;return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();return false;}
}
bool construct(uintptr_t image,const State& state,int32_t category,int32_t variant,void* shard,PrivateScenery& out)noexcept{
    __try{
        out.stage=1;out.directory_ctor_attempted=true;reinterpret_cast<void(*)(void*,const char*)>(image+0x49e090)(out.directory.data(),state.type.c_str());out.directory_ctor_returned=true;
        out.stage=2;out.receiver=reinterpret_cast<void*(*)(size_t)>(image+0xa02670)(0xc58);if(!out.receiver)return false;
        out.stage=3;reinterpret_cast<void(*)(void*,const void*)>(image+0x5d0770)(out.receiver,out.directory.data());
        auto* cursor=static_cast<unsigned char*>(out.receiver);
        out.stage=4;out.options=reinterpret_cast<void*(*)(size_t)>(image+0xa02670)(0xd0);if(!out.options)return false;
        out.stage=5;reinterpret_cast<void(*)(void*)>(image+0x5fcb00)(out.options);out.options_constructed=true;std::memcpy(cursor+0xb68,&out.options,8);
        out.stage=6;void* scene=reinterpret_cast<void*(*)(void*)>(image+0x507ba0)(shard);if(!scene)return false;
        out.stage=7;void* blueprint=reinterpret_cast<void*(*)(size_t)>(image+0xa02670)(0x260);if(!blueprint)return false;std::memcpy(cursor+0xb50,&blueprint,8);
        out.stage=8;reinterpret_cast<void(*)(void*,void*)>(image+0x400440)(blueprint,scene);
        out.stage=9;reinterpret_cast<void(*)(void*,int32_t,int32_t,bool)>(image+0x3ff720)(blueprint,category,variant,true);
        out.stage=10;void* ghost=reinterpret_cast<void*(*)(size_t)>(image+0xa02670)(0x220);if(!ghost)return false;std::memcpy(cursor+0xb58,&ghost,8);
        out.stage=11;reinterpret_cast<void(*)(void*)>(image+0x4cacc0)(ghost);
        out.stage=12;reinterpret_cast<void(*)(void*,void*)>(image+0x4cf6e0)(ghost,shard);
        out.stage=13;reinterpret_cast<void(*)(void*,void*)>(image+0x4cad10)(ghost,blueprint);
        out.stage=14;const float position[]{state.x,state.y,state.z};reinterpret_cast<void(*)(void*,const float*,const float*,const float*)>(image+0x9ff370)(static_cast<unsigned char*>(ghost)+0x100,state.rotation.data(),position,state.scale.data());
        const int32_t mode=0;std::memcpy(cursor+0x174,&mode,4);std::memcpy(cursor+0x178,&category,4);std::memcpy(cursor+0x17c,&variant,4);
        cursor[0x6c0]=0;cursor[0x6c1]=1;cursor[0x6c2]=0;cursor[0xb48]=0;
        out.stage=15;out.directory_dispose_attempted=true;reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(out.directory.data());out.directory_dispose_returned=true;out.stage=16;return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){out.exception=GetExceptionCode();return false;}
}
bool update(Update original,PrivateScenery& out)noexcept{
    out.update_attempted=true;out.stage=17;
    __try{original(out.receiver,0.0f);out.update_returned=true;out.stage=18;return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){out.exception=GetExceptionCode();return false;}
}
bool destroy(uintptr_t image,PrivateScenery& out)noexcept{
    out.cursor_dtor_attempted=true;
    __try{reinterpret_cast<void(*)(void*)>(image+0x5d0fc0)(out.receiver);out.cursor_dtor_returned=true;out.receiver=nullptr;}
    __except(EXCEPTION_EXECUTE_HANDLER){out.cleanup_exception=GetExceptionCode();return false;}
    out.options_dtor_attempted=true;
    __try{reinterpret_cast<void(*)(void*)>(image+0xbefc50)(out.options);out.options_dtor_returned=true;out.options=nullptr;out.stage=19;return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){out.cleanup_exception=GetExceptionCode();return false;}
}
uint32_t clear(uintptr_t image,uintptr_t receiver)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(image+0x5f8c20)(reinterpret_cast<void*>(receiver+0x428));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool NativeScenery::bind_before_hooks(uintptr_t image,uint32_t thread,std::string& e){
    if(image_||!image||image!=uintptr_t(GetModuleHandleW(nullptr))||!thread||thread!=GetCurrentThreadId()){e="scenery_native_actual_image_thread_required";return false;}
    uint32_t exception=0;if(!guards(image,exception)){e="scenery_native_original_guard_failed SEH="+std::to_string(exception);return false;}
    image_=image;thread_=thread;return true;
}
bool NativeScenery::publish_typed_original(Update original,std::string& e){
    if(!image_||original_||!original||GetCurrentThreadId()!=thread_){e="scenery_native_original_publication_rejected";return false;}original_=original;return true;
}
bool NativeScenery::consume_captured_intent(native::Memory& memory,Catalog& catalog,uintptr_t root_shard,uintptr_t receiver,const State& captured,uint32_t& exception,std::string& e)const{
    exception=0;if(!image_||!original_||GetCurrentThreadId()!=thread_){e="scenery_native_capture_cleanup_binding_unavailable";return false;}
    State fresh;if(read_single_intent(memory,catalog,image_,root_shard,receiver,fresh,e)!=Capture::Single||fresh!=captured){if(e.empty())e="scenery_native_capture_changed_before_cleanup";return false;}
    exception=clear(image_,receiver);if(exception){e="scenery_native_capture_Clear_failed SEH="+std::to_string(exception)+"; no replay";return false;}
    std::array<unsigned char,3> flags{};if(!memory.copy(receiver+0x6c0,flags.data(),flags.size(),e))return false;
    if(flags!=std::array<unsigned char,3>{}){e="scenery_native_capture_Clear_flags_not_zero; no replay";return false;}return true;
}
bool NativeScenery::prepare(const State& state,int32_t category,int32_t variant,void* shard,PrivateScenery& out,std::string& e)const{
    if(!image_||!original_||GetCurrentThreadId()!=thread_||!shard||out.stage||out.receiver||out.options||category<0||category>=47||!state.alive){e="scenery_native_prepare_binding_rejected";return false;}
    if(!valid_state(state,e))return false;
    native::host::ProcessMemory memory;NativeCatalog catalog(memory,image_,thread_);int32_t actual_category=0,actual_variant=0;
    if(!catalog.resolve(state,actual_category,actual_variant,e))return false;
    if(category!=actual_category||variant!=actual_variant){e="scenery_native_prepare_catalog_indices_differ";return false;}
    if(!construct(image_,state,category,variant,shard,out)){e="scenery_native_prepare_failed stage="+std::to_string(out.stage)+" SEH="+std::to_string(out.exception)+"; retain partial ownership, no replay";return false;}return true;
}
bool NativeScenery::update_once(PrivateScenery& out,std::string& e)const{
    if(!original_||GetCurrentThreadId()!=thread_||out.stage!=16||!out.receiver||!out.options_constructed||out.update_attempted){e="scenery_native_Update_binding_or_replay_rejected";return false;}
    if(!update(original_,out)){e="scenery_native_Update_failed SEH="+std::to_string(out.exception)+"; effects Unknown, no replay";return false;}return true;
}
bool NativeScenery::destroy_once(PrivateScenery& out,std::string& e)const{
    if(!image_||GetCurrentThreadId()!=thread_||out.stage!=18||!out.update_returned||!out.options_constructed||out.exception||out.cursor_dtor_attempted||out.options_dtor_attempted){e="scenery_native_cleanup_unconfirmed_or_replay_rejected";return false;}
    if(!destroy(image_,out)){e="scenery_native_cleanup_failed SEH="+std::to_string(out.cleanup_exception)+"; retain partial ownership, no replay";return false;}return true;
}
}
