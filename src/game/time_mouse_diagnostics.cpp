#include "time_mouse_diagnostics.h"
#include "time_native_button/native_button.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/net/local_session.h"
#include "native_open_runtime.h"
#include "time_integration.h"
#include <map>
#include <windows.h>

namespace mt2mp::game::time_mouse_diagnostics {
namespace {
using json=nlohmann::json;
namespace button=time_native_button;
namespace wire=time_session;
struct Job {
    button::Request request;button::Receipt native;
    uint64_t expected_sequence=0,sequence=0,session_epoch=0;
    HookCounts before{},after{};
    uint64_t submitted_before=0;
    bool admitted=false,confirmed=false,terminal=false;
    std::string error;
    wire::Message host{};
};
uintptr_t image=0;uint32_t game_thread=0;
hud_time::Reader* reader=nullptr;hud_time::Activation* activation=nullptr;Services io;
bool initialized=false;
std::map<std::string,Job> jobs;
Job* pending=nullptr;
uint64_t next_sequence=0;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool uuid(const std::string& text) {
    if(text.size()!=36)return false;
    for(size_t index=0;index<text.size();++index){
        if(index==8||index==13||index==18||index==23){if(text[index]!='-')return false;}
        else if(!((text[index]>='0'&&text[index]<='9')||(text[index]>='a'&&text[index]<='f')))return false;
    }return true;
}
uint64_t positive(const json& v,const char* name) {
    if(!v.is_number_integer()||(v.is_number_unsigned()?v.get<uint64_t>()==0:v.get<int64_t>()<=0))
        throw std::runtime_error(std::string(name)+" requires a positive uint64");
    return v.get<uint64_t>();
}
wire::Intent intent(float scale){return scale==0?wire::Intent::Pause:scale==1?wire::Intent::Normal:scale==3?wire::Intent::Fast:wire::Intent::Fastest;}
json format(const Job& job) {
    const auto& n=job.native;
    json out={{"acceptance_token",job.request.token},{"snapshot_uuid",job.request.snapshot_uuid},
        {"world_epoch",job.request.epoch},{"session_epoch",job.session_epoch},{"generation",job.request.generation},{"observed_revision",job.request.revision},
        {"scale",job.request.scale},{"expected_request_sequence",job.expected_sequence},{"request_sequence",job.sequence},
        {"state",job.confirmed?"confirmed":job.terminal?"failed":"waiting_for_host"},{"error",job.error},
        {"native",{{"phase",static_cast<int>(n.phase)},{"invocation_reserved",n.invocation_reserved},
            {"attempted",n.native_attempted},{"returned",n.native_returned},{"seh_code",n.seh_code},{"cause",n.cause}}},
        {"hook",{{"attempted_before",job.before.attempted},{"attempted_after",job.after.attempted},
            {"matched_before",job.before.matched},{"matched_after",job.after.matched},
            {"suppressed_before",job.before.suppressed},{"suppressed_after",job.after.suppressed},
            {"original_before",job.before.original},{"original_after",job.after.original},{"caller_rva",job.after.caller}}}};
    if(job.confirmed)out["host_receipt"]={{"peer",job.host.peer},{"epoch",job.host.epoch},{"sequence",job.host.sequence},
        {"revision",job.host.state.revision},{"result",static_cast<int>(job.host.result)},
        {"selected_scale",job.host.state.clock.selected},{"pause_scale",job.host.state.clock.pause}};
    return out;
}
class RootServices final:public button::RootServices {
public:
    bool read_actual_button(const button::Request& q,button::Binding& out,std::string& e) override {
        out={};
        if(GetCurrentThreadId()!=game_thread)return fail(e,"time_mouse_wrong_game_thread");
        native_time_input::Context context;
        if(!io.context(context,e))return false;
        if(!context.owned_test)return fail(e,"time_mouse_runtime_not_owned");
        if(!context.world_owned)return fail(e,"time_mouse_world_not_owned");
        if(!context.time_active)return fail(e,"time_mouse_authority_not_active");
        if(!context.overlay_hidden)return fail(e,"time_mouse_overlay_visible");
        if(!context.no_pending_operation)return fail(e,"time_mouse_other_operation_pending");
        if(!pending)return fail(e,"time_mouse_pending_job_missing");
        if(context.epoch!=pending->session_epoch)return fail(e,"time_mouse_session_epoch_changed measured="+std::to_string(context.epoch)+" required="+std::to_string(pending->session_epoch));
        if(context.revision!=q.revision)return fail(e,"time_mouse_revision_changed measured="+std::to_string(context.revision)+" required="+std::to_string(q.revision));
        building_mixed_identity::opened_file::CurrentWorld world;
        if(!native_open_runtime::read_current_world(world,e))return false;
        if(world.generation!=q.generation||world.world_epoch!=q.epoch||world.uuid!=q.snapshot_uuid)
            return fail(e,"time_mouse_actual_origin_snapshot_epoch_generation_differs");
        hud_time::ActivationBinding current;
        if(!reader->button_binding(q.scale,current,e))return false;
        if(!current.matched)return fail(e,"time_mouse_actual_hud_button_not_matched");
        if(!current.visible)return fail(e,"time_mouse_actual_hud_panel_not_visible");
        if(!current.enabled)return fail(e,"time_mouse_actual_hud_button_not_enabled");
        // The HUD Reader leases Mode+58 game state; NativeOpen resolves its separate world.
        if(current.world!=reinterpret_cast<uintptr_t>(world.state))return fail(e,"time_mouse_actual_hud_game_state_differs");
        if(!Config::instance().is_host&&current.native_selected_scale!=0)return fail(e,"time_mouse_client_native_clock_not_zero");
        out.image=image;out.world=reinterpret_cast<uintptr_t>(world.world);out.panel=current.panel;out.slot=current.slot;
        out.button=current.button;out.item=current.item;out.type=current.type;
        out.item_proxy=current.item_proxy;out.item_proxy_target=current.item+0x20;
        out.type_proxy=current.type_proxy;out.type_proxy_target=current.type+0x38;
        out.epoch=world.world_epoch;out.generation=world.generation;out.revision=context.revision;
        out.game_thread=game_thread;out.role=Config::instance().is_host?button::Role::Host:button::Role::Client;
        out.scale=current.scale;out.button_enabled=current.enabled;out.native_item_state=current.native_item_state;
        return button::validate_binding(q,out,e);
    }
    bool reserve_actual_job(const button::Request& q,button::JobAttempt& out,std::string& e) override {
        if(!pending||pending->request.token!=q.token||pending->native.actual_attempt.sequence)
            return fail(e,"time_mouse_actual_job_already_reserved_or_missing_no_replay");
        button::Binding binding;if(!read_actual_button(q,binding,e))return false;
        if(next_sequence==UINT64_MAX)return fail(e,"time_mouse_job_sequence_exhausted");
        out={q,++next_sequence,binding};pending->native.actual_attempt=out;return true;
    }
    bool read_actual_executable_sha256(std::string& digest,std::string& e) override {
        uintptr_t module=0;uint32_t thread=0;
        if(!native::read_resolver_receipt(module,thread,digest,e))return false;
        if(module!=image||thread!=game_thread)return fail(e,"time_mouse_actual_native_resolver_identity_differs");
        return true;
    }
    bool record_actual_attempt(const button::Receipt& value,std::string& e) override {
        if(!pending||pending->request.token!=value.actual_attempt.request.token||
            pending->native.actual_attempt.sequence!=value.actual_attempt.sequence)
            return fail(e,"time_mouse_native_receipt_current_job_differs");
        pending->native=value;e.clear();return true;
    }
};
}
bool initialize(uintptr_t base,uint32_t thread,hud_time::Reader& native_reader,hud_time::Activation& native_activation,Services services,std::string& e) {
    if(initialized)return fail(e,"time_mouse_diagnostics_already_initialized");
    if(!base||!thread||GetCurrentThreadId()!=thread||!services.context||!services.hook_counts)
        return fail(e,"time_mouse_diagnostics_services_or_thread_invalid");
    image=base;game_thread=thread;reader=&native_reader;activation=&native_activation;io=std::move(services);
    initialized=true;e.clear();return true;
}
bool unresolved(){for(const auto& [token,job]:jobs)if(!job.terminal||(job.native.native_attempted&&!job.confirmed))return true;return false;}
bool probe(const json& args,json& result,std::string& e) {
    result=json::object();
    if(!initialized)return fail(e,"time_mouse_diagnostics_not_initialized");
    if(GetCurrentThreadId()!=game_thread)return fail(e,"time_mouse_wrong_game_thread");
    if(!args.is_object()||args.size()!=8)throw std::runtime_error("native_time_button requires exactly eight fields");
    button::Request q;
    q.token=args.at("acceptance_token").get<std::string>();q.snapshot_uuid=args.at("snapshot_uuid").get<std::string>();
    if(!uuid(q.token)||!uuid(q.snapshot_uuid))throw std::runtime_error("native_time_button requires canonical UUIDs");
    const auto session_epoch=positive(args.at("expected_epoch"),"expected_epoch");
    q.epoch=positive(args.at("expected_world_epoch"),"expected_world_epoch");q.generation=positive(args.at("expected_generation"),"expected_generation");
    q.revision=positive(args.at("expected_revision"),"expected_revision");
    q.scale=args.at("scale").get<float>();
    if(q.scale!=0&&q.scale!=1&&q.scale!=3&&q.scale!=9)throw std::runtime_error("native_time_button requires scale0,1,3,9");
    const auto expected=positive(args.at("expected_request_sequence"),"expected_request_sequence");
    if(jobs.contains(q.token))return fail(e,"time_mouse_token_already_attempted_no_replay");
    if(pending||unresolved())return fail(e,"time_mouse_previous_effect_unresolved");
    if(jobs.size()>=64)return fail(e,"time_mouse_job_memo_full");
    wire::Message previous;bool available=false;
    if(!time_integration::instance().observe_receipt(Config::instance().peer_id,expected,previous,available,e))return false;
    if(available)return fail(e,"time_mouse_expected_sequence_already_has_host_receipt");
    auto& job=jobs.try_emplace(q.token).first->second;job.request=q;job.expected_sequence=expected;job.session_epoch=session_epoch;
    try{job.before=io.hook_counts();job.submitted_before=activation->status().submitted;}
    catch(const std::exception& cause){job.terminal=true;job.error=std::string("time_mouse_counter_read_before_exception: ")+cause.what();result=format(job);return fail(e,job.error);}
    catch(...){job.terminal=true;job.error="time_mouse_counter_read_before_unknown_exception";result=format(job);return fail(e,job.error);}
    pending=&job;RootServices services;bool invoked=false;
    try{invoked=button::invoke(q,services,job.native,e);}
    catch(const std::exception& cause){e=std::string("time_mouse_invoke_exception: ")+cause.what();}
    catch(...){e="time_mouse_invoke_unknown_exception";}
    pending=nullptr;
    try{job.after=io.hook_counts();}
    catch(const std::exception& cause){job.terminal=true;job.error=e+"; time_mouse_counter_read_after_exception: "+cause.what();job.native.phase=button::Phase::Unknown;result=format(job);return fail(e,job.error);}
    catch(...){job.terminal=true;job.error=e+"; time_mouse_counter_read_after_unknown_exception";job.native.phase=button::Phase::Unknown;result=format(job);return fail(e,job.error);}
    const auto& mouse=activation->status();job.sequence=mouse.last_sequence;
    const bool observed=job.after.attempted==job.before.attempted+1&&job.after.matched==job.before.matched+1&&
        job.after.suppressed==job.before.suppressed+1&&job.after.original==job.before.original&&
        job.after.caller==hud_time::Activation::mouse_caller_rva&&mouse.submitted==job.submitted_before+1&&
        mouse.last_epoch==session_epoch&&job.sequence==expected;
    native::ClockState clock;
    if(invoked&&(!observed||!native::read_clock(clock,e)||(!Config::instance().is_host&&clock.time_scale!=0))) {
        invoked=false;if(e.empty())e="time_mouse_native_return_without_exact_authoritative_admission: "+mouse.error;
    }
    job.admitted=invoked;
    if(!invoked){job.terminal=true;job.error=e;LOG_ERROR("[TimeMouse] token={} failed cause={} native_attempted={}; no replay",q.token,e,job.native.native_attempted);}
    else LOG_INFO("[TimeMouse] token={} native Mouse admitted sequence={} epoch={}; awaiting Host",q.token,job.sequence,q.epoch);
    result=format(job);return invoked;
}
bool receipt(const json& args,json& result,std::string& e) {
    result=json::object();
    if(!initialized||GetCurrentThreadId()!=game_thread)return fail(e,"time_mouse_receipt_not_initialized_or_wrong_thread");
    if(!args.is_object()||args.size()!=1)throw std::runtime_error("read_time_mouse requires only acceptance_token");
    const auto token=args.at("acceptance_token").get<std::string>();
    const auto found=jobs.find(token);if(found==jobs.end())return fail(e,"time_mouse_receipt_token_unknown");
    auto& job=found->second;
    if(job.terminal){result=format(job);e=job.error;return job.confirmed;}
    auto read_failure=[&](){job.error=e;result=format(job);return false;};
    try {
    building_mixed_identity::opened_file::CurrentWorld world;
    native_time_input::Context current;
    if(!native_open_runtime::read_current_world(world,e)||!io.context(current,e)) {
        job.terminal=true;job.error="time_mouse_confirmation_current_origin_read_failed: "+e;result=format(job);return fail(e,job.error);
    }
    if(world.generation!=job.request.generation||world.uuid!=job.request.snapshot_uuid||world.world_epoch!=job.request.epoch||
       current.epoch!=job.session_epoch||!current.owned_test||!current.world_owned||!current.time_active||
       !net::local_session_connected()) {
        job.terminal=true;job.error="time_mouse_confirmation_snapshot_generation_or_authority_changed";result=format(job);return fail(e,job.error);
    }
    wire::Message host;bool available=false;
    if(!time_integration::instance().observe_receipt(Config::instance().peer_id,job.sequence,host,available,e))return read_failure();
    if(!available){result=format(job);e.clear();return true;}
    const bool confirmed=host.epoch==job.session_epoch&&host.peer==Config::instance().peer_id&&host.sequence==job.sequence&&
        host.intent==intent(job.request.scale)&&host.observed_revision==job.request.revision&&host.code==wire::Code::None&&
        (host.result==wire::Result::Confirmed||host.result==wire::Result::NoChange)&&host.state.present&&!host.state.uncertain&&
        host.state.clock.selected==job.request.scale;
    shared_time::Presentation presentation;native::ClockState clock;
    if(!confirmed){job.terminal=true;job.error="time_mouse_host_receipt_rejected_or_mismatched";}
    else if(!time_integration::instance().read_presentation(presentation,e)||!native::read_clock(clock,e))return read_failure();
    else if(presentation.revision<host.state.revision){result=format(job);e.clear();return true;}
    else if(presentation.epoch!=host.epoch||presentation.selected_scale!=host.state.clock.selected||
        presentation.pause_scale!=host.state.clock.pause||(!Config::instance().is_host&&clock.time_scale!=0)) {
        job.terminal=true;job.error="time_mouse_host_presentation_or_native_clock_mismatch";
    } else {
        hud_time::State hud;
        if(!reader->read(hud,e))return read_failure();
        const auto selected=host.state.clock.selected*host.state.clock.pause;
        for(const auto& item:hud.items)if(item.active!=(item.scale==selected)){result=format(job);e.clear();return true;}
        native_time_input::Context context;if(!io.context(context,e))return read_failure();
        if(!context.time_active||!net::local_session_connected()){job.terminal=true;job.error="time_mouse_session_or_authority_ended_before_confirmation";}
        else {job.terminal=job.confirmed=true;job.host=host;job.error.clear();
            LOG_INFO("[TimeMouse] token={} confirmed peer={} sequence={} revision={} ClientClock0={}",token,host.peer,host.sequence,host.state.revision,Config::instance().is_host||clock.time_scale==0);}
    }
    result=format(job);e=job.error;return !job.terminal||job.confirmed;
    }catch(const std::exception& cause){job.terminal=true;job.error=std::string("time_mouse_receipt_exception: ")+cause.what();}
     catch(...){job.terminal=true;job.error="time_mouse_receipt_unknown_exception";}
    result=format(job);return fail(e,job.error);
}
}
