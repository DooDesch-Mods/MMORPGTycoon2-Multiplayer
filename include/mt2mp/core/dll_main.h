#pragma once

#include <windows.h>

namespace mt2mp {

// Called from DllMain or proxy on attach
bool initialize(HMODULE module_handle);

// Called on detach
void shutdown();

// Returns the module handle of our DLL
HMODULE get_module_handle();

} // namespace mt2mp
