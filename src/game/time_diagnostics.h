#pragma once
#include "shared_time.h"

namespace mt2mp::game::time_diagnostics {
bool read(shared_time::Presentation& state, std::string& error);
}
