bool framework_process_action(std::string& error){
    if(framework_actions.empty())return true;
    const auto action=std::move(framework_actions.front());framework_actions.pop_front();++framework_processed;
    framework_last_action={{"sequence",action.sequence},{"widget",action.widget},{"attempted",false},{"handler_failed",false},{"callback_pending",false}};
    const auto reject=[&](const std::string& detail){++framework_rejected;framework_last_action["handler_failed"]=true;framework_last_action["error"]=detail;error=detail;return fail(detail);};
    auto* widget=framework_widget(action.widget.c_str());
    if(action.mode!=ui_owner||action.admission!=generation||action.discovery!=list_generation||action.page!=list_page||action.choosing!=choosing)
        return framework_reject_unattempted(action.widget,action.sequence,"Framework lobby action owner/admission/list context changed before execution; no replay",error);
    if(!widget||widget->disabled||!framework_effectively_visible(*widget))return framework_reject_unattempted(action.widget,action.sequence,"Framework lobby queued action is no longer enabled; no replay",error);
    framework_widgets.at("lobby_id").text=action.lobby_id;framework_widgets.at("chat_input").text=action.chat;
    const auto failures=command_failures;
    framework_last_action["attempted"]=true;
    try {
        if(action.widget=="report"){reporting::open_report();framework_report=true;}
        else if(action.widget=="report_back")framework_report=false;
        else if(action.widget=="report_send")reporting::send_report(action.report_comment);
        else if(action.widget=="report_auto"){
            const auto reporting_state=reporting::status();
            reporting::automatic_reports(!reporting_state.at("automatic").get<bool>());
        } else dispatch_action(ui_owner,action.command);
    }
    catch(const std::exception& problem){return reject(std::string("Framework lobby action failed: ")+problem.what());}
    catch(...){return reject("Framework lobby action failed: unknown native exception");}
    framework_last_action["handler_failed"]=command_failures!=failures;
    framework_last_action["callback_pending"]=pending||list_pending||framework_new_world_requested;
    framework_last_action["admitted"]=admitted();
    if(command_failures!=failures){error=action.widget=="chat_send"?chat_error:cause;framework_last_action["error"]=error;return false;}
    error.clear();return true;
}
