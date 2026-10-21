#pragma once
#include "construction_types.h"
#include <map>
#include "job_census.h"
#include "shared_state.h"
namespace mt2mp::game::developer_staff::construction {
struct PlacementBefore {Capture capture;job_census::Census queue;};
bool settle_loaded_client(const action_session::Context&,const action_session::Binding&,bool& attempted,std::string&);
bool capture_loaded(const std::string& snapshot,const action_session::Binding&,std::vector<LocalJob>&,std::string&);
bool capture_placement(PlacementBefore&,std::string&);
bool finish_placement(const PlacementBefore&,const std::string& snapshot,const std::string& key,
                      uint64_t uid,LocalJob&,std::string&);
bool finish_duplicate(const PlacementBefore&,const std::string& snapshot,
                      const std::map<uint64_t,std::string>& created,std::vector<LocalJob>&,std::string&);
bool capture(const Capture&,const std::vector<LocalJob>&,State&,std::string&);
bool preflight(const action_session::Context&,const action_session::Binding&,const Capture&,
               const PoolState& target,const std::vector<LocalJob>&,std::string&);
bool apply(const action_session::Context&,const action_session::Binding&,const Capture&,
           const PoolState& target,const std::vector<LocalJob>&,bool& attempted,bool& unknown,std::string&);
}
