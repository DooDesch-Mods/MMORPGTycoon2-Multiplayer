bool control(uint64_t sender,const uint8_t* data,uint32_t size,std::string& error){
    if(size>1024)return fail("Steam wire handshake exceeds1024 bytes",error);
    const auto value=json::parse(data,data+size);
    if(!value.is_object()||value.size()!=5||value.at("session")!=current_session||value.at("lobby")!=lobby||
       !value.at("instance").is_number_unsigned()||!value.at("instance").get<uint64_t>()||
       !value.at("for_instance").is_number_unsigned())return fail("Steam wire handshake identity differs",error);
    const auto type=value.at("type").get<std::string>();
    const auto instance=value.at("instance").get<uint64_t>();
    const auto observe=[&]{
        if(remote_instances[sender]&&remote_instances[sender]!=instance){
            restart_pending=true;
            LOG_NET_INFO("[SessionWire] Physical peer restarted slot={}; shared snapshot required",sender);
        }
        remote_instances[sender]=instance;
    };
    if(type=="hello"){
        if(value.at("for_instance")!=0)return fail("Steam hello has nonzero acknowledgement target",error);
        observe();
        const auto bytes=json{{"type","ack"},{"session",current_session},{"lobby",lobby},
            {"instance",local_instance},{"for_instance",instance}}.dump();
        return emit(3,reinterpret_cast<const uint8_t*>(bytes.data()),uint32_t(bytes.size()),sender,error);
    }
    if(type!="ack")return fail("Steam wire handshake type invalid",error);
    if(value.at("for_instance")!=local_instance){
        if(++discarded_epoch_packets==1)LOG_NET_WARN("[SessionWire] Ignored ACK for retired process instance; repeats counted");
        error.clear();return true;
    }
    observe();hello_ack|=uint8_t(1u<<(sender-1));error.clear();return true;
}
