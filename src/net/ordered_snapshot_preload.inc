// Retained by the existing snapshot transfer owner on its game thread.
static std::optional<OrderedSnapshotIdentity> ordered_snapshot;
static uint32_t ordered_thread=0;
static bool ordered_original_cohort=false;
static json ordered_manifest=nullptr;
static std::string ordered_failure;
static std::filesystem::path ordered_source_path;
static std::filesystem::path write_root();
static bool ordered_loaded_observation(OrderedSnapshotIdentity&,std::string&);
static bool snapshot_host(){return ordered_snapshot?ordered_snapshot->local_peer==1:Config::instance().is_host;}
static bool ordered_owner(OrderedSnapshotIdentity& out,std::string& error){
    out={};
    const auto before=session_wire::session_id();
    if(!before){error="info: awaiting explicit ordered session membership preparation";return false;}
    if(!session_wire::read_binding(out.members,out.lobby,out.local_peer,error,ordered_original_cohort))return false;
    if(!session_wire::connected()||before!=session_wire::session_id()||(!ordered_original_cohort&&session_wire::membership_pending())||
       !out.lobby||out.local_peer<1||out.local_peer>4||out.members[0]||!out.members[1]||!out.members[out.local_peer]){
        error="Ordered preload requires acknowledged stable physical session membership";return false;
    }
    out.session=before;std::set<uint64_t> seen;
    for(size_t slot=1;slot<out.members.size();++slot)if(out.members[slot]){
        if(!seen.insert(out.members[slot]).second){error="Ordered preload physical member IDs are duplicated";return false;}
        out.peer_mask|=uint8_t(1u<<(slot-1));
    }
    if(out.peer_mask!=session_wire::peer_mask()||out.local_peer!=session_wire::local_peer()){
        error="Ordered preload acknowledged local slot/mask differs";return false;
    }
    return true;
}
static bool ordered_fresh(std::string& error){
    if(!ordered_snapshot){error="Ordered preload has not been explicitly selected";return false;}
    if(GetCurrentThreadId()!=ordered_thread){error="Ordered preload owner thread differs";return false;}
    if(!ordered_failure.empty()){error=ordered_failure;return false;}
    OrderedSnapshotIdentity actual;if(!ordered_owner(actual,error))return false;
    const auto& frozen=*ordered_snapshot;
    if(actual.members!=frozen.members||actual.lobby!=frozen.lobby||actual.local_peer!=frozen.local_peer||actual.session!=frozen.session||actual.peer_mask!=frozen.peer_mask){
        error="Ordered preload frozen session/member identity changed";return false;
    }
    return true;
}
bool select_ordered_snapshot(std::string& error){
    if(ordered_snapshot||receiving||load_pending||outbound_active||snapshot_world_valid){
        error="Ordered snapshot selection requires idle transfer and no active snapshot world; no mode switch";return false;
    }
    OrderedSnapshotIdentity owner;if(!ordered_owner(owner,error))return false;
    ordered_snapshot=std::move(owner);ordered_thread=GetCurrentThreadId();ordered_manifest=nullptr;ordered_failure.clear();error.clear();return true;
}
bool ordered_snapshot_selected(){return ordered_snapshot.has_value();}
bool retain_ordered_original_cohort(std::string& error){
    if(!ordered_fresh(error))return false;
    OrderedSnapshotIdentity actual;
    if(!read_ordered_snapshot(actual,error)||!actual.loaded||!actual.world_epoch){
        if(error.empty())error="Ordered cohort retention requires the completed original load";
        return false;
    }
    ordered_original_cohort=true;error.clear();return true;
}
bool select_ordered_snapshot_source(const std::string& id,const std::string& digest,uint64_t bytes,uint32_t mode,std::string& error){
    if(!ordered_fresh(error))return false;
    auto& selected=*ordered_snapshot;
    if(selected.local_peer!=1||selected.seed_present||selected.region_rng_mode||!valid_id(id)||!valid_snapshot_digest(digest)||
       !bytes||bytes>snapshot_max_bytes||(mode!=1&&mode!=2)){
        error="Ordered source import requires one validated selected native-save copy before distribution";return false;
    }
    selected.snapshot_uuid=id;selected.sha256=digest;selected.bytes=bytes;selected.region_rng_mode=mode;
    error.clear();return true;
}
static void ordered_validate_manifest(const json& begin,uint64_t sender,const OrderedSnapshotIdentity& owner){
    if(sender!=1||owner.local_peer==1||begin.at("transport_revision")!=3||!begin.contains("ordered_simulation"))
        throw std::runtime_error("Ordered preload requires explicit revision3 Host manifest");
    const auto& value=begin.at("ordered_simulation");
    if(!value.is_object()||value.size()!=6||value.at("revision")!=2||!value.at("session").is_number_unsigned()||
       !value.at("lobby").is_number_unsigned()||!value.at("seed").is_number_unsigned()||!value.at("region_rng_mode").is_number_unsigned()||
       (value.at("region_rng_mode")!=1&&value.at("region_rng_mode")!=2)||!value.at("members").is_array()||value.at("members").size()!=5)
        throw std::runtime_error("Ordered preload manifest exact identity/seed schema differs");
    std::array<uint64_t,5> members{};
    for(size_t i=0;i<members.size();++i){if(!value.at("members").at(i).is_number_unsigned())throw std::runtime_error("Ordered preload physical member is not uint64");members[i]=value.at("members").at(i).get<uint64_t>();}
    if(members!=owner.members||value.at("session").get<uint64_t>()!=owner.session||value.at("lobby").get<uint64_t>()!=owner.lobby)
        throw std::runtime_error("Ordered preload manifest differs from acknowledged physical owner");
    if(!begin.at("bytes").is_number_unsigned()||!begin.at("bytes").get<uint64_t>()||begin.at("bytes").get<uint64_t>()>snapshot_max_bytes)
        throw std::runtime_error("Ordered preload manifest byte count invalid");
    if(!begin.contains("world_name")||!begin.at("world_name").is_string())throw std::runtime_error("Ordered preload requires explicit native world_name");
    const auto id=begin.at("id").get<std::string>(),digest=begin.at("sha256").get<std::string>();
    (void)snapshot_world_path::from_manifest(begin);
    if(!valid_id(id)||!valid_snapshot_digest(digest))throw std::runtime_error("Ordered preload snapshot identity invalid");
    if(begin.contains("network_identity")||begin.contains("software"))throw std::runtime_error("Ordered preload cannot mix legacy state-copy metadata");
}
static void ordered_accept_manifest(const json& begin,uint64_t sender){
    std::string error;if(!ordered_fresh(error))throw std::runtime_error(error);
    ordered_validate_manifest(begin,sender,*ordered_snapshot);
    const auto& value=begin.at("ordered_simulation");
    const auto id=begin.at("id").get<std::string>(),digest=begin.at("sha256").get<std::string>(),folder=snapshot_world_path::from_manifest(begin);
    if(!ordered_manifest.is_null()){
        const auto& retained=*ordered_snapshot;
        if(value!=ordered_manifest||id!=retained.snapshot_uuid||digest!=retained.sha256||folder!=retained.world_name||begin.at("bytes").get<uint64_t>()!=retained.bytes)
            throw std::runtime_error("Ordered preload manifest changed after seed agreement");
        return;
    }
    ordered_manifest=value;ordered_snapshot->snapshot_uuid=id;ordered_snapshot->sha256=digest;ordered_snapshot->world_name=folder;
    ordered_snapshot->bytes=begin.at("bytes").get<uint64_t>();ordered_snapshot->seed=value.at("seed").get<uint64_t>();ordered_snapshot->seed_present=true;ordered_snapshot->region_rng_mode=value.at("region_rng_mode").get<uint32_t>();
}
static void ordered_host_manifest(json& begin,const std::filesystem::path& path){
    std::string error;if(!ordered_fresh(error))throw std::runtime_error(error);
    if(ordered_snapshot->local_peer!=1||ordered_snapshot->seed_present||!ordered_manifest.is_null())throw std::runtime_error("Ordered Host seed/manifest has already been assigned; no replay");
    if(ordered_snapshot->region_rng_mode<1||ordered_snapshot->region_rng_mode>2||
       ordered_snapshot->snapshot_uuid!=begin.at("id").get<std::string>()||ordered_snapshot->sha256!=begin.at("sha256").get<std::string>()||
       ordered_snapshot->bytes!=outbound.size())throw std::runtime_error("Ordered Host manifest lacks its exact selected native import provenance");
    uint64_t seed=0;const auto result=BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&seed),sizeof(seed),BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(result<0)throw std::runtime_error("Ordered Host seed BCryptGenRandom status="+std::to_string(result));
    ordered_source_path=path;
    auto& retained=*ordered_snapshot;
    retained.snapshot_uuid=begin.at("id").get<std::string>();retained.sha256=begin.at("sha256").get<std::string>();retained.world_name=snapshot_world_path::from_manifest(begin);
    retained.bytes=outbound.size();retained.seed=seed;retained.seed_present=true;retained.file_verified=true;
    ordered_manifest={{"revision",2},{"region_rng_mode",retained.region_rng_mode},{"session",retained.session},{"lobby",retained.lobby},{"seed",seed},{"members",retained.members}};
    begin["transport_revision"]=3;begin["ordered_simulation"]=ordered_manifest;
}
static void ordered_attach_ack(json& value){if(ordered_snapshot)value["ordered_simulation"]=ordered_manifest;}
static void ordered_check_ack(const json& value,uint64_t sender){
    if(!ordered_snapshot){if(value.contains("ordered_simulation"))throw std::runtime_error("Legacy snapshot received unpromised ordered agreement");return;}
    std::string error;if(!ordered_fresh(error))throw std::runtime_error(error);
    if(sender<2||sender>4||!ordered_snapshot->members[sender])throw std::runtime_error("Ordered acknowledgement sender is outside frozen physical membership");
    if(ordered_manifest.is_null()||!value.contains("ordered_simulation")||value.at("ordered_simulation")!=ordered_manifest)
        throw std::runtime_error("Ordered snapshot acknowledgement seed/session/member agreement differs");
}
static bool ordered_native_read(OrderedSnapshotIdentity& out,std::string& error){
#if !defined(MT2MP_SNAPSHOT_TEST) || defined(MT2MP_ORDERED_PRELOAD_TEST)
    const auto actual=game::building_mixed_identity::opened_file::status();
    if(actual.uuid!=out.snapshot_uuid||actual.sha256!=out.sha256||actual.bytes!=out.bytes||actual.hash_bytes!=out.bytes||actual.generation!=out.native_generation||
       !actual.initialized||actual.hook_mask!=game::building_mixed_identity::opened_file::required_hooks||actual.game_thread!=ordered_thread||!actual.error.empty()||!actual.original_error.empty()){
        error="Ordered preload NativeOpen generation/file/hash/owner readback differs: "+actual.error+" "+actual.original_error;return false;
    }
    out.request_attempted=actual.native_request_attempted;
    if(!out.request_attempted){
        if(actual.state!="pinned"||!actual.held||actual.completed_origin||actual.world_witness_bound){error="Ordered preload is not an unrequested pinned native file";return false;}
        out.loaded=false;out.world_epoch=0;return true;
    }
    if(actual.completed_origin){
        game::building_mixed_identity::opened_file::CurrentWorld world;
        if(!game::native_open_runtime::read_current_world(world,error))return false;
        if(world.uuid!=out.snapshot_uuid||world.sha256!=out.sha256||world.bytes!=out.bytes||world.generation!=out.native_generation||!world.world_epoch){error="Ordered loaded NativeOpen identity differs from retained preload";return false;}
        out.loaded=true;out.world_epoch=world.world_epoch;
        if(!ordered_loaded_observation(out,error))return false;
    }
    return true;
#else
    (void)out;error="Ordered NativeOpen read is unavailable in legacy snapshot fixture";return false;
#endif
}
bool read_ordered_snapshot(OrderedSnapshotIdentity& out,std::string& error){
    out={};if(!ordered_fresh(error))return false;
    auto current=*ordered_snapshot;
    if(current.native_prepared&&!ordered_native_read(current,error))return false;
    out=std::move(current);error.clear();return true;
}
bool read_ordered_snapshot_local(OrderedSnapshotIdentity& out,std::string& error){
    out={};
    if(!ordered_snapshot||GetCurrentThreadId()!=ordered_thread||!ordered_failure.empty()||!ordered_original_cohort){
        error=ordered_failure.empty()?"Recovery local proof lacks its retained original snapshot owner":ordered_failure;return false;
    }
    auto current=*ordered_snapshot;
    if(current.session!=session_wire::session_id()||current.local_peer!=session_wire::local_peer()||
       !current.file_verified||!current.native_prepared||!current.native_generation||!current.seed_present||
       !ordered_native_read(current,error)||!current.loaded||!current.world_epoch){
        if(error.empty())error="Recovery local proof changed its loaded native file, session or generation";
        return false;
    }
    out=std::move(current);error.clear();return true;
}
static void ordered_prepare_received(const std::filesystem::path& runtime){
    std::string error;if(!ordered_fresh(error))throw std::runtime_error(error);
    auto& retained=*ordered_snapshot;
    if(!retained.seed_present||retained.local_peer==1||retained.native_prepared||!buffer.committed()||retained.snapshot_uuid!=incoming_id||retained.sha256!=incoming_hash||retained.bytes!=buffer.size())
        throw std::runtime_error("Ordered receive preload lacks exact committed manifest bytes/seed");
    retained.file_verified=true;
#if !defined(MT2MP_SNAPSHOT_TEST) || defined(MT2MP_ORDERED_PRELOAD_TEST)
    if(incoming_monsters.is_null()){
        auto companion=snapshot_world_path::physical(write_root(),retained.world_name,retained.snapshot_uuid);companion+=".monsters.json";
        if(std::filesystem::exists(companion))throw std::runtime_error("Ordered receive has unpromised local Monster companion");
    }
    uint64_t generation=0;
    if(!game::native_open_runtime::prepare(runtime,retained.snapshot_uuid,retained.sha256,retained.bytes,generation,error,retained.world_name))
        throw std::runtime_error("Ordered native preload preparation failed: "+error);
    retained.native_generation=generation;retained.native_prepared=true;
    if(!ordered_native_read(retained,error))throw std::runtime_error(error);
    status="Ordered snapshot verified; awaiting fresh-load seed owner";
    LOG_NET_INFO("[SNAPSHOT] Ordered preload retained id={} generation={} bytes={}; native request not attempted",retained.snapshot_uuid,generation,retained.bytes);
#else
    (void)runtime;throw std::runtime_error("Ordered preload is unavailable in legacy snapshot fixture");
#endif
}
