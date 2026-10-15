#pragma once
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <string>

namespace mt2mp::game::tutorial_observer {
bool read(nlohmann::json& result,std::string& error);
namespace detail {
struct Origin {
    uintptr_t game=0,state=0,world=0,map=0;
    uint64_t epoch=0,generation=0;
    std::string snapshot;
    bool operator==(const Origin&)const=default;
};
struct Port {
    virtual ~Port()=default;
    virtual bool current(Origin&,std::string&)=0;
    virtual bool image(uintptr_t&,std::string&)=0;
    virtual bool copy(uintptr_t,void*,size_t,std::string&)=0;
    virtual bool owned_sequence_entry(uintptr_t,std::string& error){error="tutorial Sequence Update code guard differs";return false;}
};
bool observe(Port&,nlohmann::json&,std::string&);
}
}
