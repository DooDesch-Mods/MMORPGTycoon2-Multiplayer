#pragma once
#include "region_saved_identity.h"
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::game::snapshot_construction {
// Called only by the held snapshot preparation witness, never a tick observer.
bool read(uintptr_t image,uintptr_t map,const region_saved_identity::Index& saved,
          nlohmann::json& out,std::string& error);
}
