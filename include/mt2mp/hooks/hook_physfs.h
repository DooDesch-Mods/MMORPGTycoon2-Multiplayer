#pragma once

#include <functional>
#include <string>
#include <vector>
#include <cstdint>

namespace mt2mp::hooks {

using FileOpenCallback = std::function<void(const char* path, bool is_write)>;
using FileCloseCallback = std::function<void(const char* path, bool is_write, const std::vector<uint8_t>& data)>;
using FileWriteCallback = std::function<void(const char* path, const uint8_t* buf, uint64_t len)>;

void set_file_open_callback(FileOpenCallback cb);
void set_file_close_callback(FileCloseCallback cb);
void set_file_write_callback(FileWriteCallback cb);

bool install_physfs_hooks();

// Returns the filename of the most recent PHYSFS_openWrite call.
// Used by zlib hooks to determine if current deflate is for a world save vs metadata.
const std::string& current_write_path();

} // namespace mt2mp::hooks
