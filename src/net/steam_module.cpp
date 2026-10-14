#include "mt2mp/net/steam_module.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/installed_profile.h"
#include "mt2mp/core/private_profile_paths.h"
#include "mt2mp/core/logger.h"

namespace mt2mp::net {
HMODULE steam_module(){
    static const HMODULE selected=[]()->HMODULE{
        try{
            const auto& config=Config::instance();
            const auto* installed=installed_profile::get();
            const auto game=installed?installed->game_dir:std::filesystem::path(config.game_dir);
            if(game.empty()||!game.is_absolute())throw std::runtime_error("Steam module game directory unavailable");
            const auto expected=game/(installed?L"steam_api64.dll":L"steam_api64_original.dll");
            if(!private_profile::equal(private_profile::canonical_file(expected),expected))
                throw std::runtime_error("Steam selected module path is an alias outside its exact game location");
            HMODULE module=nullptr;
            if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN,expected.c_str(),&module)){
                module=LoadLibraryExW(expected.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
                if(!module)throw std::runtime_error("Steam exact-path load failed Win32="+std::to_string(GetLastError()));
            }
            if(!private_profile::equal(private_profile::module_file(module),private_profile::canonical_file(expected)))
                throw std::runtime_error("Steam loaded module path differs from selected profile");
            for(const char* name:{"SteamAPI_RunCallbacks","SteamAPI_Shutdown","SteamAPI_GetHSteamUser","SteamInternal_FindOrCreateUserInterface", "SteamAPI_RegisterCallback","SteamAPI_ISteamUser_GetSteamID","SteamAPI_SteamNetworking_v006", "SteamAPI_ISteamNetworking_SendP2PPacket","SteamAPI_ISteamNetworking_IsP2PPacketAvailable","SteamAPI_ISteamNetworking_ReadP2PPacket"})
                if(!GetProcAddress(module,name))throw std::runtime_error("Steam required export absent: "+std::string(name));
            HMODULE pinned=nullptr;
            if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN,expected.c_str(),&pinned)||pinned!=module)
                throw std::runtime_error("Steam selected module pin differs Win32="+std::to_string(GetLastError()));
            return module;
        }catch(const std::exception& ex){LOG_STEAM_ERROR("Exact Steam module selection failed: {}",ex.what());return nullptr;}
    }();
    return selected;
}
}
