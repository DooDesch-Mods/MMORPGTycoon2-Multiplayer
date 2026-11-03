#pragma once
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace mt2mp::framework_version {
inline bool fixed_resource_at_least(const void* resource,size_t bytes,std::string& error){
    constexpr char16_t key[]=u"VS_VERSION_INFO";
    constexpr size_t fixed_offset=(6+sizeof(key)+3)&~size_t(3);
    constexpr size_t fixed_size=52;
    const auto fail=[&](const char* reason){error=std::string("Framework 0.5.7 or newer required: ")+reason;return false;};
    if(!resource||bytes<fixed_offset+fixed_size)return fail("missing or truncated loaded version resource");
    const auto* raw=static_cast<const unsigned char*>(resource);
    uint16_t length=0,value_length=0,type=0;
    std::memcpy(&length,raw,2);std::memcpy(&value_length,raw+2,2);std::memcpy(&type,raw+4,2);
    if(length<fixed_offset+fixed_size||length>bytes||value_length!=fixed_size||type!=0||std::memcmp(raw+6,key,sizeof(key))!=0)
        return fail("malformed loaded version resource");
    std::array<uint32_t,6> fields{};std::memcpy(fields.data(),raw+fixed_offset,sizeof(fields));
    if(fields[0]!=0xfeef04bdu||fields[1]!=0x00010000u)return fail("invalid fixed version information");
    const auto parts=[](uint32_t high,uint32_t low){return std::array<uint16_t,4>{uint16_t(high>>16),uint16_t(high),uint16_t(low>>16),uint16_t(low)};};
    constexpr std::array<uint16_t,4> minimum{0,5,7,0};
    if(parts(fields[2],fields[3])<minimum||parts(fields[4],fields[5])<minimum)return fail("loaded Framework version is too old");
    error.clear();return true;
}
inline bool loaded_module_at_least(HMODULE module,std::string& error){
    if(!module){error="Framework 0.5.7 or newer required: loaded module is absent";return false;}
    const auto resource=FindResourceW(module,MAKEINTRESOURCEW(1),MAKEINTRESOURCEW(16));
    if(!resource){error="Framework 0.5.7 or newer required: loaded version resource is absent";return false;}
    const auto bytes=SizeofResource(module,resource);const auto loaded=LoadResource(module,resource);
    const auto* data=loaded?LockResource(loaded):nullptr;
    return fixed_resource_at_least(data,bytes,error);
}
}
