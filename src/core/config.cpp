#include "mt2mp/core/config.h"
#include <windows.h>
#include <shlobj.h>

namespace mt2mp {

#ifndef MT2MP_BUILD_VERSION
#define MT2MP_BUILD_VERSION "0.1.0"
#endif
const char* const MOD_VERSION = MT2MP_BUILD_VERSION;

Config& Config::instance() {
    static Config cfg;
    return cfg;
}

} // namespace mt2mp
