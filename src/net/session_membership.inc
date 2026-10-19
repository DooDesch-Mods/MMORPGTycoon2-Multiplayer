namespace membership=session_binding;
uint64_t current_session=0,local_instance=0,pending_session=0,last_refresh_session=0;
std::array<uint64_t,5> remote_instances{};
bool refresh_allowed=false,restart_pending=false;
uint64_t discarded_epoch_packets=0,discarded_unknown_packets=0;
membership::Slots observed_slots{};
constexpr const char* executable_sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";

uint64_t nonce(){
    uint64_t value=0;
    const auto status=BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&value),sizeof value,BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(status<0||!value)throw std::runtime_error("Steam session nonce generation failed NTSTATUS="+std::to_string(status));
    return value;
}
membership::Slots actual_members(){
    auto& mm=SteamMatchmaking::instance();membership::Slots slots;std::string error;
    if(!membership::topology(mm.get_lobby_members(),mm.host_steam_id(),slots,error))throw std::runtime_error(error);
    return slots;
}
void adopt(const membership::Binding& value,uint64_t physical_local){
    const auto mine=std::find(value.slots.begin()+1,value.slots.end(),physical_local);
    if(mine==value.slots.end())throw std::runtime_error("physical local Steam account absent from confirmed lobby");
    ids=value.slots;lobby=value.lobby;current_session=value.session;own=uint64_t(mine-value.slots.begin());mask=membership::mask(ids);
    hello_ack=0;hello_sent=false;hello_started={};remote_instances={};
    if(pending_session==value.session)last_refresh_session=value.session;
    refresh_allowed=restart_pending=false;pending_session=0;observed_slots=ids;
    auto& cfg=Config::instance();cfg.session_id=current_session;cfg.peer_id=own;cfg.is_host=own==1;
    bound=true;
    LOG_NET_INFO("[SessionWire] Binding acknowledged lobby={} session={} slot={} peer_mask={}; world ACK still required",lobby,current_session,own,mask);
}
bool binding(std::string& error,bool publish){
    if(ended)return fail("Steam session wire ended: "+failure,error);
    auto& mm=SteamMatchmaking::instance();
    if(!mm.in_lobby())return fail(bound?"Steam lobby disconnected":"info: awaiting confirmed Steam lobby",error);
    const auto slots=actual_members();const auto id=mm.current_lobby_id();observed_slots=slots;
    const auto physical_local=SteamNetworking::instance().local_steam_id();
    if(std::find(slots.begin()+1,slots.end(),physical_local)==slots.end())return fail("physical local Steam account absent from actual lobby",error);
    if(bound){
        const auto change=membership::compare({lobby,current_session,ids},id,slots);
        if(change==membership::Change::HostLost||change==membership::Change::LobbyLost)
            return fail("Steam original Host or lobby lost; session ended",error);
    }
    auto text=mm.get_lobby_data("mt2mp_session");
    if(text.empty()&&publish&&physical_local==slots[1]){
        if(!pending_session)pending_session=nonce();
        const auto encoded=membership::encode({id,pending_session,slots},MOD_VERSION,executable_sha).dump();
        std::string cause;
        if(!mm.publish_lobby_data("mt2mp_session",encoded,cause))return fail(cause,error);
        text=mm.get_lobby_data("mt2mp_session");
    }
    if(text.empty())return fail("info: awaiting Host session metadata",error);
    membership::Binding published;std::string cause;
    if(!membership::decode(text,MOD_VERSION,executable_sha,published,cause))return fail(cause,error);
    if(published.lobby!=id||published.slots[1]!=slots[1])return fail("Steam metadata lobby or original Host differs",error);
    if(!bound){
        if(published.slots!=slots)return fail("info: Host is preparing updated lobby membership",error);
        adopt(published,physical_local);
    }else if(published.session==current_session){
        if(published.slots!=ids)return fail("Steam physical slots changed without a new session epoch",error);
    }else{
        restart_pending=true;
        if(refresh_allowed){
            if(published.slots!=slots)return fail("info: awaiting Host metadata for current physical members",error);
            adopt(published,physical_local);
        }
    }
    error.clear();return true;
}
