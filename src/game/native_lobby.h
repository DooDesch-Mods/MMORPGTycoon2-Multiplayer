#pragma once
#include <filesystem>
#include <string>
#include <cstdint>
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::game::native_lobby {
// Install once after the exact executable resolver succeeds; tick on the bound game thread.
bool initialize(const std::filesystem::path& assets, uintptr_t image, std::string& error);
// SDL input only queues; tick applies on the bound game thread.
void request_panel_toggle();
bool tick(std::string& error);
std::string status();
bool admitted();
bool observe(nlohmann::json& result,std::string& error);
bool activate(const nlohmann::json& args,nlohmann::json& result,std::string& error);
}

namespace mt2mp::game::native_lobby {
bool initial_release_window(void* actual_mode,void** window,std::string& error);
bool begin_initial_release_fixture(void* actual_mode,std::string& error);
bool end_initial_release_fixture(uint64_t& callbacks,std::string& error);
}

#ifdef MT2MP_FRAMEWORK_PLUGIN
#include "mt2fw/plugin.h"
#include "native_ownership.h"
namespace mt2mp::game::native_lobby {
bool framework_bind(const MT2FW_Host*,const MT2FW_MT2_RoutesV1*,const MT2FW_MT2_OwnershipV1*,std::string& error);
bool framework_render(std::string& error);
bool framework_unbind(std::string& error);
}
#endif
