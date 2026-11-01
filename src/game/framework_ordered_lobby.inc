bool ordered_tick(const OrderedView& view,const MT2FW_MT2_StepReceipt& receipt,std::string& error){
    if(!image||thread!=GetCurrentThreadId()||!ordered_service_receipt(receipt,thread)){
        error="Ordered lobby UI requires its actual bound Step service callback";return false;
    }
    framework_ordered_selected=true;
    auto& steam=net::SteamMatchmaking::instance();
    const bool steam_member=!Config::instance().local_test&&steam.in_lobby();
    const auto lobby_id=steam_member?steam.current_lobby_id():uint64_t{};
    const auto member_count=steam_member?steam.get_lobby_members().size():size_t{};
    const bool free_slot=member_count>=1&&member_count<4;
    const bool ready=view.admission_ready&&view.started&&!view.leaving&&view.failure.empty()&&view.admission_error.empty()&&
        receipt.phase==MT2FW_MT2_STEP_HELD&&free_slot;
    if(steam_member&&steam.is_host()&&!publication_failed){
        const std::string next=!view.failure.empty()||!view.admission_error.empty()?"unavailable":view.leaving?"leaving":view.admission_ready?"playing":"preparing";
        std::string detail;
        if(!ordered_publication.publish(steam,lobby_id,next,ready,detail))metadata_failure(detail);
    }
    const bool invite_ready=ready&&!publication_failed&&
        (steam.is_host()?ordered_publication.lobby==lobby_id&&ordered_publication.joinable:
         steam.get_lobby_data("lobby_state")=="playing");
    native::WorldStatus world;
    if(!native::read_world_status(world,error))return false;
    const bool title_active=world.active_mode_index==2&&!world.title_transition_pending;
    const bool ingame_active=world.active_mode_index==3&&native::in_game();
    if(!title_active&&!ingame_active){
        ui_owner=nullptr;ui_mode=-1;framework_ready=false;framework_observed_open=false;framework_visibility_known=false;
        error.clear();return true;
    }
    void* game=nullptr;void* mode=nullptr;void* active=nullptr;
    if(!read(reinterpret_cast<void*>(image),0x13df8f0,game)||!read(game,title_active?0x350:0x358,mode)||!read(game,0x28,active)||mode!=active){
        error="Ordered lobby UI owner differs from actual active mode";return false;
    }
    if(ui_owner!=mode||ui_mode!=world.active_mode_index){
        ui_owner=mode;ui_mode=world.active_mode_index;framework_ready=false;framework_observed_open=false;framework_visibility_known=false;
        last_ui_status.clear();last_chat_transcript.clear();last_chat_status.clear();
    }
    if(title_active)title=mode;
    framework_browser=false;choosing=false;
    const auto heading=publication_failed?std::string("Lobby unavailable"):
        !view.admission_error.empty()?std::string("Unable to add player"):ordered_heading(view);
    const auto detail=publication_failed?publication_error:
        !view.admission_error.empty()?view.admission_error:
        last_command=="MT2MPInvite"&&!cause.empty()?cause:ordered_detail(view);
    if(framework_ordered_heading!=heading){
        framework_ordered_heading=heading;
        if(publication_failed||!view.admission_error.empty()||!view.failure.empty()||!view.started)framework_open=true;
        else if(!view.leaving)framework_open=false;
    }
    ui_status=heading;
    if(!label("load_status",heading)||!label("load_detail",detail)||!label("world",selected_world.empty()?"Shared world":selected_world)||
       !label("build_version","v"+std::string(MOD_VERSION))||!label("session_id",lobby_id?"Lobby ID: "+std::to_string(lobby_id):std::string{})||
       !set_pane_visible("leave",true)||!button_label("leave","Leave game")||
       !disable("leave",!view.started||view.leaving||(!view.failure.empty()&&!view.terminal_exit_ready))){error=cause;return false;}
    for(const char* id:{"host","host_public","search","join","pick","new_world","start","list_refresh","list_previous","list_next"})
        if(!disable(id,true)){error=cause;return false;}
    for(const char* id:{"pick","start"})if(!set_pane_visible(id,false)){error=cause;return false;}
    if(!set_pane_visible("invite",steam_member)||!disable("invite",!invite_ready)){error=cause;return false;}
    const auto rows=player_rows();
    for(size_t i=0;i<rows.size();++i)if(!label(("player"+std::to_string(i)).c_str(),rows[i])){error=cause;return false;}
    unsigned count=0;const auto mask=net::session_wire::peer_mask();for(unsigned i=0;i<4;++i)count+=(mask>>i)&1u;
    if(!label("players_heading","Player "+std::to_string(count)+"/4")){error=cause;return false;}
    const bool terminal=receipt.phase==MT2FW_MT2_STEP_FAILED;
    if((!terminal&&!process_panel_toggle(true,error))||!render_chat(error))return false;
    if(view.leaving||(!terminal&&!view.failure.empty())){error.clear();return true;}
    return framework_process_action(error);
}
