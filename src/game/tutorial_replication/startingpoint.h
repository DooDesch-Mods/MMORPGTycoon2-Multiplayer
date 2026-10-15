#pragma once
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace mt2mp::game::tutorial_replication::startingpoint {
struct CommitBugs {int32_t total=0,known=0;bool operator==(const CommitBugs&)const=default;};
struct Row {
    bool project_present=false,purchased=false,deployed=false,custom=false;
    float work=0;
    int32_t lines=0,desired_workers=0,bugs_total=0,bugs_known=0;
    int32_t inventory_count=0,release_change_count=0,release_feature_count=0;
    uint32_t release_counter=0;
    std::optional<CommitBugs> commit_bugs;
    bool operator==(const Row&)const=default;
};
enum class Plan {None,Project,Deploy};
bool valid(const Row&,std::string&);
nlohmann::json encode(const Row&);
bool decode(const nlohmann::json&,Row&,std::string&);
bool plan(const std::optional<Row>& local,const std::optional<Row>& target,Plan&,std::string&);
bool accepts_complete_tech(const std::string& scenario,int32_t index,const std::string& key,const std::optional<Row>& target);

// The caller owns the existing tutorial transaction lock and local NativeOpen
// binding. A failed native attempt is never retried by this function.
template<class Calls>
bool apply(const std::optional<Row>& local,const std::optional<Row>& target,Calls& calls,
           bool& attempted,bool& unknown,std::string& error){
    Plan operation;if(!plan(local,target,operation,error))return false;
    if(operation==Plan::None)return true;
    if(!calls.pin(*local,operation==Plan::Deploy,error))return false;
    attempted=true;unknown=true;
    if(operation==Plan::Deploy){
        if(!calls.deploy(error)||!calls.retire(error))return false;
    }else if(!calls.project(*target,error))return false;
    if(!calls.readback(*target,error))return false;
    unknown=false;return true;
}
}
