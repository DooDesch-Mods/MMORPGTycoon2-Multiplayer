#include "mt2mp/hooks/hook_fileio.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/core/logger.h"

#include <windows.h>
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace mt2mp::hooks {

static SaveWriteCallback g_save_write_cb;
void set_save_write_callback(SaveWriteCallback cb) { g_save_write_cb = std::move(cb); }

// Forward declaration
static void on_vrz_write_open(const std::string& path);

// ── CreateFileA Hook ────────────────────────────────────────
using CreateFileA_t = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static CreateFileA_t orig_CreateFileA = nullptr;

static HANDLE WINAPI detour_CreateFileA(
    LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurity, DWORD dwCreation, DWORD dwFlags, HANDLE hTemplate)
{
    HANDLE h = orig_CreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
                                 lpSecurity, dwCreation, dwFlags, hTemplate);

    if (h != INVALID_HANDLE_VALUE && lpFileName && (dwDesiredAccess & GENERIC_WRITE)) {
        std::string path(lpFileName);
        // Check if this is a .vrz save file
        bool is_vrz = false;
        if (path.size() > 4) {
            std::string ext = path.substr(path.size() - 4);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            is_vrz = (ext == ".vrz");
        }

        if (is_vrz) {
            LOG_INFO("[FILEIO] CreateFileA WRITE: \"{}\"", path);
            on_vrz_write_open(path);
        }
    }

    return h;
}

// ── CreateFileW Hook ────────────────────────────────────────
using CreateFileW_t = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static CreateFileW_t orig_CreateFileW = nullptr;

static HANDLE WINAPI detour_CreateFileW(
    LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurity, DWORD dwCreation, DWORD dwFlags, HANDLE hTemplate)
{
    HANDLE h = orig_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                                 lpSecurity, dwCreation, dwFlags, hTemplate);

    if (h != INVALID_HANDLE_VALUE && lpFileName && (dwDesiredAccess & GENERIC_WRITE)) {
        // Convert wide string to narrow for checking
        int len = WideCharToMultiByte(CP_UTF8, 0, lpFileName, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            std::string path(len - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, lpFileName, -1, path.data(), len, nullptr, nullptr);

            bool is_vrz = false;
            if (path.size() > 4) {
                std::string ext = path.substr(path.size() - 4);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                is_vrz = (ext == ".vrz");
            }

            if (is_vrz) {
                LOG_INFO("[FILEIO] CreateFileW WRITE: \"{}\"", path);
                on_vrz_write_open(path);
            }
        }
    }

    return h;
}

// NO CloseHandle hook - too dangerous, called for all handle types.
// Instead we track .vrz write opens and poll for completion in tick().

// ── Tick: poll for .vrz file completion ─────────────────────
// Called from MT2MP_Tick every frame.
static std::string g_last_vrz_write_path;
static FILETIME g_last_vrz_mtime = {};
static uint64_t g_last_vrz_size = 0;
static bool g_vrz_write_pending = false;
static float g_vrz_poll_timer = 0.0f;

void fileio_tick(float dt) {
    // When a .vrz was opened for writing, poll until the file is stable
    if (g_vrz_write_pending) {
        g_vrz_poll_timer += dt;
        if (g_vrz_poll_timer < 0.5f) return; // Check every 500ms
        g_vrz_poll_timer = 0.0f;

        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (GetFileAttributesExA(g_last_vrz_write_path.c_str(), GetFileExInfoStandard, &fad)) {
            FILETIME ft = fad.ftLastWriteTime;
            uint64_t sz = ((uint64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;

            if (ft.dwHighDateTime == g_last_vrz_mtime.dwHighDateTime &&
                ft.dwLowDateTime == g_last_vrz_mtime.dwLowDateTime &&
                sz == g_last_vrz_size && sz > 0) {
                // File is stable (mtime and size unchanged since last check)
                g_vrz_write_pending = false;
                LOG_INFO("[FILEIO] *** SAVE WRITE COMPLETE: \"{}\" ({} bytes) ***",
                    g_last_vrz_write_path, sz);
                if (g_save_write_cb) {
                    g_save_write_cb(g_last_vrz_write_path, (size_t)sz);
                }
            } else {
                g_last_vrz_mtime = ft;
                g_last_vrz_size = sz;
            }
        }
    }
}

static void on_vrz_write_open(const std::string& path) {
    g_last_vrz_write_path = path;
    g_last_vrz_mtime = {};
    g_last_vrz_size = 0;
    g_vrz_write_pending = true;
    g_vrz_poll_timer = 0.0f;
}

// ── Installation ────────────────────────────────────────────
bool install_fileio_hooks() {
    bool ok = true;

    ok &= create_hook_by_import("KERNEL32.dll", "CreateFileA",
        reinterpret_cast<void*>(&detour_CreateFileA),
        reinterpret_cast<void**>(&orig_CreateFileA));

    ok &= create_hook_by_import("KERNEL32.dll", "CreateFileW",
        reinterpret_cast<void*>(&detour_CreateFileW),
        reinterpret_cast<void**>(&orig_CreateFileW));

    // NO CloseHandle hook - too dangerous.

    return ok;
}

} // namespace mt2mp::hooks
