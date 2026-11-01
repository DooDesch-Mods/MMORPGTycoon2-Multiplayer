#pragma once
#include "tick_evidence.h"
#include "held_service_interruption.h"
#include "framework_step_adapter.h"
#include "tick_authority.h"
#include "tick_execution.h"
#include "tick_wire.h"
#include "snapshot_barrier.h"
#include <set>
#include <cstring>

namespace mt2mp::action_session::tick_commands {
// Owned by the existing Runtime. Framework retains this object's callback address
// until unbind acknowledges retirement, including BUSY and failed holds.
class OrderedRuntime {
public:
    using Now=std::function<uint64_t()>;
    using Maintain=std::function<bool(std::string&)>;
    OrderedRuntime(Session& session,Engine& engine,Controller& controller,Now now,Now monotonic_us,
                   RunOriginalGo run,Maintain maintain,Controller::OrderedCaptureAdmission capture = {},Maintain interruption = {})
        :session_(session),engine_(engine),controller_(controller),now_(std::move(now)),monotonic_us_(std::move(monotonic_us)),
         run_(std::move(run)),maintain_(std::move(maintain)),capture_(std::move(capture)),interruption_(std::move(interruption)),thread_(std::this_thread::get_id()){}
    OrderedRuntime(const OrderedRuntime&)=delete;
    OrderedRuntime& operator=(const OrderedRuntime&)=delete;
    bool bind(const MT2FW_MT2_StepServiceDraftV1& service,const Context& context,
              uint64_t provider,MT2FW_MT2_StepProofFn proof,void* proof_user,std::string& error){
        if(!thread(error)||attempted_||controller_.status().started)return reject("Ordered step binding requires an unused Runtime",error);
        attempted_=true;
        if(service.size!=sizeof(service)||service.version!=MT2FW_MT2_STEP_DRAFT_VERSION||
           !service.snapshot||!service.exit_save||!service.context||!service.bind||!service.activate||!service.read||!service.unbind||!provider||!proof||
           !now_||!monotonic_us_||!run_||!maintain_)return reject("Ordered Framework driver or current session provider is unavailable",error);
        if(!context.session||context.snapshot_uuid.empty()||context.snapshot_uuid.size()>=MT2FW_MT2_STEP_UUID_BYTES||
           context.local_peer<1||context.local_peer>4||context.host_peer!=1||!context.peer_mask||context.peer_mask>15||
           !(context.peer_mask&1)||!(context.peer_mask&(1u<<(context.local_peer-1)))||
           (context.role!=Role::Host&&context.role!=Role::Client)||
           ((context.role==Role::Host)!=(context.local_peer==context.host_peer)))return reject("Ordered immutable session fields are invalid",error);
        context_=context;service_=service;
        MT2FW_MT2_StepSpec spec{};spec.size=sizeof(spec);spec.dt_bits=MT2FW_MT2_STEP_FIXED_DT_BITS;
        spec.session=context.session;spec.session_provider_token=provider;spec.frozen_peer_mask=context.peer_mask;
        std::memcpy(spec.snapshot_uuid,context.snapshot_uuid.c_str(),context.snapshot_uuid.size()+1);
        spec.boundary=&boundary_call;spec.service=&service_call;spec.user=this;
        spec.proof=proof;spec.proof_user=proof_user;spec.save_request=&save_request_call;
        MT2FW_MT2_StepReceipt receipt{};receipt.size=sizeof(receipt);
        const auto result=service_.bind(service_.context,&spec,&receipt);
        // Keep any returned lease even when the provider reports a failed bind.
        lease_=receipt.lease;framework_thread_=receipt.thread;
        if(result!=MT2FW_MT2_OK)return api_failure("bind",result,receipt,error);
        if(!lease_||!framework_thread_||framework_thread_>UINT32_MAX||receipt.size!=sizeof(receipt)||receipt.phase!=MT2FW_MT2_STEP_ARMED||
           receipt.session!=context.session||receipt.dt_bits!=spec.dt_bits)return reject("Ordered Framework bind receipt differs",error);
        error.clear();return true;
    }
    bool start(const Config& config,ClockPort clock,const MT2FW_MT2_StepActivation& activation,std::string& error){
        if(!thread(error)||!failure_.empty()||!lease_||started_)return reject("Ordered start requires an armed current Framework lease",error);
        if(config.context!=context_||activation.size!=sizeof(activation)||activation.reserved||
           activation.lease!=lease_||activation.session!=context_.session||!activation.world_epoch||
           activation.first_tick!=config.first_tick||!config.first_tick||config.first_tick==UINT64_MAX||
           !std::memchr(activation.snapshot_uuid,0,sizeof(activation.snapshot_uuid))||context_.snapshot_uuid!=activation.snapshot_uuid||
           !clock.read||!clock.set)return reject("Ordered activation, clock or immutable session differs",error);
        MT2FW_MT2_StepReceipt bootstrap{};bootstrap.size=sizeof(bootstrap);
        const auto read=service_.read(service_.context,lease_,&bootstrap);
        if(read!=MT2FW_MT2_OK)return api_failure("activation proof read",read,bootstrap,error);
        const auto role=context_.role==Role::Host?MT2FW_MT2_SESSION_HOST:MT2FW_MT2_SESSION_CLIENT;
        if(!lease_identity(bootstrap)||bootstrap.seed_attempted!=1||bootstrap.seed_returned!=1||
           bootstrap.bootstrap_held!=1||bootstrap.bootstrap_coverage!=MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS||
            bootstrap.completed_tick||bootstrap.tick||bootstrap.invocation||!bootstrap.generation||bootstrap.world_epoch!=activation.world_epoch||
           bootstrap.actual_role!=uint32_t(role)||bootstrap.workers_held!=1||
           bootstrap.callbacks_inflight!=(startup_service_?1u:0u)||bootstrap.original_attempted||bootstrap.original_returned||bootstrap.timer_calls||
           !std::equal(std::begin(bootstrap.initial_witness_sha256),std::end(bootstrap.initial_witness_sha256),activation.initial_witness_sha256))
            return reject("Ordered activation lacks exact Framework seed/bootstrap proof",error);
        generation_=bootstrap.generation;
        std::copy(std::begin(bootstrap.initial_witness_sha256),std::end(bootstrap.initial_witness_sha256),witness_.begin());
        Context actual;
        if(!session_.current(actual,error)||actual!=context_)return reject("Ordered loaded Session differs: "+error,error);
        if(!controller_.start_ordered(config,[this](const Input& input,std::string& cause){
            std::vector<uint8_t> bytes;return wire::encode({context_.peer_mask,input},bytes,cause);
        },[this](Queue& queue,const Assigned& command,std::string& cause){
            if(!executor_||!borrowed_) {cause="Ordered executor lacks its borrowed Framework boundary";return Outcome::RejectedNoEffect;}
            return authority::run(queue,session_,engine_,controller_.bound_binding(),command,[&](std::string& detail){
                snapshot_command_order_=command.order;last_evidence_sha256_.clear();
                const auto outcome=executor_->execute(command,last_execution_,detail);
                snapshot_command_order_=0;
                snapshot_time_executed(command,outcome);
                 if(last_execution_.command==command){last_execution_tick_=borrowed_->tick;last_execution_committed_tick_=0;}
                 if(outcome==Outcome::Confirmed&&requires_evidence(command.input)){
                     std::string digest;
                     if(!canonical_evidence(context_,command,last_execution_,digest,detail)||!queue.bind_evidence(command,digest,detail))return Outcome::Unknown;
                     last_evidence_sha256_=std::move(digest);
                 }
                 return outcome;
            },cause);
        },[this](uint64_t tick,std::string& cause){
            if(!borrowed_||tick!=borrowed_->tick){cause="Ordered advance lacks its exact borrowed Framework tick";return Outcome::RejectedNoEffect;}
            return framework_step(service_,*borrowed_,next_,continuation_,context_,run_,cause);
        },now_(),error,capture_))return reject(error,error);
        if(controller_.bound_binding().native_epoch!=activation.world_epoch)return reject("Ordered activation world epoch differs from native Binding",error);
        snapshot_clock_=clock;
        executor_=std::make_unique<Executor>(session_,engine_,context_,controller_.bound_binding(),std::move(clock),
            [this](const Context& context,const Binding& binding,const Assigned& command,std::string& cause){
                if(!borrowed_||context!=context_||binding!=controller_.bound_binding()){cause="Ordered executor boundary or binding differs";return false;}
                if(const auto* request=std::get_if<Request>(&command.input.command))return authority::matches(context,binding,*request,cause);
                if(const auto* request=std::get_if<SnapshotRequest>(&command.input.command)){
                    SnapshotRequest actual;if(!authority::current_snapshot(actual,cause))return false;
                    if(actual!=*request){cause="Ordered executor snapshot request differs";return false;}return true;
                }
                TimeRequest actual;if(!authority::current_time(actual,cause))return false;
                if(actual!=std::get<TimeRequest>(command.input.command)){cause="Ordered executor time request differs";return false;}return true;
            },config.initial_order,[this](const SnapshotRequest& request,std::string& cause){return snapshot_arm(request,cause);});
        if(!controller_.bind_ordered_snapshot([this](uint64_t peer,const wire::SnapshotControl& value,std::string& cause){
            return snapshot_receive(peer,value,cause);
        },error))return reject(error,error);
        MT2FW_MT2_StepReceipt receipt{};receipt.size=sizeof(receipt);
        const auto result=service_.activate(service_.context,&activation,&receipt);
        if(result!=MT2FW_MT2_OK)return api_failure("activate",result,receipt,error);
        world_=activation.world_epoch;
        if(!identity(receipt)||receipt.phase!=MT2FW_MT2_STEP_HELD||receipt.tick!=config.first_tick||
           receipt.completed_tick!=config.first_tick-1||receipt.workers_held!=1||receipt.callbacks_inflight!=(startup_service_?1u:0u))
            return reject("Ordered Framework activation receipt differs",error);
        last_time_us_=deadline_us_=monotonic_us_();started_=true;error.clear();return true;
    }
    bool read(MT2FW_MT2_StepReceipt& out,std::string& error){
        out={};out.size=sizeof(out);
        if(!thread(error))return false;
        if(!lease_){error="Ordered Framework lease is not bound";return false;}
        const auto result=service_.read(service_.context,lease_,&out);
        if(result!=MT2FW_MT2_OK)return api_failure("read",result,out,error);
        if(!lease_identity(out))return reject("Ordered Framework read receipt identity differs",error);
        error.clear();return true;
    }
    bool preflight_exit(std::string& error){
        if(!thread(error)||!started_||!lease_||retiring_||borrowed_||!service_.preflight_exit){error="Ordered exit preflight lacks its retained owner";return false;}
        MT2FW_MT2_StepReceipt receipt{};receipt.size=sizeof(receipt);
        const auto result=service_.preflight_exit(service_.context,lease_,&receipt);
        if(result!=MT2FW_MT2_OK){error=api_cause("exit preflight",result,receipt);return false;}
        if(!identity(receipt)||(receipt.phase!=MT2FW_MT2_STEP_HELD&&receipt.phase!=MT2FW_MT2_STEP_FAILED)||
           receipt.callbacks_inflight!=1||receipt.workers_held!=1){error="Ordered exit preflight receipt differs";return false;}
        error.clear();return true;
    }
    bool hold_for_leave(std::string& error){
        if(!thread(error)||!started_||!lease_||retiring_){error="Ordered Leave hold requires its active owner";return false;}
        if(!failure_.empty()){
            if(!controller_.can_retire_lobby()){error="Terminal Leave has pending or unknown Controller work";return false;}
            if(!preflight_exit(error))return false;
        }
        leave_hold_=true;error.clear();return true;
    }
    bool unbind(std::string& error){
        if(!thread(error))return false;
        if(borrowed_||startup_service_){error="Ordered unbind requires outside the borrowed boundary/service callback";return false;}
        retiring_=true;
        if(!lease_){error.clear();return true;}
        MT2FW_MT2_StepReceipt receipt{};receipt.size=sizeof(receipt);
        const auto result=service_.unbind(service_.context,lease_,&receipt);
        if(result!=MT2FW_MT2_OK){error=api_cause("unbind",result,receipt);return false;}
        if(receipt.size!=sizeof(receipt)||receipt.lease!=lease_||receipt.session!=context_.session||receipt.thread!=framework_thread_||
           receipt.dt_bits!=MT2FW_MT2_STEP_FIXED_DT_BITS||(receipt.bootstrap_held>1||receipt.bootstrap_coverage!=(receipt.bootstrap_held?MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS:0u))||receipt.callbacks_inflight||
           (receipt.phase!=MT2FW_MT2_STEP_RETIRING&&receipt.phase!=MT2FW_MT2_STEP_FAILED)){error="Ordered Framework unbind did not confirm callback retirement";return false;}
        retired_ack_=true;lease_=0;started_=false;error.clear();return true;
    }
    bool retired_context(Context& out,std::string& error){
        out={};if(!thread(error))return false;
        if(!retired_ack_||!retiring_||lease_||borrowed_||startup_service_){error="Ordered Runtime lacks actual callback retirement acknowledgement";return false;}
        out=context_;error.clear();return true;
    }
    nlohmann::json execution_status()const{
        if(std::this_thread::get_id()!=thread_)return {{"available",false},{"error","Ordered execution receipt requires owner thread"}};
        if(requires_evidence(last_execution_.command.input)&&last_execution_tick_){
            const auto& request=std::get<Request>(last_execution_.command.input.command);const auto state=controller_.ordered_state();
            const bool terrain=request.action==Action::TerrainStroke||
                ((request.action==Action::UndoBuilding||request.action==Action::RedoBuilding)&&
                 request.canonical_record_key=="terrain:"+request.target_action_uuid);
            nlohmann::json out={{"available",true},{"action",terrain?"TerrainEvidence":"CommandEvidence"},{"action_uuid",request.action_uuid},
                {"session",context_.session},{"snapshot_uuid",context_.snapshot_uuid},{"local_peer",context_.local_peer},
                {"order",last_execution_.command.order},{"execution_tick",last_execution_tick_},{"committed_tick",last_execution_committed_tick_},
                {"confirmed",last_execution_.outcome==Outcome::Confirmed},{"evidence_sha256",last_evidence_sha256_},
                {"evidence_tick",state.evidence_tick},{"evidence_order",state.evidence_order},{"evidence_ack_mask",state.evidence_ack_mask},
                {"required_mask",context_.peer_mask},{"cause",last_execution_.cause}};
            if(!terrain)out["command_action"]=int(request.action);
            return out;
        }
        if(const auto* time=std::get_if<TimeRequest>(&last_execution_.command.input.command);time&&last_execution_tick_){
            nlohmann::json out={{"available",true},{"action","Time"},{"session",context_.session},{"snapshot_uuid",context_.snapshot_uuid},
                {"local_peer",context_.local_peer},{"origin_peer",time->peer},{"request_sequence",time->sequence},{"observed_revision",time->observed_revision},
                {"intent",int(time->intent)},{"order",last_execution_.command.order},{"execution_tick",last_execution_tick_},{"committed_tick",last_execution_committed_tick_},
                {"confirmed",last_execution_.outcome==Outcome::Confirmed},{"native_attempted",last_execution_.native_attempted},{"cause",last_execution_.cause}};
            const auto clock=[](const game::time_session::Clock& value){return nlohmann::json{{"selected",value.selected},{"pause",value.pause}};};
            if(last_execution_.clock_before)out["before"]=clock(*last_execution_.clock_before);
            if(last_execution_.clock_after)out["after"]=clock(*last_execution_.clock_after);
            return out;
        }
        const auto* request=std::get_if<Request>(&last_execution_.command.input.command);
        if(request&&last_execution_tick_&&!request->dungeon_building&&
           (request->action==Action::MoveBuilding||request->action==Action::RemoveSnapshotBuilding||
            request->action==Action::UndoBuilding||request->action==Action::RedoBuilding)&&
           (request->canonical_record_key.starts_with("action:")||request->canonical_record_key.find(":world:building:")!=std::string::npos)){
            const auto ledger=[](const Ledger& value){return nlohmann::json{{"cash",value.cash},{"flat_day",value.flat_day},{"record_present",value.record_present},
                {"record_day",value.record_day},{"record_cash",value.record_cash},{"record_income",value.record_income},{"record_expense",value.record_expense},
                {"line_counts",value.line_counts},{"line_income",value.line_income}};};
            const auto object=[](const BuildingState& value){return nlohmann::json{{"type",value.type},{"variant",value.variant},
                {"x",value.x},{"y",value.y},{"z",value.z},{"rotation",value.rotation},{"scale",value.scale},{"alive",value.alive}};};
            nlohmann::json out={{"available",true},{"family","building"},{"session",context_.session},{"local_peer",context_.local_peer},
                {"snapshot_uuid",context_.snapshot_uuid},{"action_uuid",request->action_uuid},{"action",int(request->action)},
                {"target_action_uuid",request->target_action_uuid},{"canonical_record_key",request->canonical_record_key},{"origin_peer",request->peer},
                {"request_sequence",request->sequence},{"observed_revision",request->host_revision},
                {"order",last_execution_.command.order},{"execution_tick",last_execution_tick_},{"committed_tick",last_execution_committed_tick_},
                {"confirmed",last_execution_.outcome==Outcome::Confirmed},{"native_attempted",last_execution_.native_attempted},{"cause",last_execution_.cause}};
            if(last_execution_.evidence){const auto& value=*last_execution_.evidence;
                out["retirement_confirmed"]=value.retirement_confirmed;out["undo_confirmed"]=value.undo_confirmed;
                out["selection_confirmed"]=value.selection_confirmed;out["target_retired"]=value.target_retired;
                out["before"]={{"ledger",ledger(value.before)},{"object",object(value.object_before)}};
                out["after"]={{"ledger",ledger(value.after)},{"object",object(value.object_after)}};
                out["building_clearance_present"]=value.building_clearance.has_value();
            }
            return out;
        }
        if(!request||!last_execution_tick_||(request->action!=Action::HireDeveloper&&request->action!=Action::FireDeveloper))return {{"available",false}};
        const auto ledger=[](const Ledger& value){return nlohmann::json{{"cash",value.cash},{"flat_day",value.flat_day},{"record_present",value.record_present},
            {"record_day",value.record_day},{"record_cash",value.record_cash},{"record_income",value.record_income},{"record_expense",value.record_expense},
            {"line_counts",value.line_counts},{"line_income",value.line_income}};};
        nlohmann::json out={{"available",true},{"session",context_.session},{"local_peer",context_.local_peer},{"snapshot_uuid",context_.snapshot_uuid},
            {"action_uuid",request->action_uuid},{"action",request->action==Action::HireDeveloper?"HireDeveloper":"FireDeveloper"},{"origin_peer",request->peer},
            {"order",last_execution_.command.order},{"execution_tick",last_execution_tick_},{"committed_tick",last_execution_committed_tick_},
            {"confirmed",last_execution_.outcome==Outcome::Confirmed},{"native_attempted",last_execution_.native_attempted},{"cause",last_execution_.cause}};
        if(last_execution_.evidence){const auto& value=*last_execution_.evidence;
            out["retirement_confirmed"]=value.retirement_confirmed;
            out["before"]={{"ledger",ledger(value.before)},{"developers",value.developers_before?game::developer_staff::encode(*value.developers_before):nlohmann::json(nullptr)}};
            out["after"]={{"ledger",ledger(value.after)},{"developers",value.developers_after?game::developer_staff::encode(*value.developers_after):nlohmann::json(nullptr)}};
        }
        return out;
    }
    #include "ordered_snapshot_runtime.inc"
#include "ordered_save_request.inc"
public:
    bool admission_ready()const{
        const auto controller=controller_.status();
        return started_&&!retiring_&&!borrowed_&&failure_.empty()&&!leave_hold_&&!quit_pending_&&
            !ordinary_save_pending()&&!snapshot_active()&&!reload_held_&&reload_token_.empty()&&
            controller.started&&!controller.locked&&!controller.pending;
    }
    const std::string& failure()const{return failure_;}
    uint64_t lease()const{return lease_;}
private:
    std::string held_membership_cause_;
    Session& session_;Engine& engine_;Controller& controller_;Now now_,monotonic_us_;RunOriginalGo run_;Maintain maintain_;
    Controller::OrderedCaptureAdmission capture_;Maintain interruption_;
    std::thread::id thread_;Context context_{};MT2FW_MT2_StepServiceDraftV1 service_{};
    uint64_t lease_=0,world_=0,framework_thread_=0,deadline_us_=0,last_time_us_=0;
    bool attempted_=false,started_=false,retiring_=false,startup_service_=false,retired_ack_=false,leave_hold_=false;
    uint64_t generation_=0;std::array<unsigned char,32> witness_{};
    std::string failure_,last_evidence_sha256_;std::unique_ptr<Executor> executor_;ExecutionReceipt last_execution_{};
    uint64_t last_execution_tick_=0,last_execution_committed_tick_=0;
    const MT2FW_MT2_StepBoundary* borrowed_=nullptr;MT2FW_MT2_StepNext next_=nullptr;void* continuation_=nullptr;
    bool thread(std::string& error){if(std::this_thread::get_id()==thread_)return true;error="Ordered Runtime requires its owner thread";return false;}
    bool reject(const std::string& cause,std::string& error){if(failure_.empty())failure_=cause.empty()?"Ordered Runtime failed without a cause":cause;error=failure_;return false;}
    static std::string api_cause(const char* operation,MT2FW_MT2_Result result,const MT2FW_MT2_StepReceipt& receipt){
        const auto end=std::find(std::begin(receipt.cause),std::end(receipt.cause),'\0');
        return std::string("Ordered Framework ")+operation+" result="+std::to_string(result)+": "+std::string(receipt.cause,end);
    }
    bool api_failure(const char* operation,MT2FW_MT2_Result result,const MT2FW_MT2_StepReceipt& receipt,std::string& error){return reject(api_cause(operation,result,receipt),error);}
    bool lease_identity(const MT2FW_MT2_StepReceipt& receipt)const{
        return receipt.size==sizeof(receipt)&&receipt.lease==lease_&&receipt.session==context_.session&&
            receipt.thread==framework_thread_&&receipt.dt_bits==MT2FW_MT2_STEP_FIXED_DT_BITS&&receipt.bootstrap_held<=1&&receipt.bootstrap_coverage==(receipt.bootstrap_held?MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS:0u);
    }
    bool identity(const MT2FW_MT2_StepReceipt& receipt)const{
        const auto role=context_.role==Role::Host?MT2FW_MT2_SESSION_HOST:MT2FW_MT2_SESSION_CLIENT;
        return lease_identity(receipt)&&receipt.seed_attempted==1&&receipt.seed_returned==1&&receipt.bootstrap_held==1&&
               receipt.generation==generation_&&std::equal(witness_.begin(),witness_.end(),std::begin(receipt.initial_witness_sha256))&&receipt.world_epoch==world_&&
               receipt.actual_role==uint32_t(role)&&receipt.thread==framework_thread_&&receipt.dt_bits==MT2FW_MT2_STEP_FIXED_DT_BITS;
    }
    bool current(std::string& error,bool held_service=false){
        if(!thread(error)||!failure_.empty()||!started_||retiring_)return reject("Ordered Runtime is not active",error);
        const auto status=controller_.status();Context actual;
        if(!status.started||status.locked||!controller_.ordered_mode())return reject("Ordered Controller unavailable: "+status.cause,error);
        if(!session_.current(actual,error)){
            if(held_service&&interruption_&&interruption_(error))throw HeldMembershipInterruption(error);
            return reject("Ordered current Session/Binding differs: "+error,error);
        }
        if(actual!=context_||controller_.bound_context()!=context_||
           !engine_.current(controller_.bound_binding(),context_.snapshot_uuid,error))return reject("Ordered current Session/Binding differs: "+error,error);
        return true;
    }
    MT2FW_MT2_Result maintain(const MT2FW_MT2_StepReceipt* receipt){
        std::string error;
        if(receipt&&receipt->phase==MT2FW_MT2_STEP_FAILED){
            if(borrowed_||startup_service_||!thread(error)||retiring_||!lease_)
                return MT2FW_MT2_FAILED;
            MT2FW_MT2_StepReceipt fresh{};fresh.size=sizeof(fresh);
            const auto result=service_.read(service_.context,lease_,&fresh);
            if(result!=MT2FW_MT2_OK||!lease_identity(*receipt)||!lease_identity(fresh)||
               fresh.phase!=MT2FW_MT2_STEP_FAILED||fresh.callbacks_inflight!=1||receipt->callbacks_inflight!=1||
               fresh.workers_held!=1||receipt->workers_held!=1||fresh.world_epoch!=receipt->world_epoch||
               fresh.generation!=receipt->generation||fresh.completed_tick!=receipt->completed_tick)
                return MT2FW_MT2_FAILED;
            if(failure_.empty())failure_=api_cause("retained failure",MT2FW_MT2_FAILED,fresh);
            startup_service_=true;
            struct Clear {bool& value;~Clear(){value=false;}} clear{startup_service_};
            // The FAILED branch permits local UI and separately preflighted Quit.
            // It cannot reach current Session, Controller.tick or borrowed Next.
            snapshot_terminal_pump();
            return maintain_(error)?MT2FW_MT2_OK:MT2FW_MT2_FAILED;
        }
        if(!receipt||borrowed_||startup_service_||!thread(error)||retiring_||!failure_.empty())
            return reject("Ordered service owner unavailable: "+error,error),MT2FW_MT2_FAILED;
        if(!started_){
            MT2FW_MT2_StepReceipt fresh{};fresh.size=sizeof(fresh);
            const auto read=service_.read(service_.context,lease_,&fresh);
            if(read!=MT2FW_MT2_OK)return api_failure("startup service read",read,fresh,error),MT2FW_MT2_FAILED;
            if(!lease_identity(*receipt)||!lease_identity(fresh)||fresh.phase!=receipt->phase||
               (fresh.phase!=MT2FW_MT2_STEP_ARMED&&fresh.phase!=MT2FW_MT2_STEP_LOADING&&fresh.phase!=MT2FW_MT2_STEP_HELD)||
               fresh.callbacks_inflight!=1||receipt->callbacks_inflight!=1||fresh.original_attempted||fresh.original_returned||fresh.timer_calls||
               fresh.seed_attempted>1||fresh.seed_returned>fresh.seed_attempted||fresh.bootstrap_held>1||
               fresh.seed_attempted!=receipt->seed_attempted||fresh.seed_returned!=receipt->seed_returned||
               fresh.bootstrap_held!=receipt->bootstrap_held||fresh.bootstrap_coverage!=receipt->bootstrap_coverage||(fresh.phase==MT2FW_MT2_STEP_HELD&&!fresh.bootstrap_held))
                return reject("Ordered startup service receipt differs",error),MT2FW_MT2_FAILED;
            startup_service_=true;
            struct Clear {bool& value;~Clear(){value=false;}} clear{startup_service_};
            if(!maintain_(error))return reject("Ordered startup maintenance failed: "+error,error),MT2FW_MT2_FAILED;
            return MT2FW_MT2_OK;
        }
        if(!current(error,true)||!identity(*receipt)||receipt->phase!=MT2FW_MT2_STEP_HELD||
           receipt->workers_held!=1||receipt->callbacks_inflight!=1)return reject("Ordered held service receipt differs: "+error,error),MT2FW_MT2_FAILED;
        if(!maintain_(error))return reject("Ordered held service failed: "+error,error),MT2FW_MT2_FAILED;
        if(leave_hold_){
            MT2FW_MT2_StepReceipt after{};after.size=sizeof(after);
            const auto result=service_.read(service_.context,lease_,&after);
            if(result!=MT2FW_MT2_OK)return api_failure("Leave service read",result,after,error),MT2FW_MT2_FAILED;
            if(after.phase==MT2FW_MT2_STEP_RETIRING){
                if(!identity(after)||after.outcome!=MT2FW_MT2_STEP_NOT_ATTEMPTED||after.tick!=receipt->tick||
                   after.completed_tick!=receipt->completed_tick||after.invocation||after.original_attempted||after.original_returned||after.timer_calls||
                   after.workers_held!=1||after.callbacks_inflight!=1)
                    return reject("Ordered Quit lacks exact untouched held-service exit receipt",error),MT2FW_MT2_FAILED;
                return MT2FW_MT2_OK;
            }
        }
        if(!submit_save(error))return reject("Ordered Save submission failed: "+error,error),MT2FW_MT2_FAILED;
        if(!snapshot_service(*receipt,error))return reject("Ordered snapshot service failed: "+error,error),MT2FW_MT2_FAILED;
        if(!finish_save_request(error))return reject("Ordered Quit Save completion failed: "+error,error),MT2FW_MT2_FAILED;
        if(!controller_.tick_held_service(now_(),error,interruption_))return reject("Ordered held service failed: "+error,error),MT2FW_MT2_FAILED;
        return MT2FW_MT2_OK;
    }
    MT2FW_MT2_StepDisposition boundary(const MT2FW_MT2_StepBoundary* frame,MT2FW_MT2_StepNext next,void* continuation){
        std::string error;
        if(!frame||!next||!continuation||borrowed_||!thread(error)||!failure_.empty()||!started_||retiring_)
            return reject("Ordered boundary unavailable: "+error,error),MT2FW_MT2_STEP_STOP_UNKNOWN;
        const auto status=controller_.status();
        if(!status.started||status.locked||!controller_.ordered_mode()||controller_.bound_context()!=context_)
            return reject("Ordered boundary Controller unavailable or Context changed: "+status.cause,error),MT2FW_MT2_STEP_STOP_UNKNOWN;
        MT2FW_MT2_StepReceipt receipt{};receipt.size=sizeof(receipt);
        const auto read=service_.read(service_.context,lease_,&receipt);
        if(read!=MT2FW_MT2_OK)return api_failure("boundary read",read,receipt,error),MT2FW_MT2_STEP_STOP_UNKNOWN;
        if(!identity(receipt)||frame->size!=sizeof(*frame)||frame->reserved||frame->lease!=lease_||frame->session!=context_.session||
           frame->world_epoch!=world_||frame->actual_role!=receipt.actual_role||frame->thread!=receipt.thread||frame->tick!=receipt.tick||
           !frame->invocation||frame->invocation!=receipt.invocation||frame->dt_bits!=receipt.dt_bits||receipt.phase!=MT2FW_MT2_STEP_HELD||
           receipt.callbacks_inflight!=1||receipt.workers_held!=1||receipt.original_attempted||receipt.original_returned||receipt.timer_calls)
            return reject("Ordered borrowed boundary receipt differs",error),MT2FW_MT2_STEP_STOP_UNKNOWN;
        auto state=controller_.ordered_state();
        if(state.locked||state.consuming||state.next_tick!=frame->tick)return reject("Ordered queue boundary differs: "+state.cause,error),MT2FW_MT2_STEP_STOP_UNKNOWN;
        if(!engine_.current(controller_.bound_binding(),context_.snapshot_uuid,error))
            return reject("Ordered boundary native Binding differs: "+error,error),MT2FW_MT2_STEP_STOP_UNKNOWN;
        Context actual;
        if(!session_.current(actual,error)){
            if(interruption_&&interruption_(error)){held_membership_cause_=error;return MT2FW_MT2_STEP_KEEP_HELD;}
            return reject("Ordered current Session/Binding differs: "+error,error),MT2FW_MT2_STEP_STOP_UNKNOWN;
        }
        if(actual!=context_)return reject("Ordered boundary Session differs from retained Context",error),MT2FW_MT2_STEP_STOP_UNKNOWN;
        if(snapshot_.active()||reload_held_)return MT2FW_MT2_STEP_KEEP_HELD;
        if(context_.role==Role::Host){
            const auto time=monotonic_us_();
            if(time<last_time_us_||deadline_us_>UINT64_MAX-15625)return reject("Ordered Host pacing clock regressed or exhausted",error),MT2FW_MT2_STEP_STOP_UNKNOWN;
            last_time_us_=time;
            if(time<deadline_us_)return MT2FW_MT2_STEP_KEEP_HELD;
        }
        if(state.next_assignment_tick==state.next_tick){
            if(leave_hold_&&state.in_flight)return MT2FW_MT2_STEP_KEEP_HELD;
            if(context_.role==Role::Client||state.in_flight==Queue::in_flight_limit)return MT2FW_MT2_STEP_KEEP_HELD;
            Batch batch;bool interrupted=false;
            if(!controller_.ordered_seal_held(batch,now_(),error,interruption_,interrupted))return reject(error,error),MT2FW_MT2_STEP_STOP_UNKNOWN;
            if(interrupted){held_membership_cause_=error;return MT2FW_MT2_STEP_KEEP_HELD;}
        }
        borrowed_=frame;next_=next;continuation_=continuation;
        struct Clear {OrderedRuntime& owner;~Clear(){owner.borrowed_=nullptr;owner.next_=nullptr;owner.continuation_=nullptr;}} clear{*this};
        Receipt completed;
        if(!controller_.ordered_consume(frame->tick,completed,now_(),error))return reject(error,error),MT2FW_MT2_STEP_STOP_UNKNOWN;
        if(last_execution_tick_==frame->tick&&last_execution_.outcome==Outcome::Confirmed&&completed.tick_completed&&
           completed.tick==frame->tick&&last_execution_.command.order<=completed.last_order)last_execution_committed_tick_=frame->tick;
        if(context_.role==Role::Host)deadline_us_+=15625;
        return MT2FW_MT2_STEP_COMPLETED;
    }
    static MT2FW_MT2_StepDisposition MT2FW_CALL boundary_call(void* user,const MT2FW_MT2_StepBoundary* frame,MT2FW_MT2_StepNext next,void* continuation){
        auto& self=*static_cast<OrderedRuntime*>(user);
        try{return self.boundary(frame,next,continuation);}
        catch(const std::exception& exception){std::string error;self.reject(std::string("Ordered boundary exception: ")+exception.what(),error);}
        catch(...){std::string error;self.reject("Ordered boundary unknown exception",error);}
        return MT2FW_MT2_STEP_STOP_UNKNOWN;
    }
    static MT2FW_MT2_Result MT2FW_CALL service_call(void* user,const MT2FW_MT2_StepReceipt* receipt){
        auto& self=*static_cast<OrderedRuntime*>(user);
        try{return self.maintain(receipt);}
        catch(const HeldMembershipInterruption& interruption){self.held_membership_cause_=interruption.what();return MT2FW_MT2_BUSY;}
        catch(const std::exception& exception){std::string error;self.reject(std::string("Ordered held service exception: ")+exception.what(),error);}
        catch(...){std::string error;self.reject("Ordered held service unknown exception",error);}
        return MT2FW_MT2_FAILED;
    }
};
static_assert(MT2FW_MT2_STEP_DRAFT_VERSION==9,"Ordered Runtime requires Framework Step draft v9");
static_assert(sizeof(MT2FW_MT2_StepReceipt)==408,"Ordered Runtime requires the seed/bootstrap receipt");
static_assert(sizeof(MT2FW_MT2_StepSpec)==152,"Ordered Runtime requires the retained proof callback spec");
}
