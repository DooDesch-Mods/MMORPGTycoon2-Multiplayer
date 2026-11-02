#pragma once
#include "ordered_session_proof.h"
#include "mt2mp/core/game_update_pair.h"
#include "prepare_flow.h"
#include "ordered_lobby_view.h"
#include "native_lobby.h"
#include "selected_snapshot.h"
#include "typed_action_runtime.h"
#include "session_reload/production.h"
#include "session_reload/recovery_owner.h"
#include "session_provider.h"
#include "typed_action/tick_commands.h"
#include "typed_action/held_service_interruption.h"
#include "mt2mp/core/framework_runtime.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/local_session.h"
#include "mt2mp/core/config.h"

namespace mt2mp::game::ordered_startup {
struct State {
    bool selected=false,bound=false,loaded=false,started=false,retiring=false,reloading=false;
    bool leave_queued=false,quit_attempted=false,quit_confirmed=false,steps_unbound=false,provider_unbound=false,snapshot_retired=false;
    uint64_t leave_started=0;bool leave_managed_save=false,leave_save_started=false;
    float resume_scale=0;bool resume_retained=false,resume_attempted=false,resume_submitted=false;
    std::string retirement_error,ui_error;
    uint32_t thread=0;
    std::string phase="idle",failure;
    MT2FW_MT2_StepServiceDraftV1 steps{};
    MT2FW_MT2_PatchesV1 patches{};
    shared_time::Config clock;
    std::unique_ptr<ordered_session::ProofOwner> proof;
    std::unique_ptr<session_reload::recovery::Owner> recovery;
};
inline State state;
inline std::string last_retired_failure;
inline uint64_t leave_confirmed=0,reload_confirmed=0;
inline nlohmann::json last_leave_save;
inline void phase(const char* next){
    if(state.phase==next)return;
    state.phase=next;LOG_INFO("[OrderedStartup] phase={}",next);
}
inline bool fail(const std::string& cause,std::string& error){
    if(state.failure.empty()){
        state.failure=cause.empty()?"Ordered startup failed without a cause":cause;
        LOG_ERROR("[OrderedStartup] {}",state.failure);phase("failed");
    }
    error=state.failure;return false;
}
inline bool selected(){return state.selected;}
inline bool started(){return state.started;}
inline bool leaving(){return state.leave_queued||state.retiring;}
inline bool retiring(){return state.retiring;}
inline bool request_leave(std::string& error){
    if(!state.selected||!state.started||!state.bound||state.thread!=GetCurrentThreadId()){
        error="Ordered Leave requires a running known session: "+state.failure;return false;
    }
    if(leaving()){
        if(state.leave_managed_save&&!state.failure.empty()){
            if(state.retiring||state.quit_attempted||state.reloading){error=state.failure;return false;}
            bool ready=false;if(!typed_action_runtime::ordered_leave_ready(ready,error))return false;
            if(!ready){error="Terminal Leave requires an idle actual Controller; original failure="+state.failure;return false;}
            if(!typed_action_runtime::request_ordered_leave_hold(error)||
                !typed_action_runtime::prepare_ordered_quit(true,false,error))return false;
            state.leave_managed_save=false;state.leave_started=GetTickCount64();
            phase("leave_queued");error.clear();return true;
        }
        error.clear();return true;
    }
    if(!state.failure.empty()){
        bool ready=false;if(!typed_action_runtime::ordered_leave_ready(ready,error))return false;
        if(!ready){error="Terminal Leave requires an idle actual Controller";return false;}
    }
    state.leave_managed_save=state.failure.empty()&&!state.reloading;
    if(!state.leave_managed_save){
        if(!typed_action_runtime::prepare_ordered_quit(true,state.reloading,error)||
            !typed_action_runtime::request_ordered_leave_hold(error))return false;
    }
    last_leave_save=nullptr;
    state.leave_queued=true;state.leave_started=GetTickCount64();phase("leave_queued");error.clear();return true;
}
inline bool request_reload_retirement(const std::string& token,std::string& error){
    if(!state.selected||!state.started||state.retiring||state.leave_queued||!state.failure.empty()||
       !typed_action_runtime::ordered_snapshot_reload_held(token,error)){
        if(error.empty())error="Ordered reload retirement requires its released, held running session";
        return false;
    }
    state.reloading=true;
    if(!request_leave(error)){state.reloading=false;return false;}
    return true;
}
inline bool request_recovery_retirement(std::string& error){
    if(!state.selected||!state.started||state.retiring||state.leave_queued||!state.failure.empty()||!state.recovery||state.thread!=GetCurrentThreadId()){
        error="Recovery Quit requires its untouched running original world";return false;
    }
    MT2FW_MT2_RecoveryReceipt receipt{};
    if(!state.recovery->read(receipt,error)||!typed_action_runtime::recovery_retirement_ready(receipt,error))return false;
    if(!typed_action_runtime::begin_recovery_reload(receipt,error))return false;
    state.reloading=true;state.retiring=true;state.quit_attempted=true;state.leave_started=GetTickCount64();
    last_leave_save={{"route","original-peer-recovery"},{"counter_before",reload_confirmed},{"original_quit_attempted",true},
        {"original_quit_confirmed",false},{"native_title_confirmed",false},{"recovery_nonce",receipt.recovery_nonce}};
    if(!native::request_return_to_title(error,false))return fail(error,error);
    state.quit_confirmed=true;last_leave_save["original_quit_confirmed"]=true;phase("leaving_shared_world");error.clear();return true;
}
inline bool service_leave(std::string& error){
    if(!state.leave_queued||state.retiring||state.quit_attempted){error="Ordered Leave service lacks one untouched queued request";return false;}
    // A retained Save failure is not another wait for a healthy completion.
    // Keep its request and native owner intact, without converting it to Quit.
    if(state.leave_managed_save&&!state.failure.empty()){
        phase("failed");state.retirement_error=state.failure;error=state.failure;return false;
    }
    if(GetTickCount64()-state.leave_started>=uint64_t(net::snapshot_native_load_timeout_seconds)*1000)
        return fail("Ordered Leave timeout in "+state.phase+"; required=completed Save and idle Controller; native Quit not attempted",error);
    const auto reject=[&](const std::string& cause){
        state.retirement_error=cause;
        if(state.failure.empty())return fail(cause,error);
        error=cause;return false;
    };
    if(state.leave_managed_save&&!state.leave_save_started){
        state.leave_save_started=true;
        if(!typed_action_runtime::prepare_ordered_quit(false,false,error))return reject(error);
    }
    if(state.leave_managed_save&&!typed_action_runtime::ordered_quit_ready()){
        phase("saving_before_leave");error.clear();return true;
    }
    bool ready=false;if(!typed_action_runtime::ordered_leave_ready(ready,error))return reject(error);
    if(!ready){phase("waiting_for_leave_drain");error.clear();return true;}
    last_leave_save={{"route",state.leave_managed_save?"managed":state.reloading?"reload-original-autosave":"terminal-original-autosave"},
        {"counter_before",state.reloading?reload_confirmed:leave_confirmed},{"original_quit_attempted",false},
        {"original_quit_confirmed",false},{"native_title_confirmed",false},{"terminal_cause",state.failure}};
    if(state.leave_managed_save)last_leave_save["completion"]=typed_action_runtime::ordered_native_save_status().at("quit_completion");
    else if(state.leave_save_started&&!state.reloading)
        last_leave_save["failed_managed_save"]=typed_action_runtime::ordered_native_save_status();
    if(!(state.reloading?typed_action_runtime::begin_world_reload(error):typed_action_runtime::begin_lobby_leave(error)))return reject(error);
    state.retiring=true;state.quit_attempted=true;last_leave_save["original_quit_attempted"]=true;
    if(!native::request_return_to_title(error,state.leave_managed_save))return reject(error);
    state.quit_confirmed=true;last_leave_save["original_quit_confirmed"]=true;phase("leaving_shared_world");error.clear();return true;
}
inline bool poll_leave(std::string& error){
    if(!state.selected||!state.retiring||state.thread!=GetCurrentThreadId()){
        error="Ordered retirement poll requires its selected game-thread owner";return false;
    }
    if(!state.quit_confirmed){error=state.failure.empty()?"Ordered Quit lacks original acknowledgement; no replay":state.failure;return false;}
    if(GetTickCount64()-state.leave_started>=uint64_t(net::snapshot_native_load_timeout_seconds)*1000){
        error="Ordered Leave timeout awaiting native Title/callback retirement; owner retained";state.retirement_error=error;return false;
    }
    if(!state.steps_unbound){
        if(!typed_action_runtime::unbind_ordered_steps(error)){state.retirement_error=error;return false;}
        state.steps_unbound=true;
    }
    if(!state.provider_unbound){
        if(!state.proof){error="Ordered Leave lost its retained proof owner";state.retirement_error=error;return false;}
        if(!state.proof->unbind(error)){state.retirement_error=error;return false;}
        state.provider_unbound=true;
    }
    native::WorldStatus actual;
    if(!native::read_world_status(actual,error)){state.retirement_error=error;return false;}
    if(actual.active_mode_index!=2||actual.title_transition_pending||actual.saves_in_progress>0||actual.edit_operations>0){
        phase("waiting_for_title");error.clear();return true;
    }
    if(!state.snapshot_retired){
        // Snapshot retirement clears the UUID required by the existing departure notice.
        if(!state.reloading&&Config::instance().local_test&&!net::publish_local_host_leave(error)){state.retirement_error=error;return false;}
        if(!net::retire_ordered_snapshot(state.proof.get(),error)){state.retirement_error=error;return false;}
        state.snapshot_retired=true;
    }
    bool ready=false;
    if(!(state.reloading?typed_action_runtime::finish_world_reload(ready,error):typed_action_runtime::finish_lobby_leave(ready,error))){state.retirement_error=error;return false;}
    if(!ready){phase("waiting_for_lobby_retirement");error.clear();return true;}
    if(state.recovery&&!session_reload::retire_recovery_owner(*state.recovery,error)){state.retirement_error=error;return false;}
    last_leave_save["native_title_confirmed"]=true;
    last_retired_failure=state.failure;if(state.reloading)++reload_confirmed;else ++leave_confirmed;state={};error.clear();return true;
}

inline bool current(std::string& error){
    if(!state.selected||state.retiring||state.thread!=GetCurrentThreadId()){
        error="Ordered startup requires its selected game-thread owner";return false;}
    if(!state.failure.empty()){error=state.failure;return false;}return true;
}
inline bool pump_transport(std::string& error){
    game_update_pair::Scope traced(game_update_pair::Section::OrderedTransport);
    if(Config::instance().local_test)net::tick_local_session();
    else if(!net::session_wire::tick(error)&&!error.starts_with("info:"))return false;
    if(!net::session_wire::connected()){error="Ordered session disconnected: "+net::session_wire::failure_reason();return false;}
    net::tick_snapshot_sync();
    net::OrderedSnapshotIdentity identity;
    if(!net::read_ordered_snapshot(identity,error)){
        if(state.started&&action_session::tick_commands::held_membership_interruption(net::session_wire::membership_pending(),
            [](std::string& why){net::OrderedSnapshotIdentity local;if(!net::read_ordered_snapshot_local(local,why))return false;
                if(!local.loaded){why="Original loaded snapshot unavailable";return false;}return true;},error))
            throw action_session::tick_commands::HeldMembershipInterruption(error);
        return false;
    }
    if(Config::instance().local_test&&net::local_host_left(identity.snapshot_uuid)){
        error="Host ended the selected ordered session";return false;
    }
    error.clear();return true;
}
inline bool retain_resume_scale(float scale,std::string& error){
    if(!current(error))return false;
    if(state.bound||state.started||state.resume_retained||net::session_wire::local_peer()!=1||
       (scale!=0&&scale!=1&&scale!=3&&scale!=9))return fail("Ordered resume requires one known pre-load Host speed",error);
    state.resume_scale=scale;state.resume_retained=true;error.clear();return true;
}
inline bool resume_current_world(std::string& error){
    if(!state.resume_retained||state.resume_attempted){error.clear();return true;}
    ordered_time_input::Current actual;uint64_t sequence=0,revision=0;bool captured=false;
    if(!typed_action_runtime::read_ordered_time_input(actual,sequence,revision,captured,error))return fail(error,error);
    if(!actual.ordered||actual.original_go||actual.context.local_peer!=1)return fail("Ordered resume lost actual held Host Session/Binding",error);
    if(actual.pending||captured){error.clear();return true;}
    const auto intent=state.resume_scale==0?time_session::Intent::Pause:state.resume_scale==1?time_session::Intent::Normal:state.resume_scale==3?time_session::Intent::Fast:time_session::Intent::Fastest;
    state.resume_attempted=true;
    if(!typed_action_runtime::submit_ordered_time(actual.context,actual.binding,intent,error))return fail(error,error);
    state.resume_submitted=true;error.clear();return true;
}
inline bool maintain_session(std::string& error){
    game_update_pair::Scope traced(game_update_pair::Section::OrderedMaintain);
    if(!current(error))return false;
    if(!pump_transport(error))return fail(error,error);
    if(!state.leave_queued&&typed_action_runtime::ordered_quit_pending()){
        if(!request_leave(error))return false;
    }
    if(state.leave_queued)return service_leave(error);
    if(state.started){
        session_reload::tick();
        if(state.leave_queued)return service_leave(error);
        if(session_reload::busy()){error.clear();return true;}
        return resume_current_world(error);
    }
    net::OrderedSnapshotIdentity identity;
    if(!net::read_ordered_snapshot(identity,error))return fail(error,error);
    MT2FW_MT2_StepReceipt receipt{};receipt.size=sizeof(receipt);
    if(!typed_action_runtime::read_ordered_steps(receipt,error))return fail(error,error);
    if(receipt.seed_returned!=1){phase("waiting_for_seed");error.clear();return true;}
    if(!identity.request_attempted){
        if(!net::request_ordered_snapshot_load(state.steps,receipt.lease,error)){
            if(error.starts_with("info:")){phase("waiting_for_verified_files");error.clear();return true;}
            return fail(error,error);
        }
        phase("loading_shared_world");error.clear();return true;
    }
    if(!state.loaded){
        bool pending=true;
        if(!net::poll_ordered_snapshot_load(pending,error))return fail(error,error);
        if(pending){phase("loading_shared_world");error.clear();return true;}
        state.loaded=true;
    }
    if(!typed_action_runtime::read_ordered_steps(receipt,error))return fail(error,error);
    if(receipt.bootstrap_held!=1){phase("waiting_for_native_bootstrap");error.clear();return true;}
    bool agreed=false;
    if(!net::acknowledge_ordered_bootstrap(state.steps,receipt.lease,&typed_action_runtime::prepare_initial_witness,agreed,error))return fail(error,error);
    if(!agreed){phase("waiting_for_matching_worlds");error.clear();return true;}
    action_session::tick_commands::Config commands;
    if(!action_session::selected_session().current(commands.context,error))return fail(error,error);
    MT2FW_MT2_StepActivation activation{};activation.size=sizeof(activation);
    activation.lease=receipt.lease;activation.session=receipt.session;activation.world_epoch=receipt.world_epoch;
    activation.first_tick=commands.first_tick;
    if(commands.context.snapshot_uuid.size()>=sizeof(activation.snapshot_uuid))return fail("Ordered activation UUID exceeds native capacity",error);
    std::memcpy(activation.snapshot_uuid,commands.context.snapshot_uuid.c_str(),commands.context.snapshot_uuid.size()+1);
    std::memcpy(activation.initial_witness_sha256,receipt.initial_witness_sha256,32);
    if(!typed_action_runtime::start_ordered(commands,state.clock,activation,error))return fail(error,error);
    if(!lobby_prepare::enable_captures(error))return fail(error,error);
    if(!net::retain_ordered_original_cohort(error))return fail(error,error);
    state.started=true;phase("running");error.clear();return true;
}
inline bool maintain(std::string& error){
    MT2FW_MT2_StepReceipt retained{};retained.size=sizeof(retained);
    if(state.selected&&state.bound&&state.thread==GetCurrentThreadId()&&
       typed_action_runtime::read_ordered_steps(retained,error)&&retained.phase==MT2FW_MT2_STEP_FAILED){
        if(retained.callbacks_inflight!=1||retained.workers_held!=1){error="Terminal UI lacks its held service receipt";return false;}
        if(state.failure.empty()){
            const auto end=std::find(std::begin(retained.cause),std::end(retained.cause),'\0');
            std::string ignored;fail(std::string(retained.cause,end),ignored);
        }
        // A failed managed Save retains its queued owner until a fresh Leave click.
        // Refresh the terminal UI without resuming that failed Save or attempting Quit.
        const bool awaiting_explicit=state.leave_queued&&state.leave_managed_save&&!state.retiring&&!state.quit_attempted&&!state.reloading;
        if(state.leave_queued&&!state.retiring&&!awaiting_explicit)return service_leave(error);
        const bool leave_active=leaving()&&!awaiting_explicit;
        bool exit_ready=false;std::string exit_cause;
        if(state.started&&!leave_active&&!typed_action_runtime::ordered_leave_ready(exit_ready,exit_cause))exit_ready=false;
        if(!leave_active)state.retirement_error=exit_ready?(awaiting_explicit?state.failure:std::string()):
            (exit_cause.empty()?"Terminal Leave requires an idle actual Controller":exit_cause);
        const native_lobby::OrderedView view{state.started,leave_active,state.phase,state.failure,exit_ready};
        const bool refreshed=native_lobby::ordered_tick(view,retained,error)&&native_lobby::framework_render(error);
        if(!refreshed&&state.ui_error!=error)LOG_ERROR("[OrderedStartup] Terminal UI failed: {}",error);
        state.ui_error=refreshed?std::string():error;return refreshed;
    }
    const bool result=maintain_session(error);
    MT2FW_MT2_StepReceipt receipt{};receipt.size=sizeof(receipt);std::string observed;
    native_lobby::OrderedView view{state.started,leaving(),session_reload::busy()?"preparing_membership":state.phase,state.failure};
    view.admission_error=session_reload::failure_reason();
    view.admission_ready=result&&state.started&&!leaving()&&state.failure.empty()&&view.admission_error.empty()&&!session_reload::busy()&&
        (!state.resume_retained||state.resume_submitted)&&
        net::session_wire::connected()&&!net::session_wire::membership_pending()&&
        net::session_wire::membership_retirement_mask()==net::session_wire::peer_mask()&&typed_action_runtime::ordered_admission_ready();
    const bool refreshed=typed_action_runtime::read_ordered_steps(receipt,observed)&&native_lobby::ordered_tick(view,receipt,observed);
    if(!refreshed){
        if(state.ui_error!=observed)LOG_ERROR("[OrderedStartup] Native UI maintenance failed: {}",observed);
        state.ui_error=observed;
    }else if(!state.ui_error.empty()){LOG_INFO("[OrderedStartup] Native UI maintenance recovered");state.ui_error.clear();}
    return result;
}
inline bool tick_unbound(std::string& error){
    if(!current(error))return false;
    if(state.bound){error="Ordered transport must run from the outer held service after binding";return false;}
    if(!pump_transport(error))return fail(error,error);
    net::OrderedSnapshotIdentity identity;
    if(!net::read_ordered_snapshot(identity,error))return fail(error,error);
    if(!identity.native_prepared){phase("receiving_world");error.clear();return true;}
    if(!framework_runtime::ordered_services(state.steps,state.patches,error))return fail(error,error);
    state.proof=std::make_unique<ordered_session::ProofOwner>(&ordered_session::read_native_proof,&ordered_session::read_native_region_row,&ordered_session::read_recovery_local_proof);
    if(!state.proof->bind(state.patches,error))return fail(error,error);
    action_session::Context context{identity.session,identity.local_peer,1,identity.peer_mask,
        identity.local_peer==1?action_session::Role::Host:action_session::Role::Client,identity.snapshot_uuid};
    if(!typed_action_runtime::bind_ordered_steps(state.steps,context,state.proof->token(),
        ordered_session::ProofOwner::callback(),state.proof.get(),&maintain,error))return fail(error,error);
    state.bound=true;
    MT2FW_MT2_StepRecoveryDraftV1 recovery_api{};MT2FW_MT2_StepReceipt armed{};armed.size=sizeof(armed);
    if(!framework_runtime::ordered_recovery(recovery_api,error)||!typed_action_runtime::read_ordered_steps(armed,error))return fail(error,error);
    state.recovery=std::make_unique<session_reload::recovery::Owner>();
    if(!session_reload::bind_recovery(*state.recovery,*state.proof,recovery_api,armed,identity.peer_mask,error))return fail(error,error);
    phase("waiting_for_seed");error.clear();return true;
}
inline bool select(const std::filesystem::path& write,const std::string& file,const std::string& uuid,
                   const shared_time::Config& clock,std::string& error){
    if(state.selected||typed_action_runtime::ordered_mode_selected()){
        error="Ordered startup is already selected; no implicit restart";return false;}
    native::WorldStatus world;
    if(!native::read_world_status(world,error))return false;
    if(world.active_mode_index!=2||world.title_transition_pending){error="Ordered startup requires the idle main menu";return false;}
    if(!net::select_ordered_snapshot(error))return false;
    state.selected=true;state.thread=GetCurrentThreadId();state.clock=clock;
    try{
        net::OrderedSnapshotIdentity identity;if(!net::read_ordered_snapshot(identity,error))return fail(error,error);
        if(identity.local_peer==1){
            if(file.empty())return fail("Ordered Host requires the chosen native save file",error);
            const auto record=selected_snapshot::copy(write,file,uuid,net::snapshot_max_bytes,net::hash_snapshot_file);
            if(!net::select_ordered_snapshot_source(uuid,record.at("sha256").get<std::string>(),record.at("bytes").get<uint64_t>(),
                record.at("region_rng_mode").get<uint32_t>(),error))return fail(error,error);
            if(!net::distribute_snapshot(uuid,record.at("sha256").get<std::string>(),
                net::snapshot_world_path::utf8_path(record.at("path").get<std::string>()),error,
                record.at("world_name").get<std::string>())||!net::prepare_ordered_host_snapshot(error))return fail(error,error);
        }else if(!file.empty())return fail("Ordered Client receives the Host file and cannot select another world",error);
        phase("selected");error.clear();return true;
    }catch(const std::exception& exception){return fail(exception.what(),error);}
}
inline nlohmann::json status(){
    nlohmann::json out={{"selected",state.selected},{"bound",state.bound},{"started",state.started},
        {"phase",state.phase},{"failure",state.failure},{"resume_scale",state.resume_scale},{"resume_retained",state.resume_retained},{"resume_attempted",state.resume_attempted},{"resume_submitted",state.resume_submitted},{"leave_queued",state.leave_queued},{"retiring",state.retiring},
        {"last_leave_save",last_leave_save},{"reloading",state.reloading},{"reload_confirmed",reload_confirmed},{"leave_confirmed",leave_confirmed},{"last_retired_failure",last_retired_failure},{"retirement_error",state.retirement_error},{"ui_error",state.ui_error}};
    if(state.bound){
        MT2FW_MT2_StepReceipt receipt{};receipt.size=sizeof(receipt);std::string error;
        if(typed_action_runtime::read_ordered_steps(receipt,error))out["native_steps"]={
            {"phase",receipt.phase},{"lease",receipt.lease},{"session",receipt.session},
            {"world_epoch",receipt.world_epoch},{"generation",receipt.generation},
            {"seed_returned",receipt.seed_returned},{"bootstrap_held",receipt.bootstrap_held},
            {"tick",receipt.tick},{"completed_tick",receipt.completed_tick}};
        else out["native_steps_error"]=error;
    }
    return out;
}
}
