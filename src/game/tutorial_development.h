#pragma once
#include "native_open_witness.h"
#include "tutorial_replication/startingpoint.h"
#include <array>
#include "vendor/nlohmann/json.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace mt2mp::game::tutorial_development {
struct StartingpointSnapshot {
    building_mixed_identity::opened_file::CurrentWorld origin;
    uintptr_t image=0,lounge=0,project=0,tech=0,record=0,player=0,inventory=0,slots=0,release=0;
    std::array<uintptr_t,200> items{};
    std::optional<tutorial_replication::startingpoint::Row> row;
    bool admitted=false;
    std::string admission_cause;
    nlohmann::json admission;
};
bool read(nlohmann::json& result,std::string& error);
bool read_startingpoint(StartingpointSnapshot&,nlohmann::json&,std::string&);
namespace detail {
using Origin=building_mixed_identity::opened_file::CurrentWorld;
struct Port {
    virtual ~Port()=default;
    virtual bool current(Origin&,std::string&)=0;
    virtual bool image(uintptr_t&,std::string&)=0;
    virtual bool copy(uintptr_t,void*,size_t,std::string&)=0;
};
bool observe(Port&,nlohmann::json&,std::string&,StartingpointSnapshot* snapshot=nullptr);
}
}
