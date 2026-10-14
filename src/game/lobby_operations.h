#pragma once
#include "vendor/nlohmann/json.hpp"
#include <string>
namespace mt2mp::game::lobby_operations {
// The caller and existing native executor share the bound game thread.
bool submit(const std::string& operation,const nlohmann::json& args,std::string& id,std::string& error);
bool take_response(const std::string& id,nlohmann::json& response);
bool take_request(nlohmann::json& request);
bool receive(const nlohmann::json& response);
void configure(const std::string& run);
bool pending();
bool available();
}
