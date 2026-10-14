#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::game::subscriber_lifecycle_capture {
// Called by the existing Mainthread diagnostic executor. runtime_root is its
// private configured directory, never a path accepted from the MCP payload.
bool capture(const std::filesystem::path& runtime_root, int32_t subscriber_id,
             const std::string& native_origin_uuid, nlohmann::json& result, std::string& cause);
bool capture_frame(int32_t,const std::string& native_origin,nlohmann::json& graph,nlohmann::json& receipt,std::string&);
}
