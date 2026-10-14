// steam_api64.dll PROXY
// 1095 exports via ASM JMP stubs
// SteamAPI_RunCallbacks is exported directly from this file (not via ASM stub)
// so we can inject P2P polling after each callback pump.

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <atomic>

typedef bool (*InitFn)(HMODULE);
typedef void (*ShutdownFn)();
typedef void (*RunCallbacksFn)();
typedef void (*TickFn)();

static HMODULE g_original = nullptr;
static HMODULE g_mod_dll = nullptr;
static RunCallbacksFn g_orig_RunCallbacks = nullptr;
static ShutdownFn g_orig_Shutdown = nullptr;
static std::atomic<TickFn> g_mod_tick{nullptr};
static HANDLE g_init_thread = nullptr;

extern bool fill_steam_pointers(HMODULE original);
extern bool install_local_bootstrap();

static void proxy_error(const char* operation, DWORD cause) {
    FILE* f = nullptr;
    if (fopen_s(&f, "mt2mp_proxy_error.log", "a") == 0 && f) {
        fprintf(f, "error: %s Win32=%lu\n", operation, cause);
        fclose(f);
    }
    OutputDebugStringA(operation);
}

// ── SteamAPI_RunCallbacks: our implementation ────────────────────
// The .def file exports this symbol. Since we removed it from the ASM stubs,
// the linker uses THIS function for the export.
static std::atomic<bool> g_shutting_down{false};

extern "C" __declspec(dllexport) void SteamAPI_RunCallbacks() {
    if (g_orig_RunCallbacks) g_orig_RunCallbacks();
    const auto tick=g_mod_tick.load();
    if (tick && !g_shutting_down.load()) tick();
}

extern "C" __declspec(dllexport) void SteamAPI_Shutdown() {
    if (!g_shutting_down.exchange(true)) {
        g_mod_tick=nullptr;
        bool initialized=true;
        if (g_init_thread) {
            const DWORD waited=WaitForSingleObject(g_init_thread,30000);
            initialized=waited==WAIT_OBJECT_0;
            if (!initialized) proxy_error("Mod initialization did not finish before Steam shutdown",waited==WAIT_FAILED?GetLastError():ERROR_TIMEOUT);
            else { if (!CloseHandle(g_init_thread)) proxy_error("Close mod initialization handle failed",GetLastError()); g_init_thread=nullptr; }
        }
        if (initialized && g_mod_dll) {
            auto shutdown=reinterpret_cast<ShutdownFn>(GetProcAddress(g_mod_dll,"MT2MP_Shutdown"));
            if (shutdown) shutdown();
            else proxy_error("MT2MP_Shutdown export missing",GetLastError());
        }
    }
    if (g_orig_Shutdown) g_orig_Shutdown();
}

// ── Init thread ──────────────────────────────────────────────────
static DWORD WINAPI mod_init_thread(LPVOID param) {
    Sleep(1000);
    if (!g_mod_dll || g_shutting_down.load()) return 1;

    auto init = reinterpret_cast<InitFn>(GetProcAddress(g_mod_dll, "MT2MP_Init"));
    if (!init || !init(g_mod_dll)) {
        proxy_error("MT2MP_Init failed; multiplayer inactive", GetLastError());
        return 2;
    }

    g_mod_tick = reinterpret_cast<TickFn>(GetProcAddress(g_mod_dll, "MT2MP_Tick"));

    // Log that tick is connected
    if (g_mod_tick.load()) {
        FILE* f = fopen("mt2mp_proxy_debug.log", "w");
        if (f) { fprintf(f, "MT2MP_Tick connected\n"); fclose(f); }
    }

    return 0;
}

// ── DLL Entry Point ──────────────────────────────────────────────
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);
        if (!install_local_bootstrap()) {
            proxy_error("Local test isolation failed; game startup refused", GetLastError());
            return FALSE;
        }

        char path[MAX_PATH];
        GetModuleFileNameA(hModule, path, MAX_PATH);
        char* last_slash = strrchr(path, '\\');
        if (last_slash) *(last_slash + 1) = '\0';

        // 1. Load original steam DLL and fill ASM stub pointers
        char orig_path[MAX_PATH];
        strcpy_s(orig_path, path);
        strcat_s(orig_path, "steam_api64_original.dll");
        g_original = LoadLibraryA(orig_path);
        if (g_original) {
            if (!fill_steam_pointers(g_original)) {
                proxy_error("Original Steam API has missing proxy exports", ERROR_PROC_NOT_FOUND);
                return FALSE;
            }
            // Get the original RunCallbacks for our implementation above
            g_orig_RunCallbacks = reinterpret_cast<RunCallbacksFn>(
                GetProcAddress(g_original, "SteamAPI_RunCallbacks"));
            if (!g_orig_RunCallbacks) { proxy_error("SteamAPI_RunCallbacks export missing", ERROR_PROC_NOT_FOUND); return FALSE; }
            g_orig_Shutdown=reinterpret_cast<ShutdownFn>(GetProcAddress(g_original,"SteamAPI_Shutdown"));
            if (!g_orig_Shutdown) { proxy_error("SteamAPI_Shutdown export missing",ERROR_PROC_NOT_FOUND); return FALSE; }
        }
        else {
            proxy_error("Load original Steam API failed", GetLastError());
            return FALSE;
        }

        // 2. Load mod DLL
        char mod_path[MAX_PATH];
        strcpy_s(mod_path, path);
        strcat_s(mod_path, "MT2_Multiplayer.dll");
        g_mod_dll = LoadLibraryA(mod_path);
        if (!g_mod_dll) {
            proxy_error("Load multiplayer DLL failed", GetLastError());
            return FALSE;
        }

        // 3. Init in thread (Steam needs to init first)
        g_init_thread = CreateThread(nullptr, 0, mod_init_thread, nullptr, 0, nullptr);
        if (!g_init_thread) {
            proxy_error("Create mod initialization thread failed", GetLastError());
            return FALSE;
        }
        break;
    }
    case DLL_PROCESS_DETACH:
        g_shutting_down = true; // Stop ticking immediately
        g_mod_tick = nullptr;
        // Worker joins and shared-state cleanup must occur before loader-lock detach.
        break;
    }
    return TRUE;
}
