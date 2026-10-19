#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::action_session {struct Context;}
namespace mt2mp::subscriber_lifecycle::first_append {struct PartyReferences;}

namespace mt2mp::game::subscriber_lifecycle_capture {
// Called by the existing Mainthread diagnostic executor. runtime_root is its
// private configured directory, never a path accepted from the MCP payload.
bool capture(const std::filesystem::path& runtime_root, int32_t subscriber_id,
             const std::string& native_origin_uuid, nlohmann::json& result, std::string& cause);
bool capture_frame(int32_t,const std::string& native_origin,const action_session::Context& expected,nlohmann::json& graph,nlohmann::json& receipt,std::string&);
bool capture_frame(int32_t,const std::string& native_origin,const action_session::Context& expected,
                   subscriber_lifecycle::first_append::PartyReferences&,nlohmann::json& graph,nlohmann::json& receipt,std::string&);
}
