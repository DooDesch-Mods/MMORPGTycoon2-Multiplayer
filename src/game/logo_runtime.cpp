#include "logo_runtime.h"
#include "logo_native.h"
#include "typed_action_runtime.h"
#include "session_provider.h"
#include "live_host_state/native_port.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/hooks/minhook_backend.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/core/logger.h"
#include <Windows.h>
#include <intrin.h>
#include <bcrypt.h>
#include <atomic>
#include <map>
#include <optional>
#include <cstdio>
namespace mt2mp::game::logo_runtime {
namespace {
namespace a=action_session;namespace live=live_host_state;using json=nlohmann::json;
using Set=void(*)(void*,void*);using UI=bool(*)(void*,void*);
struct Pending {a::Context context;a::Binding binding;a::LogoIntent intent;std::string id;};
struct Operation {a::Binding binding;a::Request request;a::Evidence evidence;logo_native::Owned value;
    uintptr_t software=0,old_logo=0;uint64_t original_calls=0;bool attempted=false,returned=false,confirmed=false,retired=false;std::string cause;};
struct Runtime {DWORD thread=GetCurrentThreadId();uintptr_t image=0,view=0;Set set=nullptr;UI ui=nullptr;
    bool install_attempted=false,ready=false,replay=false,unknown=false;std::string cause,install_cause;
    placement_ui_probe::ProcessMemory memory;std::optional<Pending> pending;
    std::map<std::string,logo_native::Owned> source;std::map<std::string,Operation> operations;uint64_t captures=0,submitted=0;};
std::atomic<Runtime*> published{nullptr};
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
struct Memory:subscriber_lifecycle::Memory {Runtime& runtime;explicit Memory(Runtime& r):runtime(r){}bool read(uintptr_t p,void* out,size_t size,std::string& e)override{return runtime.memory.copy(p,out,size,e);}};
template<class T>bool at(Runtime& r,uintptr_t p,size_t offset,T& out,std::string& e){if(!p||p>UINTPTR_MAX-offset)return fail(e,"Logo native field extent invalid");return r.memory.copy(p+offset,&out,sizeof out,e);}
bool ap(Runtime& r,uintptr_t pointer,uintptr_t expected,std::string& e){uintptr_t value=0;return at(r,pointer,0,value,e)&&(value==r.image+expected||fail(e,"Logo native class differs required="+std::to_string(expected)));}
void reject(Runtime& r,const std::string& e){if(r.cause!=e){r.cause=e;LOG_ERROR("[Logo] cause={}",e);}}
bool read(Runtime& r,const a::Context& context,const a::Binding& binding,uintptr_t& software,uintptr_t& logo,a::LogoState& value,a::Ledger& money,std::string& e){
    a::Context actual;if(GetCurrentThreadId()!=r.thread||!a::selected_session().current(actual,e)||actual!=context)return fail(e,e.empty()?"Logo current Session/Mainthread differs":e);
    live::NativePort port(context,binding,nullptr);live::Pin pin;if(!port.read(pin,e)||pin.image!=r.image)return false;
    if(!at(r,r.image,0x13df500,software,e)||!ap(r,software,0x1715450,e)||!at(r,software,0x68,logo,e))return false;
    Memory memory(r);json records;if(!logo_native::read(r.image,memory,logo,value,records,e))return false;
    uintptr_t after_software=0,after_logo=0;live::Pin after;if(!port.read(after,e)||after!=pin||!at(r,r.image,0x13df500,after_software,e)||after_software!=software||!at(r,software,0x68,after_logo,e)||after_logo!=logo)return fail(e,e.empty()?"Logo native owner/Ledger changed during original capture":e);
    money=live::ledger(pin.values);e.clear();return true;
}
std::string uuid(std::string& e){unsigned char bytes[16];const auto status=BCryptGenRandom(nullptr,bytes,sizeof bytes,BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(status){e="Logo BCryptGenRandom NTSTATUS="+std::to_string(status);return {};}
    bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;char text[37];std::snprintf(text,sizeof text,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);return text;}
void set_entry(void* receiver,void* object){
    auto* r=published.load(std::memory_order_acquire);if(!r||!r->set)throw std::runtime_error("Logo original setter was never published");
    const auto controller=typed_action_runtime::controller_status();
    if(GetCurrentThreadId()!=r->thread||!controller.value("started",false)){r->set(receiver,object);return;}
    const auto caller=uintptr_t(_ReturnAddress())-r->image;std::string e;
    if(r->unknown||r->pending||r->replay||!r->view||caller!=0x735e04||controller.value("locked",true)||controller.value("pending",true)){
        r->unknown=true;reject(*r,"Logo shared Apply lacks exact idle original editor/caller; setter suppressed");return;}
    uintptr_t owned=0;if(!ap(*r,r->view,0x1701010,e)||!at(*r,r->view,0x330,owned,e)||owned!=uintptr_t(object)||!object){
        r->unknown=true;reject(*r,e.empty()?"Logo editor detached ownership slot differs":e);return;}
    Pending pending;if(!a::selected_session().current(pending.context,e)||!typed_action_runtime::network_binding(pending.context.snapshot_uuid,pending.binding,e)){
        r->unknown=true;reject(*r,e);return;}
    uintptr_t software=0,old=0;a::LogoState before;a::Ledger ledger;
    if(!read(*r,pending.context,pending.binding,software,old,before,ledger,e)||software!=uintptr_t(receiver)||old==uintptr_t(object)){
        r->unknown=true;reject(*r,e.empty()?"Logo actual Software/editor owner differs":e);return;}
    Memory memory(*r);a::LogoState desired;
    if(!logo_native::read(r->image,memory,uintptr_t(object),desired,pending.intent.records,e)){r->unknown=true;reject(*r,e);return;}
    pending.intent.before_sha256=before.sha256;pending.intent.desired_sha256=desired.sha256;pending.id=uuid(e);
    if(pending.id.empty()||!a::valid_logo(pending.intent)){r->unknown=true;reject(*r,e.empty()?"Logo detached captured value invalid":e);return;}
    r->source.emplace(pending.id,logo_native::Owned{uintptr_t(object),true,false,false});r->pending=std::move(pending);++r->captures;
}
bool ui_entry(void* view,void* record){
    auto* r=published.load(std::memory_order_acquire);if(!r||!r->ui)throw std::runtime_error("Logo editor original was never published");
    if(GetCurrentThreadId()!=r->thread||r->view)return r->ui(view,record);
    r->view=uintptr_t(view);struct Reset{Runtime& r;~Reset(){r.view=0;}} reset{*r};const auto captured=r->captures;
    const bool returned=r->ui(view,record);
    if(r->captures!=captured){uintptr_t remaining=1;std::string e;
        if(!at(*r,uintptr_t(view),0x330,remaining,e)||remaining){r->unknown=true;reject(*r,e.empty()?"Logo original editor did not release transferred slot":e);}}
    return returned;
}
uint32_t apply(Runtime* r,uintptr_t software,uintptr_t logo){
    __try{r->set(reinterpret_cast<void*>(software),reinterpret_cast<void*>(logo));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
std::string hook_cause(){
#ifdef MT2MP_FRAMEWORK_PLUGIN
    return hooks::framework::process_backend().error();
#else
    return {};
#endif
}
}
bool enable(std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r){r=new Runtime;published.store(r,std::memory_order_release);}
    if(GetCurrentThreadId()!=r->thread)return fail(e,"Logo hook initialization requires Mainthread");if(r->ready){e.clear();return true;}
    if(r->install_attempted)return fail(e,"Logo hook installation failed earlier; no retry: "+r->install_cause);r->install_attempted=true;
    struct Keep{Runtime& r;std::string& e;~Keep(){if(!e.empty())r.install_cause=e;}}keep{*r,e};
    uint32_t thread=0;std::string sha;if(!native::read_resolver_receipt(r->image,thread,sha,e)||thread!=r->thread||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"Logo exact EXE/Mainthread differs: "+e);
    struct Code{uintptr_t rva;size_t size;const char* sha;};
    #include "logo_native_pins.inc"
    for(const auto& pin:codes){std::vector<unsigned char> bytes(pin.size);if(!r->memory.copy(r->image+pin.rva,bytes.data(),bytes.size(),e)||mt2mp::sha256(bytes)!=pin.sha)return fail(e,e.empty()?"Logo exact original code differs RVA="+std::to_string(pin.rva):e);}
    struct Hook{uintptr_t rva;void* entry;void** original;};const Hook hooks[]{{0x735800,reinterpret_cast<void*>(&ui_entry),reinterpret_cast<void**>(&r->ui)},{0x2dcc00,reinterpret_cast<void*>(&set_entry),reinterpret_cast<void**>(&r->set)}};
    for(const auto& h:hooks){const auto created=MH_CreateHook(reinterpret_cast<void*>(r->image+h.rva),h.entry,h.original);if(created!=MH_OK||!*h.original)return fail(e,"Logo hook create failed RVA="+std::to_string(h.rva)+" status="+std::to_string(created)+" original="+hook_cause());
        const auto enabled=MH_EnableHook(reinterpret_cast<void*>(r->image+h.rva));if(enabled!=MH_OK)return fail(e,"Logo hook enable failed RVA="+std::to_string(h.rva)+" status="+std::to_string(enabled)+" original="+hook_cause());}
    r->ready=true;e.clear();return true;
}
bool pending_ui(){auto* r=published.load(std::memory_order_acquire);return r&&(r->unknown||r->pending||r->replay||!r->source.empty());}
bool retire_lobby(std::string& e){auto* r=published.load(std::memory_order_acquire);if(!r){e.clear();return true;}
    native::WorldStatus world;if(GetCurrentThreadId()!=r->thread||!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||r->replay||r->view)return fail(e,"Logo retirement requires actual idle Title and no native scope");
    for(auto& [id,value]:r->source){(void)id;if(!logo_native::discard(r->image,value,e))return false;}r->source.clear();r->pending.reset();r->operations.clear();r->unknown=false;r->cause.clear();e.clear();return true;
}
void tick(){auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||r->view||r->replay)return;
    if(r->pending&&!r->unknown){const auto pending=*r->pending;r->pending.reset();std::string e;a::Context context;a::Binding binding;
        if(!a::selected_session().current(context,e)||context!=pending.context||!typed_action_runtime::network_binding(context.snapshot_uuid,binding,e)||binding!=pending.binding||
           !typed_action_runtime::submit_logo(pending.id,pending.intent,e)){reject(*r,e.empty()?"Logo captured binding changed before ordering":e);r->unknown=true;return;}++r->submitted;}
    const auto controller=typed_action_runtime::controller_status();if(controller.value("pending",true)||controller.value("locked",true))return;
    const auto id=controller.value("action_uuid",std::string{});const auto source=r->source.find(id);
    if(source!=r->source.end()&&controller.value("outcome",0)==2){std::string e;if(!logo_native::discard(r->image,source->second,e)){r->unknown=true;reject(*r,e);return;}r->source.erase(source);}
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* host,std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||r->unknown||r->replay||r->view||r->operations.contains(request.action_uuid)||request.action!=a::Action::SetLogo){e="Logo original execution requires ready fresh Mainthread owner";return a::Outcome::RejectedNoEffect;}
    a::Context context;const auto controller=typed_action_runtime::controller_status();
    if(!a::selected_session().current(context,e)||context.snapshot_uuid!=request.snapshot_uuid||!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid||
       (host!=nullptr)!=(context.role==a::Role::Client)){e="Logo exact Controller/session/role differs";return a::Outcome::RejectedNoEffect;}
    uintptr_t software=0,old=0;a::LogoState before;a::Ledger ledger;
    if(!read(*r,context,binding,software,old,before,ledger,e))return a::Outcome::RejectedNoEffect;
    if(before.sha256!=request.logo.before_sha256){e="Logo captured before value differs actual="+before.sha256+" required="+request.logo.before_sha256;return a::Outcome::RejectedNoEffect;}
    if(host&&(!host->logo_before||!host->logo_after||*host->logo_before!=before||host->before!=ledger||host->after!=ledger||host->logo_after->sha256!=request.logo.desired_sha256)){
        e="Logo Client actual native before/Ledger differs from Host";return a::Outcome::RejectedNoEffect;}
    auto& op=r->operations[request.action_uuid];op.binding=binding;op.request=request;op.software=software;op.old_logo=old;op.evidence.before=ledger;op.evidence.logo_before=before;
    auto unknown=[&](){if(e.empty())e="Logo original result Unknown";op.cause=e;r->unknown=true;reject(*r,e);return a::Outcome::Unknown;};
    const auto source=r->source.find(request.action_uuid);Memory memory(*r);
    if(source!=r->source.end())op.value=source->second;
    else if(!logo_native::create(r->image,memory,request.logo,op.value,e)){
        if(op.value.unknown)return unknown();std::string cleanup;if(!logo_native::discard(r->image,op.value,cleanup))e+="; "+cleanup;op.cause=e;return a::Outcome::RejectedNoEffect;}
    a::LogoState detached;json actual;if(!logo_native::read(r->image,memory,op.value.pointer,detached,actual,e)||detached.sha256!=request.logo.desired_sha256||actual!=request.logo.records)return unknown();
    r->replay=true;op.attempted=true;++op.original_calls;const auto seh=apply(r,software,op.value.pointer);r->replay=false;
    if(seh){op.value.unknown=true;e="Logo original SetLogo SEH="+std::to_string(seh)+"; no replay";return unknown();}op.returned=true;op.value.adopted=true;
    if(source!=r->source.end())r->source.erase(source);
    uintptr_t after_software=0,after_logo=0;a::LogoState after;
    if(!read(*r,context,binding,after_software,after_logo,after,op.evidence.after,e)||after_software!=software||after_logo!=op.value.pointer||after_logo==old||after.sha256!=request.logo.desired_sha256||op.evidence.after!=ledger)return unknown();
    op.evidence.logo_after=after;op.confirmed=true;e.clear();return a::Outcome::Confirmed;
}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r)return fail(e,"Logo observation owner absent");const auto it=r->operations.find(request.action_uuid);
    if(it==r->operations.end()||it->second.binding!=binding||it->second.request!=request||!it->second.confirmed)return fail(e,"Logo observation lacks exact confirmed original");
    auto& op=it->second;a::Context context;uintptr_t software=0,logo=0;a::LogoState value;a::Ledger ledger;
    if(!a::selected_session().current(context,e)||context.snapshot_uuid!=request.snapshot_uuid||!read(*r,context,binding,software,logo,value,ledger,e))return false;
    if(software!=op.software||logo!=op.value.pointer||value!=*op.evidence.logo_after||ledger!=op.evidence.after)return fail(e,"Logo actual adopted owner/value/Ledger changed");out=op.evidence;out.retirement_confirmed=op.retired;e.clear();return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& e){a::Evidence actual;if(!observe(binding,request,actual,e))return false;published.load()->operations.at(request.action_uuid).retired=true;e.clear();return true;}
json status(){auto* r=published.load(std::memory_order_acquire);if(!r)return {{"ready",false}};auto operations=json::object();for(const auto& [id,op]:r->operations)operations[id]={{"attempted",op.attempted},{"returned",op.returned},{"confirmed",op.confirmed},{"retired",op.retired},{"original_calls",op.original_calls},{"cause",op.cause},{"before",op.evidence.logo_before?op.evidence.logo_before->sha256:std::string{}},{"after",op.evidence.logo_after?op.evidence.logo_after->sha256:std::string{}}};
    return {{"ready",r->ready},{"pending",pending_ui()},{"unknown",r->unknown},{"captures",r->captures},{"submitted",r->submitted},{"cause",r->cause},{"operations",operations}};}
}
