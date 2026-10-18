#pragma once
#include <windows.h>
#include <native_services.h>
#include "mt2mp/core/config.h"
#include "vendor/nlohmann/json.hpp"
#include <filesystem>
#include <map>

namespace mt2mp::framework_profile {
inline constexpr const char* plugin_id="doodesch.multiplayer";
struct Paths {
    std::filesystem::path game,profile,game_data,mod,data,runtime,diagnostics;
};
struct LocalTest {
    std::string run_id,token;
    uint64_t peer=0,session=0;
    uint16_t control_port=0;
    bool loopback=false;
};
// Paths come only from the active Framework owner's private native service.
Paths paths(const MT2FW_MT2_ServiceV1&);
LocalTest local_test(const Paths&,const nlohmann::json&);
void configure(Config&,const Paths&,const LocalTest*);
// This checks actual loaded module locations and file hashes, not launcher claims.
nlohmann::json module_receipt(const Paths&,HMODULE plugin);
nlohmann::json instance_receipt(const Paths&,const LocalTest&,const nlohmann::json& modules);
}
