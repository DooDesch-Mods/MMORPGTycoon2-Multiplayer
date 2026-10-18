#pragma once
#include <MinHook.h>
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
