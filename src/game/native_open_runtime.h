#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include "vendor/nlohmann/json.hpp"
#include "native_open_witness.h"

namespace mt2mp::game::native_open_runtime {
bool initialize(const std::filesystem::path& root,uintptr_t image,std::string& error);
bool hooks_enabled(std::string& error);
bool prepare(const std::filesystem::path& root,const std::string& uuid,const std::string& sha,
             uint64_t bytes,uint64_t& generation,std::string& error,const std::string& world_name="MPTestSession");
bool before_request(uint64_t generation,std::string& error);
bool retire_unstarted(uint64_t generation,std::string& error);
bool finish(uint64_t generation,bool& pending,std::string& error);
bool read_current_world(building_mixed_identity::opened_file::CurrentWorld&,std::string& error);
nlohmann::json status_json();
void shutdown(const std::string& cause);
}
