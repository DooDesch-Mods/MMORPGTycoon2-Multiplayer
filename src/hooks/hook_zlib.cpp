#include "mt2mp/hooks/hook_zlib.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/hooks/hook_physfs.h"
#include "mt2mp/core/logger.h"

#include <unordered_map>
#include <mutex>

// zlib types (from zlib.h - stable C ABI)
typedef unsigned char Bytef;
typedef unsigned int uInt;
typedef unsigned long uLong;

struct z_stream_s {
    const Bytef* next_in;
    uInt avail_in;
    uLong total_in;
    Bytef* next_out;
    uInt avail_out;
    uLong total_out;
    const char* msg;
    void* state;
    // ... more fields we don't need
};
typedef z_stream_s z_stream;

// zlib constants
#define Z_OK            0
#define Z_STREAM_END    1
#define Z_FINISH        4

// Pending deflate data queued from worker threads for main-thread processing
static std::vector<std::vector<uint8_t>> g_pending_deflate_data;

// Original function pointers
using DeflateFn     = int(*)(z_stream*, int flush);
using DeflateInitFn = int(*)(z_stream*, int level, const char* version, int stream_size);
using DeflateEndFn  = int(*)(z_stream*);
using InflateFn     = int(*)(z_stream*, int flush);
using InflateInitFn = int(*)(z_stream*, const char* version, int stream_size);
using InflateEndFn  = int(*)(z_stream*);

namespace mt2mp::hooks {

static DeflateFn     orig_deflate = nullptr;
static DeflateInitFn orig_deflateInit = nullptr;
static DeflateEndFn  orig_deflateEnd = nullptr;
static InflateFn     orig_inflate = nullptr;
static InflateInitFn orig_inflateInit = nullptr;
static InflateEndFn  orig_inflateEnd = nullptr;

static DeflateDataCallback g_on_deflate;
static InflateDataCallback g_on_inflate;
static StreamCompleteCallback g_on_complete;

// Track active streams and accumulate uncompressed data
struct StreamData {
    bool is_deflate;
    std::vector<uint8_t> uncompressed;
};

static std::mutex g_mutex;
static std::unordered_map<z_stream*, StreamData> g_streams;

// --- Detours ---

static int detour_deflateInit(z_stream* strm, int level, const char* version, int stream_size) {
    int result = orig_deflateInit(strm, level, version, stream_size);
    if (result == Z_OK && strm) {
        std::lock_guard lock(g_mutex);
        g_streams[strm] = {true, {}};
        LOG_DEBUG("zlib::deflateInit (level={})", level);
    }
    return result;
}

static int detour_deflate(z_stream* strm, int flush) {
    // Capture input (uncompressed) data BEFORE deflate consumes it
    const Bytef* input_before = strm ? strm->next_in : nullptr;
    uInt avail_before = strm ? strm->avail_in : 0;

    int result = orig_deflate(strm, flush);

    if (strm && avail_before > 0) {
        uInt consumed = avail_before - strm->avail_in;
        if (consumed > 0) {
            std::lock_guard lock(g_mutex);
            auto it = g_streams.find(strm);
            if (it != g_streams.end()) {
                it->second.uncompressed.insert(
                    it->second.uncompressed.end(),
                    input_before, input_before + consumed);
            }
            if (g_on_deflate) {
                g_on_deflate(input_before, consumed);
            }
        }
    }

    return result;
}

static int detour_deflateEnd(z_stream* strm) {
    StreamData sd;
    bool found = false;
    {
        std::lock_guard lock(g_mutex);
        auto it = g_streams.find(strm);
        if (it != g_streams.end()) {
            sd = std::move(it->second);
            g_streams.erase(it);
            found = true;
        }
    }

    int result = orig_deflateEnd(strm);

    if (found && !sd.uncompressed.empty()) {
        // Filter: only process WORLD STATE saves (*.vrz), not metadata (*.meta.vrz)
        const auto& write_path = current_write_path();
        bool is_meta = (write_path.find(".meta.vrz") != std::string::npos);
        bool is_world = !is_meta && (write_path.find(".vrz") != std::string::npos);

        LOG_INFO("zlib::deflateEnd - captured {} bytes from \"{}\" [{}]",
            sd.uncompressed.size(), write_path, is_world ? "WORLD" : (is_meta ? "META" : "OTHER"));

        if (is_world) {
            // Queue world state for main thread processing
            std::lock_guard lock(g_mutex);
            g_pending_deflate_data.push_back(std::move(sd.uncompressed));
        }
    }

    return result;
}

static int detour_inflateInit(z_stream* strm, const char* version, int stream_size) {
    int result = orig_inflateInit(strm, version, stream_size);
    if (result == Z_OK && strm) {
        std::lock_guard lock(g_mutex);
        g_streams[strm] = {false, {}};
        LOG_DEBUG("zlib::inflateInit");
    }
    return result;
}

static int detour_inflate(z_stream* strm, int flush) {
    Bytef* output_before = strm ? strm->next_out : nullptr;
    uInt avail_out_before = strm ? strm->avail_out : 0;

    int result = orig_inflate(strm, flush);

    if (strm && avail_out_before > 0) {
        uInt produced = avail_out_before - strm->avail_out;
        if (produced > 0) {
            std::lock_guard lock(g_mutex);
            auto it = g_streams.find(strm);
            if (it != g_streams.end()) {
                it->second.uncompressed.insert(
                    it->second.uncompressed.end(),
                    output_before, output_before + produced);
            }
            if (g_on_inflate) {
                g_on_inflate(output_before, produced);
            }
        }
    }

    return result;
}

static int detour_inflateEnd(z_stream* strm) {
    StreamData sd;
    bool found = false;
    {
        std::lock_guard lock(g_mutex);
        auto it = g_streams.find(strm);
        if (it != g_streams.end()) {
            sd = std::move(it->second);
            g_streams.erase(it);
            found = true;
        }
    }

    int result = orig_inflateEnd(strm);

    if (found && !sd.uncompressed.empty()) {
        LOG_INFO("zlib::inflateEnd - captured {} bytes uncompressed load data", sd.uncompressed.size());
        if (g_on_complete) g_on_complete(false, sd.uncompressed);
    }

    return result;
}

// --- Public API ---

void set_deflate_callback(DeflateDataCallback cb) { g_on_deflate = std::move(cb); }
void set_inflate_callback(InflateDataCallback cb) { g_on_inflate = std::move(cb); }
void set_stream_complete_callback(StreamCompleteCallback cb) { g_on_complete = std::move(cb); }

bool install_zlib_hooks() {
    bool ok = true;
    ok &= create_hook_by_import("zlib1.dll", "deflateInit_",
        reinterpret_cast<void*>(&detour_deflateInit), reinterpret_cast<void**>(&orig_deflateInit));
    ok &= create_hook_by_import("zlib1.dll", "deflate",
        reinterpret_cast<void*>(&detour_deflate), reinterpret_cast<void**>(&orig_deflate));
    ok &= create_hook_by_import("zlib1.dll", "deflateEnd",
        reinterpret_cast<void*>(&detour_deflateEnd), reinterpret_cast<void**>(&orig_deflateEnd));
    ok &= create_hook_by_import("zlib1.dll", "inflateInit_",
        reinterpret_cast<void*>(&detour_inflateInit), reinterpret_cast<void**>(&orig_inflateInit));
    ok &= create_hook_by_import("zlib1.dll", "inflate",
        reinterpret_cast<void*>(&detour_inflate), reinterpret_cast<void**>(&orig_inflate));
    ok &= create_hook_by_import("zlib1.dll", "inflateEnd",
        reinterpret_cast<void*>(&detour_inflateEnd), reinterpret_cast<void**>(&orig_inflateEnd));
    return ok;
}

void flush_pending_deflate_data() {
    std::vector<std::vector<uint8_t>> pending;
    {
        std::lock_guard lock(g_mutex);
        pending.swap(g_pending_deflate_data);
    }
    for (auto& data : pending) {
        LOG_INFO("[ZLIB] Processing queued deflate data ({} bytes) on main thread", data.size());
        if (g_on_complete) g_on_complete(true, data);
    }
}

} // namespace mt2mp::hooks
