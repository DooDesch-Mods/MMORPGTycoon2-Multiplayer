#pragma once

#include <functional>
#include <string>

namespace mt2mp::hooks {

using SaveCallback = std::function<void(const char* path)>;
using LoadCallback = std::function<void(const char* path)>;

void set_save_callback(SaveCallback cb);
void set_load_begin_callback(LoadCallback cb);
void set_load_end_callback(LoadCallback cb);

bool install_save_hooks();

} // namespace mt2mp::hooks
