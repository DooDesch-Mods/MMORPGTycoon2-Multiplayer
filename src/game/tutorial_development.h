#pragma once
#include "native_open_witness.h"
#include "tutorial_replication/startingpoint.h"
#include <array>
#include "vendor/nlohmann/json.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::game::tutorial_development {
struct ProjectSource {
    uintptr_t address=0;
    int32_t kind=0,statistic=0;
    std::string key,art;
    bool operator==(const ProjectSource&)const=default;
};
struct DeveloperState {
    uintptr_t address=0;
    int32_t job=0;
    bool operator==(const DeveloperState&)const=default;
};
struct CreationOwners {
    uintptr_t catalogue_data=0,developer_data=0,source=0;
    uintptr_t release_manager=0,release_owner=0,release_data=0;
    int32_t catalogue_capacity=0,developer_capacity=0,total_workers=0,max_workers=0;
    int32_t release_capacity=0;
    uint8_t automatic_workers=0;
    std::vector<ProjectSource> catalogue;
    std::vector<DeveloperState> developers;
    std::vector<uintptr_t> release_changes;
    std::optional<ProjectSource> current;
    bool operator==(const CreationOwners&)const=default;
};
struct StartingpointSnapshot {
    building_mixed_identity::opened_file::CurrentWorld origin;
    uintptr_t image=0,lounge=0,project=0,tech=0,record=0,player=0,inventory=0,slots=0,release=0;
    std::array<uintptr_t,200> items{};
    std::optional<tutorial_replication::startingpoint::Row> row;
    bool admitted=false;
    std::string admission_cause;
    nlohmann::json admission;
    std::optional<CreationOwners> creation;
};
bool read(nlohmann::json& result,std::string& error);
bool read_startingpoint(StartingpointSnapshot&,nlohmann::json&,std::string&,bool creation=false);
namespace detail {
using Origin=building_mixed_identity::opened_file::CurrentWorld;
struct Port {
    virtual ~Port()=default;
    virtual bool current(Origin&,std::string&)=0;
    virtual bool image(uintptr_t&,std::string&)=0;
    virtual bool copy(uintptr_t,void*,size_t,std::string&)=0;
};
bool observe(Port&,nlohmann::json&,std::string&,StartingpointSnapshot* snapshot=nullptr,bool creation=false);
}
}
