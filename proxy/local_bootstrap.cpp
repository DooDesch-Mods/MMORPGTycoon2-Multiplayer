#include <windows.h>
#include <cstdio>
#include <cstring>

namespace {
char test_path[32768]{};
using Duplicate = char*(*)(const char*);
using SetWriteDir = int(*)(const char*);
Duplicate duplicate = nullptr;
SetWriteDir original_write_dir = nullptr;

void record(const char* operation, const char* detail, unsigned long code = 0) {
    FILE* file = nullptr;
    if (fopen_s(&file, "mt2mp_bootstrap.log", "a") == 0 && file) {
        fprintf(file, "%s: %s (code=%lu)\n", operation, detail, code);
        fclose(file);
    }
}
char* test_pref_path(const char*, const char*) {
    record("info", "SDL_GetPrefPath redirected to test root");
    return duplicate(test_path);
}
int test_write_dir(const char* path) {
    const size_t length = strlen(test_path);
    if (!path || _strnicmp(path, test_path, length) != 0 || strstr(path, "..")) {
        record("error", "PHYSFS_setWriteDir outside test root refused");
        return 0;
    }
    const int result = original_write_dir(path);
    record(result ? "info" : "error", path);
    return result;
}
bool replace_import(const char* module, const char* function, void* replacement, void** original) {
    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    const auto& entry = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + entry.VirtualAddress);
    for (; descriptor->Name; ++descriptor) {
        if (_stricmp(reinterpret_cast<char*>(base + descriptor->Name), module)) continue;
        if (!descriptor->OriginalFirstThunk) return false;
        auto* name = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor->OriginalFirstThunk);
        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor->FirstThunk);
        for (; name->u1.AddressOfData; ++name, ++thunk) {
            if (IMAGE_SNAP_BY_ORDINAL64(name->u1.Ordinal)) continue;
            auto* imported = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + name->u1.AddressOfData);
            if (strcmp(reinterpret_cast<char*>(imported->Name), function)) continue;
            DWORD protection;
            if (!VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &protection)) {
                record("error", "VirtualProtect import failed", GetLastError());
                return false;
            }
            *original = reinterpret_cast<void*>(thunk->u1.Function);
            thunk->u1.Function = reinterpret_cast<ULONGLONG>(replacement);
            DWORD ignored;
            if (!VirtualProtect(&thunk->u1.Function, sizeof(void*), protection, &ignored)) {
                record("error", "Restore import protection failed", GetLastError());
                return false;
            }
            return true;
        }
    }
    record("error", function);
    return false;
}
}

bool install_local_bootstrap() {
    const DWORD length = GetEnvironmentVariableA("MT2MP_TEST_ROOT", test_path, sizeof(test_path));
    if (!length) return true;
    if (length >= sizeof(test_path) - 2 || !((test_path[1] == ':' && test_path[2] == '\\') || test_path[0] == '\\')) {
        record("error", "Invalid absolute MT2MP_TEST_ROOT");
        return false;
    }
    for (char* current = test_path; *current; ++current) if (*current == '\\') *current = '/';
    if (test_path[length - 1] != '/') strcat_s(test_path, "/");
    duplicate = reinterpret_cast<Duplicate>(GetProcAddress(GetModuleHandleA("SDL3.dll"), "SDL_strdup"));
    void* original_pref = nullptr;
    if (!duplicate || !replace_import("SDL3.dll", "SDL_GetPrefPath", reinterpret_cast<void*>(&test_pref_path), &original_pref)) return false;
    if (!replace_import("libphysfs.dll", "PHYSFS_setWriteDir", reinterpret_cast<void*>(&test_write_dir), reinterpret_cast<void**>(&original_write_dir))) return false;
    record("info", "Local data isolation installed before engine initialization");
    return true;
}
