#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace mt2mp::hooks {

// Callback types for edit events
using EditBeginCallback = std::function<void(const char* reason)>;
using EditEndCallback = std::function<void()>;

// Set callbacks for edit events (called by sync layer)
void set_edit_begin_callback(EditBeginCallback cb);
void set_edit_end_callback(EditEndCallback cb);

// Query state
bool is_edit_in_progress();
const char* current_edit_reason();

// Install hooks (called by hook_engine)
bool install_edit_manager_hooks();

} // namespace mt2mp::hooks
