#pragma once
#include <filesystem>
#include <string>
#include <cstdint>
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::game::native_lobby {
// Install once after the exact executable resolver succeeds; tick on the bound game thread.
bool initialize(const std::filesystem::path& assets, uintptr_t image, std::string& error);
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
