#pragma once
#include <native_steps_draft.h>
#include "ordered_lobby_admission.h"
#include <string>
#include <string_view>
namespace mt2mp::game::native_lobby {
struct OrderedView {
    bool started=false,leaving=false;
    std::string phase,failure;
    bool terminal_exit_ready=false;
    bool admission_ready=false;
    std::string admission_error;
};
inline bool ordered_service_receipt(const MT2FW_MT2_StepReceipt& r,uint64_t thread){
    return r.size==sizeof(r)&&r.thread==thread&&r.lease&&r.session&&r.callbacks_inflight==1&&
        (r.phase==MT2FW_MT2_STEP_ARMED||r.phase==MT2FW_MT2_STEP_LOADING||
         (r.phase==MT2FW_MT2_STEP_HELD&&r.workers_held==1)||
         (r.phase==MT2FW_MT2_STEP_RETIRING&&r.workers_held==1)||
         (r.phase==MT2FW_MT2_STEP_FAILED&&r.workers_held==1));
}
inline bool ordered_action_allowed(std::string_view id){
    return id=="leave"||id=="invite"||id=="chat_send"||id=="report"||id=="report_back"||id=="report_send"||id=="report_auto";
}
inline std::string ordered_heading(const OrderedView& view){
    return !view.failure.empty()?"Session unavailable":view.leaving?"Leaving session":view.started?"Playing together":"Loading shared world";
}
inline std::string ordered_detail(const OrderedView& view){
    if(!view.failure.empty())return view.failure;
    if(view.leaving)return "Waiting for the game to return to the main menu.";
    if(view.phase=="preparing_membership")return "Preparing the shared world for joining players.";
    if(view.started)return "Shared world is running.";
    if(view.phase=="waiting_for_matching_worlds")return "Checking that both worlds match.";
    if(view.phase=="receiving_world"||view.phase=="waiting_for_verified_files")return "Receiving the shared world.";
    return "Preparing the shared world.";
}
bool ordered_tick(const OrderedView&,const MT2FW_MT2_StepReceipt&,std::string& error);
}
