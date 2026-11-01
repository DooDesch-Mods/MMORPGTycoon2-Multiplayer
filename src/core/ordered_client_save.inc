namespace client_save_owner {
namespace files=game::client_save_export;
static MT2FW_MT2_StepReceipt owner{};
static std::set<std::string> consumed_requests;
static json local_request;
static std::filesystem::path target,staging;
static std::string virtual_target,capture_id,completed_nonce;
static std::unique_ptr<files::PublicationGroup> publication;
static bool cache_complete=false,file_confirmed=false;
static bool current(std::string& error){
    return game::typed_action_runtime::ordered_save_owner(owner.lease,owner.session,owner.world_epoch,error);
}
static std::filesystem::path local_target(const json& request,std::string& native_target){
    std::string error;
    if(!game::native::original_save_target(request.at("mode").get<uintptr_t>(),request.at("filename").get<std::string>(),
        request.at("kind").get<uint32_t>(),request.at("option").get<bool>(),native_target,error))throw std::runtime_error(error);
    const auto write=status(false).at("write_dir").get<std::string>();
    if(write.empty())throw std::runtime_error("Client original Save PHYSFS write directory is unavailable");
#ifdef MT2MP_FRAMEWORK_PLUGIN
    return net::snapshot_world_path::physical_original(framework_open_paths::verified_write_root(framework_open_paths::absolute(runtime_root),write.c_str()),native_target);
#else
    return net::snapshot_world_path::physical_original(std::filesystem::path(write),native_target);
#endif
}
static files::Publication publish(const files::Receiver& receiver,std::string& error)try{
    if(!publication||local_request.is_null()||ordinary_save_id.empty()||receiver.receipt().at("nonce")!=ordinary_save_id){
        error="Client publication lost its original local Save request";return files::Publication::Failed;
    }
    if(!current(error))return files::Publication::Failed;
    MT2FW_MT2_StepReceipt held{};
    if(!game::typed_action_runtime::read_ordered_steps(held,error))return files::Publication::Failed;
    if(held.phase!=MT2FW_MT2_STEP_HELD||held.workers_held!=1||held.callbacks_inflight!=1)
        return files::Publication::Pending;
    std::string actual_target;
    if(local_target(local_request,actual_target)!=target||actual_target!=virtual_target){
        error="Client original Save local target changed before publication";return files::Publication::Failed;
    }
    const auto dir=runtime_paths::diagnostics(runtime_root);
    if(!publication->complete()){
        try{publication->publish(receiver);}
        catch(...){write_json(dir/("save-publication-"+ordinary_save_id+".json"),publication->receipt());throw;}
        write_json(dir/("save-publication-"+ordinary_save_id+".json"),publication->receipt());
    }
    if(!cache_complete){
        const auto before=game::snapshot_save_observation::read();
        if(!game::typed_action_runtime::publish_ordered_save_cache(ordinary_save_id,error))return files::Publication::Failed;
        const auto after=game::snapshot_save_observation::read();
        const auto changed=game::snapshot_save_observation::compare(before,after);
        if(!changed.empty()){error="Client local cache rebuild changed covered gameplay state fields="+changed.dump();return files::Publication::Failed;}
        cache_complete=true;
        auto receipt=publication->receipt();receipt["cache_completed"]=true;receipt["covered_state_changed_fields"]=changed;
        write_json(dir/("save-publication-"+ordinary_save_id+".json"),receipt);
    }
    error.clear();return files::Publication::Confirmed;
}catch(const std::exception& cause){error=cause.what();return files::Publication::Failed;}
static bool capture(const files::Identity& identity,uint32_t kind,bool option,std::string& error){
    if(!current(error))return false;
    if(!Config::instance().is_host||!ordinary_save_id.empty()||!game::lobby_operations::executor_idle()){
        error="Client Save capture requires the idle current Host native executor";return false;
    }
    MT2FW_MT2_StepReceipt receipt{};
    if(!game::typed_action_runtime::read_ordered_steps(receipt,error))return false;
    uintptr_t mode=0;std::string filename;
    if(!game::native::original_export_request(identity.nonce,kind,option,mode,filename,error))return false;
    json request={{"kind",kind},{"option",option},{"filename",filename},{"mode",mode},{"session",receipt.session},{"world_epoch",receipt.world_epoch}};
    if(!ordered_save_identity(request,error)||!game::lobby_operations::submit_export_save(request,identity.nonce,error))return false;
    capture_id=ordinary_save_id=identity.nonce;ordinary_save_result=nullptr;file_confirmed=false;
    ordinary_save_last={{"id",identity.nonce},{"filename",filename},{"requester",identity.requester},{"state","queued"},{"cause",""}};return true;
}
}
bool ordered_save_export_begin(std::string& error){
    using namespace client_save_owner;
    if(net::save_export_status().value("configured",false)){error="Client Save export already has a session owner";return false;}
    if(!game::typed_action_runtime::read_ordered_steps(owner,error))return false;
    game::client_save_export::Protocol::Ports ports;
    ports.current=&current;ports.capture=&capture;ports.publish=&publish;
    ports.accept=[](const auto& identity,std::string& cause){
        if(!client_save_owner::current(cause))return false;
        if(!ordinary_save_id.empty()||!game::lobby_operations::executor_idle()||game::typed_action_runtime::ordered_snapshot_active()){
            cause="Client Save request arrived while another native Save or Snapshot owns the executor";return false;
        }
        if(!consumed_requests.insert(identity.nonce).second){cause="Client Save nonce was already consumed; no capture replay";return false;}
        return true;
    };
    return net::configure_save_export(std::move(ports),error);
}
bool ordered_save_export_retire(std::string& error){
    if(!net::retire_save_export(error))return false;
    client_save_owner::owner={};client_save_owner::local_request=nullptr;client_save_owner::publication.reset();
    client_save_owner::target.clear();client_save_owner::staging.clear();client_save_owner::virtual_target.clear();
    client_save_owner::capture_id.clear();client_save_owner::cache_complete=client_save_owner::file_confirmed=false;
    ordinary_save_id.clear();ordinary_save_result=nullptr;error.clear();return true;
}
static bool request_client_save(const json& request,std::string& nonce,std::string& error)try{
    using namespace client_save_owner;
    if(request.at("kind").get<uint32_t>()==3){error="Periodic Save belongs to the Host";return false;}
    if(!current(error))return false;
    if(!net::save_export_status().value("configured",false)){error="Client Save export session is unavailable";return false;}
    nonce=game::lobby_operations::fresh_id();local_request=request;
    target=local_target(request,virtual_target);
    staging=runtime_paths::diagnostics(runtime_root)/net::snapshot_world_path::utf8_path("save-export-"+nonce);
    private_profile::DirectoryPins directories;directories.verify(staging,true);
    publication=std::make_unique<files::PublicationGroup>();publication->retain(target,staging,nonce,&current);
    ordinary_save_id=nonce;ordinary_save_result=nullptr;cache_complete=false;
    ordinary_save_last={{"id",nonce},{"filename",request.at("filename")},{"state","queued"},{"cause",""}};
    if(!net::request_save_export(nonce,target,staging,request.at("kind").get<uint32_t>(),request.at("option").get<bool>(),error))return false;
    return true;
}catch(const std::exception& cause){error=cause.what();return false;}
bool ordered_client_save_complete(const std::string& nonce){return !nonce.empty()&&client_save_owner::completed_nonce==nonce;}
static bool finish_client_save_tick()try{
    using namespace client_save_owner;
    if(ordinary_save_id.empty())return false;
    const auto state=net::save_export_status();
    const bool host_export=Config::instance().is_host&&capture_id==ordinary_save_id;
    if(Config::instance().is_host&&!host_export)return false;
    const auto barrier=game::typed_action_runtime::ordered_snapshot_status();
    std::string error=state.value("cause",std::string{});
    if(error.empty())error=barrier.value("runtime_failure",std::string{});
    if(error.empty()&&barrier.value("phase",std::string{})=="failed")error=barrier.value("cause",std::string{});
    if(!error.empty()){ordinary_save_last["state"]="failed";ordinary_save_last["cause"]=error;
        net::fail_save_export(error);game::typed_action_runtime::fail_ordered_snapshot(error);return true;}
    if(host_export){
        if(ordinary_save_result.is_null())game::lobby_operations::take_response(ordinary_save_id,ordinary_save_result);
        if(ordinary_save_result.is_null())return true;
        if(!ordinary_save_result.value("ok",false))throw std::runtime_error(ordinary_save_result.value("error",json::object()).value("message",std::string("Host export capture failed without cause")));
        if(!net::save_export_publication_confirmed())return true;
        if(!file_confirmed){
            if(!game::typed_action_runtime::confirm_ordered_snapshot_file(ordinary_save_id,error))throw std::runtime_error(error);
            file_confirmed=true;return true;
        }
        if(barrier.value("nonce",std::string{})!=ordinary_save_id||barrier.value("phase",std::string{})!="released"||
            game::typed_action_runtime::ordered_snapshot_active())return true;
        if(!net::complete_save_export(error))throw std::runtime_error(error);
    }else if(!state.value("released",false))return true;
    completed_nonce=ordinary_save_id;
    ordinary_save_last["state"]="completed";ordinary_save_last["cause"]="";
    LOG_INFO("[NativeSave] Client export published, cache completed and all peers released id={}",completed_nonce);
    if(!ordered_save_export_retire(error)||!ordered_save_export_begin(error))throw std::runtime_error(error);
    return true;
}catch(const std::exception& cause){
    ordinary_save_last["state"]="failed";ordinary_save_last["cause"]=cause.what();
    net::fail_save_export(cause.what());game::typed_action_runtime::fail_ordered_snapshot(cause.what());
    LOG_ERROR("[NativeSave] Client export failed id={} cause={}",ordinary_save_id,cause.what());return true;
}
