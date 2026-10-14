#include "mt2mp/core/config.h"
#include <windows.h>
#include <shlobj.h>

namespace mt2mp {

Config& Config::instance() {
    static Config cfg;
    return cfg;
}

} // namespace mt2mp
