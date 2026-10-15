#pragma once
#include "construction_types.h"
#include "job_census.h"
#include "shared_state.h"
namespace mt2mp::game::developer_staff::construction {
struct PlacementBefore {Capture capture;job_census::Census queue;};
bool capture_placement(PlacementBefore&,std::string&);
bool finish_placement(const PlacementBefore&,const std::string& snapshot,const std::string& key,
                      uint64_t uid,LocalJob&,std::string&);
bool capture(const Capture&,const std::vector<LocalJob>&,State&,std::string&);
bool preflight(const action_session::Context&,const action_session::Binding&,const Capture&,
               const PoolState& target,const std::vector<LocalJob>&,std::string&);
bool apply(const action_session::Context&,const action_session::Binding&,const Capture&,
           const PoolState& target,const std::vector<LocalJob>&,bool& attempted,bool& unknown,std::string&);
}
