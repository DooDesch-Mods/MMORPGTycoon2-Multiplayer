#pragma once

#include <functional>
#include <string>

namespace mt2mp::hooks {

// Callback when a .vrz save file is written (closed after write).
// Parameters: full file path, file size in bytes.
using SaveWriteCallback = std::function<void(const std::string& path, size_t size)>;

void set_save_write_callback(SaveWriteCallback cb);

bool install_fileio_hooks();

// Call every frame from MT2MP_Tick to poll for .vrz write completion
void fileio_tick(float dt);

} // namespace mt2mp::hooks
