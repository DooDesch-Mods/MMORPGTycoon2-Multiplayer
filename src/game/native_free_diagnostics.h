#pragma once
#include "vendor/nlohmann/json.hpp"
#include <string>

namespace mt2mp::game::native_free_diagnostics {
nlohmann::json status();
bool probe(const nlohmann::json&,nlohmann::json&,std::string&);
void drain_logs();
}
