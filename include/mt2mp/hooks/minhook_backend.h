#pragma once
#include <MinHook.h>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#if defined(MT2MP_FRAMEWORK_PLUGIN)
#include "framework_backend.h"
// Keep existing status checks while routing every call to this plugin's owner.
// No MinHook implementation or process-global hook manager is linked here.
#define MH_Initialize() (::mt2mp::hooks::framework::process_backend().initialize())
#define MH_Uninitialize() (::mt2mp::hooks::framework::process_backend().cleanup())
#define MH_CreateHook(target, detour, original) (::mt2mp::hooks::framework::process_backend().create(target, detour, original))
#define MH_EnableHook(target) (::mt2mp::hooks::framework::process_backend().enable(target))
#define MH_DisableHook(target) (::mt2mp::hooks::framework::process_backend().disable(target))
#define MH_RemoveHook(target) (::mt2mp::hooks::framework::process_backend().remove(target))
#define MH_StatusToString(status) (::mt2mp::hooks::framework::status_text(status))
#endif

namespace mt2mp::hooks {
inline bool enable_prepared_targets(std::span<const uintptr_t> targets,const char* operation,std::string& error){
#if defined(MT2MP_FRAMEWORK_PLUGIN)
    auto& backend=framework::process_backend();
    const auto result=backend.enable_prepared_batch(targets);
    if(result!=MH_OK){error=std::string(operation)+" enable failed: "+backend.error();return false;}
#else
    for(size_t i=0;i<targets.size();++i){const auto result=MH_EnableHook(reinterpret_cast<void*>(targets[i]));
        if(result!=MH_OK){error=std::string(operation)+" enable failed index="+std::to_string(i)+" target="+
            std::to_string(targets[i])+" status="+MH_StatusToString(result);return false;}}
#endif
    return true;
}
template<class Routes> bool enable_prepared_routes(uintptr_t image,const Routes& routes,const char* operation,std::string& error){
    std::vector<uintptr_t> targets;
    for(const auto& route:routes)targets.push_back(image+route.rva);
    return enable_prepared_targets(targets,operation,error);
}
}
