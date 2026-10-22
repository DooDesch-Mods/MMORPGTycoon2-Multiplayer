bool fail(const std::string& detail) {
    ++command_failures;
    if(cause!=detail)LOG_ERROR("[NativeLobby] {}",detail);
    cause=detail;ui_status="Session unavailable";return false;
}

bool metadata_failure(const std::string& detail){publication_failed=true;publication_error=detail;return fail(detail);}

std::array<std::string,5> chat_speaker_names(const net::chat::Snapshot& snapshot){
    // Retain known names during a disconnect, but never reuse them in a new conversation.
    static uint64_t session=0,conversation=0;
    static uint8_t local=0;
    static std::array<std::string,5> names;
    if(session!=snapshot.session||conversation!=snapshot.conversation||local!=snapshot.local_peer){
        session=snapshot.session;conversation=snapshot.conversation;local=snapshot.local_peer;
        for(size_t peer=1;peer<names.size();++peer)
            names[peer]=peer==local?"You":peer==1?"Host":"Player "+std::to_string(peer);
    }
    if(!Config::instance().local_test&&snapshot.connected){
        std::array<uint64_t,5> ids{};uint64_t lobby=0,mine=0;std::string error;
        if(net::session_wire::read_binding(ids,lobby,mine,error)&&
           snapshot.session==net::session_wire::session_id()&&mine==local){
            for(size_t peer=1;peer<names.size();++peer)if(peer!=local&&ids[peer]){
                const auto name=net::SteamNetworking::instance().get_persona_name(ids[peer]);
                if(!name.empty()&&name!=std::to_string(ids[peer]))names[peer]=name;
            }
        }
    }
    return names;
}

std::string chat_transcript(const net::chat::Snapshot& snapshot){
    const auto names=chat_speaker_names(snapshot);
    std::string result;
    const auto first=snapshot.entries.size()>3?snapshot.entries.size()-3:0;
    for(size_t i=first;i<snapshot.entries.size();++i){
        const auto& entry=snapshot.entries[i];
        if(!result.empty())result+='\n';
        result+=entry.origin>0&&entry.origin<names.size()?names[entry.origin]:"Player";
        result+=": "+entry.text;
        if(entry.origin==snapshot.local_peer){
            if(entry.delivery==net::chat::Delivery::Queued)result+=" - Sending";
            else if(entry.delivery==net::chat::Delivery::Failed)result+=" - Not delivered";
        }
    }
    return result;
}

bool send_chat(){
    auto& runtime=net::chat::Runtime::instance();
    const auto reject=[](const std::string& detail){
        ++command_failures;
        if(chat_error!=detail)LOG_WARN("[NativeLobby] Chat send failed: {}",detail);
        chat_error=detail;return false;
    };
    if(!admitted()||!runtime.snapshot().connected)return reject("chat requires confirmed lobby and current conversation");
    std::string draft;
    if(!input_text("chat_input",draft,net::chat::max_text_bytes))
        return reject("native chat input unreadable or exceeds240 UTF-8 bytes");
    if(!net::chat::valid_text(draft))return reject("chat text must contain1..240 UTF-8 bytes without control characters");
    std::string error;uint64_t sequence=0;
    if(!runtime.submit(draft,error,&sequence))return reject(error);
    chat_last_sequence=sequence;
    if(!label("chat_input","")){
        chat_draft_clear_failed=true;
        chat_error="chat queued sequence="+std::to_string(sequence)+" but native draft clear failed; no replay: "+cause;
        return false;
    }
    chat_draft_clear_failed=false;chat_error.clear();return true;
}

bool render_chat(std::string& error){
    const auto snapshot=net::chat::Runtime::instance().snapshot();
    const auto transcript=chat_transcript(snapshot);
    const std::string state=snapshot.retired?"Chat ended":chat_draft_clear_failed?"Chat - message queued, draft not cleared":!chat_error.empty()?"Chat - message not sent":
        snapshot.connected?"Chat":admitted()?"Chat connecting":"Chat - join a lobby first";
    if(last_chat_transcript!=transcript){
        if(!label("chat_transcript",transcript)){error=cause;return false;}
        last_chat_transcript=transcript;
    }
    if(last_chat_status!=state){if(!label("chat_status",state)){error=cause;return false;}last_chat_status=state;}
    if(!disable("chat_send",!admitted()||!snapshot.connected)){error=cause;return false;}
    return true;
}

nlohmann::json panel_toggle_status(){
    return {{"requested",toggle_requested.load()},{"processed",toggle_processed},{"attempted",toggle_attempted},
        {"returned",toggle_returned},{"confirmed",toggle_confirmed},{"failed",toggle_failed},
        {"last_request",toggle_processed},{"last_error",toggle_error},{"first_error",toggle_first_error}};
}

bool close_playing_panel_once(bool ingame_active,const nlohmann::json& preparation,std::string& error){
    if(!ingame_active||!preparation.value("playing",false)||playing_close_attempted)return true;
    playing_close_attempted=true;
    const auto exception=view_show(false,false);
    const auto browser_exception=view_show(true,false);
    if(exception||browser_exception){error="native playing panel close failed exception="+std::to_string(exception)+" browser_exception="+std::to_string(browser_exception);return fail(error);}
    return true;
}

bool show_failed_panel_once(const nlohmann::json& preparation,std::string& error){
    const auto detail=preparation.value("error",std::string{});
    if(detail.empty()||preparation.value("leaving",false))return true;
    cause=detail;ui_status="Session ended: sync error";
    if(failure_notice_attempted==detail){error=failure_notice_error;if(!error.empty())cause=error;return error.empty();}
    failure_notice_attempted=detail;failure_notice_error.clear();
    const auto exception=view_show(false,true);
    if(exception){error="native failed session panel show failed exception="+std::to_string(exception)+" original="+detail;failure_notice_error=error;return fail(error);}
    LOG_ERROR("[NativeLobby] Session ended: {}",detail);return true;
}

bool refresh_saved_worlds(std::string& error) {
    using Enumerate=char**(*)(const char*);
    using FreeList=void(*)(void*);
    using IsDirectory=int(*)(const char*);
    const auto dll=GetModuleHandleW(L"libphysfs.dll");
    const auto enumerate=dll?reinterpret_cast<Enumerate>(GetProcAddress(dll,"PHYSFS_enumerateFiles")):nullptr;
    const auto release=dll?reinterpret_cast<FreeList>(GetProcAddress(dll,"PHYSFS_freeList")):nullptr;
    const auto is_directory=dll?reinterpret_cast<IsDirectory>(GetProcAddress(dll,"PHYSFS_isDirectory")):nullptr;
    if(!enumerate||!release||!is_directory){error="native save enumeration exports unavailable";return false;}
    saved_worlds.clear();
    if(!is_directory("user/save")){error.clear();return true;}
    auto** worlds=enumerate("user/save");
    if(!worlds){error="native save directory enumeration failed";return false;}
    for(auto** item=worlds;*item;++item){
        const std::string name=*item;
        if(!world_selection::valid_name(name))continue;
        const auto path="user/save/"+name;
        if(!is_directory(path.c_str()))continue;
        auto** saves=enumerate(path.c_str());
        if(!saves){release(worlds);error="native save enumeration failed world="+name;return false;}
        bool found=false;
        for(auto** save=saves;*save;++save)if(world_selection::save_file(*save)){found=true;break;}
        release(saves);
        if(found)saved_worlds.push_back(name);
    }
    release(worlds);
    std::sort(saved_worlds.begin(),saved_worlds.end());
    error.clear();return true;
}

void begin_save_cache_refresh(){
    framework_save_refresh.begin(std::chrono::steady_clock::now());
    framework_save_refresh_owner=ui_owner;framework_save_refresh_admission=generation;
    list_pending=true;++list_generation;list_page=0;saved_worlds.clear();list_status="Looking for saves";
}

bool process_save_cache_refresh(std::string& error){
    if(!framework_save_refresh.waiting())return true;
    if(!choosing||ui_mode!=2||ui_owner!=framework_save_refresh_owner||generation!=framework_save_refresh_admission){
        framework_save_refresh.cancel();list_pending=false;
        LOG_INFO("[NativeLobby] Picker refresh observer cancelled; original cache task retained");return true;
    }
    void* task=nullptr;
    if(!read(reinterpret_cast<void*>(image),save_cache_guard::task_rva,task)){
        framework_save_refresh.cancel();list_pending=false;error="native save cache task pointer unreadable";return fail(error);
    }
    const auto action=framework_save_refresh.tick(task!=nullptr,std::chrono::steady_clock::now());
    if(action==SaveCacheRefresh::Action::TimedOut){
        list_pending=false;list_status="Could not load saves. Refresh the list.";
        error="native save cache refresh timed out required<120s; original task retained";return fail(error);
    }
    if(action==SaveCacheRefresh::Action::Queue){
        const auto* body=reinterpret_cast<const uint8_t*>(image+save_cache_guard::rva);
        if(!readable(body,save_cache_guard::bytes)||mt2mp::sha256(std::vector<uint8_t>(body,body+save_cache_guard::bytes))!=save_cache_guard::sha256){
            framework_save_refresh.cancel();list_pending=false;error="native save cache rebuild function hash differs";return fail(error);
        }
        const auto exception=rebuild_save_cache();
        if(exception){framework_save_refresh.cancel();list_pending=false;error="native save cache rebuild exception="+std::to_string(exception)+"; no replay";return fail(error);}
        LOG_INFO("[NativeLobby] Picker native save cache refresh queued once");
    }
    if(action==SaveCacheRefresh::Action::Ready){
        list_pending=false;
        if(!refresh_saved_worlds(error))return fail(error);
        list_status=saved_worlds.empty()?"No saved worlds yet":"Saved worlds";
        if(cause.starts_with("native save cache")||cause.starts_with("native latest world save unavailable"))cause.clear();
        LOG_INFO("[NativeLobby] Picker native save cache refresh completed worlds={}",saved_worlds.size());
    }
    error.clear();return true;
}

bool start_host_world(std::string& error);

bool select_world(void* mode,const std::string& command) {
    const std::string prefix="Connect ";
    if(!command.starts_with(prefix))return false;
    std::string world=command.substr(prefix.size());
    if(world.size()>=2&&world.front()=='"'&&world.back()=='"')world=world.substr(1,world.size()-2);
    if(!world_selection::valid_name(world)) {fail("native world selection name invalid");return true;}
    String output{};bool returned=false;DWORD code=latest_save(mode,world.c_str(),&output,&returned);
    std::string stem;const bool valid=!code&&returned&&text(&output,stem,200);
    if(returned){const auto cleanup=dispose(&output);if(!code)code=cleanup;}
    if(!valid||code||stem.empty()||stem.find_first_of("/\\\"\r\n\t")!=std::string::npos){list_status="Cannot read this save. Choose another world.";fail("native latest world save unavailable exception="+std::to_string(code));return true;}
    selected_world=world;selected_file="save/"+world+"/"+stem+".vrz";choosing=false;
    std::string metadata_error;if(!Config::instance().local_test&&!net::SteamMatchmaking::instance().publish_lobby_data("game_name",world,metadata_error)){metadata_failure(metadata_error);return true;}
    const auto hidden=view_show(true,false);
    if(hidden){fail("native world selection window close failed exception="+std::to_string(hidden));return true;}
    const auto shown=view_show(false,true);
    if(shown)fail("native lobby show after world selection failed exception="+std::to_string(shown));
    else {
        if(cause.starts_with("native save cache")||cause.starts_with("native latest world save unavailable"))cause.clear();
        LOG_INFO("[NativeLobby] Shared world load selected path={}",selected_file);
        std::string error;
        if(!start_host_world(error))return true;
    }
    return true;
}

bool lobby_host(){return admitted()&&(Config::instance().local_test?Config::instance().peer_id==1:net::SteamMatchmaking::instance().is_host());}

bool start_host_world(std::string& error){
    if(!lobby_host()||(!current_host_world&&selected_file.empty())){error="start requires confirmed Host and selected world";return fail(error);}
    if(publication_failed){error=publication_error;return fail(error);}
    if(load_attempted){
        const auto preparation=lobby_prepare::status();
        error=preparation.value("error",std::string{});
        if(!error.empty())return fail(error);
        if(preparation.value("start_requested",false)){error.clear();return true;}
        error="native world start already attempted; no replay";return fail(error);
    }
    // Pin the attempted source before a native request so observation cannot replay a failure.
    load_attempted=true;
    if(!(current_host_world?lobby_prepare::prepare_current(error):lobby_prepare::prepare(selected_file,error)))return fail(error);
    if(!lobby_prepare::start(error))return fail(error);
    ui_status="Preparing session";error.clear();return true;
}

bool adopt_current_host_world(void* game,bool ingame_active,bool title_active,const nlohmann::json& preparation,std::string& error){
    if(!lobby_host()||load_attempted||preparation.value("state",0)!=0)return true;
    if(publication_failed){error=publication_error;return false;}
    if(!lobby_operations::executor_idle())return true;
    if(!ingame_active){
        if(title_active&&current_host_world){
            std::string detail;
            if(!Config::instance().local_test&&!net::SteamMatchmaking::instance().publish_lobby_data("game_name","Welt wird ausgewählt",detail)){error=detail;return metadata_failure(error);}
            current_host_world=false;selected_world.clear();selected_file.clear();
        }
        return true;
    }
    std::string name;
    // Native CreateGame and Title Load both acknowledge this game-folder string.
    if(!text(static_cast<unsigned char*>(game)+0x470,name,200)||!world_selection::valid_name(name)){
        error="active Host world name is unreadable or invalid";
        if(current_host_world_error!=error)LOG_ERROR("[NativeLobby] {}",error);
        current_host_world_error=error;return false;
    }
    if(!current_host_world_error.empty()){LOG_INFO("[NativeLobby] Active Host world name recovered");current_host_world_error.clear();}
    if(current_host_world&&selected_world==name)return true;
    std::string detail;
    if(!Config::instance().local_test&&!net::SteamMatchmaking::instance().publish_lobby_data("game_name",name,detail)){error=detail;return metadata_failure(error);}
    selected_world=name;selected_file.clear();current_host_world=true;current_host_world_error.clear();choosing=false;
    ui_status="World selected";
    LOG_INFO("[NativeLobby] Active Host world selected name={}; starting shared preparation",name);
    return start_host_world(error);
}

bool admit_local(bool host){
    const auto peer=net::session_wire::local_peer();
    if(!net::session_wire::connected()||peer<1||peer>4)return fail("local lobby admission awaits confirmed launcher peer membership");
    if(host!=(peer==1))return fail("local lobby action differs from launcher-owned Host/Client role");
    std::string error;if(!net::join_local_lobby(error))return fail(error);
    net::chat::Runtime::instance().set_lobby_admitted(true);chat_error.clear();chat_last_sequence=0;chat_draft_clear_failed=false;
    local_admitted=true;Config::instance().is_host=peer==1;host_world_pick_requested=host&&selected_world.empty()&&!current_host_world;++generation;cause.clear();ui_status="Local lobby open";return true;
}

void update_avatars(){
    if(!admitted()){
        framework_avatar_paths.fill({});
        for(size_t slot=0;slot<4;++slot)framework_widgets.at("avatar"+std::to_string(slot)).visible=false;
        return;
    }
    static auto next=std::chrono::steady_clock::time_point{};
    if(std::chrono::steady_clock::now()<next)return;
    next=std::chrono::steady_clock::now()+std::chrono::seconds(1);
    if(!framework_avatar_attempted){
        framework_avatar_attempted=true;
        framework_avatar_ready=framework_avatars.configure(framework_runtime::paths().data,framework_avatar_error);
        if(!framework_avatar_ready)LOG_WARN("[LobbyAvatars] {}",framework_avatar_error);
    }
    if(!framework_avatar_ready)return;
    std::vector<uint64_t> members;
    std::array<uint64_t,4> slots{};
    if(admitted()){
        if(Config::instance().local_test){
            using Access=void*(*)();using GetId=uint64_t(*)(void*);
            const auto module=net::steam_module();
            const auto access=module?reinterpret_cast<Access>(GetProcAddress(module,"SteamAPI_SteamUser_v023")):nullptr;
            const auto get_id=module?reinterpret_cast<GetId>(GetProcAddress(module,"SteamAPI_ISteamUser_GetSteamID")):nullptr;
            auto* user=access?access():nullptr;
            const auto self=user&&get_id?get_id(user):0;
            const auto peer=net::session_wire::local_peer();
            if(self&&peer>=1&&peer<=4){members.push_back(self);slots[peer-1]=self;}
        }else{
            members=lobby_ui::ordered_members(net::SteamMatchmaking::instance().get_lobby_members(),owner);
            for(size_t i=0;i<slots.size()&&i<members.size();++i)slots[i]=members[i];
        }
    }
    std::string error;
    if(!framework_avatars.set_members(members,error)){
        if(error!=framework_avatar_error)LOG_WARN("[LobbyAvatars] {}",error);
        framework_avatar_error=error;return;
    }
    if(!framework_avatar_error.empty()){LOG_INFO("[LobbyAvatars] Roster recovered");framework_avatar_error.clear();}
    for(size_t slot=0;slot<slots.size();++slot){
        const auto result=slots[slot]?framework_avatars.poll(slots[slot]):net::avatars::Result{};
        auto& widget=framework_widgets.at("avatar"+std::to_string(slot));
        const auto path=result.state==net::avatars::State::Ready?result.relative_path:std::string{};
        if(path!=framework_avatar_paths[slot]&&!path.empty())LOG_INFO("[LobbyAvatars] Native image ready slot={} width={} height={}",slot+1,result.width,result.height);
        framework_avatar_paths[slot]=path;
        widget.text=path.empty()?framework_assets.lookup("person").relative_path:path;widget.visible=admitted();
        auto& previous=framework_avatar_errors[slot];
        const auto problem=result.state==net::avatars::State::Error?result.cause:std::string{};
        if(problem!=previous){
            if(problem.empty()){if(!previous.empty())LOG_INFO("[LobbyAvatars] Image recovered slot={}",slot+1);}
            else LOG_WARN("[LobbyAvatars] slot={} cause={}",slot+1,problem);
            previous=problem;
        }
    }
}

std::array<std::string,4> player_rows(){
    std::array<std::string,4> rows;rows.fill("Open slot");
    framework_host_name.clear();
    if(Config::instance().local_test){
        const auto mask=admitted()?net::session_wire::peer_mask():0;
        if(mask&1)framework_host_name="Player 1";
        for(size_t i=0;i<4;++i)if(mask&(1u<<i))rows[i]=(i==0?"Host":"Player "+std::to_string(i+1))+(net::session_wire::local_peer()==i+1?std::string(" (You)"):std::string{});
    }else{
        const auto members=lobby_ui::ordered_members(net::SteamMatchmaking::instance().get_lobby_members(),owner);
        const auto self=net::SteamNetworking::instance().local_steam_id();
        for(size_t i=0;i<rows.size()&&i<members.size();++i){
            const auto name=net::SteamNetworking::instance().get_persona_name(members[i]);
            if(members[i]==owner)framework_host_name=name;
            rows[i]=name+(members[i]==self?" (You)":"")+(members[i]==owner?"\nHost":"");
        }
    }
    return rows;
}

bool open_world_picker(){
    if(ui_mode!=2||pending||load_attempted||!lobby_host())return fail("world selection rejected: Host with idle Title lobby required");
    choosing=true;begin_save_cache_refresh();
    const auto hidden=view_show(false,false);
    if(hidden){choosing=false;return fail("native world selection lobby hide failed exception="+std::to_string(hidden));}
    const auto shown=view_show(true,true);
    if(shown){choosing=false;return fail("native world selection browser show failed exception="+std::to_string(shown));}
    return true;
}

bool open_native_new_world(){
    if(ui_mode!=2||pending||load_attempted||framework_creating_world||framework_new_world_requested||!lobby_host())
        return fail("New World requires the current idle Title Host lobby");
    framework_new_world_owner=ui_owner;framework_new_world_generation=generation;
    framework_new_world_requested=true;return true;
}

bool render_native_new_world(std::string& error){
    if(!framework_new_world_requested)return true;
    framework_new_world_requested=false;
    if(ui_owner!=framework_new_world_owner||generation!=framework_new_world_generation||ui_mode!=2||
       pending||load_attempted||framework_creating_world||!lobby_host()){
        error="New World UI callback owner or admission changed before native launch; no replay";return fail(error);
    }
    const auto receipt=framework_request("ui.new_world",nlohmann::json::object());
    framework_new_world_receipt=receipt;
    const auto state=receipt.value("status",std::string{});
    if(state=="rejected"&&receipt.contains("confirmed")&&receipt.at("confirmed").is_boolean()&&!receipt.at("confirmed").get<bool>()&&
       receipt.contains("attempted")&&receipt.at("attempted").is_boolean()&&!receipt.at("attempted").get<bool>()&&
       receipt.contains("cause")&&receipt.at("cause").is_string()&&!receipt.at("cause").get_ref<const std::string&>().empty()){
        // An unattempted rejection ends this action without invalidating the update loop.
        const auto detail="New World native wizard handoff was not confirmed: "+receipt.dump();
        const bool action_failed=!fail(detail);
        framework_last_action["handler_failed"]=action_failed;
        framework_last_action["callback_pending"]=false;
        framework_last_action["error"]=detail;
        list_status="Could not open new world setup.";
        // The recorded action failure is handled; only the render callback succeeds.
        error.clear();return action_failed;
    }
    if(!receipt.value("confirmed",false)||(state!="opened"&&state!="already_open")||
       receipt.value("mode",0)!=2||receipt.value("window",std::string{})!="NewGame"||!receipt.value("visible",false))
        {error="New World native wizard handoff was not confirmed: "+receipt.dump();return fail(error);}
    framework_creating_world=true;choosing=false;cause.clear();ui_status="Host is creating a world";
    if(view_show(false,false)||view_show(true,false))return fail("New World native wizard opened but Multiplayer could not be hidden; no launch replay");
    return true;
}

bool observe_native_new_world(bool title_active,bool ingame_active,std::string& error){
    if(!framework_creating_world)return true;
    if(ingame_active){framework_creating_world=false;return true;}
    if(!title_active)return true;
    native::WorldStatus activity;
    if(!native::read_world_status(activity,error)){
        error="New World transition observation failed: "+error;return fail(error);
    }
    // The original Create command can request InGame before the cached UI mode changes.
    if(activity.active_mode_index!=2||activity.requested_mode_index!=2||activity.title_transition_pending)return true;
    const auto receipt=framework_request("ui.new_world",{{"observe",true}});
    framework_new_world_observation=receipt;
    if(!receipt.value("confirmed",false)||receipt.value("status",std::string{})!="observed"||
       receipt.value("attempted",true)||receipt.value("mode",0)!=2||receipt.value("window",std::string{})!="NewGame"){
        error="New World visibility observation failed: "+receipt.dump();return fail(error);
    }
    if(receipt.at("visible").get<bool>())return true;
    framework_creating_world=false;
    if(lobby_host()&&!pending&&!load_attempted&&!open_world_picker()){error=cause;return false;}
    return true;
}

bool process_host_world_picker_request(bool title_active,std::string& error){
    if(!host_world_pick_requested)return true;
    host_world_pick_requested=false;
    if(title_active&&lobby_host()&&!pending&&!load_attempted&&!current_host_world&&selected_world.empty()&&selected_file.empty()){
        if(!open_world_picker()){error=cause;return false;}
    }
    return true;
}

bool render_context(bool title_active,bool ingame_active,const nlohmann::json& preparation,const nlohmann::json& transfer,std::string& error){
    const bool member=admitted(),host=lobby_host();
    if(!label("build_version","v"+std::string(MOD_VERSION))){error=cause;return false;}
    const auto lobby_id=net::SteamMatchmaking::instance().current_lobby_id();
    if(!label("session_id",member&&!Config::instance().local_test&&lobby_id?"Lobby ID: "+std::to_string(lobby_id):std::string{})){error=cause;return false;}
    const bool choose=title_active&&host&&!load_attempted;
    std::string entry_notice;
    if(pending)entry_notice=ui_status;
    else if(ui_status=="Invalid lobby ID")entry_notice="Enter a valid lobby ID";
    else if(!cause.empty()){
        if(cause=="Steam lobby interface unavailable"||cause=="Steam discovery interface unavailable")entry_notice="Steam is unavailable.";
        else if(last_command=="MT2MPHost"||last_command=="MT2MPHostPublic")entry_notice="Could not open the lobby.";
        else if(last_command=="MT2MPJoin")entry_notice="Could not join the lobby.";
        else if(last_command=="MT2MPSearch"||last_command=="MT2MPRefresh")entry_notice="Could not load games.";
        else entry_notice="Could not complete the action.";
    }
    if(!label("entry_state",entry_notice)){error=cause;return false;}
    if(!set_pane_visible("invite",member&&!Config::instance().local_test&&!load_attempted)||
       !disable("invite",pending||preparation.value("leaving",false))||
       !set_pane_visible("pick",choose)||!set_pane_visible("start",false)||
       !set_pane_visible("leave",member||!preparation.value("error",std::string{}).empty())||
       !button_label("leave",ingame_active?"Leave game":"Leave lobby")){
        error=cause;return false;
    }
    const auto local_phase=lobby_presentation::host_phase(preparation.value("state",0),false,!selected_world.empty(),transfer.value("phase",std::string{}),framework_creating_world);
    const auto message=lobby_presentation::describe(host,member,pending,preparation.value("state",0),host?local_phase:lobby_progress::remote_phase(),
        transfer.value("phase",std::string{}),transfer.value(host?"bytes_acknowledged_min":"bytes_written",uint64_t{}),transfer.value("bytes_total",uint64_t{}),!cause.empty());
    auto detail=message.detail;
    if(preparation.value("playing",false)&&cause.empty()&&!preparation.value("leaving",false)){
        detail=framework_host_name.empty()?std::string{}:"Host: "+framework_host_name;
        const auto append_fact=[&](const std::string& value){if(!detail.empty())detail+='\n';detail+=value;};
        if(Config::instance().local_test)append_fact("Connection: Local");
        else {
            if(host&&!framework_host_visibility.empty())append_fact(framework_host_visibility);
        }
    }
    if(!label("load_status",message.heading)||!label("load_detail",detail)||
       !label("browser_heading",choosing?"Choose a world":"Find a game")){
        error=cause;return false;
    }
    size_t count=0;
    if(Config::instance().local_test){const auto mask=member?net::session_wire::peer_mask():0;for(unsigned i=0;i<4;++i)if(mask&(1u<<i))++count;}
    else if(member)count=net::SteamMatchmaking::instance().get_lobby_members().size();
    if(!label("players_heading","Player "+std::to_string(count)+"/4")){error=cause;return false;}
    if(member&&message.busy&&!framework_busy_notice&&!framework_report&&!(host&&framework_creating_world)){framework_open=true;framework_browser=false;}
    framework_busy_notice=member&&message.busy;
    if(member&&!host&&title_active&&preparation.value("state",0)==0&&cause.empty())ui_status=message.heading;
    error.clear();return true;
}

bool ready_for_start() {
    return lobby_prepare::status().value("ready",false);
}

void dispatch_action(void* mode,std::string cmd);
void process_steam_invite(bool title_active){
    if(!pending_steam_invite)return;
    const auto target=pending_steam_invite;pending_steam_invite=0;
    framework_open=true;framework_browser=false;last_command="MT2MPJoin";
    if(!title_active||pending||admitted())fail("Steam invite requires the main menu without an active lobby or pending admission");
    else if(label("lobby_id",std::to_string(target)))dispatch_action(ui_owner,"MT2MPJoin");
}

void dispatch_action(void* mode,std::string cmd) {
    if(GetCurrentThreadId()!=thread||mode!=ui_owner){fail("Framework lobby action outside bound mode/thread");return;}
    ++command_count;last_command=cmd;
    if(cmd=="MT2MPJoin"||cmd=="MT2MPSearch"||cmd=="MT2MPRefresh"||cmd=="MT2MPInvite")
        LOG_INFO("[NativeLobby] Lobby action={} mode={} pending={} admitted={}",cmd,ui_mode,pending,admitted());
    auto& mm=net::SteamMatchmaking::instance();
    if(publication_failed&&cmd!="MT2MPLeave"&&cmd!="MT2MPOpen"&&cmd!="MT2MPChatSend"){fail("Host metadata unavailable: "+publication_error);return;}
    if(cmd=="MT2MPChatSend"){const auto failed=command_failures;if(!send_chat()&&command_failures==failed)++command_failures;return;}
    if(cmd=="MT2MPOpen"){framework_open=true;framework_browser=false;return;}
    if(cmd=="MT2MPRefresh"&&choosing){
        begin_save_cache_refresh();return;
    }
    if(cmd=="MT2MPSearch"||cmd=="MT2MPRefresh"){
        if(ui_mode!=2||pending||admitted()||list_pending){fail("native discovery requires idle Title and no pending request");return;}
        if(!net::SteamNetworking::instance().init()||!mm.init()){fail("Steam discovery interface unavailable");return;}
        if(view_show(false,false)||view_show(true,true)){fail("native discovery window visibility failed");return;}
        list_pending=true;list_callback_completed=list_callback_ok=false;cause.clear();list_page=0;selected_lobby=0;discovered.clear();list_status="Looking for lobbies";const auto current=++list_generation;
        mm.request_lobby_list([current](bool okay,const std::vector<net::LobbyInfo>& rows){
            if(current!=list_generation){LOG_WARN("[NativeLobby] Stale discovery callback discarded generation={}",current);return;}
            list_pending=false;list_callback_completed=true;list_callback_ok=okay;discovered=rows;list_page=0;
            list_status=okay?(rows.empty()?"No public lobbies found":"Public lobbies: "+std::to_string(rows.size())):"Lobby search failed";
            if(!okay)LOG_ERROR("[NativeLobby] Steam discovery callback failed; see SDK callback/timeout cause");
            for(auto& [token,receipt]:route_receipts)if(receipt.value("discovery_generation",uint64_t{})==current){receipt["callback_pending"]=false;receipt["callback_completed"]=true;receipt["backend_confirmed"]=okay;receipt["lobby_count"]=rows.size();}
        });return;
    }
    if(cmd=="MT2MPListBack"){choosing=false;if(view_show(true,false)||view_show(false,true))fail("native discovery Back visibility failed");return;}
    if(cmd=="MT2MPListPrevious"||cmd=="MT2MPListNext"){
        if(list_pending){fail("native discovery page awaits callback");return;}
        if(cmd=="MT2MPListPrevious"){if(list_page) --list_page;}
        else if((list_page+1)*lobby_ui::page_size<(choosing?saved_worlds.size():discovered.size()))++list_page;return;
    }
    if(cmd.size()==10&&cmd.starts_with("MT2MPList")&&cmd.back()>='0'&&size_t(cmd.back()-'0')<lobby_ui::page_size){
        const auto index=list_page*lobby_ui::page_size+size_t(cmd.back()-'0');
        if(choosing){
            if(index>=saved_worlds.size()){fail("native selected world absent from current list");return;}
            select_world(mode,"Connect "+saved_worlds[index]);return;
        }
        if(ui_mode!=2||pending||admitted()){fail("lobby row Join requires idle Title and no pending admission or active lobby");return;}
        if(list_pending||index>=discovered.size()){fail("native selected lobby is not in completed current discovery");return;}
        const auto& row=discovered[index];
        if(row.state!="open"||row.player_count<1||row.max_players<=row.player_count){fail("native selected lobby is not open or has no free slot");return;}
        selected_lobby=row.lobby_id;
        if(!label("lobby_id",std::to_string(selected_lobby))||view_show(true,false)||view_show(false,true)){fail("native selected lobby field/visibility failed");return;}
        // Continue through the same callback-confirmed admission path as manual Join.
        cmd="MT2MPJoin";
    }
    if(cmd=="MT2MPPickWorld") {open_world_picker();return;}
    if(cmd=="MT2MPNewWorld") {open_native_new_world();return;}
    if(cmd=="MT2MPHost"||cmd=="MT2MPHostPublic"||cmd=="MT2MPJoin") {
        if(ui_mode!=2||pending||admitted()){fail("lobby admission rejected: idle Title and no active lobby required");return;}
        if(Config::instance().local_test){admit_local(cmd!="MT2MPJoin");return;}
        if(!net::SteamNetworking::instance().init()||!mm.init()){fail("Steam lobby interface unavailable");return;}
        pending=true;cause.clear();ui_status="Connecting";const auto current=++generation;
        const bool host=cmd!="MT2MPJoin";const bool public_lobby=cmd=="MT2MPHostPublic";
        const auto completion=[current,host,public_lobby](bool ok,uint64_t id){
            if(current!=generation){LOG_WARN("[NativeLobby] Cancelled admission callback discarded generation={}",current);return;}
            for(auto& [token,receipt]:route_receipts)if(receipt.value("admission_generation",uint64_t{})==current){
                receipt["callback_pending"]=false;receipt["callback_completed"]=true;receipt["backend_confirmed"]=ok&&id!=0;receipt["steam_lobby_id"]=id;
            }
            pending=false;if(!ok||!id){fail("Steam lobby admission failed");return;}
            Config::instance().is_host=host;owner=net::SteamMatchmaking::instance().host_steam_id();ui_status="Lobby open";
            net::chat::Runtime::instance().set_lobby_admitted(true);chat_error.clear();chat_last_sequence=0;chat_draft_clear_failed=false;
            if(host){std::string error;auto& mm=net::SteamMatchmaking::instance();
                framework_host_visibility=public_lobby?"Public":"Friends only";
                if(!mm.publish_lobby_data("mt2mp","1",error)||!mm.publish_lobby_data("host_name",net::SteamNetworking::instance().get_persona_name(owner),error)||
                   !mm.publish_lobby_data("mt2mp_version",MOD_VERSION,error)||!mm.publish_lobby_data("game_name","Welt wird ausgewählt",error)||
                   !mm.publish_lobby_data("lobby_state","open",error)||!mm.set_lobby_joinable(true,error)){metadata_failure(error);return;}
                published_state="open";
                host_world_pick_requested=ui_mode==2&&selected_world.empty()&&!current_host_world;
            }
        };
        if(host)mm.create_lobby(4,completion,public_lobby);
        else {
            std::string id;uint64_t number=0;
            if(!input_text("lobby_id",id,20)){pending=false;fail("native lobby ID field unreadable");return;}
            auto parse=std::from_chars(id.data(),id.data()+id.size(),number);
            if(parse.ec!=std::errc{}||parse.ptr!=id.data()+id.size()||!number){pending=false;ui_status="Invalid lobby ID";return;}
            mm.join_lobby(number,completion);
        }
        return;
    }
    if(cmd=="MT2MPStart"||cmd=="MT2MPPrepare") {
        std::string error;if(!start_host_world(error))return;
        return;
    }
    if(cmd=="MT2MPLeave") {
        if(pending){fail("native Leave awaits actual pending admission callback; no cancellation replay");return;}
        std::string error;if(!lobby_prepare::leave(error)){fail("native Leave failed: "+error);return;}
        ++generation;pending=false;choosing=false;ui_status="Leaving session";return;
    }
    if(cmd=="MT2MPInvite") {
        const auto reject_invite=[](const char* detail,const char* status){fail(detail);ui_status=status;};
        if(!mm.in_lobby()){reject_invite("Steam invite dialog rejected: no active lobby","No Steam lobby");return;}
        const auto lobby_id=mm.current_lobby_id();
        if(!lobby_id){reject_invite("Steam invite dialog rejected: active lobby ID is zero","Steam lobby ID missing");return;}
        const auto steam=mt2mp::net::steam_module();
        if(!steam){reject_invite("Steam invite dialog unavailable: selected Steam module unavailable","Steam library unavailable");return;}
        using Access=void*(*)();using Enabled=bool(*)(void*);using Invite=void(*)(void*,uint64_t);
        auto friends=reinterpret_cast<Access>(GetProcAddress(steam,"SteamAPI_SteamFriends_v018"));
        auto utils=reinterpret_cast<Access>(GetProcAddress(steam,"SteamAPI_SteamUtils_v010"));
        auto enabled=reinterpret_cast<Enabled>(GetProcAddress(steam,"SteamAPI_ISteamUtils_IsOverlayEnabled"));
        auto invite=reinterpret_cast<Invite>(GetProcAddress(steam,"SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog"));
        if(!friends){reject_invite("Steam invite dialog unavailable: missing export SteamAPI_SteamFriends_v018","Steam friends interface missing");return;}
        if(!utils){reject_invite("Steam invite dialog unavailable: missing export SteamAPI_SteamUtils_v010","Steam services interface missing");return;}
        if(!enabled){reject_invite("Steam invite dialog unavailable: missing export SteamAPI_ISteamUtils_IsOverlayEnabled","Steam overlay check unavailable");return;}
        if(!invite){reject_invite("Steam invite dialog unavailable: missing export SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog","Steam invite function missing");return;}
        auto* f=friends();
        if(!f){reject_invite("Steam invite dialog unavailable: SteamFriends interface is null","Steam friends interface not ready");return;}
        auto* u=utils();
        if(!u){reject_invite("Steam invite dialog unavailable: SteamUtils interface is null","Steam services interface not ready");return;}
        if(!enabled(u)){reject_invite("Steam invite dialog unavailable: overlay disabled","Steam overlay disabled");return;}
        invite(f,lobby_id);cause.clear();ui_status="Steam invite dialog requested";
        LOG_INFO("[NativeLobby] Steam invite dialog requested; recipient selection and delivery not confirmed");return;
    }
    fail("unknown native lobby command "+cmd);
}
