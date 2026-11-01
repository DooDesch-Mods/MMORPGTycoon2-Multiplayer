#pragma once
#include "ordered_message_gate.h"
#include "../typed_action/action_session.h"
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <vector>
namespace mt2mp::game::tutorial_replication::ordered_message {
struct Release {
    Intent step;
    bool clear_attempted=false,clear_returned=false,advance_attempted=false,advance_returned=false;
    int32_t undo_before=-1,redo_before=-1,undo_after=-1,redo_after=-1;
    std::string cause;
};
inline nlohmann::json release_json(const Release& value){
    return {{"asset",value.step.asset},{"index",value.step.index},{"step_id",value.step.step_id},
        {"message_type",value.step.message_type},{"clear_undo",value.step.clear_undo},{"clear_attempted",value.clear_attempted},
        {"clear_returned",value.clear_returned},{"advance_attempted",value.advance_attempted},{"advance_returned",value.advance_returned},
        {"undo_before",value.undo_before},{"redo_before",value.redo_before},{"undo_after",value.undo_after},{"redo_after",value.redo_after},{"cause",value.cause}};
}
// Retain readback across later automatic prompts. Native effect admission remains in Gate.
class Readback {
    struct Grant {
        Intent intent;
        std::string first_action,last_action,snapshot;
        std::array<uint64_t,5> granted{},retired{};
        bool released=false;
    };
    std::map<std::pair<std::string,int32_t>,Grant> grants_;
    std::vector<nlohmann::json> releases_;
    static bool fail(std::string& e,const char* cause){e=cause;return false;}
public:
    void reset(){grants_.clear();releases_.clear();}
    bool grant(const action_session::Request& request,bool duplicate,std::string& e){
        if(!action_session::tutorial_valid(request.tutorial)||request.peer<1||request.peer>4||request.action_uuid.empty()||request.snapshot_uuid.empty())
            return fail(e,"Tutorial readback exact grant identity missing");
        const auto key=std::make_pair(request.tutorial.asset,request.tutorial.index);auto found=grants_.find(key);
        if((found!=grants_.end())!=duplicate)return fail(e,"Tutorial readback first/duplicate grant differs");
        if(found==grants_.end()){
            if(grants_.size()>=124)return fail(e,"Tutorial readback exceeds both native assets");
            Grant value;value.intent=request.tutorial;value.first_action=request.action_uuid;value.snapshot=request.snapshot_uuid;
            found=grants_.emplace(key,std::move(value)).first;
        }
        auto& value=found->second;
        if(!action_session::tutorial_same_step(value.intent,request.tutorial)||value.snapshot!=request.snapshot_uuid)
            return fail(e,"Tutorial readback retained grant identity differs");
        ++value.granted[request.peer];value.last_action=request.action_uuid;e.clear();return true;
    }
    bool retire(const action_session::Request& request,std::string& e){
        const auto found=grants_.find({request.tutorial.asset,request.tutorial.index});
        if(found==grants_.end()||request.peer<1||request.peer>4||found->second.snapshot!=request.snapshot_uuid||
           !action_session::tutorial_same_step(found->second.intent,request.tutorial)||
           found->second.retired[request.peer]>=found->second.granted[request.peer])return fail(e,"Tutorial readback retirement lacks its exact grant");
        ++found->second.retired[request.peer];e.clear();return true;
    }
    bool released(const Release& release,std::string& e){
        const auto found=grants_.find({release.step.asset,release.step.index});
        if(found==grants_.end()||found->second.released||!action_session::tutorial_same_step(found->second.intent,release.step)||
           !release.cause.empty()||!release.advance_attempted||!release.advance_returned||
           release.clear_attempted!=release.step.clear_undo||release.clear_returned!=release.step.clear_undo||
           (release.step.clear_undo&&(release.undo_after||release.redo_after)))return fail(e,"Tutorial readback original release is incomplete or duplicated");
        auto row=release_json(release);row["action_uuid"]=found->second.first_action;row["snapshot_uuid"]=found->second.snapshot;
        row["release_sequence"]=releases_.size()+1;releases_.push_back(std::move(row));found->second.released=true;e.clear();return true;
    }
    nlohmann::json status()const{
        nlohmann::json grants=nlohmann::json::array();
        for(const auto& [key,value]:grants_){
            grants.push_back({{"asset",value.intent.asset},{"index",value.intent.index},{"step_id",value.intent.step_id},
                {"message_type",value.intent.message_type},{"first_action_uuid",value.first_action},{"last_action_uuid",value.last_action},
                {"snapshot_uuid",value.snapshot},{"granted_by_peer",value.granted},{"retired_by_peer",value.retired},{"released",value.released}});
        }
        return {{"schema",1},{"grants",std::move(grants)},{"releases",releases_}};
    }
};
}
