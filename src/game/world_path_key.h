#pragma once
#include <cstdint>
#include <string_view>

namespace mt2mp::game::world_path_key {
inline bool valid(std::string_view key){
    const bool saved=key.starts_with("snapshot:");
    const size_t prefix=saved?9:7;
    if((!saved&&!key.starts_with("action:"))||key.size()>128||key.size()<prefix+43)return false;
    for(size_t i=0;i<36;++i){const char c=key[prefix+i];
        if(i==8||i==13||i==18||i==23){if(c!='-')return false;}
        else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    }
    if(key.substr(prefix+36,6)!=":path:")return false;
    size_t offset=prefix+42;
    const size_t fields=saved?3:2;
    for(size_t field=0;field<fields;++field){
        const auto end=key.find(':',offset);const auto count=(end==std::string_view::npos?key.size():end)-offset;
        if(!count||count>10||(count>1&&key[offset]=='0')||(field+1==fields)!=(end==std::string_view::npos))return false;
        uint64_t value=0;
        for(size_t i=0;i<count;++i){const char c=key[offset+i];if(c<'0'||c>'9')return false;value=value*10+uint64_t(c-'0');}
        const uint64_t limit=(saved&&field==0)?uint64_t(INT32_MAX):(field+2==fields?10:999999);
        if(value>limit)return false;
        offset+=count+1;
    }
    return true;
}
}
