#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "action_native.h"
#include "action_calls.h"
#include "frame_apply.h"
#include "construction_native.h"
#include "tutorial_replication/native.h"
#include "owned_ui_entry.h"
#include "entry_fingerprints.h"
#include "window.h"
#include "placement_ui_probe/ui_probe.h"
#include "live_host_state/native_port.h"
#include "typed_action_runtime.h"
#include "session_provider.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/game/native_engine.h"
#include "hud_time_runtime.h"
#include <MinHook.h>
#include <atomic>
#include <cstdio>
#include <map>
#include <optional>
#include <regex>
namespace mt2mp::game::developer_staff {uint32_t original_show(uintptr_t,uintptr_t);}
namespace mt2mp::game::developer_staff::action_runtime {
namespace {
namespace action=action_session;
using json=nlohmann::json;
struct Pending {bool hire=false;std::string id;action::Context context;action::Binding binding;Origin origin;};
struct Operation {action::Binding binding;action::Request request;action::Evidence evidence;NativeReceipt native;bool count_attempted=false,count_returned=false,confirmed=false;std::string cause;};
struct Runtime {
    uintptr_t image=0;uint32_t thread=GetCurrentThreadId();
    std::atomic<initial_release::UiCommand> original{nullptr};
    std::atomic<bool> ready{false};bool install_attempted=false;
    UiEntryOwner owner;placement_ui_probe::ProcessMemory memory;
    std::optional<Pending> pending;
    std::map<std::string,Operation> operations;
    IntentReceipt intent;uint64_t captured=0,submitted=0,rejected=0,forwarded=0;
    std::string cause,install_cause;
    bool frame_locked=false;std::string frame_cause;uint64_t frame_fields=0;
    std::map<std::string,json> ui_proofs;bool ui_proof_locked=false;std::string ui_proof_cause;
};
std::atomic<Runtime*> published{nullptr};
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
void rejected(Runtime& r,const std::string& cause){++r.rejected;if(r.cause!=cause){r.cause=cause;LOG_ERROR("[DeveloperStaff] UI rejected cause={}",cause);}}
bool entry(Runtime& r,std::string& e){return verify_owned_ui(r.owner,r.image+0x6c7b70,[&](auto p,auto out,auto n,auto& error){return r.memory.copy(p,out,n,error);},[](const auto& bytes){return mt2mp::sha256(bytes);},e);}
bool full_code(Runtime& r,std::string& e){
    if(!entry(r,e))return false;
    try{for(const auto& b:original_bodies){if(b.rva==0x6c7b70)continue;std::vector<unsigned char> bytes(b.size);
        if(!r.memory.copy(r.image+b.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=b.sha256)return fail(e,"Developer original native body differs RVA="+std::to_string(b.rva));}
        std::vector<unsigned char> count(656);if(!r.memory.copy(r.image+0x25eed0,count.data(),count.size(),e))return false;
        if(mt2mp::sha256(count)!="65f97a9e0f4e2853f55018ae69afd70841fb3803774682f0e605afee71d279dd")return fail(e,"Developer original SetDevCount complete body differs");
    }catch(const std::exception& ex){return fail(e,std::string("Developer original body hash: ")+ex.what());}
    return true;
}
bool project(const Capture& c,PoolState& out,std::string& e){
    out={};if(c.state.business_count!=c.state.lounge_count)return fail(e,"Developer native count projection is not settled");
    PoolState p;p.count=c.state.business_count;std::map<uintptr_t,int32_t> ids;
    if(c.state.rows.size()!=50||c.local.developers.size()!=50)return fail(e,"Developer actual fixed50 projection absent");
    for(size_t i=0;i<50;++i){const auto& r=c.state.rows[i];p.rows.push_back({r.id,r.state,r.employed,r.active,r.on_map,r.job_link,r.position});ids.emplace(c.local.developers[i],r.id);}
    for(const auto* source:{&c.local.inside,&c.local.outside})for(auto address:*source){const auto found=ids.find(address);if(found==ids.end())return fail(e,"Developer manager has foreign receiver during projection");(source==&c.local.inside?p.indoor:p.outdoor).push_back(found->second);}
    if(!valid(p,e))return false;out=std::move(p);return true;
}
bool read(Runtime& r,const action::Context& expected,const action::Binding& binding,Capture& c,PoolState& out,std::string& e){
    action::Context context;if(!action::selected_session().current(context,e)||context!=expected)return fail(e,e.empty()?"Developer actual common Context changed":e);
    if(!read_process(c,e))return false;
    if(c.image!=r.image||c.thread!=r.thread||GetCurrentThreadId()!=r.thread||c.pin.origin.world_epoch!=binding.native_epoch||c.pin.origin.generation!=binding.native_generation||
       Config::instance().is_host!=(context.role==action::Role::Host)||(context.role==action::Role::Client&&c.pin.time_scale!=0))
        return fail(e,"Developer actual process/role/thread/native World binding or Client Clock0 differs");
    native::WorldStatus world;if(!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=3||world.in_game_state!=4||world.quitting||!world.world_present||world.title_transition_pending||world.map_task_pending||world.saves_in_progress>0||world.edit_operations)
        return fail(e,"Developer operation requires actual idle loaded native World");
    if(!project(c,out,e))return false;
    std::vector<construction::LocalJob> jobs;
    if(!typed_action_runtime::construction_jobs(context,binding,jobs,e))return false;
    construction::State extra;if(!construction::capture(c,jobs,extra,e))return false;
    if(!jobs.empty()||extra.work){
        out.construction=std::move(extra);if(!valid(out,e))return false;
    }
    return true;
}
struct FrameCalls {
    Runtime& runtime;const action::Context& context;const action::Binding& binding;Capture before;
    bool pin(const PoolState& expected,std::string& e){
        PoolState observed;if(!action_runtime::read(runtime,context,binding,before,observed,e))return false;
        if(context.role!=action::Role::Client||observed!=expected)return fail(e,"Developer frame requires exact actual Client BEFORE pool");
        for(const auto actor:before.local.developers){uintptr_t carried=0;
            if(!runtime.memory.copy(actor+0x4d0,&carried,sizeof carried,e))return false;
            if(carried)return fail(e,"Developer frame actual carried Monster closure is not yet bound");}
        return true;
    }
    bool scalar(const FieldChange& change,std::string& e){
        scalar::Receipt receipt;
        const bool ok=scalar::apply([&](auto p,auto out,auto n,auto& cause){return runtime.memory.copy(p,out,n,cause);},
            runtime.image,before.local.developers.at(change.slot),change.field,change.value,receipt,e);
        if(receipt.attempted)++runtime.frame_fields;return ok;
    }
    bool read(PoolState& out,std::string& e){
        Capture after;if(!action_runtime::read(runtime,context,binding,after,out,e))return false;
        return (same_origin(before.pin.origin,after.pin.origin)&&before.pin.ledger==after.pin.ledger&&before.pin.time_scale==after.pin.time_scale&&
            before.pin.pause_scale==after.pin.pause_scale&&before.local==after.local)||fail(e,"Developer frame native property changed Origin/full ledger/Clock/local owners");
    }
    bool invalidate_display(std::string& e){return tutorial_replication::invalidate_developer_pool(context,binding,e);}
};
std::string uuid(std::string& e){unsigned char b[16]{};const auto code=BCryptGenRandom(nullptr,b,sizeof b,BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(code){e="Developer UUID NTSTATUS="+std::to_string(code);return {};}
    b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;char text[37]{};std::snprintf(text,sizeof text,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);return text;}
struct CaptureCalls {
    Runtime& r;void* receiver;void* record;
    action::Context context;action::Binding binding;Capture native;PoolState pool;
    bool original(){++r.forwarded;return r.original.load(std::memory_order_acquire)(receiver,record);}
    bool admit(UiIntent,std::string& e){
        if(r.pending)return fail(e,"Developer UI already has one retained intent");
        if(!action::selected_session().current(context,e)||!typed_action_runtime::network_binding(context.snapshot_uuid,binding,e)||!read(r,context,binding,native,pool,e)||!full_code(r,e))return false;
        uintptr_t window=0;if(!read_window([&](auto p,auto out,auto n,auto& cause){return r.memory.copy(p,out,n,cause);},native,window,e))return false;
        return window==reinterpret_cast<uintptr_t>(receiver)||fail(e,"Developer UI receiver is not actual owned DevelopmentWindow");
    }
    bool uuid(std::string& id,std::string& e){id=action_runtime::uuid(e);return !id.empty();}
    bool retain(UiIntent intent,const std::string& id,std::string&){r.pending=Pending{intent==UiIntent::Hire,id,context,binding,native.pin.origin};++r.captured;return true;}
};
bool ui_command(void* window,void* record){
    auto* r=published.load(std::memory_order_acquire);if(!r)return false;
    const auto original=r->original.load(std::memory_order_acquire);if(!original)return false;
    if(GetCurrentThreadId()!=r->thread)return original(window,record);
    try{
        const auto controller=typed_action_runtime::controller_status();
        if(!r->ready||!controller.value("initialized",false)||!controller.value("started",false)){++r->forwarded;return original(window,record);}
        uintptr_t ap=0;std::string error;
        if(!window||!r->memory.copy(reinterpret_cast<uintptr_t>(window),&ap,sizeof ap,error)||ap!=r->image+0x1722e60){++r->forwarded;return original(window,record);}
        uint32_t fault=0;const auto intent=classify(r->image,record,fault);CaptureCalls calls{*r,window,record};IntentReceipt receipt;
        const bool accepted=capture_ui(true,true,intent,calls,receipt);
        if(receipt.suppressed){if(fault)receipt.cause="Developer UI genuine command read SEH="+std::to_string(fault);r->intent=receipt;if(!receipt.captured)rejected(*r,receipt.cause);}
        return accepted;
    }catch(const std::exception& ex){rejected(*r,std::string("Developer UI entry exception; original suppressed: ")+ex.what());return true;}
    catch(...){rejected(*r,"Developer UI entry nonstandard exception; original suppressed");return true;}
}
}
bool enabled(){auto* r=published.load(std::memory_order_acquire);return r&&r->ready.load(std::memory_order_acquire);}
bool pending_ui(){auto* r=published.load(std::memory_order_acquire);return r&&r->pending.has_value();}
bool retire_lobby(std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r){e.clear();return true;}
    if(GetCurrentThreadId()!=r->thread)return fail(e,"Developer lobby retirement requires Mainthread");
    native::WorldStatus world;if(!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0)return fail(e,"Developer lobby retirement requires actual idle Title");
    r->pending.reset();r->operations.clear();r->intent={};r->cause.clear();r->frame_locked=false;r->frame_cause.clear();r->ui_proofs.clear();r->ui_proof_locked=false;r->ui_proof_cause.clear();e.clear();return true;
}
bool verify_entry(uintptr_t target,std::string& e){
    auto* r=published.load(std::memory_order_acquire);
    if(r&&r->owner.enabled){if(target!=r->image+0x6c7b70)return fail(e,"Developer UI hook target differs");return entry(*r,e);}
    const auto image=uintptr_t(GetModuleHandleW(nullptr));
    if(!image||target!=image+0x6c7b70)return fail(e,"Developer UI original target differs");
    placement_ui_probe::ProcessMemory memory;std::vector<unsigned char> body(2560);
    if(!memory.copy(target,body.data(),body.size(),e))return false;
    try{return mt2mp::sha256(body)=="d204e8e24dc14d66e170adae7b4b60433fa82baa6eecb81f65353f32d8c5a2af"||fail(e,"Developer UI original full body differs");}
    catch(const std::exception& ex){return fail(e,std::string("Developer UI original body hash: ")+ex.what());}
}
bool enable(std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r){r=new Runtime;published.store(r,std::memory_order_release);}
    if(GetCurrentThreadId()!=r->thread)return fail(e,"Developer UI hook requires actual Mainthread");
    if(r->ready)return entry(*r,e);
    if(r->install_attempted)return fail(e,"Developer UI hook installation previously failed: "+r->install_cause);
    r->install_attempted=true;struct Cause{Runtime& r;std::string& e;~Cause(){if(!e.empty())r.install_cause=e;}} cause{*r,e};
    uint32_t thread=0;std::string sha;if(!native::read_resolver_receipt(r->image,thread,sha,e))return false;
    if(thread!=r->thread||r->image!=uintptr_t(GetModuleHandleW(nullptr))||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"Developer UI hook actual EXE/process/thread differs");
    std::vector<unsigned char> body(2560);if(!r->memory.copy(r->image+0x6c7b70,body.data(),body.size(),e))return false;
    if(mt2mp::sha256(body)!="d204e8e24dc14d66e170adae7b4b60433fa82baa6eecb81f65353f32d8c5a2af")return fail(e,"Developer UI original full body differs before hook");
    initial_release::UiCommand original=nullptr;const auto created=MH_CreateHook(reinterpret_cast<void*>(r->image+0x6c7b70),reinterpret_cast<void*>(&ui_command),reinterpret_cast<void**>(&original));
    if(created!=MH_OK||!original)return fail(e,"Developer UI MinHook create failed status="+std::to_string(created));
    r->original.store(original,std::memory_order_release);r->owner={r->image+0x6c7b70,reinterpret_cast<uintptr_t>(&ui_command),reinterpret_cast<uintptr_t>(original),true,false};
    const auto result=MH_EnableHook(reinterpret_cast<void*>(r->owner.target));if(result!=MH_OK)return fail(e,"Developer UI MinHook enable failed status="+std::to_string(result));
    r->owner.enabled=true;if(!entry(*r,e))return false;r->ready=true;e.clear();return true;
}
bool capture(const action::Context& c,const action::Binding& b,PoolState& out,std::string& e){auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready)return fail(e,"Developer native owner unavailable");Capture actual;return read(*r,c,b,actual,out,e);}
bool preflight_frame(const action::Context& c,const action::Binding& b,const PoolState& target,std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready)return fail(e,"Developer frame native owner unavailable");
    if(r->frame_locked)return fail(e,r->frame_cause);
    Capture actual;PoolState before;if(!read(*r,c,b,actual,before,e))return false;
    if(before.construction||target.construction){
        if(!target.construction)return fail(e,"Developer construction frame cannot discard retained Job ownership");
        std::vector<construction::LocalJob> jobs;if(!typed_action_runtime::construction_jobs(c,b,jobs,e))return false;
        return construction::preflight(c,b,actual,target,jobs,e);
    }
    std::vector<FieldChange> changes;if(!frame_plan(before,target,changes,e))return false;
    FrameCalls calls{*r,c,b};return calls.pin(before,e);
}
bool adopt_frame(const action::Context& c,const action::Binding& b,const PoolState& target,bool& attempted,bool& unknown,std::string& e){
    attempted=unknown=false;auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready)return fail(e,"Developer frame native owner unavailable");
    if(r->frame_locked)return fail(e,r->frame_cause);
    Capture actual;PoolState before;if(!read(*r,c,b,actual,before,e))return false;
    bool ok=false;
    if(before.construction||target.construction){
        if(!target.construction)return fail(e,"Developer construction frame cannot discard retained Job ownership");
        std::vector<construction::LocalJob> jobs;if(!typed_action_runtime::construction_jobs(c,b,jobs,e))return false;
        ok=construction::apply(c,b,actual,target,jobs,attempted,unknown,e);
        if(ok&&attempted&&!tutorial_replication::invalidate_developer_pool(c,b,e)){ok=false;unknown=true;}
    }else{FrameCalls calls{*r,c,b};ok=apply_frame(before,target,calls,attempted,unknown,e);}
    if(unknown||(attempted&&!ok)){r->frame_locked=true;r->frame_cause=e.empty()?"Developer frame original property attempt unconfirmed; no replay":e;unknown=true;e=r->frame_cause;}
    return ok;
}
void tick(){auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||!r->pending)return;
    const auto pending=*r->pending;r->pending.reset();std::string e;Capture actual;PoolState state;
    if(!read(*r,pending.context,pending.binding,actual,state,e)||!same_origin(actual.pin.origin,pending.origin)||!full_code(*r,e)){rejected(*r,e.empty()?"Developer pending UI native Origin changed":e);return;}
    if(!typed_action_runtime::submit_developer(pending.hire,pending.id,e)){rejected(*r,e);return;}++r->submitted;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* host,std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||(request.action!=action::Action::HireDeveloper&&request.action!=action::Action::FireDeveloper)){e="Developer native action owner or type unavailable";return action::Outcome::RejectedNoEffect;}
    if(r->operations.contains(request.action_uuid)){e="Developer native action UUID consumed; no replay";return action::Outcome::Unknown;}
    if(r->operations.size()>=action::history_limit){e="Developer retained action capacity reached";return action::Outcome::RejectedNoEffect;}
    const auto controller=typed_action_runtime::controller_status();if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){e="Developer action requires exact Controller pending UUID";return action::Outcome::RejectedNoEffect;}
    action::Context context;if(!action::selected_session().current(context,e)||context.snapshot_uuid!=request.snapshot_uuid||(context.role==action::Role::Client)!=(host!=nullptr)){if(e.empty())e="Developer native action actual role/session differs";return action::Outcome::RejectedNoEffect;}
    Capture before;PoolState state;if(!read(*r,context,binding,before,state,e)||!full_code(*r,e))return action::Outcome::RejectedNoEffect;
    const bool hire=request.action==action::Action::HireDeveloper;
    std::vector<FieldChange> projected;if(!state.construction&&!frame_plan(state,state,projected,e))return action::Outcome::RejectedNoEffect;
    if((hire&&state.count==50)||(!hire&&!state.count)){e="Developer native requested count is at boundary";return action::Outcome::RejectedNoEffect;}
    action::Ledger expected_ledger;if(!expected_payment(hire,before.pin.ledger,expected_ledger,e))return action::Outcome::RejectedNoEffect;
    const auto affected=size_t(hire?state.count:state.count-1);
    if(state.rows[affected].job_link&&!state.construction){e="Developer affected native JobQueue occurrence is not bound";return action::Outcome::RejectedNoEffect;}
    if(host&&(host->before!=before.pin.ledger||host->developers_before!=state||!host->developers_after)){e="Developer Client native BEFORE differs from Host";return action::Outcome::RejectedNoEffect;}
    uintptr_t window=0;if(!read_window([&](auto p,auto out,auto n,auto& error){return r->memory.copy(p,out,n,error);},before,window,e))return action::Outcome::RejectedNoEffect;
    auto& op=r->operations.emplace(request.action_uuid,Operation{binding,request}).first->second;op.evidence.before=before.pin.ledger;op.evidence.developers_before=state;
    op.native.seh_code=invoke_ui(r->original.load(std::memory_order_acquire),r->image,window,hire,&op.native);
    if(op.native.seh_code||op.native.cleanup_exception||!op.native.attempted||!op.native.returned||!op.native.accepted||!op.native.label_confirmed||!op.native.cleanup_confirmed){e=op.cause="Developer original UI/record cleanup unconfirmed SEH="+std::to_string(op.native.seh_code)+" cleanup="+std::to_string(op.native.cleanup_exception);return action::Outcome::Unknown;}
    Capture immediate;auto expected=before.state;expected.business_count+=hire?1:-1;
    if(!read_process(immediate,e)||!same_origin(before.pin.origin,immediate.pin.origin)||before.local!=immediate.local||immediate.state!=expected||immediate.pin.ledger!=expected_ledger){
        if(e.empty())e="Developer original UI immediate count/ledger differs; no forced count projection";op.cause=e;return action::Outcome::Unknown;}
    op.count_attempted=true;const auto count=state.count+(hire?1:-1);const auto code=invoke_count(r->image,before.local.lounge,count);op.count_returned=code==0;
    if(code){e=op.cause="Developer original SetDevCount SEH="+std::to_string(code);return action::Outcome::Unknown;}
    Capture after;PoolState result;if(!read(*r,context,binding,after,result,e)||!same_origin(before.pin.origin,after.pin.origin)||before.local!=after.local||
       !count_effect(hire,state,result,e)||!payment(hire,before.pin.ledger,after.pin.ledger,e)){if(e.empty())e="Developer original native count/owners/full ledger differs";op.cause=e;return action::Outcome::Unknown;}
    op.evidence.after=after.pin.ledger;op.evidence.developers_after=result;
    if(host&&(host->after!=op.evidence.after||host->developers_after!=op.evidence.developers_after)){e=op.cause="Developer Client native AFTER differs from Host";return action::Outcome::Unknown;}
    if(!tutorial_replication::invalidate_developer_pool(context,binding,e)){op.cause=e;return action::Outcome::Unknown;}
    op.confirmed=true;e.clear();return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r||GetCurrentThreadId()!=r->thread)return fail(e,"Developer action observation requires native Mainthread owner");
    const auto found=r->operations.find(request.action_uuid);if(found==r->operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)return fail(e,"Developer exact confirmed native action receipt absent");
    action::Context context;Capture actual;PoolState state;if(!action::selected_session().current(context,e)||!read(*r,context,binding,actual,state,e))return false;
    if(found->second.evidence.after!=actual.pin.ledger||found->second.evidence.developers_after!=state)return fail(e,"Developer retained native action AFTER changed");out=found->second.evidence;return true;
}
bool retire(const action::Binding& b,const action::Request& request,std::string& e){action::Evidence actual;if(!observe(b,request,actual,e))return false;published.load(std::memory_order_acquire)->operations.at(request.action_uuid).evidence.retirement_confirmed=true;return true;}
bool ui_fixture(const json& args,json& out,std::string& e){
    out={{"admission_only",true},{"execution_ack",false},{"ui_attempted",false},{"ui_returned",false},{"captured",false},{"unknown",false}};
    auto* r=published.load(std::memory_order_acquire);bool attempted=false;std::string token;
    try{
        if(!r||!r->ready||GetCurrentThreadId()!=r->thread||!Config::instance().local_test)return fail(e,"Developer UI fixture requires owned ready local-test Mainthread");
        Capture update;if(!authorize_ui_update(update,e))return false;
        if(!args.is_object()||args.size()!=6||!args.contains("command")||!args.contains("token")||!args.contains("native_origin_uuid")||!args.contains("epoch")||!args.contains("generation")||!args.contains("expected_count")||
           !args.at("command").is_string()||!args.at("token").is_string()||!args.at("native_origin_uuid").is_string()||!args.at("epoch").is_number_unsigned()||!args.at("generation").is_number_unsigned()||!args.at("expected_count").is_number_integer())
            return fail(e,"Developer UI fixture exact six typed fields required");
        const auto command=args.at("command").get<std::string>();token=args.at("token").get<std::string>();
        static const std::regex canonical("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}");
        if((command!="Show"&&command!="Hire"&&command!="Fire")||!std::regex_match(token,canonical)||!std::regex_match(args.at("native_origin_uuid").get<std::string>(),canonical)||
           args.at("epoch").get<uint64_t>()==0||args.at("generation").get<uint64_t>()==0||
           (args.at("expected_count").is_number_unsigned()?args.at("expected_count").get<uint64_t>()>50:args.at("expected_count").get<int64_t>()<0||args.at("expected_count").get<int64_t>()>50))
            return fail(e,"Developer UI fixture command/UUID/Origin/count domain invalid");
        if(r->ui_proofs.contains(token)){out=r->ui_proofs.at(token);out["replay_rejected"]=true;return fail(e,"Developer UI fixture token consumed; no replay");}
        if(r->frame_locked)return fail(e,r->frame_cause);
        if(r->ui_proof_locked)return fail(e,r->ui_proof_cause);
        if(r->pending)return fail(e,"Developer UI fixture already has one retained pending UI intent");
        if(r->ui_proofs.size()>=64)return fail(e,"Developer UI fixture retained capacity64 reached");
        const auto controller=typed_action_runtime::controller_status();
        if(!controller.value("initialized",false)||!controller.value("started",false)||controller.value("pending",true)||controller.value("locked",true)||controller.contains("error"))
            return fail(e,"Developer UI fixture requires actual ready idle Controller");
        action::Context context;action::Binding binding;
        if(!action::selected_session().current(context,e)||!typed_action_runtime::network_binding(context.snapshot_uuid,binding,e))return false;
        Capture before;PoolState pool;if(!read(*r,context,binding,before,pool,e)||!full_code(*r,e))return false;
        if(!same_origin(update.pin.origin,before.pin.origin))return fail(e,"Developer UI fixture GameUpdate Origin changed");
        if(args.at("native_origin_uuid")!=before.pin.origin.uuid||args.at("epoch").get<uint64_t>()!=before.pin.origin.world_epoch||
           args.at("generation").get<uint64_t>()!=before.pin.origin.generation||args.at("expected_count").get<int32_t>()!=pool.count)
            return fail(e,"Developer UI fixture stale native Origin/count");
        uintptr_t window=0;if(!read_window([&](auto p,auto target,auto n,auto& cause){return r->memory.copy(p,target,n,cause);},before,window,e))return false;
        if(command=="Show"&&!hud_time_runtime::verify_window_show_hook(r->image+0x81ec20,e))return false;
        const auto captured=r->captured;out["token"]=token;out["command"]=command;out["before"]=encode(before);out["unknown"]=true;
        r->ui_proofs.emplace(token,out);r->ui_proof_locked=true;r->ui_proof_cause="Developer UI fixture native call outstanding; no replay";attempted=true;out["ui_attempted"]=true;
        NativeReceipt native;
        const auto code=command=="Show"?original_show(r->image,window):invoke_ui(reinterpret_cast<initial_release::UiCommand>(r->owner.target),r->image,window,command=="Hire",&native);
        out["exception_code"]=code;out["ui_returned"]=code==0;out["cleanup_exception"]=native.cleanup_exception;
        if(command!="Show"){out["record_confirmed"]=native.label_confirmed;out["record_cleanup_confirmed"]=native.cleanup_confirmed;out["ui_entry_accepted"]=native.accepted;}
        bool ok=!code&&(command=="Show"||(native.attempted&&native.returned&&native.accepted&&native.label_confirmed&&native.cleanup_confirmed&&!native.cleanup_exception));
        if(!ok)e="Developer UI fixture original entry/record cleanup unconfirmed SEH="+std::to_string(code)+" cleanup="+std::to_string(native.cleanup_exception);
        Capture after;PoolState observed;std::string read_error;
        if(!read(*r,context,binding,after,observed,read_error)){ok=false;e+=(e.empty()?"":"; ")+read_error;}
        else{
            out["after"]=encode(after);
            if(!same_origin(before.pin.origin,after.pin.origin)||before.local!=after.local||before.pin.ledger!=after.pin.ledger||before.state!=after.state||before.pin.time_scale!=after.pin.time_scale||before.pin.pause_scale!=after.pin.pause_scale){ok=false;e+=(e.empty()?"":"; ");e+="Developer UI fixture changed native gameplay before Host authority";}
            if(ok&&command=="Show"){uint8_t visible=0;if(!r->memory.copy(window+0x45f,&visible,sizeof visible,e)||visible!=1){ok=false;if(e.empty())e="Developer UI fixture original Show not visible";}out["shown"]=ok;}
            if(ok&&command!="Show"){
                ok=r->captured==captured+1&&r->pending&&r->pending->hire==(command=="Hire")&&r->pending->context==context&&r->pending->binding==binding&&same_origin(r->pending->origin,before.pin.origin)&&!r->pending->id.empty();
                if(ok){out["captured"]=true;out["action_uuid"]=r->pending->id;}else e=r->intent.cause.empty()?"Developer hooked UI entry did not retain exactly one intent":r->intent.cause;
            }
        }
        out["admitted"]=ok;out["unknown"]=!ok;out["cause"]=e;r->ui_proofs.at(token)=out;r->ui_proof_cause=e;if(ok)r->ui_proof_locked=false;return ok;
    }catch(const std::exception& cause){e=std::string("Developer UI fixture exception: ")+cause.what();}
    catch(...){e="Developer UI fixture nonstandard exception";}
    out["unknown"]=attempted;out["cause"]=e;if(r&&attempted&&!token.empty()&&r->ui_proofs.contains(token)){r->ui_proof_cause=e;r->ui_proofs.at(token)=out;}return false;
}
json status(){auto* r=published.load(std::memory_order_acquire);if(!r)return {{"initialized",false}};if(GetCurrentThreadId()!=r->thread)return {{"initialized",true},{"error","Developer status requires Mainthread"}};
    auto ops=json::array();for(const auto& [id,op]:r->operations)ops.push_back({{"action_uuid",id},{"action",uint8_t(op.request.action)},{"native_attempted",op.native.attempted},{"native_returned",op.native.returned},{"count_attempted",op.count_attempted},{"count_returned",op.count_returned},{"confirmed",op.confirmed},{"retired",op.evidence.retirement_confirmed},{"cause",op.cause}});
    auto receipts=json::array();for(const auto& [token,proof]:r->ui_proofs)receipts.push_back({{"token",token},{"command",proof.value("command",std::string{})},{"action_uuid",proof.value("action_uuid",std::string{})},{"captured",proof.value("captured",false)},{"unknown",proof.value("unknown",true)},{"cause",proof.value("cause",std::string{})}});
    return {{"initialized",true},{"enabled",bool(r->ready)},{"pending_ui",r->pending.has_value()},{"captured",r->captured},{"submitted",r->submitted},{"rejected",r->rejected},{"forwarded",r->forwarded},{"intent_action_uuid",r->intent.action_uuid},{"cause",r->cause},{"frame_locked",r->frame_locked},{"frame_cause",r->frame_cause},{"frame_fields",r->frame_fields},{"ui_proof_locked",r->ui_proof_locked},{"ui_proof_cause",r->ui_proof_cause},{"ui_receipts",receipts},{"jobful_native_proven",false},{"operations",ops}};}
}
