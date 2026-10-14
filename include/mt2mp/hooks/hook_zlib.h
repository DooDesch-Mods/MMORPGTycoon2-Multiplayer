#pragma once

#include <functional>
#include <vector>
#include <cstdint>

namespace mt2mp::hooks {

// Called with uncompressed data chunks during deflate (save) and inflate (load)
using DeflateDataCallback = std::function<void(const uint8_t* data, size_t len)>;
using InflateDataCallback = std::function<void(const uint8_t* data, size_t len)>;
using StreamCompleteCallback = std::function<void(bool is_deflate, const std::vector<uint8_t>& full_data)>;

void set_deflate_callback(DeflateDataCallback cb);
void set_inflate_callback(InflateDataCallback cb);
void set_stream_complete_callback(StreamCompleteCallback cb);

bool install_zlib_hooks();

// Call from main thread (MT2MP_Tick) to process queued deflate data.
// Worker threads queue data; this flushes it to the callback on the main thread.
void flush_pending_deflate_data();

} // namespace mt2mp::hooks
