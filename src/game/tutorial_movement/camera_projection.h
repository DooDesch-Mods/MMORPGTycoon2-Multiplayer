#pragma once
#include "camera_input.h"
namespace mt2mp::game::tutorial_movement {
inline bool camera_projection(const nlohmann::json& progress,int32_t index,Snapshot& out,std::string& error){
    const auto fail=[&](const std::string& cause){error="tutorial camera "+cause;return false;};
    try{
        const auto* step=camera_step(index);if(!step)return fail("unsupported exact step");
        if(progress.at("schema")!=2)return fail("requires ordered progress schema2");
        const auto& owner=progress.at("tutorial_observer");const auto& gate=progress.at("ordered_message");
        if(!owner.at("mainthread_confirmed").get<bool>()||!owner.at("lifetime_confirmed").get<bool>()||
           owner.at("snapshot_uuid")!=out.origin||owner.at("world_epoch")!=out.epoch||owner.at("world_generation")!=out.generation||
           owner.at("initial_tutorial_complete").get<bool>())return fail("native owner changed");
        if(!gate.at("fault").get<std::string>().empty())return fail("ordered gate failed: "+gate.at("fault").get<std::string>());
        const nlohmann::json* row=nullptr;
        for(const auto& value:owner.at("tutorials"))if(value.at("scenario_id")=="initial_tutorial"){
            if(row)return fail("duplicate initial sequence");row=&value;
        }
        if(!row||row->at("complete").get<bool>()||row->at("sequence_complete").get<bool>()||row->at("step_count")!=107)return fail("initial sequence unavailable");
        out.tutorial_index=row->at("current_step_index").get<int32_t>();out.history_count=row->at("history_count").get<uint32_t>();
        out.granted=gate.at("granted").get<uint64_t>();out.released=gate.at("release_attempted").get<uint64_t>();
        if(row->at("next_step_index")!=out.tutorial_index+1)return fail("native next index differs");
        const auto type=row->at("current_step_type").get<std::string>();const auto id=row->at("current_step_id").get<std::string>();
        if(out.tutorial_index==index){
            if(type!="mmoStepMessage"||id!=step->id)return fail("current message identity differs");
            out.threshold_reached=row->at("current_step_complete").get<bool>();out.ordered_release=false;error.clear();return true;
        }
        const bool next=out.tutorial_index==step->next&&type=="mmoStepMessage"&&id==step->next_id;
        const bool wait=(out.tutorial_index==index+1&&index!=12&&type=="mmoStepWait"&&id.empty())||
            (index==12&&id.empty()&&((out.tutorial_index==13&&type=="mmoStepCommand")||(out.tutorial_index==14&&type=="mmoStepWait")));
        if(!next&&!wait)return fail("unexpected successor step");
        const auto& r=gate.at("last_release");
        if(r.at("asset")!="scenarios/initial_tutorial"||r.at("index")!=index||r.at("step_id")!=step->id||r.at("message_type")!=step->type||
           !r.at("clear_undo").get<bool>()||!r.at("clear_attempted").get<bool>()||!r.at("clear_returned").get<bool>()||
           !r.at("advance_attempted").get<bool>()||!r.at("advance_returned").get<bool>()||r.at("undo_after")!=0||r.at("redo_after")!=0||
           !r.at("cause").get<std::string>().empty())return fail("original Clear/Advance receipt incomplete");
        out.threshold_reached=true;out.ordered_release=true;error.clear();return true;
    }catch(const std::exception& cause){return fail(std::string("projection: ")+cause.what());}
}
}
