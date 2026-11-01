public:
    struct SaveIntent {
        uint32_t kind=0;bool option=false;
        uint64_t lease=0,session=0,world_epoch=0,tick=0,invocation=0,mode=0,caller_rva=0;
        std::string filename;
    };
    using SaveSubmit=std::function<bool(const SaveIntent&,std::string&,std::string&)>;
    void save_submit(SaveSubmit submit){save_submit_=std::move(submit);}
    void save_complete(std::function<bool(const std::string&)> complete){save_complete_=std::move(complete);}
    bool export_owner(uint64_t lease,uint64_t session,uint64_t world,std::string& error){
        if(!current(error))return false;
        if(lease!=lease_||session!=context_.session||world!=world_){error="Client export retained native owner changed";return false;}
        error.clear();return true;
    }
    bool export_cache(const std::string& nonce,std::string& error){
        if(!current(error))return false;
        if(context_.role!=Role::Client||nonce!=save_queued_nonce_||nonce!=snapshot_.nonce||snapshot_.phase!=SnapshotBarrier::Phase::Sealed){
            error="Client cache publication lacks its requested sealed barrier";return false;
        }
        return snapshot_api(MT2FW_MT2_SNAPSHOT_CLIENT_CACHE,error);
    }
    bool ordinary_save_pending()const{return save_intent_.has_value()||!save_queued_nonce_.empty();}
    bool quit_pending()const{return quit_pending_;}
    bool quit_ready()const{return quit_ready_;}
    bool request_quit(std::string& error){return exit_save_call(MT2FW_MT2_EXIT_SAVE_BEGIN,{},error);}
    bool legacy_quit(bool reload,std::string& error){
        if(reload&&(!reload_held_||reload_nonce_!=snapshot_.nonce||snapshot_.phase!=SnapshotBarrier::Phase::Released)){
            error="Reload Quit lacks its retained released Snapshot";return false;
        }
        return exit_save_call(reload?MT2FW_MT2_EXIT_SAVE_RELOAD:MT2FW_MT2_EXIT_SAVE_TERMINAL,reload?snapshot_.nonce:std::string{},error);
    }
    nlohmann::json save_request_status()const{
        nlohmann::json out={{"retained",save_intent_.has_value()},{"queued_nonce",save_queued_nonce_},{"client_periodic_delegated",client_periodic_delegated_}};
        if(save_intent_){out["filename"]=save_intent_->filename;out["kind"]=save_intent_->kind;out["option"]=save_intent_->option;}
        out["quit_completion"]=quit_completion_;return out;
    }
private:
    std::function<bool(const std::string&)> save_complete_;
    SaveSubmit save_submit_;std::optional<SaveIntent> save_intent_,save_active_;
    std::string save_queued_nonce_;
    bool save_callback_=false;uint64_t client_periodic_delegated_=0;
    bool quit_pending_=false,quit_ready_=false,quit_requires_save_=false;
    nlohmann::json quit_completion_;
    void retain_quit_completion(const std::string& nonce){
        quit_completion_={{"state","save-complete-before-original-quit"},{"lease",lease_},{"session",context_.session},
            {"world_epoch",world_},{"origin_peer",context_.local_peer},{"native_autosave_required",quit_requires_save_},
            {"nonce",nonce},{"filename",quit_requires_save_?"autosave":""},
            {"final_host_ack",context_.role==Role::Client&&quit_requires_save_},
            {"barrier",quit_requires_save_?snapshot_status():nlohmann::json{}}};
    }
    bool exit_save_call(uint32_t operation,const std::string& nonce,std::string& error){
        if(!thread(error)||!started_||!lease_||retiring_||borrowed_||!service_.exit_save){
            error="Quit Save operation lacks its retained held owner";return false;
        }
        MT2FW_MT2_StepReceipt receipt{};receipt.size=sizeof(receipt);
        const auto result=service_.exit_save(service_.context,lease_,operation,nonce.c_str(),&receipt);
        if(result!=MT2FW_MT2_OK){error=api_cause("Quit Save",result,receipt);return false;}
        if(!identity(receipt)||(receipt.phase!=MT2FW_MT2_STEP_HELD&&receipt.phase!=MT2FW_MT2_STEP_FAILED)||
            receipt.callbacks_inflight!=1||receipt.workers_held!=1){error="Quit Save operation changed its retained receipt";return false;}
        error.clear();return true;
    }
    bool retain_save(const MT2FW_MT2_SaveRequest* request,std::string& error){
        if(!thread(error)||!request||request->size!=sizeof(*request)||request->reserved||request->reserved1||
           request->option>1||(request->kind!=MT2FW_MT2_SAVE_ASYNC&&request->kind!=MT2FW_MT2_SAVE_BLOCKING&&request->kind!=MT2FW_MT2_SAVE_PERIODIC&&request->kind!=MT2FW_MT2_SAVE_QUIT)||
           !request->filename||!request->filename_bytes||request->filename_bytes>4096||
           std::memchr(request->filename,0,request->filename_bytes)){
            error="Ordered Save request shape or owner thread differs";return false;
        }
        if(save_callback_||!reload_token_.empty()||(!ordinary_save_pending()&&snapshot_active())||!save_submit_||retiring_||leave_hold_||!failure_.empty()){
            error="Ordered Save request lacks its idle retained owner";return false;
        }
        if(!current(error))return false;
        MT2FW_MT2_StepReceipt receipt{};receipt.size=sizeof(receipt);
        const auto result=service_.read(service_.context,lease_,&receipt);
        if(result!=MT2FW_MT2_OK){error=api_cause("Save request read",result,receipt);return false;}
        if(!identity(receipt)||request->lease!=lease_||request->session!=context_.session||
           request->world_epoch!=world_||request->thread!=framework_thread_||request->tick!=receipt.tick||
           request->invocation!=receipt.invocation||!request->mode||!request->caller_rva||
           (receipt.phase!=MT2FW_MT2_STEP_HELD&&receipt.phase!=MT2FW_MT2_STEP_ENTERED)){
            error="Ordered Save request has stale native execution identity";return false;
        }
        if(request->kind==MT2FW_MT2_SAVE_QUIT){
            if(quit_pending_||ordinary_save_pending()||request->caller_rva!=0x4a0690||request->filename_bytes!=8||
                std::memcmp(request->filename,"autosave",8)){
                error="Quit Save intent differs from the untouched original branch";return false;
            }
            quit_pending_=true;quit_requires_save_=request->option!=0;
            if(!quit_requires_save_)return true;
        }
        if(request->kind==MT2FW_MT2_SAVE_PERIODIC&&(request->option||request->filename_bytes!=8||std::memcmp(request->filename,"periodic",8))){
            error="Periodic Save request changed its fixed filename/options";return false;
        }
        if(context_.role==Role::Client&&request->kind==MT2FW_MT2_SAVE_PERIODIC){
            // The Host owns periodic rotation. The framework resets only this
            // Client's original periodic timer after validating this callback.
            ++client_periodic_delegated_;return true;
        }
        if(ordinary_save_pending()){
            // Native Update can request the same due periodic save until the
            // original outer call resets its timer inside the agreed capture.
            if(save_active_&&request->kind==MT2FW_MT2_SAVE_PERIODIC&&save_active_->kind==request->kind&&
               save_active_->lease==request->lease&&save_active_->session==request->session&&save_active_->world_epoch==request->world_epoch&&
               save_active_->mode==request->mode&&save_active_->filename==std::string(request->filename,request->filename_bytes))return true;
            error="Ordered Save request differs from its pending retained owner";return false;
        }
        // These bytes belong to the requesting process. A Client pathname must
        // never become the Host's native output target.
        save_callback_=true;struct Clear {bool& value;~Clear(){value=false;}} clear{save_callback_};
        SaveIntent owned{request->kind==MT2FW_MT2_SAVE_QUIT?uint32_t(MT2FW_MT2_SAVE_ASYNC):request->kind,request->kind==MT2FW_MT2_SAVE_QUIT?false:request->option!=0,request->lease,request->session,request->world_epoch,
            request->tick,request->invocation,request->mode,request->caller_rva,
            std::string(request->filename,request->filename_bytes)};
        save_active_=owned;save_intent_=std::move(owned);return true;
    }
    bool submit_save(std::string& error){
        if(!save_intent_)return true;
        if(save_callback_||borrowed_||!current(error)){
            if(error.empty())error="Ordered Save submit requires its current held service";return false;
        }
        if(controller_.status().pending)return true;
        auto request=std::move(*save_intent_);save_intent_.reset();
        if(!save_submit_(request,save_queued_nonce_,error)){
            if(error.empty())error="Ordered Save submit failed without original cause; no replay";return false;
        }
        return true;
    }
    bool finish_save_request(std::string& error){
        if(quit_pending_&&!quit_requires_save_&&!quit_ready_){
            if(!exit_save_call(MT2FW_MT2_EXIT_SAVE_COMPLETE,{},error))return false;
            retain_quit_completion({});quit_ready_=true;leave_hold_=true;
        }
        if(!save_queued_nonce_.empty()&&snapshot_.nonce==save_queued_nonce_&&snapshot_.phase==SnapshotBarrier::Phase::Released&&!snapshot_clock_pending()&&
           (context_.role==Role::Host||(save_complete_&&save_complete_(save_queued_nonce_))))
            {
                if(quit_pending_&&!quit_ready_){
                    if(!exit_save_call(MT2FW_MT2_EXIT_SAVE_COMPLETE,save_queued_nonce_,error))return false;
                    retain_quit_completion(save_queued_nonce_);quit_ready_=true;leave_hold_=true;
                }
                save_queued_nonce_.clear();save_active_.reset();
            }
        return true;
    }
    static MT2FW_MT2_Result MT2FW_CALL save_request_call(void* user,const MT2FW_MT2_SaveRequest* request,char* cause,uint32_t capacity){
        if(!user)return MT2FW_MT2_INVALID;
        auto& self=*static_cast<OrderedRuntime*>(user);std::string error;
        try{if(self.retain_save(request,error))return MT2FW_MT2_OK;}
        catch(const std::exception& problem){error=std::string("Ordered Save capture exception: ")+problem.what();}
        catch(...){error="Ordered Save capture unknown exception";}
        if(cause&&capacity){const auto count=std::min<size_t>(error.size(),capacity-1);std::memcpy(cause,error.data(),count);cause[count]=0;}
        return MT2FW_MT2_FAILED;
    }
