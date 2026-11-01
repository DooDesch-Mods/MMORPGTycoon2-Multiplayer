#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::action_session {
struct TutorialIntent {
    std::string asset,step_id;
    int32_t index=-1,message_type=-1;
    bool clear_undo=false;
    bool operator==(const TutorialIntent&)const=default;
};
// A wire identity for the native Update's unsigned out-of-range branch.
// This value is never written back to the native Message.
inline constexpr int32_t tutorial_plain_message=19;
inline bool tutorial_authored_plain(const TutorialIntent& value){
    if(value.asset=="scenarios/initial_tutorial")switch(value.index){
        case 4:return value.step_id=="welcome";
        case 15:return value.step_id=="mainheader";
        case 19:return value.step_id=="pregrid";
        case 22:return value.step_id=="actionbar";
        case 32:return value.step_id=="assigndev_addendum";
        case 38:return value.step_id=="";
        case 43:return value.step_id=="postzone";
        case 48:return value.step_id=="terrain";
        case 53:return value.step_id=="respawn_02";
        case 66:return value.step_id=="inn_2";
        case 79:return value.step_id=="monsters";
        case 83:return value.step_id=="monsters_3";
        case 87:return value.step_id=="questgiver_2";
        case 90:return value.step_id=="roads";
        case 103:return value.step_id=="allDone";
        default:return false;
    }
    if(value.asset=="scenarios/leveltwo_tutorial")switch(value.index){
        case 2:return value.step_id=="welcome";
        case 4:return value.step_id=="customise";
        case 6:return value.step_id=="trainers";
        case 9:return value.step_id=="new_region";
        case 11:return value.step_id=="mountain_pass";
        case 13:return value.step_id=="doitagain";
        case 14:return value.step_id=="readytogo";
        default:return false;
    }
    return false;
}
inline bool tutorial_valid(const TutorialIntent& value){
    const int32_t count=value.asset=="scenarios/initial_tutorial"?107:
        value.asset=="scenarios/leveltwo_tutorial"?17:0;
    return count&&value.index>=0&&value.index<count&&((value.message_type>=0&&value.message_type<=18)||
        (value.message_type==tutorial_plain_message&&tutorial_authored_plain(value)))&&
        value.step_id.size()<=200&&value.step_id.find('\0')==std::string::npos;
}
// Missing authored type leaves native storage unchanged. Type zero and the
// unsigned default branch share the native no-condition path. Typed branches
// 1 through 18 remain distinct, including on an unexpected authored plain step.
inline bool tutorial_observed_identity(TutorialIntent& value){
    if((value.message_type==0||uint32_t(value.message_type)>18)&&tutorial_authored_plain(value))
        value.message_type=tutorial_plain_message;
    return tutorial_valid(value);
}
inline bool tutorial_same_step(const TutorialIntent& a,const TutorialIntent& b){
    return a.asset==b.asset&&a.index==b.index&&a.message_type==b.message_type&&a.step_id==b.step_id;
}
struct TutorialProof {
    TutorialIntent step;
    bool granted=false;
    bool operator==(const TutorialProof&)const=default;
};
}
