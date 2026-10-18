using FrameworkWidget=lobby_ui::Widget;
const MT2FW_Host* framework_host=nullptr;
MT2FW_MT2_RoutesV1 framework_routes{};
MT2FW_MT2_OwnershipV1 framework_ownership{};
uint64_t framework_command_token=0,framework_command_invocation=0;
bool framework_command_exception=false;
void MT2FW_CALL framework_command_observer(void* cookie,const MT2FW_MT2_CommandCall* call) noexcept {
    if(!release_fixture_active)return;
    try {
        if(cookie!=&framework_command_token||!framework_command_token||!call||call->size<sizeof(*call)||
           call->thread!=thread||GetCurrentThreadId()!=thread||call->receiver!=release_fixture_mode)
            release_fixture_error="release fixture actual Mode callback owner/thread differs";
        else if(!call->invocation||call->invocation<=framework_command_invocation)
            release_fixture_error="release fixture Mode callback invocation repeated or out of order";
        else {
            framework_command_invocation=call->invocation;
            if(!call->readable||call->read_error||!call->text||call->length>4096)
                release_fixture_error="release fixture original Mode callback string unreadable";
            else if(std::string_view(call->text,call->length)=="LevelUp")++release_fixture_callbacks;
            else release_fixture_error="release fixture original Mode callback is not LevelUp";
        }
    }catch(...){framework_command_exception=true;}
}
std::map<std::string,FrameworkWidget> framework_widgets;
bool framework_open=false,framework_browser=false,framework_ready=false,framework_observed_open=false;
bool framework_visibility_known=false,framework_visible=false,framework_suspended=false;
bool framework_report=false;
bool framework_creating_world=false;
bool framework_new_world_requested=false;
void* framework_new_world_owner=nullptr;
uint64_t framework_new_world_generation=0;
nlohmann::json framework_new_world_receipt,framework_new_world_observation;
nlohmann::json framework_last_action;
std::string framework_report_comment;
net::avatars::Provider framework_avatars;
bool framework_avatar_attempted=false,framework_avatar_ready=false,framework_busy_notice=false;
std::array<std::string,4> framework_avatar_paths;
std::string framework_host_name,framework_host_visibility;
lobby_assets::Provider framework_assets;
bool framework_assets_attempted=false;
std::string framework_avatar_error;
std::array<std::string,4> framework_avatar_errors;
SaveCacheRefresh framework_save_refresh;
void* framework_save_refresh_owner=nullptr;
uint64_t framework_save_refresh_admission=0;
bool framework_view_initialized=false;
bool framework_toggle_waiting=false,framework_toggle_target=false,framework_clear_pending=false;
uint64_t framework_queued=0,framework_processed=0,framework_rejected=0;
struct FrameworkAction {
    std::string widget,command,lobby_id,chat,report_comment;
    void* mode=nullptr;
    uint64_t admission=0,discovery=0,sequence=0;
    size_t page=0;
    bool choosing=false;
};
std::deque<FrameworkAction> framework_actions;
nlohmann::json framework_response,framework_app_response;
const std::map<std::string,std::string> framework_commands={
    {"report","MT2MPReport"},{"report_send","MT2MPReportSend"},{"report_back","MT2MPReportBack"},{"report_auto","MT2MPReportAutomatic"},
    {"host","MT2MPHost"},{"host_public","MT2MPHostPublic"},{"search","MT2MPSearch"},{"join","MT2MPJoin"},
    {"invite","MT2MPInvite"},{"pick","MT2MPPickWorld"},{"new_world","MT2MPNewWorld"},{"start","MT2MPStart"},{"leave","MT2MPLeave"},{"chat_send","MT2MPChatSend"},
    {"list_refresh","MT2MPRefresh"},{"list_previous","MT2MPListPrevious"},{"list_next","MT2MPListNext"},{"list_back","MT2MPListBack"},
    {"list0","MT2MPList0"},{"list1","MT2MPList1"},{"find0","MT2MPList0"},{"find1","MT2MPList1"}};
bool fail(const std::string&);
void framework_layout(){
    if(framework_view_initialized)return;
    framework_widgets=lobby_ui::make_layout();
    framework_view_initialized=true;
}
FrameworkWidget* framework_widget(const char* id){
    framework_layout();auto key=std::string_view(id)=="lobby_id_label"?"lobby_id_hint":id;
    auto found=framework_widgets.find(key);if(found==framework_widgets.end()){fail("Framework lobby widget is absent: "+std::string(id));return nullptr;}return &found->second;
}
bool label(const char* id,const std::string& value){auto* widget=framework_widget(id);if(!widget)return false;if(std::string_view(id)=="chat_input"&&value.empty()&&!widget->text.empty())framework_clear_pending=true;widget->text=value;return true;}
bool button_label(const char* id,const std::string& value){return label(id,value);}
bool disable(const char* id,bool value){auto* widget=framework_widget(id);if(!widget)return false;widget->disabled=value;return true;}
bool set_pane_visible(const char* id,bool value){auto* widget=framework_widget(id);if(!widget)return false;widget->visible=value;return true;}
bool input_text(const char* id,std::string& value,size_t limit){auto* widget=framework_widget(id);if(!widget||widget->type!="input"||widget->text.size()>limit)return false;value=widget->text;return true;}
// This records intent. Only ui.native's ready/open response confirms visibility.
DWORD view_show(bool browser,bool visible){
    if(visible){framework_browser=browser;framework_open=true;}
    else if(framework_browser==browser)framework_open=false;
    return 0;
}
nlohmann::json framework_request(const char* operation,const nlohmann::json& arguments){
    if(!framework_host)throw std::runtime_error("Framework lobby Host is unavailable");
    thread_local std::vector<char> output(4*1024*1024);
    const auto encoded=arguments.dump();const auto size=framework_host->request(framework_host->context,operation,encoded.c_str(),output.data(),output.size());
    if(!size||size>output.size()||output[size-1]!=0)throw std::runtime_error("Framework lobby response invalid; request was not replayed");
    auto result=nlohmann::json::parse(output.data(),output.data()+size-1);
    if(!result.at("ok").get<bool>())throw std::runtime_error(result.at("error").get<std::string>());
    return result.at("result");
}
bool framework_effectively_visible(const FrameworkWidget& widget){
    if(!widget.visible)return false;
    if(framework_report)return widget.group=="report";
    if(widget.group=="legacy"||widget.group=="report")return false;
    if(widget.group=="shell")return true;
    if(widget.group=="party")return admitted()&&(!framework_browser||choosing);
    if(framework_browser)return widget.group=="browser"||widget.group==(choosing?"picker":"find");
    return widget.group==(admitted()?"session":"entry");
}
bool framework_apply_response(const nlohmann::json& response,std::string& error){
    framework_response=response;framework_ready=response.at("ready").get<bool>();
    if(!framework_ready){framework_observed_open=false;framework_visibility_known=false;framework_visible=false;framework_suspended=false;error.clear();return true;}
    framework_observed_open=response.at("open").get<bool>();framework_open=framework_observed_open;
    framework_visibility_known=response.at("visibility_known").get<bool>();
    framework_visible=framework_visibility_known&&response.at("visible").get<bool>();
    framework_suspended=response.at("suspended").get<bool>();
    for(const auto& [key,value]:response.at("values").items()){
        auto* widget=framework_widget(key.c_str());
        if(!widget||widget->type!="input"||!value.is_string()){error="Framework lobby returned an unknown input";return fail(error);}
        const auto text=value.get<std::string>();
        const auto limit=key=="report_comment"?size_t(1000):key=="chat_input"?size_t(net::chat::max_text_bytes):size_t(20);
        if(text.size()>limit){error="Framework lobby input exceeds its domain byte limit";return fail(error);}
        widget->text=text;
        if(key=="report_comment")framework_report_comment=text;
    }
    for(const auto& value:response.at("clicked")){
        if(!value.is_string()){error="Framework lobby click is not a widget ID";return fail(error);}
        const auto id=value.get<std::string>();auto* widget=framework_widget(id.c_str());auto command=framework_commands.find(id);
        if(!framework_observed_open||!framework_visibility_known||!framework_visible||framework_suspended||!ui_owner||!widget||widget->type!="button"||widget->disabled||!framework_effectively_visible(*widget)||command==framework_commands.end()){
            ++framework_rejected;error="Framework lobby click is not currently enabled: "+id;return fail(error);
        }
        if(framework_actions.size()>=16){++framework_rejected;error="Framework lobby action queue full; click not replayed";return fail(error);}
        framework_actions.push_back({id,command->second,framework_widgets.at("lobby_id").text,framework_widgets.at("chat_input").text,framework_report_comment,ui_owner,generation,list_generation,++framework_queued,list_page,choosing});
    }
    error.clear();return true;
}
bool process_panel_toggle(bool available,std::string& error){
    if(framework_toggle_waiting)return true;
    if(toggle_processed==toggle_requested.load())return true;
    ++toggle_processed;
    if(!available||!ui_owner||GetCurrentThreadId()!=thread){++toggle_failed;error="Framework lobby toggle has no current game-thread UI owner";toggle_error=error;if(toggle_first_error.empty())toggle_first_error=error;return fail(error);}
    ++toggle_attempted;framework_open=!framework_open;framework_browser=false;++toggle_returned;
    framework_toggle_waiting=true;framework_toggle_target=framework_open;
    // Native confirmation is reported by framework_render, not by this request.
    error.clear();return true;
}
