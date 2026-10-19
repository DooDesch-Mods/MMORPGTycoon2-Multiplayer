#include "dungeon_scene_runtime.h"
#include "dungeon_scene_key.h"
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "mt2mp/game/native_engine.h"
#include "remove_diagnostics.h"
#include "typed_action_runtime.h"
#include "typed_action/action_session.h"
#include "session_provider.h"
#include <Windows.h>

namespace mt2mp::game::dungeon_scene_runtime {
namespace {
namespace host=mt2mp::build_actions::private_native::host;
constexpr const char* game_sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
bool fail(std::string& error,const char* cause){error=cause;return false;}
uint32_t native_cast(host::DynamicCast call,uintptr_t object,uintptr_t source,uintptr_t target,uintptr_t& result)noexcept{
    __try{result=reinterpret_cast<uintptr_t>(call(reinterpret_cast<void*>(object),reinterpret_cast<const void*>(source),
        reinterpret_cast<const void*>(target),0));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool cursor_scene(std::string& scene,std::string& error)try{
    scene.clear();error.clear();uintptr_t image=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(image,thread,sha,error))return false;
    if(sha!=game_sha||image!=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))||thread!=GetCurrentThreadId())
        return fail(error,"Dungeon cursor scene requires the pinned native image and bound Mainthread");
    action_session::Context context;action_session::Binding binding;
    if(!action_session::selected_session().current(context,error)||!typed_action_runtime::network_binding(context.snapshot_uuid,binding,error))return false;
    host::ProcessMemory memory;host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(memory,image,sha,api,error))return false;
    host::Reader reader(memory,origin,image,thread,api);host::WorldState before;
    if(!reader.current(before,false,error)||before.origin.world_epoch!=binding.native_epoch||before.origin.generation!=binding.native_generation)
        return fail(error,error.empty()?"Dungeon cursor scene current NativeOpen/Controller lifetime differs":error.c_str());
    const auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){
        if(!object||offset>UINTPTR_MAX-object||object+offset>UINTPTR_MAX-(sizeof value-1)){cause="Dungeon cursor scene field address invalid";return false;}
        return memory.copy(object+offset,&value,sizeof value,cause);
    };
    uintptr_t manager=0,ap=0,current=0;
    if(!at(image,0x13df440,manager,error)||!at(manager,0,ap,error)||ap!=image+0x1702b00||!at(manager,0x60,current,error))
        return fail(error,error.empty()?"Dungeon cursor scene current ShardManager addresspoint/active Shard differs":error.c_str());
    const auto typed=[&](uintptr_t object,uintptr_t source,uintptr_t target,std::string& cause){
        uintptr_t result=0;const auto seh=native_cast(api.cast,object,image+source,image+target,result);
        if(seh||result!=object){cause="Dungeon cursor scene exact native RTTI failed SEH="+std::to_string(seh);return false;}return true;
    };
    const auto identify=[&](uintptr_t exterior,uint64_t uid,std::string& key,std::string& cause){
        return remove_diagnostics::resolve_current_building_key(context.snapshot_uuid,exterior,uid,key,cause);
    };
    dungeon_scene_key::Scene actual;
    if(!dungeon_scene_key::read(image,before.root,current,at,typed,identify,actual,error))return false;
    host::WorldState after;action_session::Context final_context;action_session::Binding final_binding;uintptr_t final_manager=0,final_shard=0;
    if(!reader.current(after,false,error)||!host::same_world(before.origin,after.origin)||after.root!=before.root||
       !action_session::selected_session().current(final_context,error)||final_context!=context||
       !typed_action_runtime::network_binding(context.snapshot_uuid,final_binding,error)||final_binding!=binding||
       !at(image,0x13df440,final_manager,error)||final_manager!=manager||!at(manager,0x60,final_shard,error)||final_shard!=current)
        return fail(error,error.empty()?"Dungeon cursor scene current world/active Shard changed during sample":error.c_str());
    scene=std::move(actual.key);return true;
}catch(const std::exception& exception){scene.clear();error=std::string("Dungeon cursor scene boundary: ")+exception.what();return false;}
catch(...){scene.clear();return fail(error,"Dungeon cursor scene non-standard exception");}
}
