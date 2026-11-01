void clear();
struct RecoveryState {
    recovery::Owner* owner=nullptr;
    recovery::Barrier barrier;
    recovery::Packet attempt;
    MT2FW_MT2_RecoveryReceipt last{};
    std::vector<recovery::Packet> inbox;
    recovery::Owner::Capture capture;
    json completed=nullptr,file=nullptr;
    bool active=false,prepared=false,captured=false,published=false,retirement_requested=false;
    uint64_t observed_at=0;
    std::string capture_id;
};
RecoveryState recovery_state;
recovery::Packet recovery_packet(recovery::Kind kind){
    const auto& r=recovery_state.last;recovery::Packet p;
    p.kind=kind;p.session=r.session;p.sender=r.local_slot;p.process=r.local_process;p.interruption=r.interruption;
    p.lease=r.lease;p.world=r.world_epoch;p.generation=r.generation;p.tick=r.completed_tick;
    p.token=recovery_state.barrier.token();p.original=r.original;p.survivors=recovery_state.barrier.survivors();
    if(kind==recovery::Kind::Published)p.file=recovery_state.file;
    return p;
}
bool recovery_send(const recovery::Packet& packet,uint64_t target,std::string& error){
    const auto text=recovery::encode(packet).dump();
    if(!net::session_wire::send(channel,reinterpret_cast<const uint8_t*>(text.data()),uint32_t(text.size()),target)){
        error="Recovery original-survivor packet queue failed: "+net::session_wire::failure_reason();return false;
    }
    error.clear();return true;
}
bool recovery_send_survivors(const recovery::Packet& packet,std::string& error){
    for(uint64_t peer=2;peer<=4;++peer)if(recovery_state.barrier.required()&recovery::bit(peer))
        if(!recovery_send(packet,peer,error))return false;
    return true;
}
bool recovery_receive(uint64_t peer,const uint8_t* bytes,uint32_t size){
    const auto value=json::parse(bytes,bytes+size,nullptr,false);
    if(!value.is_object()||!value.contains("recovery"))return false;
    std::string error;recovery::Packet packet;
    if(!recovery::decode(value,packet,error)||peer!=packet.sender){fail("Recovery packet rejected: "+error,false);return true;}
    if(packet.session!=net::session_wire::session_id()){++rejected_count;return true;}
    // A replacement process can hear old-session traffic but cannot act for the old process.
    net::session_wire::ProcessCohort observed;
    if(!net::session_wire::read_process_cohort(observed,error)){fail(error,false);return true;}
    if(observed.local<1||observed.local>4||packet.original.process_nonces[observed.local-1]!=observed.process_nonces[observed.local]||
       packet.original.physical_ids[observed.local-1]!=observed.physical_ids[observed.local]||
       observed.process_nonces[peer]!=packet.process){++rejected_count;return true;}
    if(recovery_state.inbox.size()>=32){fail("Recovery retained packet inbox exceeded 32 messages",false);return true;}
    recovery_state.inbox.push_back(std::move(packet));return true;
}
bool recovery_file(MT2FW_MT2_RecoveryPublication& publication,std::string& error){
    publication={};publication.size=sizeof(publication);
    const auto& record=recovery_state.completed;
    if(!record.is_object()||record.value("snapshot_id",std::string{})!=recovery_state.capture_id||
       record.value("state",std::string{})!="snapshot_file_complete"||!record.value("export_only",false)||
       !record.value("companion_complete",false)||!record.value("building_bindings_confirmed",false)||
       record.value("distribution_requested",true)||record.value("snapshot_identities_published",true)){
        error="Recovery publication lacks its exact completed isolated file and full companion";return false;
    }
    const auto path=net::snapshot_world_path::utf8_path(record.at("path").get<std::string>());
    if(!path.is_absolute()||path.filename()!=net::snapshot_world_path::utf8_path("snapshot-"+recovery_state.capture_id+".vrz")||
       path.parent_path().filename()!=net::snapshot_world_path::utf8_path(record.at("world_name").get<std::string>())){
        error="Recovery publication path differs from its owned native capture";return false;
    }
    std::string file_hash,companion_hash;auto companion=path;companion+=".monsters.json";
    if(!net::hash_snapshot_file(path,file_hash,publication.file_bytes,error)||
       !net::hash_snapshot_file(companion,companion_hash,publication.companion_bytes,error))return false;
    if(publication.file_bytes!=record.at("bytes").get<uint64_t>()||file_hash!=record.at("sha256").get<std::string>()||
       !publication.companion_bytes||publication.companion_bytes>monster_snapshot_companion::maximum_bytes||
       !recovery::digest(file_hash,publication.file_sha256)||!recovery::digest(companion_hash,publication.companion_sha256)){
        error="Recovery completed native file or companion digest/length differs";return false;
    }
    net::SnapshotFileHandle file;uint64_t bytes=0;
    if(!file.open(companion,GENERIC_READ,OPEN_EXISTING,error)||!net::snapshot_file_length(file.get(),bytes,error))return false;
    if(bytes!=publication.companion_bytes){error="Recovery companion changed before complete readback";return false;}
    std::string data(size_t(bytes),'\0');DWORD read=0;
    if(!ReadFile(file.get(),data.data(),DWORD(bytes),&read,nullptr)||read!=bytes){error="Recovery companion read incomplete Win32="+std::to_string(GetLastError());return false;}
    monster_snapshot_companion::Manifest manifest;
    if(!monster_snapshot_companion::decode(data,file_hash,publication.file_bytes,manifest,error)||!manifest.region_rng.present||!file.close(&error)){
        if(error.empty())error="Recovery companion lacks the complete preserved Region RNG census";return false;
    }
    net::SnapshotSha256 actual;actual.add(reinterpret_cast<const uint8_t*>(data.data()),data.size());
    if(actual.finish()!=companion_hash){error="Recovery companion changed during complete readback";return false;}
    error.clear();return true;
}
bool recovery_begin(recovery::Owner& owner,const MT2FW_MT2_RecoveryReceipt& receipt,const recovery::Packet& packet,std::string& error){
    if(!typed_action_runtime::recovery_idle(receipt,error))return false;
    if(phase!=Phase::Idle||!first_failure.empty()||!recovery_state.barrier.begin(receipt,packet.token,packet.survivors,error))return false;
    if(!owner.operation(MT2FW_MT2_RECOVERY_BEGIN,packet.token,packet.survivors,recovery_state.barrier.required(),nullptr,error))return false;
    recovery_state.last=owner.receipt();recovery_state.attempt=packet;recovery_state.active=true;
    net::session_wire::suspend_actions();
    if(!recovery_state.barrier.held(recovery_state.last,error))return false;
    old_session=receipt.session;was_host=receipt.local_slot==1;had_world=true;ordered_reload=true;manual_requested=false;
    enter(Phase::CaptureReady);
    if(was_host)return recovery_send_survivors(recovery_packet(recovery::Kind::Begin),error);
    return recovery_send(recovery_packet(recovery::Kind::Held),1,error);
}
bool recovery_inbox(recovery::Owner* owner,const MT2FW_MT2_RecoveryReceipt* receipt,std::string& error){
    auto packets=std::move(recovery_state.inbox);recovery_state.inbox.clear();
    for(const auto& packet:packets){
        if(packet.kind==recovery::Kind::Begin&&!recovery_state.active){
            if(!owner||!receipt||receipt->local_slot==1||!recovery::same(packet.original,receipt->original)||
               !recovery_begin(*owner,*receipt,packet,error))return false;
            continue;
        }
        if(!recovery_state.active||!recovery::same_attempt(packet,recovery_state.attempt)){
            error="Recovery packet differs from the fixed old-session attempt";return false;
        }
        if(packet.kind==recovery::Kind::Failed){error="Original survivor recovery failed: "+packet.cause;return false;}
        if(packet.kind==recovery::Kind::Held){
            if(!was_host||!recovery_state.barrier.acknowledge(packet.sender,packet.process,recovery::bit(packet.sender),false,error))return false;
        }else if(packet.kind==recovery::Kind::Published){
            if(was_host||!recovery_state.barrier.accept_host_publication(packet.session,packet.token,packet.original,packet.survivors,error))return false;
            if(recovery_state.published&&recovery_state.file!=packet.file){error="Recovery Host publication changed after acceptance";return false;}
            recovery_state.file=packet.file;recovery_state.published=true;
        }else if(packet.kind==recovery::Kind::Retired){
            if(!was_host||!recovery_state.barrier.acknowledge(packet.sender,packet.process,recovery::bit(packet.sender),true,error))return false;
        }else if(packet.kind!=recovery::Kind::Begin){error="Recovery packet phase is not admitted";return false;}
    }
    error.clear();return true;
}
bool recovery_service(recovery::Owner& owner,const MT2FW_MT2_RecoveryReceipt& receipt,std::string& error){
    if(!bind_thread(error))return false;
    if(recovery_state.owner!=&owner||!first_failure.empty()){error=first_failure.empty()?"Recovery service owner changed":first_failure;return false;}
    if(!recovery_state.observed_at)recovery_state.observed_at=GetTickCount64();
    if(GetTickCount64()-recovery_state.observed_at>=uint64_t(net::snapshot_native_load_timeout_seconds)*1000){error="Original-peer recovery wait exceeded 900 seconds; held native owner retained";return false;}
    if(!recovery_inbox(&owner,&receipt,error))return false;
    if(!recovery_state.active&&receipt.local_slot==1){
        recovery::Packet packet;packet.token=token(error);if(packet.token.empty())return false;
        packet.original=receipt.original;
        for(size_t i=0;i<4;++i)if(receipt.original.physical_ids[i]&&receipt.original.physical_ids[i]==receipt.observed.physical_ids[i]&&
            receipt.original.process_nonces[i]==receipt.observed.process_nonces[i]){
            packet.survivors.physical_ids[i]=receipt.original.physical_ids[i];packet.survivors.process_nonces[i]=receipt.original.process_nonces[i];
        }
        if(!recovery_begin(owner,receipt,packet,error))return false;
        recovery_state.attempt=recovery_packet(recovery::Kind::Begin);
    }
    if(!recovery_state.active){error.clear();return true;}
    if(GetTickCount64()-began>=uint64_t(net::snapshot_native_load_timeout_seconds)*1000){error="Original-peer recovery timed out; native attempts retained without replay";return false;}
    recovery_state.last=owner.receipt();
    const auto op=[&](uint32_t operation_code,const MT2FW_MT2_RecoveryPublication* publication=nullptr,recovery::Owner::Capture capture={}){
        const auto okay=owner.operation(operation_code,recovery_state.barrier.token(),recovery_state.barrier.survivors(),recovery_state.barrier.required(),publication,error,std::move(capture));
        recovery_state.last=owner.receipt();return okay;
    };
    if(was_host&&recovery_state.barrier.all_held()&&!recovery_state.prepared){
        if(!lobby_operations::executor_idle()){error="Recovery capture conflicts with an existing native operation";return false;}
        if(!op(MT2FW_MT2_RECOVERY_PREPARE)||!op(MT2FW_MT2_RECOVERY_SEAL))return false;
        std::memcpy(&resume_scale,&recovery_state.last.selected_clock_bits,sizeof(resume_scale));
        if(resume_scale!=0&&resume_scale!=1&&resume_scale!=3&&resume_scale!=9){error="Recovery selected Clock scale is outside original 0/1/3/9";return false;}
        recovery_state.prepared=true;
        if(!lobby_operations::submit("capture_recovery_snapshot",json::object(),operation,error))return false;
        recovery_state.capture_id=operation;enter(Phase::Capture);
    }
    if(was_host&&recovery_state.prepared&&!recovery_state.published){
        mt2mp::ordered_snapshot_diagnostics();
        if(recovery_state.capture&&!recovery_state.captured){
            auto capture=std::move(recovery_state.capture);recovery_state.captured=true;
            if(!op(MT2FW_MT2_RECOVERY_CAPTURE,nullptr,[&owner,capture=std::move(capture)](std::string& cause){
                MT2FW_MT2_RecoveryReceipt current{};if(!owner.read(current,cause))return false;
                return typed_action_runtime::read_recovery_snapshot(current,capture,cause);
            }))return false;
        }
        if(recovery_state.captured&&recovery_state.last.phase==MT2FW_MT2_RECOVERY_CONSUMED)
            if(!op(MT2FW_MT2_RECOVERY_PUMP))return false;
        json response;
        if(lobby_operations::take_response(operation,response)){
            if(!response.value("ok",false)){error=response.value("error",json::object()).value("message",std::string("Recovery export failed"));return false;}
            recovery_state.completed=response.at("result");
            MT2FW_MT2_RecoveryPublication publication{};if(!recovery_file(publication,error)||!op(MT2FW_MT2_RECOVERY_PUBLISH,&publication))return false;
            if(!recovery_state.barrier.publish(recovery_state.last,error))return false;
            saved_snapshot=recovery_state.capture_id;saved_world_name=recovery_state.completed.at("world_name").get<std::string>();
            recovery_state.file={{"snapshot_id",saved_snapshot},{"world_name",saved_world_name},{"bytes",publication.file_bytes},
                {"sha256",recovery::hex(publication.file_sha256)},{"companion_bytes",publication.companion_bytes},
                {"companion_sha256",recovery::hex(publication.companion_sha256)},{"witness_sha256",recovery::hex(recovery_state.last.witness_sha256)}};
            recovery_state.published=true;operation.clear();
            if(!recovery_send_survivors(recovery_packet(recovery::Kind::Published),error))return false;
        }
    }
    if(recovery_state.published&&!recovery_state.retirement_requested){
        if(!op(MT2FW_MT2_RECOVERY_PREFLIGHT_RETIRE))return false;
        retirement_before=ordered_startup::reload_confirmed;recovery_state.retirement_requested=true;
        if(!ordered_startup::request_recovery_retirement(error))return false;
        title_requested=true;enter(Phase::Retire);
    }
    error.clear();return true;
}
bool recovery_tick(std::string& error){
    if(!recovery_state.active||phase==Phase::Handshake)return false;
    if(phase==Phase::Failed)return true;
    if(GetTickCount64()-began>=uint64_t(net::snapshot_native_load_timeout_seconds)*1000){fail("Recovery retirement or replacement-process wait exceeded 900 seconds",false);return true;}
    if(!recovery_inbox(nullptr,nullptr,error)){fail(error,false);return true;}
    if(phase==Phase::Retire){
        if(ordered_startup::selected())return true;
        recovery_state.owner=nullptr;
        if(ordered_startup::reload_confirmed!=retirement_before+1){fail("Recovery lost its exact original Title retirement acknowledgement",false);return true;}
        if(!lobby_prepare::reset_after_world_reload(error)){fail(error,false);return true;}
        auto packet=recovery_packet(recovery::Kind::Retired);packet.retirement=ordered_startup::reload_confirmed;
        if(was_host){
            if(!recovery_state.barrier.acknowledge(1,packet.process,1,true,error)){fail(error,false);return true;}
            enter(Phase::Peers);
        }else{
            if(!recovery_send(packet,1,error)){fail(error,false);return true;}
            enter(Phase::Handshake);return true;
        }
    }
    if(phase==Phase::Peers&&recovery_state.barrier.all_retired()){
        net::session_wire::ProcessCohort observed;
        if(!net::session_wire::read_process_cohort(observed,error)){fail(error,false);return true;}
        for(size_t i=0;i<4;++i)if(recovery_state.last.original.physical_ids[i]&&
           (observed.physical_ids[i+1]!=recovery_state.last.original.physical_ids[i]||!observed.process_nonces[i+1])){
            wait("awaiting the original physical accounts with current processes before new epoch publication");return true;
        }
        if(!net::session_wire::refresh_membership(error)){if(error.starts_with("info:")){wait(error);return true;}fail(error,false);return true;}
        last_ordered_handoff={{"route","original-peer-recovery"},{"old_session",old_session},{"nonce",recovery_state.barrier.token()},
            {"original_mask",recovery_state.last.original_mask},{"survivor_mask",recovery_state.barrier.required()},
            {"held_mask",recovery_state.barrier.held_mask()},{"retired_mask",recovery_state.barrier.retired_mask()},{"capture",recovery_state.file}};
        enter(Phase::Handshake);
    }
    return true;
}
