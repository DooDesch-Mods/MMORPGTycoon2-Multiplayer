#include <cstdint>
namespace {const char* path=nullptr;uint64_t calls=0;}
extern "C" __declspec(dllexport) const char* PHYSFS_getWriteDir(){++calls;return path;}
extern "C" __declspec(dllexport) void fixture_set_path(const char* value){path=value;}
extern "C" __declspec(dllexport) uint64_t fixture_get_calls(){return calls;}
