#pragma once
#include <cstdint>
#include <string_view>

namespace mt2mp::core {
inline bool host_periodic_save(uint32_t kind,std::string_view filename,bool option){
    return kind==3&&filename=="periodic"&&!option;
}
}
