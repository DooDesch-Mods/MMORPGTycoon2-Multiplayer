#pragma once
#include "movement_input.h"
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::tutorial_movement {
inline bool threshold_projection(const nlohmann::json& progress,Snapshot& out,std::string& error){
    const auto fail=[&](const std::string& cause){error="tutorial threshold "+cause;return false;};
    try{
        if(progress.at("schema")!=2)return fail("requires ordered tutorial progress");
        const auto& owner=progress.at("tutorial_observer");const auto& gate=progress.at("ordered_message");
        if(!owner.at("mainthread_confirmed").get<bool>()||!owner.at("lifetime_confirmed").get<bool>()||
           owner.at("snapshot_uuid")!=out.origin||owner.at("world_epoch")!=out.epoch||owner.at("world_generation")!=out.generation||
           owner.at("initial_tutorial_complete").get<bool>())return fail("native owner changed");
        if(!gate.at("fault").get<std::string>().empty())return fail("ordered gate failed: "+gate.at("fault").get<std::string>());
        const nlohmann::json* row=nullptr;
        for(const auto& candidate:owner.at("tutorials"))if(candidate.at("scenario_id")=="initial_tutorial"){
            if(row)return fail("initial sequence duplicated");row=&candidate;
        }
        if(!row||row->at("complete").get<bool>()||row->at("sequence_complete").get<bool>()||row->at("step_count")!=107)
            return fail("initial sequence unavailable");
        out.tutorial_index=row->at("current_step_index").get<int32_t>();
        out.history_count=row->at("history_count").get<uint32_t>();
        out.granted=gate.at("granted").get<uint64_t>();out.released=gate.at("release_attempted").get<uint64_t>();
        if(row->at("next_step_index")!=out.tutorial_index+1)return fail("native next index differs");
        const auto type=row->at("current_step_type").get<std::string>();
        const auto id=row->at("current_step_id").get<std::string>();
        if(out.tutorial_index==6){
            if(type!="mmoStepMessage"||id!="wasd")return fail("current WASD identity differs");
            out.threshold_reached=row->at("current_step_complete").get<bool>();
            out.ordered_release=false;error.clear();return true;
        }
        if(!((out.tutorial_index==7&&type=="mmoStepWait"&&id.empty())||
             (out.tutorial_index==8&&type=="mmoStepMessage"&&id=="zoom")))return fail("unexpected successor step");
        const auto& release=gate.at("last_release");
        const auto message_type=release.at("message_type").get<int32_t>();
        if(release.at("asset")!="scenarios/initial_tutorial"||release.at("index")!=6||release.at("step_id")!="wasd"||
           message_type<0||message_type>18||!release.at("clear_undo").get<bool>()||
           !release.at("clear_attempted").get<bool>()||!release.at("clear_returned").get<bool>()||
           !release.at("advance_attempted").get<bool>()||!release.at("advance_returned").get<bool>()||
           release.at("undo_after")!=0||release.at("redo_after")!=0||!release.at("cause").get<std::string>().empty())
            return fail("original WASD Clear/Advance receipt incomplete");
        out.threshold_reached=true;out.ordered_release=true;error.clear();return true;
    }catch(const std::exception& cause){return fail(std::string("projection: ")+cause.what());}
}
}
