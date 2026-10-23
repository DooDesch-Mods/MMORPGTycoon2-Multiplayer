#pragma once
#include "../general_project_state.h"
#include "../tutorial_development.h"
#include <utility>
namespace mt2mp::game::tutorial_startingpoint_fixture {
inline bool expected_project_intent(const tutorial_development::StartingpointSnapshot& snapshot,
    const std::string& tech,general_project::Command command,general_project::Intent& out,std::string& error){
    if(!snapshot.creation||(command!=general_project::Command::Start&&command!=general_project::Command::Workers)){
        error="General project fixture native creation/command absent";return false;
    }
    const auto& owners=*snapshot.creation;
    const tutorial_development::ProjectSource* source=nullptr;
    if(command==general_project::Command::Start){
        for(const auto& row:owners.catalogue)if(row.address==owners.source){
            if(source){error="General project fixture selected catalogue source duplicated";return false;}source=&row;
        }
    }else if(owners.current&&snapshot.project&&owners.current->address==snapshot.project){
        source=&*owners.current;
        for(const auto& row:owners.catalogue)if(row.address==snapshot.project){
            error="General project fixture current clone aliases catalogue";return false;
        }
    }
    if(!source||!source->address||source->kind!=0||source->key!=tech){
        error="General project fixture exact native Tech source differs";return false;
    }
    general_project::Intent value;value.command=command;
    value.source={source->kind,source->statistic,source->key,source->art};
    value.workers=command==general_project::Command::Workers?1:0;
    if(!general_project::valid(value)){error="General project fixture native intent invalid";return false;}
    out=std::move(value);error.clear();return true;
}
}
