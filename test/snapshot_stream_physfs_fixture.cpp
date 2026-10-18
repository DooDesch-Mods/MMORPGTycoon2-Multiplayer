#include <windows.h>
extern "C" __declspec(dllexport) const char* PHYSFS_getWriteDir() {
    static char path[32768];
    const auto length=GetEnvironmentVariableA("SNAPSHOT_FIXTURE_WRITE_ROOT",path,sizeof(path));
    return length&&length<sizeof(path)?path:nullptr;
}
