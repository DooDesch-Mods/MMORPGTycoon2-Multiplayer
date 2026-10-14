#include "mt2mp/hooks/hook_physfs.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/net/steam_matchmaking.h"
#include "mt2mp/net/steam_networking.h"
#include "mt2mp/net/save_transfer.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/config.h"

#include <unordered_map>
#include <mutex>
#include <cstring>

// PhysFS types (from physfs.h - stable C ABI)
typedef struct PHYSFS_File PHYSFS_File;
typedef int64_t PHYSFS_sint64;
typedef uint64_t PHYSFS_uint64;

// Original function pointers
using OpenWriteFn = PHYSFS_File*(*)(const char*);
using OpenReadFn  = PHYSFS_File*(*)(const char*);
using WriteBytesFn = PHYSFS_sint64(*)(PHYSFS_File*, const void*, PHYSFS_uint64);
using ReadBytesFn  = PHYSFS_sint64(*)(PHYSFS_File*, void*, PHYSFS_uint64);
using CloseFn      = int(*)(PHYSFS_File*);

namespace mt2mp::hooks {

static OpenWriteFn  orig_openWrite = nullptr;
static OpenReadFn   orig_openRead = nullptr;
static WriteBytesFn orig_writeBytes = nullptr;
static ReadBytesFn  orig_readBytes = nullptr;
static CloseFn      orig_close = nullptr;

static FileOpenCallback g_on_open;
static FileCloseCallback g_on_close;
static FileWriteCallback g_on_write;

// Track open file handles
struct TrackedFile {
    std::string path;
    bool is_write;
    bool is_vrz; // only accumulate data for save files
    std::vector<uint8_t> data;
};

static std::mutex g_mutex;
static std::unordered_map<PHYSFS_File*, TrackedFile> g_files;

static bool ends_with(const char* str, const char* suffix) {
    size_t slen = strlen(str);
    size_t sufflen = strlen(suffix);
    if (slen < sufflen) return false;
    return strcmp(str + slen - sufflen, suffix) == 0;
}

// --- Detours ---

static std::string g_current_write_path;

const std::string& current_write_path() { return g_current_write_path; }

static PHYSFS_File* detour_openWrite(const char* filename) {
    PHYSFS_File* handle = orig_openWrite(filename);
    if (handle && filename) {
        g_current_write_path = filename;
        bool is_vrz = ends_with(filename, ".vrz");
        LOG_FS_INFO("PhysFS::openWrite \"{}\" -> 0x{:X} {}", filename,
            reinterpret_cast<uintptr_t>(handle), is_vrz ? "[SAVE]" : "");

        std::lock_guard lock(g_mutex);
        g_files[handle] = {filename, true, is_vrz, {}};

        if (g_on_open) g_on_open(filename, true);
    }
    return handle;
}

static PHYSFS_File* detour_openRead(const char* filename) {
    // Remap Discord icon image to a multiplayer-looking icon
    const char* resolved = filename;
    if (filename && strcmp(filename, "desktop/discord.png") == 0) {
        resolved = "desktop/gameicons/crossedswords.png";
        LOG_FS_INFO("Remap: discord.png -> crossedswords.png");
    }
    PHYSFS_File* handle = orig_openRead(resolved);
    if (handle && filename) {
        bool is_vrz = ends_with(filename, ".vrz");
        if (is_vrz) {
            LOG_FS_INFO("PhysFS::openRead \"{}\" -> 0x{:X} [LOAD]", filename,
                reinterpret_cast<uintptr_t>(handle));

            // Detect save game load and publish to lobby
            // Pattern: "user/save/<GameName>/autosave.vrz"
            const char* save_prefix = "user/save/";
            if (strncmp(filename, save_prefix, 10) == 0 && strstr(filename, "autosave.vrz")) {
                // Extract game name: between "user/save/" and "/autosave.vrz"
                std::string path(filename);
                size_t start = 10; // after "user/save/"
                size_t end = path.find("/autosave.vrz");
                if (end != std::string::npos && end > start) {
                    std::string game_name = path.substr(start, end - start);

                    // Publish to lobby if we're the host
                    auto& mm = mt2mp::net::SteamMatchmaking::instance();
                    if (mm.in_lobby() && mm.is_host()) {
                        mm.set_lobby_data("game_name", game_name.c_str());
                        mm.set_lobby_data("game_state", "loading");
                        LOG_FS_INFO("Host loading save '{}' - published to lobby", game_name);
                    }
                }
            }
        } else {
            LOG_FS_DEBUG("PhysFS::openRead \"{}\"", filename);
        }

        std::lock_guard lock(g_mutex);
        g_files[handle] = {filename, false, is_vrz, {}};

        if (g_on_open) g_on_open(filename, false);
    }
    return handle;
}

static PHYSFS_sint64 detour_writeBytes(PHYSFS_File* handle, const void* buffer, PHYSFS_uint64 len) {
    PHYSFS_sint64 result = orig_writeBytes(handle, buffer, len);

    if (result > 0) {
        std::lock_guard lock(g_mutex);
        auto it = g_files.find(handle);
        if (it != g_files.end()) {
            auto& tf = it->second;
            if (tf.is_vrz) {
                // Accumulate save data
                const auto* bytes = static_cast<const uint8_t*>(buffer);
                tf.data.insert(tf.data.end(), bytes, bytes + result);
            }
            if (g_on_write && tf.is_vrz) {
                g_on_write(tf.path.c_str(), static_cast<const uint8_t*>(buffer),
                    static_cast<uint64_t>(result));
            }
        }
    }

    return result;
}

static PHYSFS_sint64 detour_readBytes(PHYSFS_File* handle, void* buffer, PHYSFS_uint64 len) {
    PHYSFS_sint64 result = orig_readBytes(handle, buffer, len);

    if (result > 0) {
        std::lock_guard lock(g_mutex);
        auto it = g_files.find(handle);
        if (it != g_files.end() && it->second.is_vrz) {
            const auto* bytes = static_cast<const uint8_t*>(buffer);
            it->second.data.insert(it->second.data.end(), bytes, bytes + result);
        }
    }

    return result;
}

static int detour_close(PHYSFS_File* handle) {
    TrackedFile tf;
    bool was_tracked = false;

    {
        std::lock_guard lock(g_mutex);
        auto it = g_files.find(handle);
        if (it != g_files.end()) {
            tf = std::move(it->second);
            g_files.erase(it);
            was_tracked = true;
        }
    }

    int result = orig_close(handle);

    if (was_tracked && tf.is_vrz) {
        LOG_FS_INFO("PhysFS::close \"{}\" ({} bytes) [{}]",
            tf.path, tf.data.size(), tf.is_write ? "SAVE COMPLETE" : "LOAD COMPLETE");

        // When host WRITES an autosave (ongoing gameplay), stream it to joiners
        if (tf.is_write && tf.path.find("autosave.vrz") != std::string::npos) {
            auto& mm = mt2mp::net::SteamMatchmaking::instance();
            if (mm.in_lobby() && mm.is_host() && !tf.data.empty()) {
                // Don't re-send if a transfer is already in progress
                if (!mt2mp::net::SaveTransfer::instance().is_sending()) {
                    std::string path(tf.path);
                    size_t start = path.find("save/");
                    size_t end = path.find("/autosave.vrz");
                    if (start != std::string::npos && end != std::string::npos) {
                        std::string game_name = path.substr(start + 5, end - start - 5);
                        int members = mt2mp::net::SteamNetworking::instance().get_lobby_member_count(mm.current_lobby_id());
                        if (members > 1) {
                            mt2mp::net::SaveTransfer::instance().send_save(
                                mm.current_lobby_id(), game_name,
                                tf.data.data(), static_cast<uint32_t>(tf.data.size()));
                            LOG_FS_INFO("Auto-streaming autosave '{}' ({} bytes) to {} members",
                                game_name, tf.data.size(), members - 1);
                        }
                    }
                }
            }
        }

        // When save game load completes: update lobby + send save to joiners
        if (!tf.is_write && tf.path.find("autosave.vrz") != std::string::npos) {
            auto& mm = mt2mp::net::SteamMatchmaking::instance();
            if (mm.in_lobby() && mm.is_host()) {
                mm.set_lobby_data("game_state", "ingame");
                LOG_FS_INFO("Host entered game - lobby state set to 'ingame'");

                // Extract game name from path
                std::string path(tf.path);
                size_t start = path.find("save/");
                size_t end = path.find("/autosave.vrz");
                if (start != std::string::npos && end != std::string::npos) {
                    std::string game_name = path.substr(start + 5, end - start - 5);

                    // Send the compressed .vrz data to all lobby members
                    if (!tf.data.empty()) {
                        mt2mp::net::SaveTransfer::instance().send_save(
                            mm.current_lobby_id(), game_name,
                            tf.data.data(), static_cast<uint32_t>(tf.data.size()));
                        LOG_FS_INFO("Auto-sending save '{}' ({} bytes) to lobby",
                            game_name, tf.data.size());
                    }
                }
            }
        }

        if (g_on_close) {
            g_on_close(tf.path.c_str(), tf.is_write, tf.data);
        }
    }

    return result;
}

// --- Public API ---

void set_file_open_callback(FileOpenCallback cb) { g_on_open = std::move(cb); }
void set_file_close_callback(FileCloseCallback cb) { g_on_close = std::move(cb); }
void set_file_write_callback(FileWriteCallback cb) { g_on_write = std::move(cb); }

bool install_physfs_hooks() {
    bool ok = true;
    ok &= create_hook_by_import("libphysfs.dll", "PHYSFS_openWrite",
        reinterpret_cast<void*>(&detour_openWrite), reinterpret_cast<void**>(&orig_openWrite));
    ok &= create_hook_by_import("libphysfs.dll", "PHYSFS_openRead",
        reinterpret_cast<void*>(&detour_openRead), reinterpret_cast<void**>(&orig_openRead));
    ok &= create_hook_by_import("libphysfs.dll", "PHYSFS_writeBytes",
        reinterpret_cast<void*>(&detour_writeBytes), reinterpret_cast<void**>(&orig_writeBytes));
    ok &= create_hook_by_import("libphysfs.dll", "PHYSFS_readBytes",
        reinterpret_cast<void*>(&detour_readBytes), reinterpret_cast<void**>(&orig_readBytes));
    ok &= create_hook_by_import("libphysfs.dll", "PHYSFS_close",
        reinterpret_cast<void*>(&detour_close), reinterpret_cast<void**>(&orig_close));
    return ok;
}

} // namespace mt2mp::hooks
