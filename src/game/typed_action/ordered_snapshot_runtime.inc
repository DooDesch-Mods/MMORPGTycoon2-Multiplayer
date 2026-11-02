public:
    using SnapshotDiagnostic=std::function<nlohmann::json(const nlohmann::json&,const nlohmann::json&)>;
    void snapshot_diagnostic(SnapshotDiagnostic write){snapshot_diagnostic_=std::move(write);}
    using SnapshotCapture=std::function<bool(std::string&)>;
    using SnapshotWitness=std::function<bool(std::array<uint8_t,32>&,nlohmann::json&,std::string&)>;
    void snapshot_witness(SnapshotWitness read,uint64_t completion_timeout_ms){snapshot_witness_=std::move(read);snapshot_completion_timeout_=completion_timeout_ms;}
    bool hold_after_snapshot(const std::string& token,std::string& error){
        if(!current(error))return false;
        if(token.size()!=36||token.find_first_not_of("0123456789abcdef-")!=std::string::npos||snapshot_active()||ordinary_save_pending()||!reload_token_.empty()){
            error="Ordered reload requires one idle Snapshot owner and fresh hold token";return false;
        }
        reload_token_=token;error.clear();return true;
    }
    bool snapshot_reload_held(const std::string& token,std::string& error){
        if(!current(error))return false;
        if(token!=reload_token_||!reload_held_||reload_nonce_!=snapshot_.nonce||snapshot_.phase!=SnapshotBarrier::Phase::Released){
            error="Ordered reload lacks its exact released Snapshot hold";return false;
        }
        error.clear();return true;
    }
    bool snapshot_active()const{return snapshot_.active()||!snapshot_requested_.empty()||snapshot_clock_pending();}
    void snapshot_native_pending(std::function<bool(bool&,std::string&)> read){snapshot_native_pending_=std::move(read);}
    bool request_snapshot(const std::string& nonce,SnapshotCapture capture,std::string& error,bool manage_clock=false,bool asynchronous=false,bool periodic=false){
        if(!current(error))return false;
        if((!save_queued_nonce_.empty()&&save_queued_nonce_!=nonce)||context_.role!=Role::Host||reload_held_||(!reload_token_.empty()&&(manage_clock||asynchronous))||snapshot_active()||!capture||!snapshot_witness_||!snapshot_completion_timeout_||snapshot_nonces_.contains(nonce)){
            error="Snapshot request requires idle Host, fresh nonce and owned capture/witness";return false;
        }
        snapshot_requested_=nonce;snapshot_periodic_=periodic;snapshot_capture_=std::move(capture);snapshot_nonces_.insert(nonce);
        snapshot_async_=asynchronous;snapshot_native_live_=false;snapshot_pump_error_.clear();
        snapshot_submitted_=false;snapshot_manage_clock_=manage_clock||!reload_token_.empty();snapshot_clock_phase_=SnapshotClockPhase::Idle;
        snapshot_clock_sequence_=0;snapshot_clock_confirmed_=false;snapshot_resume_scale_=0;
        return true;
    }
    bool snapshot_file(const std::string& nonce,std::string& error){
        if(!current(error))return false;
        return snapshot_.confirm_file(nonce,error);
    }
    void snapshot_failed(const std::string& cause){std::string error;snapshot_.fail(cause,error);}
    nlohmann::json snapshot_status()const{
        static constexpr const char* names[]={"idle","armed","sealed","capturing","captured","releasing","released","failed"};
        return {{"phase",names[int(snapshot_.phase)]},{"requested",snapshot_requested_},{"nonce",snapshot_.nonce},
            {"tick",snapshot_.tick},{"order",snapshot_.order},{"prepared_mask",snapshot_.prepared},{"released_mask",snapshot_.released},
            {"peer_mask",context_.peer_mask},{"file_confirmed",snapshot_.file_confirmed},{"witness_sha256",snapshot_.witness},
            {"native_pending",snapshot_native_live_},{"native_pump_error",snapshot_pump_error_},{"managed_clock",snapshot_manage_clock_},{"clock_phase",int(snapshot_clock_phase_)},{"resume_scale",snapshot_resume_scale_},{"reload_token",reload_token_},{"reload_nonce",reload_nonce_},{"reload_held",reload_held_},{"covered_observation_artifact",snapshot_diagnostic_receipt_},{"wait_deadline_ms",snapshot_deadline_},{"cause",snapshot_.cause},{"runtime_failure",failure_},{ "held_membership_interruption",held_membership_cause_}};
    }
private:
    enum class SnapshotClockPhase {Idle,PauseQueued,Paused,ResumeQueued,Done};
    SnapshotClockPhase snapshot_clock_phase_=SnapshotClockPhase::Idle;
    ClockPort snapshot_clock_;bool snapshot_manage_clock_=false,snapshot_clock_confirmed_=false;
    float snapshot_resume_scale_=0;uint64_t snapshot_clock_sequence_=0;
    bool snapshot_clock_pending()const{return snapshot_manage_clock_&&snapshot_clock_phase_!=SnapshotClockPhase::Done;}
    void snapshot_time_executed(const Assigned& command,Outcome outcome){
        const auto* time=std::get_if<TimeRequest>(&command.input.command);
        if(!snapshot_clock_pending()||!time||time->peer!=context_.local_peer||time->sequence!=snapshot_clock_sequence_)return;
        snapshot_clock_confirmed_=outcome==Outcome::Confirmed;
    }
    bool snapshot_clock_submit(game::time_session::Intent intent,std::string& error){
        snapshot_clock_confirmed_=false;
        if(!controller_.submit_ordered_time(intent,now_(),error))return snapshot_.fail(error,error);
        snapshot_clock_sequence_=controller_.ordered_time_sequence();return true;
    }
    bool snapshot_read_clock(game::time_session::Clock& clock,std::string& error){
        if(!current(error))return false;
        if(!snapshot_clock_.read_held){error="Snapshot held clock reader missing";return false;}
        if(!snapshot_clock_.read_held(clock,error))return false;
        return current(error);
    }
    bool snapshot_pause(bool& ready,std::string& error){
        ready=false;if(!snapshot_manage_clock_){ready=true;return true;}
        if(controller_.status().pending)return true;
        game::time_session::Clock clock;
        if(!snapshot_read_clock(clock,error))return snapshot_.fail("Snapshot managed Clock read failed: "+error,error);
        if(snapshot_clock_phase_==SnapshotClockPhase::Idle){
            if(clock.selected!=0&&clock.selected!=1&&clock.selected!=3&&clock.selected!=9)
                return snapshot_.fail("Snapshot selected Clock scale is unsupported actual="+std::to_string(clock.selected),error);
            snapshot_resume_scale_=clock.selected;
            if(clock.selected==0){snapshot_clock_phase_=SnapshotClockPhase::Paused;ready=true;return true;}
            if(!snapshot_clock_submit(game::time_session::Intent::Pause,error))return false;
            snapshot_clock_phase_=SnapshotClockPhase::PauseQueued;return true;
        }
        if(snapshot_clock_phase_!=SnapshotClockPhase::PauseQueued||!snapshot_clock_confirmed_||clock.selected!=0)
            return snapshot_.fail("Snapshot Pause lacks confirmed ordered execution and all peer acknowledgements; selected="+std::to_string(clock.selected),error);
        snapshot_clock_phase_=SnapshotClockPhase::Paused;ready=true;return true;
    }
    bool snapshot_restore(std::string& error){
        if(!snapshot_clock_pending()||!snapshot_requested_.empty()||snapshot_.phase!=SnapshotBarrier::Phase::Released)return true;
        if(controller_.status().pending)return true;
        if(snapshot_clock_phase_==SnapshotClockPhase::Paused){
            if(snapshot_resume_scale_==0){snapshot_clock_phase_=SnapshotClockPhase::Done;return true;}
            const auto intent=snapshot_resume_scale_==1?game::time_session::Intent::Normal:
                snapshot_resume_scale_==3?game::time_session::Intent::Fast:game::time_session::Intent::Fastest;
            if(!snapshot_clock_submit(intent,error))return false;
            snapshot_clock_phase_=SnapshotClockPhase::ResumeQueued;return true;
        }
        game::time_session::Clock clock;
        if(snapshot_clock_phase_!=SnapshotClockPhase::ResumeQueued||!snapshot_clock_confirmed_||
            !snapshot_read_clock(clock,error)||clock.selected!=snapshot_resume_scale_)
            return snapshot_.fail("Snapshot speed restore lacks confirmed ordered execution/readback: "+error,error);
        snapshot_clock_phase_=SnapshotClockPhase::Done;return true;
    }
    std::function<bool(bool&,std::string&)> snapshot_native_pending_;
    bool snapshot_async_=false,snapshot_native_live_=false;std::string snapshot_pump_error_;
    bool snapshot_pump(std::string& error,bool terminal=false){
        if(!snapshot_native_live_)return true;
        if(!snapshot_api(MT2FW_MT2_SNAPSHOT_PUMP,error,terminal))return false;
        bool pending=true;if(!snapshot_native_pending_||!snapshot_native_pending_(pending,error))return false;
        snapshot_native_live_=pending;return true;
    }
    void snapshot_terminal_pump(){
        if(!snapshot_native_live_)return;
        std::string cause;if(!snapshot_pump(cause,true))snapshot_pump_error_=cause;
        else snapshot_pump_error_.clear();
    }
    SnapshotDiagnostic snapshot_diagnostic_;nlohmann::json snapshot_diagnostic_receipt_;
    SnapshotBarrier snapshot_;SnapshotWitness snapshot_witness_;SnapshotCapture snapshot_capture_;
    std::array<std::optional<wire::SnapshotControl>,5> snapshot_early_{};
    std::set<std::string> snapshot_nonces_;std::string snapshot_requested_;nlohmann::json snapshot_observation_;
    uint64_t snapshot_command_order_=0;
    bool snapshot_submitted_=false,snapshot_periodic_=false;
    std::string reload_token_,reload_nonce_;bool reload_held_=false;
    uint64_t snapshot_completion_timeout_=0,snapshot_deadline_=0;
    bool snapshot_wait(uint64_t duration,std::string& error){
        const auto now=now_();if(!duration||now>UINT64_MAX-duration)return snapshot_.fail("Snapshot wait deadline is invalid",error);
        snapshot_deadline_=now+duration;return true;
    }
    bool snapshot_receive(uint64_t peer,const wire::SnapshotControl& value,std::string& error){
        if(!snapshot_.active()&&context_.role==Role::Host&&value.nonce==snapshot_requested_&&
           value.phase==wire::SnapshotPhase::Prepared&&peer>=1&&peer<=4&&peer!=context_.local_peer&&
           (context_.peer_mask&SnapshotBarrier::bit(peer))&&value.ack_mask==SnapshotBarrier::bit(peer)){
            auto& prior=snapshot_early_[peer];
            if(prior&&*prior!=value)return snapshot_.fail("Conflicting early Snapshot Prepared",error);
            prior=value;return true;
        }
        return snapshot_.receive(peer,value,error);
    }
    bool snapshot_api(uint32_t operation,std::string& error,bool terminal=false){
        MT2FW_MT2_SnapshotRequest request{};request.size=sizeof(request);request.operation=operation;
        request.lease=lease_;request.session=context_.session;request.world_epoch=world_;
        request.tick=snapshot_.tick;request.order=snapshot_.order;request.peer_mask=context_.peer_mask;
        std::memcpy(request.nonce,snapshot_.nonce.c_str(),snapshot_.nonce.size()+1);
        if(operation==MT2FW_MT2_SNAPSHOT_ARM||operation==MT2FW_MT2_SNAPSHOT_ARM_PERIODIC)request.invocation=borrowed_?borrowed_->invocation:0;
        else std::copy(snapshot_.witness.begin(),snapshot_.witness.end(),request.witness_sha256);
        if(operation==MT2FW_MT2_SNAPSHOT_CAPTURE||operation==MT2FW_MT2_SNAPSHOT_RELEASE||operation==MT2FW_MT2_SNAPSHOT_PUMP||operation==MT2FW_MT2_SNAPSHOT_CLIENT_CACHE)request.ack_mask=context_.peer_mask;
        MT2FW_MT2_StepReceipt receipt{};receipt.size=sizeof(receipt);
        const auto result=service_.snapshot(service_.context,&request,
            operation==MT2FW_MT2_SNAPSHOT_CAPTURE?&snapshot_capture_call:nullptr,
            operation==MT2FW_MT2_SNAPSHOT_CAPTURE?this:nullptr,&receipt);
        if(result!=MT2FW_MT2_OK){error=api_cause(("snapshot operation "+std::to_string(operation)).c_str(),result,receipt);
            return terminal?false:snapshot_.fail(error,error);}
        if(!identity(receipt)||receipt.workers_held!=1||receipt.callbacks_inflight!=1||
           (operation!=MT2FW_MT2_SNAPSHOT_ARM&&operation!=MT2FW_MT2_SNAPSHOT_ARM_PERIODIC&&receipt.completed_tick!=snapshot_.tick))
            return snapshot_.fail("Snapshot Framework receipt owner/completed tick differs",error);
        return true;
    }
    bool snapshot_arm(const SnapshotRequest& request,std::string& error){
        if(!borrowed_||!snapshot_witness_||!snapshot_completion_timeout_||!snapshot_command_order_)return snapshot_.fail("Snapshot ARM lacks borrowed command and witness",error);
        if(context_.role==Role::Host&&(snapshot_requested_!=request.nonce||snapshot_periodic_!=(request.purpose==SnapshotPurpose::HostPeriodic)||!snapshot_capture_||!snapshot_submitted_))return snapshot_.fail("Snapshot Host capture owner differs",error);
        if(!snapshot_.arm(context_,request,borrowed_->tick,snapshot_command_order_,error))return false;
        snapshot_diagnostic_receipt_=nullptr;
        for(uint64_t peer=1;peer<=4;++peer)if(snapshot_early_[peer]){
            if(!snapshot_.receive(peer,*snapshot_early_[peer],error))return false;snapshot_early_[peer].reset();
        }
        if(!reload_token_.empty()){
            if(!reload_nonce_.empty())return snapshot_.fail("Ordered reload Snapshot ARM repeated",error);
            reload_nonce_=request.nonce;
        }
        controller_.hold_ordered_snapshot(true);return snapshot_api(snapshot_.purpose==SnapshotPurpose::HostPeriodic?MT2FW_MT2_SNAPSHOT_ARM_PERIODIC:MT2FW_MT2_SNAPSHOT_ARM,error);
    }
    bool snapshot_service(const MT2FW_MT2_StepReceipt& receipt,std::string& error){
        using Phase=SnapshotBarrier::Phase;
        if(snapshot_.phase==Phase::Failed){error=snapshot_.cause;return false;}
        if(!snapshot_restore(error))return false;
        if(!snapshot_.active()&&!snapshot_requested_.empty()&&!snapshot_submitted_){
            // Original Go can retain a request, but only the held service enters Controller transport.
            if(controller_.status().pending)return true;
            bool paused=false;if(!snapshot_pause(paused,error))return false;if(!paused)return true;
            if(!controller_.submit_ordered_snapshot(snapshot_requested_,now_(),error,snapshot_periodic_))return snapshot_.fail(error,error);
            snapshot_submitted_=true;return true;
        }
        if(!snapshot_.active())return true;
        if(receipt.completed_tick!=snapshot_.tick)return snapshot_.fail("Snapshot held service completed tick differs",error);
        if(snapshot_deadline_&&now_()>=snapshot_deadline_)
            return snapshot_.fail("Snapshot barrier wait expired phase="+std::to_string(int(snapshot_.phase))+
                " now_ms="+std::to_string(now_())+" deadline_ms="+std::to_string(snapshot_deadline_)+
                " prepared_mask="+std::to_string(snapshot_.prepared)+" released_mask="+std::to_string(snapshot_.released)+
                " required_mask="+std::to_string(context_.peer_mask)+"; no capture replay or automatic release",error);
        if(snapshot_.phase==Phase::Armed){
            std::array<uint8_t,32> hash{};
            if(!snapshot_witness_(hash,snapshot_observation_,error))return snapshot_.fail(error,error);
            if(snapshot_diagnostic_){
                std::string sha;constexpr char digits[]="0123456789abcdef";
                for(auto byte:hash){sha+=digits[byte>>4];sha+=digits[byte&15];}
                snapshot_diagnostic_receipt_=snapshot_diagnostic_({{"nonce",snapshot_.nonce},{"tick",snapshot_.tick},{"order",snapshot_.order},
                    {"lease",lease_},{"session",context_.session},{"world_epoch",world_},{"peer",context_.local_peer},
                    {"peer_mask",context_.peer_mask},{"sha256",sha}},snapshot_observation_);
            }
            if(!snapshot_.seal(hash,error)||!snapshot_api(MT2FW_MT2_SNAPSHOT_SEAL,error))return false;
            if(!snapshot_wait(snapshot_.host()?controller_.ack_timeout_ms():snapshot_completion_timeout_,error))return false;
            if(!snapshot_.host()&&!controller_.send_ordered_snapshot(context_.host_peer,
                snapshot_.packet(wire::SnapshotPhase::Prepared,SnapshotBarrier::bit(context_.local_peer)),error))return snapshot_.fail(error,error);
        }
        if(snapshot_.host()&&snapshot_.phase==Phase::Sealed&&snapshot_.prepared==context_.peer_mask){
            std::array<uint8_t,32> hash{};nlohmann::json fresh;
            if(!snapshot_witness_(hash,fresh,error))return snapshot_.fail(error,error);
            if(hash!=snapshot_.witness||fresh!=snapshot_observation_)return snapshot_.fail("Snapshot covered witness changed after SEAL",error);
            if(!snapshot_.begin_capture(error))return false;
            const bool accepted=snapshot_api(MT2FW_MT2_SNAPSHOT_CAPTURE,error);
            if(!accepted)return false;
            if(!snapshot_.capture_returned(true,{},error))return false;
            if(!snapshot_wait(snapshot_completion_timeout_,error))return false;
        }
        if(snapshot_.host()&&snapshot_.phase==Phase::Captured&&!snapshot_pump(error))return snapshot_.fail(error,error);
        if(snapshot_.host()&&snapshot_.phase==Phase::Captured&&snapshot_.file_confirmed){
            if(snapshot_native_live_)return snapshot_.fail("Snapshot file acknowledged before native async completion",error);
            if(!snapshot_wait(controller_.ack_timeout_ms(),error))return false;
            if(!snapshot_.begin_release(error)||!controller_.send_ordered_snapshot(0,
                snapshot_.packet(wire::SnapshotPhase::Release,context_.peer_mask),error))return snapshot_.fail(error,error);
        }
        if(snapshot_.phase==Phase::Releasing&&(!snapshot_.host()||snapshot_.released==context_.peer_mask)){
            if(!snapshot_api(MT2FW_MT2_SNAPSHOT_RELEASE,error)||!snapshot_.finish_release(error))return false;
            if(!snapshot_.host()&&!controller_.send_ordered_snapshot(context_.host_peer,
                snapshot_.packet(wire::SnapshotPhase::Released,context_.peer_mask),error))return snapshot_.fail(error,error);
            if(!reload_token_.empty()){
                if(reload_nonce_!=snapshot_.nonce)return snapshot_.fail("Ordered reload released Snapshot nonce differs",error);
                reload_held_=true;
                // Reload restores the retained selected speed only in the new epoch.
                snapshot_clock_phase_=SnapshotClockPhase::Done;
            }
            controller_.hold_ordered_snapshot(false);snapshot_requested_.clear();snapshot_periodic_=false;snapshot_capture_={};
            last_time_us_=deadline_us_=monotonic_us_();
            snapshot_deadline_=0;
        }
        return true;
    }
    static MT2FW_MT2_Result MT2FW_CALL snapshot_capture_call(void* user,char* cause,uint32_t capacity){
        auto& self=*static_cast<OrderedRuntime*>(user);std::string error;bool okay=false;
        try{
            if(self.snapshot_.phase!=SnapshotBarrier::Phase::Capturing||!self.snapshot_capture_)
                error="Snapshot capture callback lacks its single-use Host owner";
            else {self.snapshot_native_live_=self.snapshot_async_;auto capture=std::move(self.snapshot_capture_);okay=capture(error);}
        }catch(const std::exception& e){error=std::string("Snapshot capture exception: ")+e.what();}
        catch(...){error="Snapshot capture unknown exception";}
        if(!okay){
            if(error.empty())error="Snapshot capture failed without original cause";
            if(cause&&capacity){const auto count=std::min<size_t>(error.size(),capacity-1);std::memcpy(cause,error.data(),count);cause[count]=0;}
            return MT2FW_MT2_FAILED;
        }
        return MT2FW_MT2_OK;
    }
