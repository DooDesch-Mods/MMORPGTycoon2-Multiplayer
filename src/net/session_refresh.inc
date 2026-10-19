bool membership_pending(){
    return !local()&&bound&&!ended&&(restart_pending||observed_slots!=ids);
}
uint8_t membership_retirement_mask(){
    if(local())return peer_mask();
    uint8_t result=0;
    for(size_t i=1;i<ids.size();++i)if(ids[i]&&std::find(observed_slots.begin()+1,observed_slots.end(),ids[i])!=observed_slots.end())result|=uint8_t(1u<<(i-1));
    return result;
}
bool retire_world(std::string& error){
    if(!actions_suspended){error="wire world retirement requires suspended actions";return false;}
    if(current_world_epoch()){error="wire world retirement requires invalidated snapshot authority";return false;}
    if(local())return SidecarClient::instance().retire_action_handler(error);
    handlers.erase(9);refresh_allowed=true;error.clear();return true;
}
bool refresh_membership(std::string& error){
    if(local()){error.clear();return true;}
    try{
        if(bound&&!ended&&own==1&&actions_suspended&&!current_world_epoch()&&!refresh_allowed&&
           current_session==last_refresh_session&&!pending_session){error.clear();return true;}
        if(!bound||ended||own!=1||!refresh_allowed||!actions_suspended||current_world_epoch())
            return fail("Steam membership refresh requires retired Host world and action handler",error);
        auto& mm=SteamMatchmaking::instance();
        const auto slots=actual_members();
        if(mm.current_lobby_id()!=lobby||slots[1]!=ids[1])return fail("Steam membership refresh original Host/lobby lost",error);
        if(!pending_session){
            pending_session=nonce();
            if(pending_session==current_session)return fail("Steam membership nonce repeated; no publication attempted",error);
            const auto text=membership::encode({lobby,pending_session,slots},MOD_VERSION,executable_sha).dump();
            std::string cause;if(!mm.publish_lobby_data("mt2mp_session",text,cause))return fail(cause,error);
        }
        if(!binding(error,false))return false;
        if(pending_session){error="info: awaiting refreshed Host metadata readback";return false;}
        error.clear();return true;
    }catch(const std::exception& cause){return fail(cause.what(),error);}
}
