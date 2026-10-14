#pragma once
#include "mt2mp/game/native_engine.h"
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::net {
void initialize_software_sync();
void begin_software_sync(uint64_t epoch, const game::native::SoftwareCache& initial);
void tick_software_sync();
nlohmann::json software_sync_status();
}
