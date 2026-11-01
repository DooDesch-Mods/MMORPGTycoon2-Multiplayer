namespace export_files=game::client_save_export;
static std::unique_ptr<export_files::Protocol> save_export;
bool configure_save_export(export_files::Protocol::Ports ports,std::string& error)try{
    if(save_export){error="Client Save export owner has not retired";return false;}
    std::array<uint64_t,5> ids{};uint64_t lobby=0,local=0;
    if(!ports.current||!ordered_snapshot_selected()||!session_wire::connected()||!session_wire::read_binding(ids,lobby,local,error,true)){
        if(error.empty())error="Client Save export requires the current ordered physical binding";return false;
    }
    const auto session=session_wire::session_id(),world=current_world_epoch();const auto mask=session_wire::peer_mask();
    const auto native_current=std::move(ports.current);
    ports.current=[ids,lobby,local,session,world,mask,native_current](std::string& cause){
        std::array<uint64_t,5> now{};uint64_t now_lobby=0,now_local=0;
        if(!session_wire::connected()||session_wire::session_id()!=session||current_world_epoch()!=world||session_wire::peer_mask()!=mask||
            !session_wire::read_binding(now,now_lobby,now_local,cause,true)||now!=ids||now_lobby!=lobby||now_local!=local){
            if(cause.empty())cause="Client Save export physical session/world binding changed";return false;
        }
        return native_current(cause);
    };
    ports.control=[](uint64_t target,const json& value){const auto bytes=value.dump();
        if(bytes.size()>8192)throw std::runtime_error("Client Save export control exceeds existing byte bound");
        return session_wire::send(4,reinterpret_cast<const uint8_t*>(bytes.data()),uint32_t(bytes.size()),target);};
    ports.chunk=[](uint64_t target,const export_files::Identity& identity,const export_files::Chunk& chunk){
        if(chunk.bytes.empty()||chunk.bytes.size()>snapshot_chunk_bytes)throw std::runtime_error("Client Save export chunk exceeds existing byte bound");
        std::vector<uint8_t> packet(44+chunk.bytes.size());std::memcpy(packet.data(),"MT2E",4);
        std::memcpy(packet.data()+4,identity.nonce.data(),36);std::memcpy(packet.data()+40,&chunk.index,4);std::memcpy(packet.data()+44,chunk.bytes.data(),chunk.bytes.size());
        return session_wire::send(5,packet.data(),uint32_t(packet.size()),target);
    };
    save_export=std::make_unique<export_files::Protocol>(session,world,local,mask,std::move(ports));error.clear();return true;
}catch(const std::exception& cause){error=cause.what();return false;}
bool request_save_export(const std::string& nonce,const std::filesystem::path& local_target,const std::filesystem::path& local_staging,uint32_t kind,bool option,std::string& error)try{
    if(!save_export){error="Client Save export owner is not configured";return false;}
    save_export->request(nonce,local_target,local_staging,kind,option);error.clear();return true;
}catch(const std::exception& cause){error=cause.what();if(save_export)save_export->failed(error);return false;}
bool provide_save_export(const std::string& nonce,const std::filesystem::path& native,std::string& error)try{
    if(!save_export){error="Host Save export owner is not configured";return false;}
    save_export->captured(nonce,native);error.clear();return true;
}catch(const std::exception& cause){error=cause.what();if(save_export)save_export->failed(error);return false;}
bool save_export_publication_confirmed(){return save_export&&save_export->publication_confirmed();}
json save_export_status(){return save_export?json{{"configured",true},{"released",save_export->release_confirmed()},{"phase",int(save_export->phase())},{"nonce",save_export->identity().nonce},
    {"publication_confirmed",save_export->publication_confirmed()},{"cause",save_export->cause()}}:json{{"configured",false},{"phase",0}};}
bool complete_save_export(std::string& error)try{
    if(!save_export){error="Host export completion owner is unavailable";return false;}
    save_export->released();error.clear();return true;
}catch(const std::exception& cause){error=cause.what();if(save_export)save_export->failed(error);return false;}
void fail_save_export(const std::string& cause){if(save_export)save_export->failed(cause);}
bool retire_save_export(std::string& error){
    if(!save_export){error.clear();return true;}
    if(!save_export->retire(error))return false;save_export.reset();error.clear();return true;
}
static bool receive_save_export_control(uint64_t sender,const json& value){
    if(!value.at("type").get<std::string>().starts_with("save_export_"))return false;
    if(!save_export)throw std::runtime_error("Client Save export message arrived without its retained owner");
    save_export->control(sender,value);return true;
}
static bool receive_save_export_chunk(uint64_t sender,const uint8_t* data,uint32_t length){
    if(length<4||std::memcmp(data,"MT2E",4))return false;
    if(!save_export||length<=44||length>44+snapshot_chunk_bytes)throw std::runtime_error("Client Save export chunk lacks bounded retained owner");
    auto identity=save_export->identity();const std::string nonce(reinterpret_cast<const char*>(data+4),36);
    if(nonce!=identity.nonce)throw std::runtime_error("Client Save export chunk nonce differs");
    export_files::Chunk chunk;std::memcpy(&chunk.index,data+40,4);chunk.bytes.assign(data+44,data+length);
    save_export->chunk(sender,identity,chunk);return true;
}
