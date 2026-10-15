#pragma once
#include "native_open_witness.h"
#include "vendor/nlohmann/json.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace mt2mp::game::tutorial_development {
bool read(nlohmann::json& result,std::string& error);
namespace detail {
using Origin=building_mixed_identity::opened_file::CurrentWorld;
struct Port {
    virtual ~Port()=default;
    virtual bool current(Origin&,std::string&)=0;
    virtual bool image(uintptr_t&,std::string&)=0;
    virtual bool copy(uintptr_t,void*,size_t,std::string&)=0;
};
bool observe(Port&,nlohmann::json&,std::string&);
}
}
