static SnapshotLoadWatch ordered_load_watch;
static bool ordered_load_requested=false,ordered_load_complete=false,ordered_load_acknowledged=false;
static game::normal_load_identity::Status ordered_normal_receipt;
static uint64_t ordered_step_lease=0;
static std::optional<MT2FW_MT2_StepServiceDraftV1> ordered_step_service;
static std::array<std::string,5> ordered_bootstrap_hashes;
static std::string ordered_bootstrap_ready_hash;
static std::array<std::string,5> ordered_covered_hashes;
static std::string ordered_covered_ready_hash,ordered_covered_evidence_path;
static bool ordered_covered_attempted=false;
static SnapshotLoadWatch ordered_covered_watch;
static bool ordered_bootstrap_sent=false,ordered_bootstrap_ready=false;
static bool ordered_title(std::string& error){
    game::native::WorldStatus world;if(!game::native::read_world_status(world,error))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0||world.edit_operations>0){
        error="Ordered fresh load requires idle actual Title before world replacement";return false;
    }
    return true;
}
bool prepare_ordered_host_snapshot(std::string& error){
    try{
        if(!ordered_fresh(error))return false;
        auto& retained=*ordered_snapshot;
        if(retained.local_peer!=1||!retained.seed_present||!retained.file_verified||retained.native_prepared||ordered_source_path.empty()){
            error="Ordered Host preparation requires one retained verified source before any native preparation";return false;
        }
        if(!ordered_title(error))return false;
        const auto write=write_root();
        const auto expected=snapshot_world_path::physical(write,retained.world_name,retained.snapshot_uuid);
        if(std::filesystem::weakly_canonical(ordered_source_path)!=std::filesystem::weakly_canonical(expected)){
            error="Ordered Host source must be the retained canonical snapshot in its actual chosen world folder; import the selected filename before distribution";return false;
        }
        std::string digest;uint64_t bytes=0;
        if(!hash_snapshot_file(expected,digest,bytes,error)||digest!=retained.sha256||bytes!=retained.bytes){
            if(error.empty())error="Ordered Host retained snapshot bytes/hash changed before native preparation";return false;
        }
        auto companion=expected;companion+=".monsters.json";
        if(outbound_monsters.is_null()?std::filesystem::exists(companion):
           !hash_snapshot_file(companion,digest,bytes,error)||digest!=outbound_monsters.at("sha256").get<std::string>()||bytes!=outbound_monsters.at("bytes").get<uint64_t>()){
            if(error.empty())error="Ordered Host retained companion bytes/hash changed before native preparation";return false;
        }
        uint64_t generation=0;
        const auto runtime=std::filesystem::path(Config::instance().runtime_root);
        if(!game::native_open_runtime::prepare(runtime,retained.snapshot_uuid,retained.sha256,retained.bytes,generation,error,retained.world_name))return false;
        retained.native_generation=generation;retained.native_prepared=true;
        if(!ordered_native_read(retained,error))return false;
        LOG_NET_INFO("[SNAPSHOT] Host fresh preload retained id={} generation={} bytes={}",retained.snapshot_uuid,generation,retained.bytes);
        return true;
    }catch(const std::exception& cause){error=cause.what();return false;}
}
static bool ordered_seed_receipt(const MT2FW_MT2_StepServiceDraftV1& service,uint64_t lease,
    const OrderedSnapshotIdentity& proof,std::string& error,bool before_request=true){
    static_assert(sizeof(MT2FW_MT2_StepReceipt)==408);
    if(service.size!=sizeof(service)||service.version!=MT2FW_MT2_STEP_DRAFT_VERSION||!service.context||!service.read||!lease){
        error="Ordered load requires current Framework Step service and retained lease";return false;
    }
    MT2FW_MT2_StepReceipt actual{};actual.size=sizeof(actual);
    const auto result=service.read(service.context,lease,&actual);
    if(result!=MT2FW_MT2_OK){
        const auto end=std::find(std::begin(actual.cause),std::end(actual.cause),'\0');
        error="Ordered load Framework seed read failed result="+std::to_string(result)+" cause="+std::string(std::begin(actual.cause),end);return false;
    }
    const auto role=proof.local_peer==1?MT2FW_MT2_SESSION_HOST:MT2FW_MT2_SESSION_CLIENT;
    const bool phase=before_request?actual.phase==MT2FW_MT2_STEP_ARMED:
        actual.phase==MT2FW_MT2_STEP_ARMED||actual.phase==MT2FW_MT2_STEP_LOADING||actual.phase==MT2FW_MT2_STEP_HELD;
    if(actual.size!=sizeof(actual)||!phase||actual.outcome!=MT2FW_MT2_STEP_NOT_ATTEMPTED||actual.actual_role!=uint32_t(role)||
       actual.lease!=lease||actual.session!=proof.session||actual.thread!=ordered_thread||actual.generation!=proof.native_generation||
       actual.dt_bits!=MT2FW_MT2_STEP_FIXED_DT_BITS||actual.seed_attempted!=1||actual.seed_returned!=1||(actual.bootstrap_held>1||actual.bootstrap_coverage!=(actual.bootstrap_held?MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS:0u))||
       actual.original_attempted||actual.original_returned||actual.timer_calls||
       (before_request&&(actual.bootstrap_held||actual.world_epoch||
        std::any_of(std::begin(actual.initial_witness_sha256),std::end(actual.initial_witness_sha256),[](unsigned char value){return value!=0;})))){
        error="Ordered load requires live ARMED seed-return acknowledgement for its exact role/lease/session/thread/generation";return false;
    }
    return true;
}
bool request_ordered_snapshot_load(const MT2FW_MT2_StepServiceDraftV1& service,uint64_t lease,std::string& error){
    try{
        OrderedSnapshotIdentity before;if(!read_ordered_snapshot(before,error))return false;
        if(!before.seed_present||!before.file_verified||!before.native_prepared||before.request_attempted||before.loaded||ordered_load_requested){
            error="Ordered load continuation requires exact unrequested native preload; no replay";return false;
        }
        if(before.local_peer==1&&(outbound_peers.size()!=outbound_expected_peers||
           std::any_of(outbound_peers.begin(),outbound_peers.end(),[](const auto& peer){return !peer.second.verified;}))){
            error="info: Ordered Host waits for exact peer file/seed agreement";return false;
        }
        if(!ordered_title(error)||!ordered_seed_receipt(service,lease,before,error))return false;
        if(!game::normal_load_identity::begin(before.sha256,before.snapshot_uuid,error))return false;
        const auto normal=game::normal_load_identity::status();
        if(normal.snapshot_uuid!=before.snapshot_uuid||normal.snapshot_sha256!=before.sha256||!normal.load_pending||normal.state!="armed"||!normal.error.empty()){
            error="Ordered normal-load identity arm readback differs: "+normal.error;game::normal_load_identity::cancel(error);return false;
        }
        OrderedSnapshotIdentity again;
        if(!read_ordered_snapshot(again,error)||again.native_generation!=before.native_generation||again.seed!=before.seed||again.seed_present!=before.seed_present||
           !ordered_seed_receipt(service,lease,again,error)){
            if(error.empty())error="Ordered seed/preload identity changed before native request";game::normal_load_identity::cancel(error);return false;
        }
        ordered_step_service=service;ordered_step_lease=lease;
        if(!game::native_open_runtime::before_request(before.native_generation,error)){game::normal_load_identity::cancel(error);return false;}
        // The retained attempt precedes the original call. An unknown request is
        // terminal and must not be retried after a missing acknowledgement.
        ordered_load_requested=true;ordered_load_watch.begin(snapshot_now());
        if(!game::native::request_load(snapshot_world_path::load(before.world_name,before.snapshot_uuid),error)){
            game::normal_load_identity::cancel(error);fail("Ordered original load request failed: "+error);return false;
        }
        status="Ordered original load requested";
        LOG_NET_INFO("[SNAPSHOT] Ordered original load acknowledged id={} generation={} lease={}",before.snapshot_uuid,before.native_generation,lease);
        return true;
    }catch(const std::exception& cause){error=cause.what();if(ordered_load_requested)fail(error);return false;}
}
static void ordered_validate_loaded_ack(const json& value,uint64_t sender){
    ordered_check_ack(value,sender);
    const auto& proof=value.at("ordered_load");
    if(value.at("sha256")!=ordered_snapshot->sha256||!proof.is_object()||proof.size()!=4||
       !proof.at("generation").is_number_unsigned()||!proof.at("world_epoch").is_number_unsigned()||
       !proof.at("registrations").is_number_unsigned()||!proof.at("normal_sha256").is_string()||
       !proof.at("generation").get<uint64_t>()||!proof.at("world_epoch").get<uint64_t>()||proof.at("normal_sha256")!=ordered_snapshot->sha256)
        throw std::runtime_error("Ordered loaded acknowledgement lacks sealed native/normal-load identity");
    if(value.contains("network_identity")||value.contains("network_identity_error"))throw std::runtime_error("Ordered loaded acknowledgement mixes legacy network state-copy proof");
}
static bool ordered_read_normal(const OrderedSnapshotIdentity& proof,std::string& error){
    bool confirmed=false;
    if(!ordered_load_complete||!game::normal_load_identity::read_load_phase(proof.sha256,proof.snapshot_uuid,confirmed,error)||!confirmed){
        if(error.empty())error="Ordered loaded normal-load receipt no longer matches retained native generation";return false;
    }
    return true;
}
static bool ordered_loaded_observation(OrderedSnapshotIdentity& proof,std::string& error){
    if(ordered_load_complete)return ordered_read_normal(proof,error);
    bool confirmed=false;
    if(!ordered_load_requested||!game::normal_load_identity::read_load_phase(proof.sha256,proof.snapshot_uuid,confirmed,error)){
        if(error.empty())error="Ordered native completion lacks its pending normal-load identity";return false;
    }
    proof.loaded=false;proof.world_epoch=0;return true;
}
bool poll_ordered_snapshot_load(bool& pending,std::string& error){
    pending=false;
    try{
        OrderedSnapshotIdentity proof;if(!read_ordered_snapshot(proof,error))return false;
        if(!ordered_load_requested||!ordered_step_service||!ordered_step_lease){error="Ordered load polling requires its retained native request";return false;}
        if(!ordered_seed_receipt(*ordered_step_service,ordered_step_lease,proof,error,false))throw std::runtime_error(error);
        if(ordered_load_acknowledged){if(!proof.loaded||!ordered_read_normal(proof,error))return false;return true;}
        if(ordered_load_watch.expired(snapshot_now()))throw std::runtime_error(ordered_load_watch.timeout_cause(snapshot_now()));
        game::native::WorldStatus world;if(!game::native::read_world_status(world,error))throw std::runtime_error(error);
        if(world.crash_report_active)throw std::runtime_error("Ordered native load entered crash report mode");
        const auto normal=game::normal_load_identity::status();
        if(normal.snapshot_uuid!=proof.snapshot_uuid||normal.snapshot_sha256!=proof.sha256||!normal.error.empty())throw std::runtime_error("Ordered normal-load observation identity failed: "+normal.error);
        if(ordered_load_watch.observe(world.active_mode_index,world.requested_mode_index,normal.registrations,snapshot_now())&&proof.local_peer!=1)
            message({{"type","snapshot_load_progress"},{"id",proof.snapshot_uuid},{"active_mode",world.active_mode_index},{"requested_mode",world.requested_mode_index},{"registrations",normal.registrations}},1);
        if(!game::native::in_game()){pending=true;return true;}
        bool native_pending=false;
        if(!game::native_open_runtime::finish(proof.native_generation,native_pending,error))throw std::runtime_error("Ordered NativeOpen completion failed: "+error);
        if(native_pending){pending=true;return true;}
        if(!game::normal_load_identity::finalize(ordered_normal_receipt,error))throw std::runtime_error("Ordered normal-load completion failed: "+error);
        if(ordered_normal_receipt.state!="confirmed"||ordered_normal_receipt.load_pending||!ordered_normal_receipt.error.empty()||
           ordered_normal_receipt.snapshot_uuid!=proof.snapshot_uuid||ordered_normal_receipt.snapshot_sha256!=proof.sha256)
            throw std::runtime_error("Ordered finalized normal-load receipt differs");
        ordered_load_complete=true;
        if(!read_ordered_snapshot(proof,error)||!proof.loaded||!ordered_read_normal(proof,error))throw std::runtime_error(error.empty()?"Ordered native loaded proof unavailable":error);
        json ack={{"type","snapshot_loaded"},{"id",proof.snapshot_uuid},{"sha256",proof.sha256},
            {"ordered_load",{{"generation",proof.native_generation},{"world_epoch",proof.world_epoch},{"registrations",ordered_normal_receipt.registrations},{"normal_sha256",ordered_normal_receipt.snapshot_sha256}}}};
        ordered_attach_ack(ack);
        const auto& monsters=proof.local_peer==1?outbound_monsters:incoming_monsters;
        if(!monsters.is_null())ack["monster_identity_sha256"]=monsters.at("sha256");
        if(proof.local_peer!=1){message(ack,1);last_loaded_ack=ack;last_loaded=proof.snapshot_uuid;}
        ordered_load_acknowledged=true;snapshot_world_valid=true;ordered_load_watch.finish(snapshot_now());
        status="Ordered native snapshot loaded";
        LOG_NET_INFO("[SNAPSHOT] Ordered native load sealed id={} generation={} world_epoch={}",proof.snapshot_uuid,proof.native_generation,proof.world_epoch);
        return true;
    }catch(const std::exception& cause){error=cause.what();fail(error);return false;}
}

static std::string ordered_bootstrap_digest(const json& value){
    if(!value.is_string())throw std::runtime_error("Ordered bootstrap witness must be a SHA256 string");
    const auto digest=value.get<std::string>();
    if(!valid_snapshot_digest(digest)||digest==std::string(64,'0'))throw std::runtime_error("Ordered bootstrap witness SHA256 invalid");
    return digest;
}
static bool ordered_covered_same(const OrderedSnapshotIdentity& before,const MT2FW_MT2_StepReceipt& held,
                                 const OrderedSnapshotIdentity& after,const MT2FW_MT2_StepReceipt& actual){
    return before.members==after.members&&before.lobby==after.lobby&&before.session==after.session&&
        before.local_peer==after.local_peer&&before.peer_mask==after.peer_mask&&before.region_rng_mode==after.region_rng_mode&&
        before.snapshot_uuid==after.snapshot_uuid&&before.sha256==after.sha256&&before.bytes==after.bytes&&before.seed==after.seed&&
        before.native_generation==after.native_generation&&before.world_epoch==after.world_epoch&&
        after.seed_present&&after.file_verified&&after.native_prepared&&after.request_attempted&&after.loaded&&
        actual.size==sizeof(actual)&&actual.phase==held.phase&&actual.outcome==held.outcome&&
        actual.lease==held.lease&&actual.session==held.session&&actual.thread==held.thread&&
        actual.generation==held.generation&&actual.world_epoch==held.world_epoch&&actual.actual_role==held.actual_role&&
        actual.dt_bits==held.dt_bits&&actual.seed_attempted==1&&actual.seed_returned==1&&
        actual.bootstrap_held==1&&actual.bootstrap_coverage==MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS&&
        actual.workers_held==1&&actual.callbacks_inflight==1&&!actual.original_attempted&&!actual.original_returned&&
        !actual.timer_calls&&!actual.tick&&!actual.invocation&&!actual.completed_tick&&
        std::equal(std::begin(held.initial_witness_sha256),std::end(held.initial_witness_sha256),actual.initial_witness_sha256);
}
static void ordered_export_covered(const OrderedSnapshotIdentity& proof,const std::string& digest,const json& details){
    try{
        const auto runtime=framework_open_paths::absolute(Config::instance().runtime_root);
        const auto directory=runtime_paths::diagnostics(runtime);std::filesystem::create_directories(directory);
        const auto path=directory/("initial-world-"+proof.snapshot_uuid+"-"+std::to_string(proof.session)+"-"+std::to_string(proof.local_peer)+".json");
        std::ofstream stream(path,std::ios::binary|std::ios::trunc);
        stream<<json{{"snapshot_id",proof.snapshot_uuid},{"session",proof.session},{"peer",proof.local_peer},
            {"covered_witness_sha256",digest},{"projection",details}}.dump();stream.close();
        if(!stream)throw std::runtime_error("Initial world witness diagnostic write failed");
        ordered_covered_evidence_path=snapshot_world_path::utf8(path);
    }catch(const std::exception& cause){
        // The local diagnostic file is optional; the held digest agreement is not.
        LOG_WARN("[SNAPSHOT] Initial world witness diagnostic unavailable snapshot={} cause={}",proof.snapshot_uuid,cause.what());
    }
}

static bool ordered_receive_bootstrap(uint64_t sender,const json& value){
    const auto type=value.at("type").get<std::string>();
    if(type!="ordered_bootstrap"&&type!="ordered_bootstrap_ready")return false;
    std::string error;if(!ordered_fresh(error))throw std::runtime_error(error);
    const auto& proof=*ordered_snapshot;
    if(value.size()!=5||value.at("id")!=proof.snapshot_uuid||ordered_manifest.is_null()||
       value.at("ordered_simulation")!=ordered_manifest||!proof.seed_present||!proof.file_verified)
        throw std::runtime_error("Ordered bootstrap snapshot/seed/member agreement differs");
    const auto digest=ordered_bootstrap_digest(value.at("initial_witness_sha256"));
    const auto covered=ordered_bootstrap_digest(value.at("covered_witness_sha256"));
    if(type=="ordered_bootstrap"){
        if(proof.local_peer!=1||sender<2||sender>4||!proof.members[sender])throw std::runtime_error("Ordered bootstrap sender is outside frozen Client membership");
        const auto loaded=outbound_peers.find(sender);
        if(loaded==outbound_peers.end()||!loaded->second.loaded||!loaded->second.ordered_loaded)
            throw std::runtime_error("Ordered bootstrap arrived before physical peer native LoadACK");
        auto& retained=ordered_bootstrap_hashes[sender];
        if(!retained.empty()&&retained!=digest)throw std::runtime_error("Ordered bootstrap duplicate changed its physical sender witness");
        auto& retained_covered=ordered_covered_hashes[sender];
        if(!retained_covered.empty()&&retained_covered!=covered)throw std::runtime_error("Ordered bootstrap duplicate changed its covered world witness");
        retained=digest;retained_covered=covered;
        if(!ordered_covered_hashes[1].empty()&&covered!=ordered_covered_hashes[1])throw std::runtime_error("Ordered peer covered world witness differs from local Host witness");
        if(!ordered_bootstrap_hashes[1].empty()&&digest!=ordered_bootstrap_hashes[1])throw std::runtime_error("Ordered peer bootstrap witness differs from local Host witness");
    }else{
        if(proof.local_peer==1||sender!=1||!ordered_bootstrap_sent)throw std::runtime_error("Ordered bootstrap ready requires actual Host after local witness acknowledgement");
        if(digest!=ordered_bootstrap_hashes[proof.local_peer]||(!ordered_bootstrap_ready_hash.empty()&&ordered_bootstrap_ready_hash!=digest))
            throw std::runtime_error("Ordered Host bootstrap ready witness differs");
        if(covered!=ordered_covered_hashes[proof.local_peer]||(!ordered_covered_ready_hash.empty()&&ordered_covered_ready_hash!=covered))
            throw std::runtime_error("Ordered Host covered world ready witness differs");
        ordered_bootstrap_ready_hash=digest;ordered_covered_ready_hash=covered;
    }
    return true;
}
bool acknowledge_ordered_bootstrap(const MT2FW_MT2_StepServiceDraftV1& service,uint64_t lease,OrderedInitialWitness witness,bool& ready,std::string& error){
    ready=false;
    try{
        OrderedSnapshotIdentity proof;if(!read_ordered_snapshot(proof,error))return false;
        if(!proof.loaded||!ordered_load_acknowledged||!ordered_step_service||lease!=ordered_step_lease||
           service.size!=sizeof(service)||service.version!=MT2FW_MT2_STEP_DRAFT_VERSION||service.context!=ordered_step_service->context||service.read!=ordered_step_service->read||!service.read||!witness){
            error="Ordered bootstrap acknowledgement requires retained loaded native identity and exact live Step service/lease";return false;
        }
        MT2FW_MT2_StepReceipt actual{};actual.size=sizeof(actual);
        const auto result=service.read(service.context,lease,&actual);
        if(result!=MT2FW_MT2_OK){
            const auto end=std::find(std::begin(actual.cause),std::end(actual.cause),'\0');
            throw std::runtime_error("Ordered bootstrap Framework read failed result="+std::to_string(result)+" cause="+std::string(std::begin(actual.cause),end));
        }
        const auto role=proof.local_peer==1?MT2FW_MT2_SESSION_HOST:MT2FW_MT2_SESSION_CLIENT;
        if(actual.size!=sizeof(actual)||(actual.phase!=MT2FW_MT2_STEP_ARMED&&actual.phase!=MT2FW_MT2_STEP_LOADING)||
           actual.outcome!=MT2FW_MT2_STEP_NOT_ATTEMPTED||actual.actual_role!=uint32_t(role)||actual.lease!=lease||
           actual.session!=proof.session||actual.thread!=ordered_thread||actual.generation!=proof.native_generation||
           actual.dt_bits!=MT2FW_MT2_STEP_FIXED_DT_BITS||actual.seed_attempted!=1||actual.seed_returned!=1||(actual.bootstrap_held>1||actual.bootstrap_coverage!=(actual.bootstrap_held?MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS:0u))||
           actual.callbacks_inflight!=1||actual.original_attempted||actual.original_returned||actual.timer_calls||actual.tick||actual.invocation||actual.completed_tick)
            throw std::runtime_error("Ordered bootstrap live Framework identity/seed/phase differs before activation");
        if(!actual.bootstrap_held){error.clear();return true;}
        if(actual.bootstrap_held!=1||actual.workers_held!=1||actual.world_epoch!=proof.world_epoch)throw std::runtime_error("Ordered bootstrap requires Framework-held original workers and actual native epoch");
        static constexpr char hex[]="0123456789abcdef";std::string digest;digest.reserve(64);
        for(const auto byte:actual.initial_witness_sha256){digest.push_back(hex[byte>>4]);digest.push_back(hex[byte&15]);}
        digest=ordered_bootstrap_digest(digest);
        if(!ordered_covered_attempted){
            ordered_covered_attempted=true;ordered_covered_watch.begin(snapshot_now());
            std::array<uint8_t,32> covered{};json details;
            if(!witness(covered,details,error))throw std::runtime_error("Initial held world projection failed: "+error);
            OrderedSnapshotIdentity after;MT2FW_MT2_StepReceipt retained{};retained.size=sizeof(retained);
            const auto readback=service.read(service.context,lease,&retained);
            if(readback!=MT2FW_MT2_OK||!read_ordered_snapshot(after,error)||!ordered_covered_same(proof,actual,after,retained))
                throw std::runtime_error("Initial world projection lost its exact held zero-step owner result="+std::to_string(readback)+" cause="+error);
            std::string covered_hash;covered_hash.reserve(64);
            for(auto byte:covered){covered_hash.push_back(hex[byte>>4]);covered_hash.push_back(hex[byte&15]);}
            ordered_covered_hashes[proof.local_peer]=ordered_bootstrap_digest(covered_hash);
            ordered_export_covered(proof,covered_hash,details);
            LOG_NET_INFO("[SNAPSHOT] Initial covered world sealed id={} peer={} sha256={}",proof.snapshot_uuid,proof.local_peer,covered_hash);
        }
        if(ordered_covered_hashes[proof.local_peer].empty())throw std::runtime_error("Initial world projection was attempted without a complete digest");
        if(ordered_covered_watch.expired(snapshot_now()))throw std::runtime_error(
            "Initial covered world agreement timeout elapsed_ms="+std::to_string(ordered_covered_watch.elapsed_ms(snapshot_now()))+
            " required<"+std::to_string(snapshot_native_load_timeout_seconds)+"s");
        auto& local=ordered_bootstrap_hashes[proof.local_peer];
        if(!local.empty()&&local!=digest)throw std::runtime_error("Ordered local bootstrap witness changed before activation");
        local=digest;
        json ack={{"type","ordered_bootstrap"},{"id",proof.snapshot_uuid},{"initial_witness_sha256",digest},{"covered_witness_sha256",ordered_covered_hashes[proof.local_peer]}};ordered_attach_ack(ack);
        if(proof.local_peer==1){
            for(size_t slot=2;slot<proof.members.size();++slot)if(proof.members[slot]){
                if(ordered_bootstrap_hashes[slot].empty()||ordered_covered_hashes[slot].empty()){error.clear();return true;}
                if(ordered_covered_hashes[slot]!=ordered_covered_hashes[1])throw std::runtime_error("Ordered peer covered world witness differs from actual local Host witness");
                if(ordered_bootstrap_hashes[slot]!=digest)throw std::runtime_error("Ordered peer bootstrap witness differs from actual local Host witness");
            }
            if(!ordered_bootstrap_sent){ack["type"]="ordered_bootstrap_ready";message(ack);ordered_bootstrap_sent=true;}
            ordered_bootstrap_ready_hash=digest;ordered_covered_ready_hash=ordered_covered_hashes[1];
        }else if(!ordered_bootstrap_sent){message(ack,1);ordered_bootstrap_sent=true;}
        ordered_bootstrap_ready=ordered_bootstrap_ready_hash==digest&&ordered_covered_ready_hash==ordered_covered_hashes[proof.local_peer];
        if(ordered_bootstrap_ready&&!ordered_covered_watch.completed)ordered_covered_watch.finish(snapshot_now());
        ready=ordered_bootstrap_ready;error.clear();return true;
    }catch(const std::exception& cause){error=cause.what();fail(error);return false;}
}

bool read_ordered_initial_bootstrap(OrderedSnapshotIdentity& proof,MT2FW_MT2_StepReceipt& actual,std::string& error){
    proof={};actual={};actual.size=sizeof(actual);
    if(!ordered_bootstrap_ready||!ordered_step_service||!ordered_step_lease||!ordered_load_acknowledged||
       !read_ordered_snapshot(proof,error)||!proof.loaded){
        if(error.empty())error="Initial Developer census requires actual loaded peer bootstrap agreement";return false;
    }
    const auto result=ordered_step_service->read(ordered_step_service->context,ordered_step_lease,&actual);
    if(result!=MT2FW_MT2_OK){
        const auto end=std::find(std::begin(actual.cause),std::end(actual.cause),'\0');
        error="Initial Developer live bootstrap read failed result="+std::to_string(result)+" cause="+std::string(std::begin(actual.cause),end);return false;
    }
    const auto role=proof.local_peer==1?MT2FW_MT2_SESSION_HOST:MT2FW_MT2_SESSION_CLIENT;
    if(actual.size!=sizeof(actual)||(actual.phase!=MT2FW_MT2_STEP_ARMED&&actual.phase!=MT2FW_MT2_STEP_LOADING)||
       actual.outcome!=MT2FW_MT2_STEP_NOT_ATTEMPTED||actual.actual_role!=uint32_t(role)||actual.lease!=ordered_step_lease||
       actual.session!=proof.session||actual.thread!=ordered_thread||actual.thread!=GetCurrentThreadId()||
       actual.generation!=proof.native_generation||actual.world_epoch!=proof.world_epoch||actual.dt_bits!=MT2FW_MT2_STEP_FIXED_DT_BITS||
       actual.seed_attempted!=1||actual.seed_returned!=1||actual.bootstrap_held!=1||
       actual.bootstrap_coverage!=MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS||actual.workers_held!=1||actual.callbacks_inflight!=1||
       actual.original_attempted||actual.original_returned||actual.timer_calls||actual.tick||actual.invocation||actual.completed_tick){
        error="Initial Developer census requires exact covered zero-step held bootstrap identity";return false;
    }
    static constexpr char hex[]="0123456789abcdef";std::string digest;digest.reserve(64);
    for(const auto byte:actual.initial_witness_sha256){digest.push_back(hex[byte>>4]);digest.push_back(hex[byte&15]);}
    if(digest!=ordered_bootstrap_ready_hash||digest!=ordered_bootstrap_hashes[proof.local_peer]||
       ordered_covered_hashes[proof.local_peer].empty()||ordered_covered_ready_hash!=ordered_covered_hashes[proof.local_peer]){
        error="Initial Developer census live witness differs from actual peer agreement";return false;
    }
    error.clear();return true;
}
