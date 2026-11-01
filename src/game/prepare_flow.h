#pragma once
#include "vendor/nlohmann/json.hpp"
#include <string>
namespace mt2mp::game::lobby_prepare {
bool enable_captures(std::string& error);
using OrderedSelector=bool(*)(const std::string& selected_file,std::string& error);
void configure_ordered_entry(OrderedSelector);
bool admit_ordered_client(std::string& error);
bool prepare(const std::string& selected_file,std::string& error);
bool prepare_current(std::string& error);
bool start(std::string& error);
bool leaving();
bool tick(std::string& error);
nlohmann::json status();
bool status_is_handshake(const nlohmann::json& value);
}

namespace mt2mp::game::lobby_prepare { bool leave(std::string&); }

namespace mt2mp::game::lobby_prepare {
bool prepare_snapshot(const std::string& id,std::string& error,const std::string& world_name={});
bool reset_after_world_reload(std::string& error);
}
