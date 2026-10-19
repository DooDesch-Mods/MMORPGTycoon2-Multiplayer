#pragma once
#include <Windows.h>
#include <array>
#include <cstring>
#include <string>

namespace mt2mp::game::duplicate_history_value_calls {
template<class Memory>bool code(Memory& memory,uintptr_t image,std::string& e){
    struct Guard{uintptr_t rva;const char* prefix;};
    const Guard guards[]{
        {0x40e560,"\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x81\xec\x08"},
        {0x4028f0,"\x48\x8b\x89\x50\x02\x00\x00\x48\x85\xc9\x74\x0c\xe9\xff\xb4\xff"},
        {0x40ab60,"\x53\x48\x83\xec\x20\x48\x89\xcb\xe8\x63\x6e\x72\x00\x48\x8d\x93"},
        {0x9a3a60,"\x48\x8b\x49\x50\xe9\xd7\x05\x00\x00\x90\x90\x90\x90\x90\x90\x90"},
        {0x548b00,"\x41\x54\x55\x57\x56\x53\x48\x83\xec\x40\x45\x31\xc0\x48\x89\xcb"},
        {0x548c70,"\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x81\xec\x68"},
        {0x5491d0,"\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x81\xec\x68"},
        {0x547370,"\x57\x56\x53\x48\x83\xec\x20\x48\x89\xce\x48\x8b\x89\xf0\x01\x00"},
        {0x563f50,"\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x83\xec\x38"},
        {0xf8c370,"\x53\x48\x83\xec\x30\x48\x8b\x01\x48\x8b\x40\x30\x49\x89\xd0\x48"},
        {0xf8c3d0,"\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x83\xec\x50\x48\x8d"},
        {0xf29ab0,"\x53\x48\x83\xec\x30\x48\x8b\x01\x48\x8b\x40\x30\x49\x89\xd0\x48"},
        {0xf29b10,"\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x83\xec\x50\x48\x8d"},
        {0xf5d680,"\x53\x48\x83\xec\x30\x48\x8b\x01\x48\x8b\x40\x30\x49\x89\xd0\x48"},
        {0xf5d6e0,"\x41\x55\x41\x54\x55\x57\x56\x53\x48\x83\xec\x58\x48\x8d\x2d\x6d"},
        {0x11b2b00,"\x55\x57\x56\x53\x48\x83\xec\x28\x48\x8b\x79\x08\x48\x8b\x19\x48"},
        {0x11bd1e0,"\x48\x8b\x01\x48\x83\xc1\x10\x48\x39\xc8\x74\x0c\x48\x89\xc1\xe9"},
    };
    for(const auto& guard:guards){std::array<unsigned char,16> actual{};
        if(!memory.read(image+guard.rva,actual.data(),actual.size(),e))return false;
        if(std::memcmp(actual.data(),guard.prefix,actual.size())){
            e="Duplicate Building owned-value original helper differs RVA="+std::to_string(guard.rva);return false;}}
    return true;
}
inline uint32_t get_fluff(uintptr_t image,uintptr_t building,void* value){
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x40e560)(value,reinterpret_cast<const void*>(building));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t destroy_fluff(uintptr_t image,void* value,bool arguments){
    __try{reinterpret_cast<void(*)(void*)>(image+(arguments?0x11b2b00:0x11bd1e0))(
        arguments?static_cast<unsigned char*>(value)+0x20:value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t theme(uintptr_t image,uintptr_t building,uintptr_t& out){
    __try{out=reinterpret_cast<uintptr_t(*)(const void*)>(image+0x40ab60)(reinterpret_cast<const void*>(building));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
inline uint32_t model(uintptr_t image,uintptr_t building,uintptr_t& out){
    __try{const auto instance=reinterpret_cast<uintptr_t(*)(const void*,int32_t)>(image+0x4028f0)(reinterpret_cast<const void*>(building),0);
        if(!instance)return 0xe0010001;
        out=reinterpret_cast<uintptr_t(*)(const void*)>(image+0x9a3a60)(reinterpret_cast<const void*>(instance));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
