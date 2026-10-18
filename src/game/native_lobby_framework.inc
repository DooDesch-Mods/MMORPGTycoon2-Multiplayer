namespace mt2mp::game::native_lobby {
namespace {

uintptr_t image=0;
DWORD thread=0;
void* title=nullptr;
void* ui_owner=nullptr;int ui_mode=-1;
void* panel=nullptr;void* browser_panel=nullptr;
bool list_pending=false,list_callback_completed=false,list_callback_ok=false,publication_failed=false;std::string publication_error;size_t list_page=0;uint64_t list_generation=0,selected_lobby=0;
std::vector<std::string> saved_worlds;
std::vector<net::LobbyInfo> discovered;std::string list_status="Öffentliche Lobbys",published_state;
using Command=void(*)(void*,const void*);
Command original_command=nullptr,original_ingame_command=nullptr;
bool release_fixture_active=false;void* release_fixture_mode=nullptr;uint64_t release_fixture_callbacks=0;std::string release_fixture_error;
std::string ui_status="Multiplayer",cause,last_ui_status,selected_world,selected_file;
bool local_admitted=false;
std::string chat_error,last_chat_transcript,last_chat_status;uint64_t chat_last_sequence=0;bool chat_draft_clear_failed=false;
bool current_host_world=false;
std::string current_host_world_error;
bool choosing=false,pending=false,load_attempted=false,append_attempted=false;
bool host_world_pick_requested=false;
bool playing_close_attempted=false;
std::string failure_notice_attempted,failure_notice_error;
std::string departed_host_attempt, departed_host_error;
uint64_t owner=0,generation=0,last_leave_confirmed=0;
uint64_t command_count=0;std::string last_command;
uint64_t command_failures=0;
std::map<std::string,nlohmann::json> route_receipts;

std::atomic<uint64_t> toggle_requested{0};
uint64_t toggle_processed=0,toggle_attempted=0,toggle_returned=0,toggle_confirmed=0,toggle_failed=0;
std::string toggle_error,toggle_first_error;

struct alignas(8) String {unsigned char bytes[32];};

bool readable(const void* p,size_t n) {
    MEMORY_BASIC_INFORMATION m{};
    if(!p||!VirtualQuery(p,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto offset=reinterpret_cast<uintptr_t>(p)-reinterpret_cast<uintptr_t>(m.BaseAddress);
    return offset<=m.RegionSize&&n<=m.RegionSize-offset;
}

template<class T> bool read(const void* p,size_t off,T& out) {
    if(!p)return false;const auto* q=static_cast<const unsigned char*>(p)+off;
    if(!readable(q,sizeof(T)))return false;std::memcpy(&out,q,sizeof(T));return true;
}

bool text(const void* p,std::string& out,size_t limit=4096) {
    const char* data=nullptr;size_t size=0;
    if(!read(p,0,data)||!read(p,8,size)||size>limit||!readable(data,size+1)||data[size]!=0)return false;
    out.assign(data,size);return true;
}

DWORD find_window(const char* id,void** out) {
    String s{};bool built=false;DWORD code=0;
    __try {reinterpret_cast<void(*)(void*,const char*)>(image+0x49e090)(&s,id);built=true;*out=reinterpret_cast<void*(*)(const void*)>(image+0x81aa90)(&s);}
    __except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(built){__try {reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(&s);}__except(EXCEPTION_EXECUTE_HANDLER){if(!code)code=GetExceptionCode();}}
    return code;
}

DWORD latest_save(void* mode,const char* world,String* output,bool* returned) {
    String input{};bool built=false;DWORD code=0;
    __try {
        reinterpret_cast<void(*)(void*,const char*)>(image+0x49e090)(&input,world);built=true;
        reinterpret_cast<void*(*)(void*,void*,const void*)>(image+0x4b9910)(output,mode,&input);*returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(built){__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(&input);}__except(EXCEPTION_EXECUTE_HANDLER){if(!code)code=GetExceptionCode();}}
    return code;
}

DWORD dispose(String* value) {
    __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(value);}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}

DWORD rebuild_save_cache(){
    __try {reinterpret_cast<void(*)()>(image+save_cache_guard::rva)();}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}

#include "framework_lobby_view.inc"

#include "framework_lobby_domain.inc"
#include "framework_lobby_actions.inc"
}

bool framework_bind(const MT2FW_Host* host,const MT2FW_MT2_RoutesV1* routes,const MT2FW_MT2_OwnershipV1* ownership,std::string& error){
    if(framework_host){error="Framework lobby Host is already bound";return false;}
    if(!host||host->size<sizeof(*host)||host->abi_version!=MT2FW_ABI_VERSION||!host->context||!host->request){error="Framework lobby Host ABI is incomplete";return false;}
    if(!routes||routes->size<sizeof(*routes)||routes->version!=MT2FW_MT2_ROUTES_VERSION||!routes->context||!routes->code_verify||
       !ownership||ownership->size<sizeof(*ownership)||ownership->version!=MT2FW_MT2_OWNERSHIP_VERSION||!ownership->context||!ownership->command_subscribe||!ownership->command_unsubscribe){error="Framework lobby typed routes/command observer ABI is incomplete";return false;}
    framework_layout();
    const auto token=ownership->command_subscribe(ownership->context,&framework_command_observer,&framework_command_token);
    if(!token){error="Framework lobby original Mode observer subscription failed";return false;}
    framework_routes=*routes;framework_ownership=*ownership;framework_command_token=token;
    framework_host=host;thread=GetCurrentThreadId();error.clear();return true;
}
bool initialize(const std::filesystem::path&,uintptr_t base,std::string& error){
    if(image){error="native lobby already initialized";return false;}
    if(!framework_host||GetCurrentThreadId()!=thread){error="Framework lobby initialization requires its bound active game-thread Host";return false;}
    uintptr_t resolved=0;uint32_t game_thread=0;std::string digest;
    if(!native::read_resolver_receipt(resolved,game_thread,digest,error)||resolved!=base||game_thread!=thread||
       digest!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"){
        error="Framework lobby exact executable/thread resolver mismatch: "+error;return false;
    }
    image=base;error.clear();return true;
}
bool framework_render(std::string& error){
    if(!framework_host||GetCurrentThreadId()!=thread){error="Framework lobby render requires the bound active Host/thread";return false;}
    try {
        const auto app=framework_request("ui.app",{{"id","main"},{"title","Multiplayer"},{"desktop",true},{"icon","assets/multiplayer.png"}});
        framework_app_response=app;
        const auto& desktop=app.at("desktop");
        if(!desktop.at("requested").get<bool>())throw std::runtime_error("Framework did not acknowledge the Multiplayer Desktop registration request");
        const auto desktop_error=desktop.at("error").get<std::string>();
        if(!desktop_error.empty())throw std::runtime_error("Multiplayer Desktop registration failed: "+desktop_error);
        if(app.at("open_requested").get<bool>()){framework_open=true;framework_browser=false;}
        if(!image||!ui_owner){error.clear();return true;}
        if(!render_native_new_world(error)||!observe_native_new_world(ui_mode==2,ui_mode==3,error))return false;
        if(!framework_open&&!framework_observed_open){error.clear();return true;}
        framework_widgets.at("status").visible=false;
        if(!framework_report)framework_widgets.at("status").text="Multiplayer";
        if(framework_report){
            const auto report=reporting::status();
            const auto phase=report.at("phase").get<std::string>();
            framework_widgets.at("report_state").text=phase=="sent"?"Danke. Bericht empfangen: "+report.at("receipt").get<std::string>():
                phase=="sending"?"Bericht wird gesendet...":
                phase=="failed"?"Bericht konnte nicht gesendet werden: "+report.at("error").get<std::string>():phase=="preparing"?"Bericht wird vorbereitet...":"Bereit. "+std::to_string(report.at("event_count").get<size_t>())+" Diagnoseereignisse beigefügt.";
            framework_widgets.at("report_send").disabled=phase=="sending"||phase=="sent"||phase=="preparing"||!report.at("ready").get<bool>()||report.at("report_id")=="";
            framework_widgets.at("report_send").text=phase=="failed"?"Privaten Upload wiederholen":"Bericht privat senden";
            const bool sealed=report.at("sealed").get<bool>();
            framework_widgets.at("report_comment").disabled=sealed;
            framework_widgets.at("report_comment").visible=!sealed;
            framework_widgets.at("report_hint").text=phase=="sent"?"Beschreibung wurde privat gesendet.":sealed?"Beschreibung gespeichert. Beim Wiederholen wird derselbe Bericht gesendet.":"Was ist passiert? (optional)";
            framework_widgets.at("report_auto").text=report.at("automatic").get<bool>()?"Automatische technische Berichte: An":report.at("automatic_inflight").get<bool>()?"Automatische technische Berichte: Aus (aktueller Bericht wird abgeschlossen)":"Automatische technische Berichte: Aus";
        }
        nlohmann::json widgets=nlohmann::json::array();
        // Native panels must be created before the controls drawn above them.
        for(bool panels:{true,false})for(const auto& [id,widget]:framework_widgets){
            if((widget.type=="panel")!=panels)continue;
            nlohmann::json value={{"id",id},{"type",widget.type},{"x",widget.x},{"y",widget.y},{"width",widget.width},{"height",widget.height},
                {"visible",framework_effectively_visible(widget)},{"disabled",widget.disabled},{"size",widget.size}};
            if(widget.type=="button")value["style"]=widget.style;
            if(!widget.tone.empty())value["tone"]=widget.tone;
            if(widget.type=="image"){value["path"]=widget.text;value["visible"]=framework_effectively_visible(widget)&&!widget.text.empty();}
            else if(widget.type=="input"){value["value"]=widget.text;value["max_bytes"]=widget.max_length;}
            else value["text"]=widget.text;
            widgets.push_back(std::move(value));
        }
        auto response=framework_request("ui.native",{{"id","main"},{"open",framework_open},{"width",820},{"height",466},{"widgets",widgets}});
        if(!framework_apply_response(response,error))return false;
        if(framework_clear_pending&&framework_ready&&framework_visibility_known&&framework_visible&&!framework_suspended&&framework_effectively_visible(framework_widgets.at("chat_input"))){
            framework_clear_pending=false;
            if(!framework_widgets.at("chat_input").text.empty()){
                chat_draft_clear_failed=true;chat_error="chat queued sequence="+std::to_string(chat_last_sequence)+" but native draft clear readback differs; no replay";
                error=chat_error;return fail(error);
            }
        }
        if(framework_toggle_waiting&&framework_ready&&framework_visibility_known&&!framework_suspended){
            framework_toggle_waiting=false;
            if(framework_visible!=framework_toggle_target){++toggle_failed;error="Framework native panel toggle visibility readback differs";toggle_error=error;if(toggle_first_error.empty())toggle_first_error=error;return fail(error);}
            ++toggle_confirmed;toggle_error.clear();
        }
        error.clear();return true;
    }catch(const std::exception& problem){error=std::string("Framework native lobby render failed: ")+problem.what();}
    catch(...){error="Framework native lobby render failed: unknown native exception";}
    if(framework_clear_pending){framework_clear_pending=false;chat_draft_clear_failed=true;chat_error="chat queued sequence="+std::to_string(chat_last_sequence)+" but native draft clear failed; no replay: "+error;}
    if(framework_toggle_waiting){framework_toggle_waiting=false;++toggle_failed;toggle_error=error;if(toggle_first_error.empty())toggle_first_error=error;}
    return fail(error);
}
bool framework_unbind(std::string& error){
    if(!framework_host){error.clear();return true;}
    if(GetCurrentThreadId()!=thread){error="Framework lobby retirement requires the bound game thread";return false;}
    if(release_fixture_active){error="Framework lobby retirement waits for the active Initial Release callback scope";return false;}
    if(framework_command_token){
        const auto result=framework_ownership.command_unsubscribe(framework_ownership.context,framework_command_token);
        if(result!=MT2FW_MT2_OK){error="Framework lobby Mode observer retirement failed result="+std::to_string(result);return false;}
        framework_command_token=0;
    }
    if(!framework_actions.empty()){
        framework_rejected+=framework_actions.size();framework_actions.clear();
        LOG_WARN("[NativeLobby] Framework retirement discarded unexecuted UI actions; no replay");
    }
    framework_host=nullptr;framework_open=false;framework_ready=false;framework_observed_open=false;framework_visibility_known=false;framework_visible=false;framework_suspended=false;error.clear();return true;
}
bool activate(const nlohmann::json&,nlohmann::json&,std::string& error){
    error="Framework Multiplayer activation requires the existing Framework desktop_activate native UI route";return false;
}

bool retire_departed_host(const nlohmann::json& preparation,std::string& error) {
    if(Config::instance().is_host||preparation.value("leaving",false)){error.clear();return true;}
    std::string identity;
    if(Config::instance().local_test){
        const auto snapshot=preparation.value("snapshot_id",std::string{});
        if(!local_admitted||!net::local_host_left(snapshot)){error.clear();return true;}
        identity="local:"+std::to_string(net::session_wire::session_id())+":"+snapshot;
    }else{
        auto& mm=net::SteamMatchmaking::instance();
        if(!owner||mm.host_steam_id()==owner){error.clear();return true;}
        identity="steam:"+std::to_string(owner)+":"+std::to_string(generation);
    }
    if(departed_host_attempt==identity){error=departed_host_error;return error.empty();}
    departed_host_attempt=identity;
    std::string detail;
    if(!lobby_prepare::leave(detail)){
        departed_host_error="Host connection lost; native retirement failed: "+detail;
        error=departed_host_error;return fail(error);
    }
    ui_status="Host hat die Sitzung beendet";error.clear();return true;
}
void request_panel_toggle(){++toggle_requested;}

bool tick(std::string& error) {
    if(!image){error="native lobby not initialized";return false;}
    if(!thread)thread=GetCurrentThreadId();
    if(thread!=GetCurrentThreadId()){error="native lobby tick outside bound game thread";return false;}
    auto& mm=net::SteamMatchmaking::instance();
    mm.poll();
    if(!Config::instance().local_test&&mm.in_lobby()&&!owner){owner=mm.host_steam_id();Config::instance().is_host=owner!=0&&owner==net::SteamNetworking::instance().local_steam_id();}
    const auto preparation=lobby_prepare::status();
    mm.update_rich_presence(!Config::instance().local_test, preparation.value("playing",false),
        preparation.value("state",0)!=0, publication_failed || preparation.value("leaving",false) ||
        !preparation.value("error",std::string{}).empty() ||
        (preparation.value("playing",false)&&!net::session_wire::connected()), owner);
    const auto left=preparation.value("leave_confirmed",uint64_t{});
    if(left!=last_leave_confirmed){framework_host_visibility.clear();framework_creating_world=false;framework_new_world_requested=false;}
    if(left!=last_leave_confirmed){net::chat::Runtime::instance().set_lobby_admitted(false);last_leave_confirmed=left;playing_close_attempted=false;failure_notice_attempted.clear();failure_notice_error.clear();departed_host_attempt.clear();departed_host_error.clear();local_admitted=false;host_world_pick_requested=false;owner=0;selected_world.clear();selected_file.clear();current_host_world=false;current_host_world_error.clear();load_attempted=false;selected_lobby=0;publication_failed=false;publication_error.clear();published_state.clear();cause.clear();ui_status="Sitzung beendet";}
    if(preparation.value("state",0)!=0)ui_status=preparation.value("label",std::string("Sitzung nicht verfügbar"));
    if(!retire_departed_host(preparation,error))return false;
    native::WorldStatus world;
    if(!native::read_world_status(world,error))return false;
    const auto shared=net::snapshot_progress();
    const auto transfer_phase=shared.value("phase",std::string{});
    lobby_progress::tick(admitted(),lobby_host(),lobby_presentation::host_phase(preparation.value("state",0),
        world.title_transition_pending||(world.active_mode_index!=2&&world.active_mode_index!=3),!selected_world.empty(),transfer_phase,framework_creating_world));
    const bool title_active=world.active_mode_index==2&&!world.title_transition_pending;
    const bool ingame_active=world.active_mode_index==3&&native::in_game();
    if(!title_active&&!ingame_active){ui_owner=nullptr;ui_mode=-1;framework_ready=false;framework_observed_open=false;framework_visibility_known=false;error.clear();return process_panel_toggle(false,error);}
    void* game=nullptr;void* mode=nullptr;void* active=nullptr;
    const size_t mode_offset=title_active?0x350:0x358;
    if(!read(reinterpret_cast<void*>(image),0x13df8f0,game)||!read(game,mode_offset,mode)||!read(game,0x28,active)||mode!=active){error="native UI owner differs from actual active mode";return false;}
    if(ui_owner!=mode||ui_mode!=world.active_mode_index){ui_owner=mode;ui_mode=world.active_mode_index;framework_ready=false;framework_observed_open=false;framework_visibility_known=false;last_ui_status.clear();last_chat_transcript.clear();last_chat_status.clear();}
    if(title_active)title=mode;
    if(!framework_assets_attempted){
        framework_assets_attempted=true;
        if(!framework_assets.prepare(framework_runtime::paths().data))LOG_WARN("[LobbyAssets] {}",framework_assets.diagnostic());
        else LOG_INFO("[LobbyAssets] Native game icons cached once");
        for(auto& [id,widget]:framework_widgets)if(!widget.asset.empty())widget.text=framework_assets.lookup(widget.asset).relative_path;
    }
    if(ingame_active)framework_creating_world=false;
    if(!adopt_current_host_world(game,ingame_active,title_active,preparation,error))return false;
    if(!process_host_world_picker_request(title_active,error))return false;
    if(!process_save_cache_refresh(error))return false;
    if(!close_playing_panel_once(ingame_active,preparation,error)||!show_failed_panel_once(preparation,error)||!process_panel_toggle(true,error)||!render_chat(error))return false;
    if(!Config::instance().local_test&&!publication_failed&&mm.in_lobby()&&mm.is_host()){
        const std::string state=preparation.value("leaving",false)?"leaving":preparation.value("playing",false)?"playing":
            (net::session_wire::connected()||(load_attempted&&mm.get_lobby_members().size()>1))?"preparing":"open";
        if(state!=published_state){std::string detail;if(!mm.publish_lobby_data("lobby_state",state,detail)||!mm.set_lobby_joinable(state=="open",detail))metadata_failure(detail);else published_state=state;}
    }
    auto list_caption=list_status;
    if(choosing&&!list_pending&&cause.empty()&&!Config::instance().local_test&&!framework_host_visibility.empty())
        list_caption+=" - "+framework_host_visibility;
    if(!label("list_status",list_caption)){error=cause;return false;}
    for(size_t slot=0;slot<lobby_ui::page_size;++slot){const auto index=list_page*lobby_ui::page_size+slot;std::string value;bool disabled=true;
        if(choosing){
            if(index<saved_worlds.size()){value=saved_worlds[index];disabled=false;}
        }else if(index<discovered.size()){const auto& row=discovered[index];
            const std::string state=row.state=="open"?"Offen":row.state=="preparing"?"Wird vorbereitet":row.state=="playing"?"Im Spiel":row.state=="leaving"?"Wird beendet":"Nicht verfügbar";
            value=row.host_name+"\n"+row.save_name+" - "+(row.state=="unavailable"?state:std::to_string(row.player_count)+"/"+std::to_string(row.max_players)+" Spieler - "+state);
            disabled=pending||admitted()||list_pending||row.state!="open"||row.player_count<1||row.max_players<=row.player_count;
        }
        const bool present=index<(choosing?saved_worlds.size():discovered.size());
        const auto n=std::to_string(slot);
        for(const auto& id:{"list_text"+n,"find_text"+n})if(!label(id.c_str(),value)){error=cause;return false;}
        for(const auto& id:{"list"+n,"find"+n})if(!disable(id.c_str(),disabled)){error=cause;return false;}
        for(const auto& id:{"list_text"+n,"list"+n,"list_icon"+n,"list_panel"+n,"find_text"+n,"find"+n,"find_icon"+n,"find_panel"+n})
            if(!set_pane_visible(id.c_str(),present)){error=cause;return false;}
    }
    if(!disable("list_refresh",list_pending)||!disable("list_previous",list_pending||!list_page)||!disable("list_next",list_pending||(list_page+1)*lobby_ui::page_size>=(choosing?saved_worlds.size():discovered.size()))){error=cause;return false;}
    const bool multiple_pages=(choosing?saved_worlds.size():discovered.size())>lobby_ui::page_size;
    if(!set_pane_visible("list_previous",multiple_pages)||!set_pane_visible("list_next",multiple_pages)){error=cause;return false;}
    if(publication_failed)ui_status="Host-Metadaten nicht verfügbar";
    if(last_ui_status!=ui_status){if(!label("status",ui_status)){error=cause;return false;}last_ui_status=ui_status;}
    const bool shared_client_world=!Config::instance().is_host&&ingame_active&&admitted()&&preparation.value("ready",false)&&
        shared.value("phase",std::string{})=="active"&&shared.value("world_authority_valid",false)&&
        !preparation.value("snapshot_id",std::string{}).empty()&&shared.value("id",std::string{})==preparation.value("snapshot_id",std::string{});
    std::string world_label=selected_world.empty()?(shared_client_world?"Gemeinsame Welt":"Eure Welt"):selected_world;
    if(!Config::instance().local_test&&!Config::instance().is_host&&admitted()&&selected_world.empty()){
        const auto host_world=mm.get_lobby_data("game_name");
        if(world_selection::valid_name(host_world)&&host_world!="Welt wird ausgewählt")world_label=host_world;
    }
    if(!label("world",world_label)){error=cause;return false;}
    const auto rows=player_rows();
    for(size_t i=0;i<rows.size();++i)if(!label(("player"+std::to_string(i)).c_str(),rows[i])){error=cause;return false;}
    update_avatars();
    if(!disable("host_public",!title_active||pending||admitted())||!disable("search",!title_active||pending||admitted()||list_pending)||!disable("host",!title_active||pending||admitted())||!disable("join",!title_active||pending||admitted())||!disable("pick",!title_active||!lobby_host()||pending||load_attempted)||
       !disable("new_world",!title_active||!lobby_host()||pending||load_attempted||framework_creating_world||framework_new_world_requested)||
       !disable("leave",pending||preparation.value("leaving",false))||
       !disable("start",publication_failed||!lobby_host()||(!current_host_world&&selected_file.empty())||pending||load_attempted||preparation.value("start_requested",false)||!preparation.value("error",std::string{}).empty())){error=cause;return false;}
    if(!render_context(title_active,ingame_active,preparation,shared,error))return false;
    return framework_process_action(error);
}
bool admitted(){return Config::instance().local_test?local_admitted&&net::session_wire::connected():net::SteamMatchmaking::instance().in_lobby();}
std::string status(){return cause.empty()?ui_status:cause;}

bool observe(nlohmann::json& result,std::string& error) {
    if(!image||!thread||GetCurrentThreadId()!=thread){error="native lobby observation requires initialized bound game thread";return false;}
    native::WorldStatus world;if(!native::read_world_status(world,error))return false;
    if(!tick(error)){result["panel_toggle"]=panel_toggle_status();return false;}
    result={{"title_active",world.active_mode_index==2&&!world.title_transition_pending},{"state",status()},
        {"transport",Config::instance().local_test?"local":"steam"},{"admission_source",Config::instance().local_test?"launcher_loopback":"steam_callback"},{"admitted",admitted()},{"host_permission",lobby_host()},
        {"local_admitted",Config::instance().local_test&&local_admitted},{"local_role",Config::instance().local_test?(net::session_wire::local_peer()==1?"host":"client"):"none"},{"local_session_id",Config::instance().local_test?net::session_wire::session_id():0},
        {"local_peer",Config::instance().local_test?net::session_wire::local_peer():0},{"local_peer_mask",Config::instance().local_test?net::session_wire::peer_mask():0},{"wire_connected",net::session_wire::connected()},{"player_rows",player_rows()},{"playing_close_attempted",playing_close_attempted},
        {"generation",generation},{"commands",command_count},{"last_command",last_command},{"command_failures",command_failures},
        {"metadata_failed",publication_failed},{"metadata_error",publication_error},{"discovery_callback_completed",list_callback_completed},{"discovery_callback_ok",list_callback_ok},{"discovery_pending",list_pending},{"discovery_generation",list_generation},{"discovery_state",list_status},{"discovery_page",list_page},{"selected_lobby",selected_lobby},{"lobbies",nlohmann::json::array()},{"callback_pending",pending},{"selected_world",selected_world},{"selected_file",selected_file},{"current_host_world",current_host_world},{"world_selection_open",choosing},{"saved_worlds",saved_worlds},{"prepare",lobby_prepare::status()},
        {"steam_lobby_id",net::SteamMatchmaking::instance().current_lobby_id()},{"steam_host_id",net::SteamMatchmaking::instance().host_steam_id()},
        {"lobby_world_metadata",Config::instance().local_test?selected_world:net::SteamMatchmaking::instance().get_lobby_data("game_name")},
        {"steam_in_lobby",net::SteamMatchmaking::instance().in_lobby()},{"steam_members",net::SteamMatchmaking::instance().get_lobby_members()},
        {"native_panes",nlohmann::json::array()},{"route_receipts",nlohmann::json::array()}};
    for(const auto& row:discovered)result["lobbies"].push_back({{"lobby_id",row.lobby_id},{"host",row.host_name},{"world",row.save_name},{"players",row.player_count},{"slots",row.max_players},{"state",row.state}});
    for(const auto& [token,receipt]:route_receipts)result["route_receipts"].push_back(receipt);
    result["ui_mode"]=ui_mode;result["ui_owner_current"]=ui_owner!=nullptr;
    result["snapshot"]=net::snapshot_progress();result["controller"]=typed_action_runtime::status();
    result["panel_toggle"]=panel_toggle_status();
    result["entry_owner"]="framework_native_app";
    result["framework_app"]=framework_app_response;
    result["new_world"]={{"active",framework_creating_world},{"requested",framework_new_world_requested},{"handoff",framework_new_world_receipt},{"observation",framework_new_world_observation}};
    result["panel_visible"]=framework_visibility_known?nlohmann::json(framework_visible):nlohmann::json();
    result["physical_handshake"]=net::session_wire::connected();
    framework_layout();
    result["loading_ui"]={{"heading",framework_widgets.at("load_status").text},{"detail",framework_widgets.at("load_detail").text},
        {"host_phase",unsigned(lobby_progress::remote_phase())}};
    result["avatar_paths"]=framework_avatar_paths;
    result["asset_cache"]={{"attempted",framework_assets_attempted},{"error",framework_assets.diagnostic()}};
    result["ui_page_size"]=lobby_ui::page_size;
    const auto chat=net::chat::Runtime::instance().snapshot();
    result["chat"]={{"connected",chat.connected},{"retired",chat.retired},{"conversation",chat.conversation},
        {"last_submit_sequence",chat_last_sequence},{"draft_clear_failed",chat_draft_clear_failed},{"error",chat_error}};
    result["panel_registered"]=framework_ready;
    result["framework_ui"]={{"app_id","main"},{"ready",framework_ready},{"open",framework_observed_open},{"requested_open",framework_open},{"visibility_known",framework_visibility_known},{"visible",framework_visibility_known?nlohmann::json(framework_visible):nlohmann::json()},{"suspended",framework_ready?nlohmann::json(framework_suspended):nlohmann::json()},{"browser",framework_browser},{"queued",framework_queued},{"processed",framework_processed},{"rejected",framework_rejected},{"last_action",framework_last_action},{"native_response",framework_response}};
    error.clear();return true;
}

namespace {
bool release_route(void* mode,std::string& error){
    if(!image||GetCurrentThreadId()!=thread||ui_mode!=3||!mode||mode!=ui_owner){error="release fixture actual active native Lobby Mode/thread differs";return false;}
    class Memory final:public initial_release::Memory {
        placement_ui_probe::ProcessMemory native_;
        bool copy(uintptr_t p,void* out,size_t n,std::string& e)override{return native_.copy(p,out,n,e);}
    } memory;
    if(!framework_host||!framework_command_token||!framework_routes.code_verify){error="release fixture Framework Mode observer is not registered";return false;}
    MT2FW_MT2_RouteReceipt receipt{};receipt.size=sizeof(receipt);
    const auto verified=framework_routes.code_verify(framework_routes.context,"ingame.command",0,initial_release::mode_bytes::body,uint32_t(sizeof(initial_release::mode_bytes::body)),&receipt);
    if(verified!=MT2FW_MT2_OK||receipt.state!=MT2FW_MT2_ROUTE_ENABLED||receipt.rva!=0x4a2f90||receipt.entry!=image+0x4a2f90||!receipt.detour||!receipt.trampoline||
       std::string_view(receipt.owner,9)!="framework"||receipt.owner[9]!=0||
       std::string_view(receipt.game_sha256,64)!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"||receipt.game_sha256[64]!=0){
        error="release fixture Framework shared Mode physical route proof failed result="+std::to_string(verified)+" cause="+std::string(receipt.cause,std::find(receipt.cause,receipt.cause+sizeof(receipt.cause),'\0'));return false;
    }
    return initial_release::owned_mode_route(memory,image,receipt.detour,receipt.trampoline,error);
}
}

bool initial_release_window(void* mode,void** window,std::string& error){
    if(!window){error="release fixture window output absent";return false;}*window=nullptr;
    if(!release_route(mode,error))return false;
    const auto fault=find_window("release_hud",window);
    if(fault||!*window){error="release fixture original registered ReleaseHUD lookup failed SEH="+std::to_string(fault);return false;}
    error.clear();return true;
}
bool begin_initial_release_fixture(void* mode,std::string& error){
    if(release_fixture_active){error="release fixture original callback scope already active";return false;}
    if(!release_route(mode,error))return false;
    release_fixture_mode=mode;release_fixture_callbacks=0;release_fixture_error.clear();framework_command_exception=false;release_fixture_active=true;return true;
}
bool end_initial_release_fixture(uint64_t& callbacks,std::string& error){
    callbacks=release_fixture_callbacks;
    if(!release_fixture_active){error="release fixture original callback scope absent";return false;}
    void* mode=release_fixture_mode;release_fixture_active=false;release_fixture_mode=nullptr;
    if(framework_command_exception){error="release fixture Mode observer callback threw; original forwarding was not changed";return false;}
    if(!release_fixture_error.empty()){error=release_fixture_error;return false;}
    return release_route(mode,error);
}
}
