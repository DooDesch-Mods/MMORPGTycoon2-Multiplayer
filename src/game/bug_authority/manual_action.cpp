#include "manual_action.h"
#include "manual_ui.h"
#include "manual_flow.h"
#include "manual_intent.h"
#include "manual_fixture.h"
#include "developer_staff/probe.h"
#include "hud_time_runtime.h"
#include "mt2mp/core/config.h"
#include "runtime_internal.h"
#include "work_runtime.h"
#include "developer_staff/action_native.h"
#include "developer_staff/construction_native.h"
#include "typed_action_runtime.h"
#include "session_provider.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/game/native_engine.h"
#include <Windows.h>
#include <bcrypt.h>
#include "mt2mp/hooks/minhook_backend.h"
#include <cstdio>
#include <atomic>

namespace mt2mp::game::developer_staff {uint32_t original_show(uintptr_t,uintptr_t);}
namespace mt2mp::game::bug_authority::manual_action {
namespace {
namespace action=action_session;
namespace staff=developer_staff;
using json=nlohmann::json;
struct Pending {action::Context context;action::Binding binding;staff::Origin origin;action::BugFixIntent intent;std::string id;};
struct Operation {action::Binding binding;action::Request request;action::Evidence evidence;CallReceipt native;bool attempted=false,confirmed=false;std::string cause;};
struct Owner {
    uintptr_t image=0;uint32_t thread=GetCurrentThreadId();Entry entry;
    std::atomic<UiCommand> original{nullptr};std::atomic<bool> ready=false;bool install_attempted=false;
    std::string install_error,cause;std::optional<Pending> pending;std::map<std::string,Operation> operations;
    uint64_t captured=0,submitted=0,rejected=0,forwarded=0;
    std::map<std::string,json> fixture_receipts;
    bool fixture_locked=false;
};
std::atomic<Owner*> owner{nullptr};
void rejected(Owner& o,const std::string& e){++o.rejected;if(o.cause!=e){o.cause=e;LOG_ERROR("[BugButton] {}",e);}}
bool code(Owner& o,std::string& e){
    auto read=[](auto p,auto out,auto n,auto& error){return runtime().memory.copy(p,out,n,error);};
    if(!verify_entry(o.entry,read,[](const auto& bytes){return mt2mp::sha256(bytes);},e)||!native_code(e))return false;
    struct Body {uintptr_t rva;size_t size;const char* sha;};
    constexpr Body bodies[]{
        {0x58e20,96,"f9f3da29f37f02dda964a78dc799eb464f3aba93ab3f12b30459fd88429a5794"},
        {0x284b50,32,"655ec01a1427a753aabb58a8e527e3dc1822636a8ae0fe624cad304d23ad0e62"},
        {0x284900,592,"dedb6ec3b14358de102842d143034cf0abf9aa21a221d3f90221f51e6c4b55e0"},
        {0x2817a0,96,"29b374120b45c5491394a14655dc51f4a3fcfee060ea3b9f7c7781581a61c0d6"},
        {0x284280,176,"dff406f7a16ce7463e43e671c6ff783a2d6c9a2f90c6846d622f0dd6e5a1961e"},
        {0x96e1d0,80,"8804bc96497105c4fc82d693ffc88533c3e6c1b9e70b92e3d310cdad72a0f2cc"}};
    for(const auto& b:bodies){std::vector<unsigned char> bytes(b.size);
        if(!read(o.image+b.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=b.sha)return fail(e,"BugButton original full body differs RVA="+std::to_string(b.rva));}
    uintptr_t offset=0;if(!field(o.image+0x13b6820,0x38,offset,e))return false;
    return offset==0x45||fail(e,"BugButton original manualJob property offset differs");
}
bool read_state(Owner& o,const action::Context& c,const action::Binding& b,staff::Capture& capture,State& bugs,staff::PoolState& pool,std::string& e){
    if(GetCurrentThreadId()!=o.thread||!action_state(c,b,bugs,capture,e)||!staff::action_runtime::capture(c,b,pool,e)||!code(o,e))return false;
    if(capture.image!=o.image||capture.thread!=o.thread)return fail(e,"BugButton actual process/Mainthread changed");
    // The action receipt includes counters even before the first witnessed Job.
    if(!pool.construction){std::vector<staff::construction::LocalJob> jobs;
        if(!typed_action_runtime::construction_jobs(c,b,jobs,e))return false;
        staff::construction::State extra;if(!staff::construction::capture(capture,jobs,extra,e))return false;
        pool.construction=std::move(extra);
    }
    staff::job_census::Census queue;WorkState work;std::vector<uintptr_t> bound;
    if(!staff::job_census::read([](auto p,auto out,auto n,auto& error){return runtime().memory.copy(p,out,n,error);},capture,queue,e)||
       !capture_work(capture,queue,work,bound,e))return false;
    pool.construction->work=std::move(work);return staff::valid(pool,e);
}
bool intent_now(const action::BugFixIntent& intent,const State& bugs,uintptr_t& pointer,std::string& e){
    if(!action::valid_bug_fix(intent))return fail(e,"BugButton intent shape invalid");
    return resolve_intent(intent,bugs,runtime().lifetimes,runtime().jobs,
        [](auto p,auto out,auto n,auto& error){return runtime().memory.copy(p,out,n,error);},pointer,e);
}
std::string uuid(std::string& e){unsigned char bytes[16]{};
    const auto status=BCryptGenRandom(nullptr,bytes,sizeof bytes,BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(status){e="BugButton UUID NTSTATUS="+std::to_string(status);return {};}
    bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;char text[37]{};
    std::snprintf(text,sizeof text,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);return text;
}
struct UiCalls {
    Owner& owner;void* view;void* record;Pending pending;
    bool original(){++owner.forwarded;return owner.original.load(std::memory_order_acquire)(view,record);}
    bool admit(std::string& e){
        if(owner.pending)return fail(e,"BugButton already has a retained UI intent");
        auto& r=runtime();std::lock_guard lock(r.gate);
        if(!action::selected_session().current(pending.context,e)||!typed_action_runtime::network_binding(pending.context.snapshot_uuid,pending.binding,e))return false;
        staff::Capture capture;State bugs;staff::PoolState pool;
        if(!read_state(owner,pending.context,pending.binding,capture,bugs,pool,e))return false;
        uintptr_t bug=0;if(!selected_bug([](auto p,auto out,auto n,auto& error){return runtime().memory.copy(p,out,n,error);},capture,reinterpret_cast<uintptr_t>(view),bug,e))return false;
        auto* local=r.lifetimes.pointer(bug);if(!local)return fail(e,"BugButton selected UI receiver lacks original stable birth");
        pending.intent.bug=bug_identity(local->row);uintptr_t job=0;if(!field(bug,0x48,job,e))return false;
        pending.intent.schedule=!job;
        if(job){const auto* work=r.jobs.pointer(job);if(!work||work->row.bug!=pending.intent.bug)return fail(e,"BugButton selected UI Job lacks reciprocal birth");pending.intent.expected_job=work->row.birth;}
        uintptr_t current=0;if(!intent_now(pending.intent,bugs,current,e)||current!=bug)return fail(e,e.empty()?"BugButton UI identity changed":e);
        pending.origin=capture.pin.origin;pending.id=uuid(e);return !pending.id.empty();
    }
    bool retain(std::string&){owner.pending=std::move(pending);++owner.captured;return true;}
};
bool ui_command(void* view,void* record){
    auto* o=owner.load(std::memory_order_acquire);if(!o)return false;
    const auto original=o->original.load(std::memory_order_acquire);if(!original)return false;
    if(GetCurrentThreadId()!=o->thread)return original(view,record);
    try{
        const auto controller=typed_action_runtime::controller_status();
        const bool bound=o->ready&&controller.value("initialized",false)&&controller.value("started",false);
        if(!bound){++o->forwarded;return original(view,record);}
        uint32_t fault=0;const auto command=classify(o->image,record,fault);UiCalls calls{*o,view,record,{}};std::string e;
        const bool result=capture_ui(true,command,calls,e);
        if(fault)e+=" SEH="+std::to_string(fault);if(!e.empty())rejected(*o,e);return result;
    }catch(const std::exception& e){rejected(*o,std::string("BugButton capture failed; original suppressed: ")+e.what());return true;}
    catch(...){rejected(*o,"BugButton capture failed with nonstandard exception; original suppressed");return true;}
}
struct ClientCalls {
    const action::Context& context;const action::Binding& binding;uint64_t publication;const State& target;const staff::PoolState& pool;
    bool preflight(std::string& e){return preflight_frame(context,binding,publication,target,e,&*pool.construction->work)&&staff::action_runtime::preflight_frame(context,binding,pool,e);}
    bool bugs(bool& attempted,std::string& e){bool unknown=false;return adopt_frame(context,binding,publication,target,attempted,unknown,e);}
    bool developers(bool& attempted,std::string& e){bool unknown=false;return staff::action_runtime::adopt_frame(context,binding,pool,attempted,unknown,e);}
    bool finish(bool& attempted,std::string& e){bool unknown=false;return finish_frame(context,binding,publication,target,attempted,unknown,e);}
};
}
bool enable(std::string& e){
    static Owner state;auto* o=&state;owner.store(o,std::memory_order_release);
    if(GetCurrentThreadId()!=o->thread)return fail(e,"BugButton hook requires Mainthread");
    if(o->ready)return code(*o,e);
    if(o->install_attempted)return fail(e,"BugButton hook installation already attempted: "+o->install_error);
    o->install_attempted=true;struct SaveCause{Owner& o;std::string& e;~SaveCause(){o.install_error=e;}} save{*o,e};
    uint32_t thread=0;std::string sha;if(!native::read_resolver_receipt(o->image,thread,sha,e))return false;
    if(thread!=o->thread||o->image!=uintptr_t(GetModuleHandleW(nullptr))||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"BugButton actual EXE/process/thread differs");
    std::vector<unsigned char> bytes(320);if(!runtime().memory.copy(o->image+0x69df50,bytes.data(),bytes.size(),e))return false;
    if(mt2mp::sha256(bytes)!="b74314774d32edfffddf7f73ad97d9946ba868f819376459348cfb74b631a3fb")return fail(e,"BugButton complete original UI body differs before hook");
    UiCommand original=nullptr;const auto created=MH_CreateHook(reinterpret_cast<void*>(o->image+0x69df50),reinterpret_cast<void*>(&ui_command),reinterpret_cast<void**>(&original));
    if(created!=MH_OK||!original)return fail(e,"BugButton MinHook create failed status="+std::to_string(created));
    o->original.store(original,std::memory_order_release);o->entry={o->image+0x69df50,reinterpret_cast<uintptr_t>(&ui_command),reinterpret_cast<uintptr_t>(original),false};
    const auto enabled=MH_EnableHook(reinterpret_cast<void*>(o->entry.target));if(enabled!=MH_OK)return fail(e,"BugButton MinHook enable failed status="+std::to_string(enabled));
    o->entry.enabled=true;if(!code(*o,e))return false;o->ready=true;return true;
}
void tick(){auto* o=owner.load(std::memory_order_acquire);if(!o||!o->ready||!o->pending)return;
    const auto pending=*o->pending;o->pending.reset();std::string e;staff::Capture capture;State bugs;staff::PoolState pool;uintptr_t bug=0;
    if(!read_state(*o,pending.context,pending.binding,capture,bugs,pool,e)||!staff::same_origin(capture.pin.origin,pending.origin)||!intent_now(pending.intent,bugs,bug,e)){
        rejected(*o,e.empty()?"BugButton queued native Origin changed":e);return;}
    if(!typed_action_runtime::submit_bug_fix(pending.id,pending.intent,e)){rejected(*o,e);return;}++o->submitted;
}
bool pending_ui(){auto* o=owner.load(std::memory_order_acquire);return o&&o->pending.has_value();}
bool retire_lobby(std::string& e){auto* o=owner.load(std::memory_order_acquire);if(!o)return true;
    native::WorldStatus world;if(GetCurrentThreadId()!=o->thread||!native::read_world_status(world,e))return fail(e,e.empty()?"BugButton lobby retirement requires Mainthread":e);
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0)return fail(e,"BugButton lobby retirement requires idle Title");
    o->pending.reset();o->operations.clear();o->cause.clear();o->fixture_receipts.clear();o->fixture_locked=false;return true;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* host,std::string& e){
    auto* o=owner.load(std::memory_order_acquire);
    if(!o||!o->ready||GetCurrentThreadId()!=o->thread||request.action!=action::Action::SetBugFix||!action::valid_bug_fix(request.bug_fix)){
        fail(e,"BugButton action requires ready Mainthread owner and valid intent");return action::Outcome::RejectedNoEffect;}
    auto& r=runtime();std::lock_guard lock(r.gate);
    if(o->operations.contains(request.action_uuid)){fail(e,"BugButton native action UUID already consumed; no replay");return action::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid||r.action!=request.action_uuid){
        fail(e,"BugButton action requires exact pending Controller/action birth scope");return action::Outcome::RejectedNoEffect;}
    action::Context context;staff::Capture before;State bugs;staff::PoolState pool;uintptr_t pointer=0;
    if(!action::selected_session().current(context,e)||context.snapshot_uuid!=request.snapshot_uuid||(context.role==action::Role::Client)!=(host!=nullptr)||
       !read_state(*o,context,binding,before,bugs,pool,e)||!intent_now(request.bug_fix,bugs,pointer,e)){
        if(e.empty())e="BugButton action actual session/role differs";return action::Outcome::RejectedNoEffect;}
    const Row old=*selected(bugs,request.bug_fix);
    if(host&&(!action::valid_bug_fix_effect(request,*host)||host->before!=before.pin.ledger||host->bug_before!=old||host->developers_before!=pool)){
        fail(e,"BugButton exact Client BEFORE/effect differs from Host");return action::Outcome::RejectedNoEffect;}
    if(host&&(!r.frame_publication||r.ordered_job_birth)){
        fail(e,"BugButton Client requires completed authority-before frame and no nested birth");return action::Outcome::RejectedNoEffect;}
    // The Controller retains replay history. Old native receipts can leave once
    // its matching retirement readback was confirmed.
    for(auto it=o->operations.begin();it!=o->operations.end();)if(it->second.evidence.retirement_confirmed)it=o->operations.erase(it);else ++it;
    auto& operation=o->operations.emplace(request.action_uuid,Operation{}).first->second;
    operation.binding=binding;operation.request=request;operation.evidence.before=before.pin.ledger;
    operation.evidence.bug_before=old;operation.evidence.developers_before=pool;
    auto failed=[&](){operation.attempted|=operation.native.attempted;operation.cause=e;return operation.attempted?action::Outcome::Unknown:action::Outcome::RejectedNoEffect;};
    try{
        if(!host){
            struct HostCalls {
                Owner& owner;const action::Context& context;const action::Binding& binding;const action::Request& request;
                const staff::Capture& before;const State& bugs;const staff::PoolState& pool;uintptr_t pointer;
                bool pin(std::string& e){staff::Capture fresh;State actual;staff::PoolState state;uintptr_t receiver=0;
                    return read_state(owner,context,binding,fresh,actual,state,e)&&intent_now(request.bug_fix,actual,receiver,e)&&
                        ((receiver==pointer&&staff::same_origin(fresh.pin.origin,before.pin.origin)&&fresh.pin.ledger==before.pin.ledger&&actual==bugs&&state==pool)||fail(e,"BugButton native BEFORE changed before original call"));}
                void job(JobCall& call){work_unary(owner.image+(request.bug_fix.schedule?0x58da0:0x58e20),pointer,&call);}
                bool after_job(std::string& e){
                    if(runtime().locked)return fail(e,runtime().cause);
                    uintptr_t job=0;if(!field(pointer,0x48,job,e))return false;
                    if(!request.bug_fix.schedule)return !job||fail(e,"BugButton original Cancel retained Bug Job link");
                    const auto* local=runtime().jobs.pointer(job);
                    const Birth expected{BirthKind::Action,request.action_uuid,0,1};
                    return (job&&local&&local->row.birth==expected&&local->row.bug==request.bug_fix.bug)||fail(e,"BugButton original Schedule birth/backlink differs");
                }
                void manual(JobCall& call){work_property(owner.image,owner.image+0x13b6820,0xd257f0,pointer,"manualJob true",&call);}
                bool after_manual(std::string& e){uint8_t manual=0;if(!field(pointer,0x45,manual,e))return false;return manual==1||fail(e,"BugButton original manualJob readback differs");}
            } calls{*o,context,binding,request,before,bugs,pool,pointer};
            if(!execute_host(calls,operation.native,e))return failed();
            operation.attempted=operation.native.attempted;
        }else{
            State target=bugs;for(auto& row:target.rows)if(bug_identity(row)==request.bug_fix.bug)row=*host->bug_after;
            struct BirthScope {Runtime& runtime;~BirthScope(){runtime.ordered_job_birth.reset();}} scope{r};
            if(request.bug_fix.schedule)r.ordered_job_birth=Birth{BirthKind::Action,request.action_uuid,0,1};
            ClientCalls calls{context,binding,r.frame_publication,target,*host->developers_after};
            if(!apply_client(calls,operation.attempted,e))return failed();
        }
        staff::Capture after;State actual;staff::PoolState result;
        if(!read_state(*o,context,binding,after,actual,result,e)||!staff::same_origin(before.pin.origin,after.pin.origin)||
           before.local!=after.local||before.state!=after.state||before.pin.time_scale!=after.pin.time_scale||before.pin.pause_scale!=after.pin.pause_scale){
            if(e.empty())e="BugButton original action changed native Origin/Developer actors/Clock";return failed();}
        const auto* next=selected(actual,request.bug_fix);if(!next){e="BugButton action retired its selected Bug";return failed();}
        State expected=bugs;for(auto& row:expected.rows)if(bug_identity(row)==request.bug_fix.bug)row.value.manual=true;
        if(actual!=expected){e="BugButton action changed unrelated Bug values/identities";return failed();}
        operation.evidence.after=after.pin.ledger;operation.evidence.bug_after=*next;operation.evidence.developers_after=result;
        if(!action::valid_bug_fix_effect(request,operation.evidence)||
           (host&&(operation.evidence.after!=host->after||operation.evidence.bug_after!=host->bug_after||operation.evidence.developers_after!=host->developers_after))){
            e="BugButton complete native selected effect differs from original/Host contract";return failed();}
        operation.confirmed=true;e.clear();return action::Outcome::Confirmed;
    }catch(const std::exception& cause){if(e.empty())e=std::string("BugButton action exception: ")+cause.what();}
    catch(...){if(e.empty())e="BugButton action nonstandard exception";}
    return failed();
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* o=owner.load(std::memory_order_acquire);if(!o||GetCurrentThreadId()!=o->thread)return fail(e,"BugButton observation requires Mainthread");
    const auto found=o->operations.find(request.action_uuid);
    if(found==o->operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)return fail(e,"BugButton confirmed exact native receipt absent");
    action::Context context;staff::Capture capture;State bugs;staff::PoolState pool;
    if(!action::selected_session().current(context,e)||!read_state(*o,context,binding,capture,bugs,pool,e))return false;
    const auto* row=selected(bugs,request.bug_fix);
    if(!row||found->second.evidence.after!=capture.pin.ledger||found->second.evidence.bug_after!=*row||found->second.evidence.developers_after!=pool)
        return fail(e,"BugButton retained AFTER changed before acknowledgement");
    out=found->second.evidence;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence observed;if(!observe(binding,request,observed,e))return false;
    owner.load(std::memory_order_acquire)->operations.at(request.action_uuid).evidence.retirement_confirmed=true;return true;
}
json status(){auto* o=owner.load(std::memory_order_acquire);if(!o)return {{"initialized",false}};
    if(GetCurrentThreadId()!=o->thread)return {{"initialized",true},{"error","BugButton status requires Mainthread"}};
    auto operations=json::array();for(const auto& [id,op]:o->operations)operations.push_back({{"action_uuid",id},{"schedule",op.request.bug_fix.schedule},
        {"attempted",op.attempted||op.native.attempted},{"job_returned",op.native.job.returned},{"manual_returned",op.native.manual.returned},
        {"confirmed",op.confirmed},{"retired",op.evidence.retirement_confirmed},{"cause",op.cause}});
    return {{"initialized",true},{"enabled",bool(o->ready)},{"pending_ui",o->pending.has_value()},{"captured",o->captured},{"submitted",o->submitted},
        {"rejected",o->rejected},{"forwarded",o->forwarded},{"cause",o->cause},{"operations",operations}};
}
#include "manual_fixture.inc"
}
