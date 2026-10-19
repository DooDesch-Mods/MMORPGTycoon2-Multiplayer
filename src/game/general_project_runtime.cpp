#include "general_project_runtime.h"
#include "general_project_reader.h"
#include "general_project_identity_reader.h"
#include "typed_action_runtime.h"
#include "session_provider.h"
#include "native_open_runtime.h"
#include "live_host_state/native_port.h"
#include "live_host_state/scalar_store.h"
#include "live_host_state/apply_timing.h"
#include "live_host_state/memory_query_timing.h"
#include "developer_staff/action_native.h"
#include "tutorial_replication/native.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/core/logger.h"
#include <windows.h>
#include <bcrypt.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include "tutorial_replication/start_presentation.h"
#include "bug_authority/runtime_internal.h"

namespace mt2mp::game::general_project_runtime {
namespace {
namespace a=action_session;namespace p=general_project;namespace live=live_host_state;
struct Pending {a::Context context;a::Binding binding;p::Intent intent;std::string id;uintptr_t window=0,tabs=0;};
struct Operation {a::Context context;a::Binding binding;a::Request request;a::Evidence evidence;bool attempted=false,native_entered=false,returned=false,confirmed=false,retired=false,ui_attempted=false,ui_returned=false,ui_closed=false;uint32_t calls=0;std::string cause;std::array<uint64_t,4> local_random{};};
struct Replay {std::string id;a::Context context;a::Binding binding;p::Intent intent;uintptr_t source=0,lounge=0,software=0;uint32_t entered=0;std::string cause;};
struct Runtime {
    DWORD thread=GetCurrentThreadId();uintptr_t image=0;bool ready=false;
    placement_ui_probe::ProcessMemory memory;std::optional<Pending> pending;std::optional<Replay> replay;
    std::map<std::string,Operation> operations;std::map<std::string,Pending> presentations;uint64_t captures=0,submitted=0,rejected=0;std::string cause;
};
std::atomic<Runtime*> published{nullptr};
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
void rejected(Runtime& r,const std::string& cause){++r.rejected;if(r.cause!=cause){r.cause=cause;LOG_ERROR("[GeneralProject] rejected cause={}",cause);}}
template<class T>bool at(Runtime& r,uintptr_t address,size_t offset,T& value,std::string& e){if(!address||address>UINTPTR_MAX-offset)return fail(e,"General project native address overflow");return r.memory.copy(address+offset,&value,sizeof value,e);}
bool string(Runtime& r,uintptr_t address,std::string& value,std::string& e){uintptr_t data=0;uint64_t count=0;if(!at(r,address,0,data,e)||!at(r,address,8,count,e))return false;
    if(!data||count>200||data>UINTPTR_MAX-count)return fail(e,"General project GNU string extent invalid");std::array<char,201> bytes{};if(!r.memory.copy(data,bytes.data(),size_t(count)+1,e))return false;
    if(bytes[count]||std::memchr(bytes.data(),0,size_t(count)))return fail(e,"General project GNU string termination differs");value.assign(bytes.data(),size_t(count));return true;
}
uint32_t getter(uintptr_t image,uintptr_t rva,uintptr_t object,bool& value)noexcept{
    __try{value=reinterpret_cast<bool(*)(const void*)>(image+rva)(reinterpret_cast<const void*>(object));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t pack_getter(uintptr_t image,uintptr_t rva,uintptr_t store,uintptr_t pack,bool& value)noexcept{
    __try{value=reinterpret_cast<bool(*)(const void*,const void*)>(image+rva)(reinterpret_cast<const void*>(store),reinterpret_cast<const void*>(pack));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t call(uintptr_t image,p::Command command,uintptr_t lounge,uintptr_t source,int32_t workers,uint32_t& returned,bool& accepted)noexcept{
    __try{accepted=true;
        if(command==p::Command::Start){accepted=reinterpret_cast<bool(*)(void*)>(image+0x262ec0)(reinterpret_cast<void*>(source));returned|=1;if(accepted){reinterpret_cast<void(*)(void*,const void*)>(image+0x25f290)(reinterpret_cast<void*>(lounge),reinterpret_cast<const void*>(source));returned|=2;}}
        else if(command==p::Command::Cancel){reinterpret_cast<void(*)(void*)>(image+0x25f1e0)(reinterpret_cast<void*>(lounge));returned|=4;}
        else if(command==p::Command::Commit){reinterpret_cast<void(*)(void*)>(image+0x25fb10)(reinterpret_cast<void*>(lounge));returned|=8;}
        else if(command==p::Command::Workers){reinterpret_cast<void(*)(void*,int32_t)>(image+0x25ec40)(reinterpret_cast<void*>(lounge),workers);returned|=16;}
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t seed(uintptr_t address,const std::array<uint64_t,4>& before,const std::array<uint64_t,4>& desired)noexcept{
    __try{if(std::memcmp(reinterpret_cast<const void*>(address),before.data(),32))return ERROR_INVALID_DATA;std::memcpy(reinterpret_cast<void*>(address),desired.data(),32);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
std::string hash(const std::string& value){return mt2mp::sha256(std::vector<unsigned char>(value.begin(),value.end()));}
#include "general_project_pins.inc"
bool code(Runtime& r,std::string& e){
    if(!developer_staff::action_runtime::verify_entry(r.image+0x6c7b70,e)||!tutorial_replication::verify_pay_entry(r.image+0x262ec0,e)||!tutorial_replication::verify_commit_entry(r.image+0x25fb10,e)||!tutorial_replication::verify_allocate_entry(r.image+0x25ec40,e))return false;
    if(!tutorial_replication::start_intent::verify_ui(r.image,[&](auto address,auto out,auto count,auto& cause){return r.memory.copy(address,out,count,cause);},e,developer_staff::action_runtime::verify_entry))return false;
    for(const auto& pin:project_pins){std::vector<unsigned char> bytes(pin.bytes);if(!r.memory.copy(r.image+pin.rva,bytes.data(),bytes.size(),e))return false;if(mt2mp::sha256(bytes)!=pin.sha)return fail(e,"General project original body differs RVA="+std::to_string(pin.rva));}return true;
}
bool context(Runtime& r,const a::Context& c,const a::Binding& b,live::Pin& pin,std::string& e){
    a::Context actual;uintptr_t image=0;uint32_t thread=0;std::string sha;
    if(GetCurrentThreadId()!=r.thread||!a::selected_session().current(actual,e)||actual!=c||!native::read_resolver_receipt(image,thread,sha,e)||image!=r.image||thread!=r.thread||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,e.empty()?"General project actual Session/Mainthread/EXE differs":e);
    live::NativePort port(c,b,nullptr);if(!port.read(pin,e))return false;
    if(c.role==a::Role::Client&&pin.selected!=0)return fail(e,"General project Client actual Clock requires0");return true;
}
bool reference_snapshot(Runtime& r,const a::Context& c,const a::Binding& b,p::ReferenceOwners& out,std::string& e){
    out={};live::Pin pin;if(!context(r,c,b,pin,e))return false;
    auto read=[&](uintptr_t address,size_t offset,auto& value,std::string& cause){return at(r,address,offset,value,cause);};
    auto text=[&](uintptr_t address,std::string& value,std::string& cause){return string(r,address,value,cause);};
    p::ReferenceOwners first,second;
    {
        live::apply_timing::PageReadScope pages(!live::apply_timing::PageReadScope::reader_active());
        if(!p::reference_owners(read,text,r.image,first,e)||!p::reference_owners(read,text,r.image,second,e))return false;
    }
    if(first!=second)return fail(e,"General project canonical reference owners changed during double read");
    building_mixed_identity::opened_file::CurrentWorld world;uintptr_t software=0;
    if(!native_open_runtime::read_current_world(world,e)||world.world_epoch!=b.native_epoch||world.generation!=b.native_generation||
       uintptr_t(world.world)!=pin.world||!at(r,uintptr_t(world.state),0x1c8,software,e)||software!=second.software)
        return fail(e,e.empty()?"General project canonical reference World/State/Software owner differs":e);
    out=std::move(second);e.clear();return true;
}
bool snapshot(Runtime& r,const a::Context& c,const a::Binding& b,p::Local& local,p::State& out,a::Ledger& money,std::string& e){
    live::Pin pin;if(!context(r,c,b,pin,e))return false;
    auto read=[&](uintptr_t address,size_t offset,auto& value,std::string& cause){return at(r,address,offset,value,cause);};
    auto text=[&](uintptr_t address,std::string& value,std::string& cause){return string(r,address,value,cause);};
    auto flags=[&](uintptr_t source,p::Catalogue& row,std::string& cause){
        const std::array<uintptr_t,5> entries{0x262b20,0x263210,0x262bf0,0x262d30,0x263010};bool* values[]{&row.valid,&row.visible,&row.owned,&row.choose,&row.afford};
        for(size_t i=0;i<entries.size();++i){const auto seh=getter(r.image,entries[i],source,*values[i]);if(seh)return fail(cause,"General project original predicate SEH="+std::to_string(seh)+" RVA="+std::to_string(entries[i]));}return true;
    };
    auto packs=[&](uintptr_t store,uintptr_t pack,bool& purchased,bool& deployed,std::string& cause){const auto a=pack_getter(r.image,0x45250,store,pack,purchased);if(a)return fail(cause,"General project original pack purchase getter SEH="+std::to_string(a));const auto b=pack_getter(r.image,0x45330,store,pack,deployed);return !b||fail(cause,"General project original pack deploy getter SEH="+std::to_string(b));};
    p::State first;p::Local before;
    if(!p::read(read,text,flags,packs,r.image,before,first,e)||!p::read(read,text,flags,packs,r.image,local,out,e))return false;
    if(before!=local||first!=out)return fail(e,"General project complete native owners/state changed during double read");
    building_mixed_identity::opened_file::CurrentWorld world;if(!native_open_runtime::read_current_world(world,e))return false;uintptr_t owned=0;
    if(world.world_epoch!=b.native_epoch||world.generation!=b.native_generation||!at(r,uintptr_t(world.state),0x180,owned,e)||owned!=local.lounge||!at(r,uintptr_t(world.state),0x1c8,owned,e)||owned!=local.software||!at(r,uintptr_t(world.mode),0x150,owned,e)||owned!=local.player)return fail(e,e.empty()?"General project actual World/State/Mode reciprocal owners differ":e);
    money=live::ledger(pin.values);e.clear();return true;
}
std::string uuid(std::string& e){unsigned char b[16]{};const auto code=BCryptGenRandom(nullptr,b,sizeof b,BCRYPT_USE_SYSTEM_PREFERRED_RNG);if(code){e="General project UUID NTSTATUS="+std::to_string(code);return {};}
    b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;char text[37]{};std::snprintf(text,sizeof text,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);return text;
}
uint32_t command_label(uintptr_t image,void* record,uintptr_t& result)noexcept{
    __try{result=uintptr_t(reinterpret_cast<const void*(*)(const void*)>(image+0x96d880)(record));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t command_workers(uintptr_t image,void* record,int32_t& value)noexcept{
    __try{void* token=reinterpret_cast<void*(*)(void*,int32_t)>(image+0x967740)(record,0);value=reinterpret_cast<int32_t(*)(const void*)>(image+0x96d2c0)(token);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool window(Runtime& r,uintptr_t receiver,uintptr_t& tabs,std::string& e){
    building_mixed_identity::opened_file::CurrentWorld world;if(!native_open_runtime::read_current_world(world,e))return false;
    auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return at(r,base,offset,value,cause);};std::vector<uintptr_t> windows;
    if(!p::array(read,uintptr_t(world.mode),0xa0,windows,e)||std::count(windows.begin(),windows.end(),receiver)!=1)return fail(e,e.empty()?"General project UI is not an actual owned Mode window":e);
    uintptr_t ap=0,logic=1;uint8_t visible=0;std::string name;
    if(!at(r,receiver,0,ap,e)||ap!=r.image+0x1722e60||!at(r,receiver,0x318,logic,e)||logic||!at(r,receiver,0x45f,visible,e)||visible!=1||!at(r,receiver,0x540,tabs,e)||!tabs||!string(r,receiver+0x1b0,name,e)||name!="development")return fail(e,e.empty()?"General project actual DevelopmentWindow class/owner/name/visibility differs":e);return true;
}
bool completed(Runtime& r,uintptr_t lounge,p::Command command,std::string& e){
    if(command!=p::Command::Commit&&command!=p::Command::Workers)return true;
    bool value=false;const auto seh=getter(r.image,command==p::Command::Commit?0x25ed50:0x25edb0,lounge,value);
    if(seh)return fail(e,"General project original completion predicate SEH="+std::to_string(seh));
    return (command==p::Command::Commit?value:!value)||fail(e,"General project original implementation/debugging prerequisite rejects command");
}
uint32_t refresh(uintptr_t image,uintptr_t window,uintptr_t tabs,p::Command command)noexcept{
    if(command==p::Command::Start)return tutorial_replication::start_intent::native_present(image,window,tabs);
    __try{if(command==p::Command::Cancel)reinterpret_cast<void(*)(void*)>(image+0x6c37f0)(reinterpret_cast<void*>(window));
        if(command==p::Command::Cancel||command==p::Command::Commit)reinterpret_cast<void(*)(void*)>(image+0x6c2d70)(reinterpret_cast<void*>(window));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool presentation(Runtime& r,Operation& op,std::string& e){
    const auto found=r.presentations.find(op.request.action_uuid);if(found==r.presentations.end())return true;
    if(op.ui_attempted)return op.ui_returned||fail(e,op.cause.empty()?"General project presentation already attempted; no replay":op.cause);
    const auto& retained=found->second;building_mixed_identity::opened_file::CurrentWorld world;if(!native_open_runtime::read_current_world(world,e))return false;
    auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return at(r,base,offset,value,cause);};std::vector<uintptr_t> windows;
    if(!p::array(read,uintptr_t(world.mode),0xa0,windows,e))return false;
    if(std::find(windows.begin(),windows.end(),retained.window)==windows.end()){op.ui_closed=true;r.presentations.erase(found);LOG_INFO("[GeneralProject] Source window closed before confirmed presentation action={}",op.request.action_uuid);return true;}
    uintptr_t tabs=0;if(!window(r,retained.window,tabs,e)||tabs!=retained.tabs||!code(r,e))return fail(e,e.empty()?"General project retained presentation window/tabs changed":e);
    p::Local before_local,after_local;p::State before,after;a::Ledger before_money,after_money;
    if(!snapshot(r,op.context,op.binding,before_local,before,before_money,e))return false;
    op.ui_attempted=true;const auto exception=refresh(r.image,retained.window,tabs,retained.intent.command);op.ui_returned=!exception;
    if(exception)return fail(e,"General project original source presentation exception="+std::to_string(exception)+"; no replay");
    if(!snapshot(r,op.context,op.binding,after_local,after,after_money,e)||before_local!=after_local||before!=after||before_money!=after_money)return fail(e,e.empty()?"General project source presentation changed actual gameplay/owners/RNG/Ledger":e);
    r.presentations.erase(found);return true;
}
bool progress_once(Runtime& r,const a::Context& c,const a::Binding& b,p::Progress& out,uintptr_t& lounge,uintptr_t& current,std::string& e,bool validate=true){
    live::Pin pin;if(!context(r,c,b,pin,e))return false;uintptr_t ap=0;out={};
    if(!at(r,r.image,0x13df290,lounge,e)||!at(r,lounge,0,ap,e)||ap!=r.image+0x16de6c0||!at(r,lounge,0xa8,current,e))return fail(e,e.empty()?"General project Frame current Lounge/class differs":e);
    auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return at(r,base,offset,value,cause);};auto text=[&](uintptr_t address,std::string& value,std::string& cause){return string(r,address,value,cause);};
    if(current){p::Source source;if(!p::source(read,text,r.image,current,source,e))return false;std::vector<uintptr_t> catalogue;if(!p::array(read,lounge,0xd8,catalogue,e))return false;size_t matches=0;
        for(auto object:catalogue){p::Source candidate;if(object==current)return fail(e,"General project Frame current clone aliases owned catalogue");if(!p::source(read,text,r.image,object,candidate,e))return false;if(candidate==source)++matches;}
        if(matches!=1)return fail(e,"General project Frame current source lacks unique current catalogue identity");out.current=std::move(source);
    }
    if(!at(r,lounge,0xb0,out.work,e)||!at(r,lounge,0xb4,out.lines,e)||!at(r,lounge,0xb8,out.bugs_total,e)||!at(r,lounge,0xbc,out.bugs_known,e)||!at(r,lounge,0xc4,out.workers,e))return false;
    return (!validate||p::valid(out))||fail(e,"General project Frame current native progress invalid");
}
}
bool enable(std::string& e){auto* old=published.load(std::memory_order_acquire);if(old){if(old->ready){e.clear();return true;}return fail(e,old->cause);}
    auto* r=new Runtime;published.store(r,std::memory_order_release);uintptr_t image=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(image,thread,sha,e)||image!=uintptr_t(GetModuleHandleW(nullptr))||thread!=r->thread||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"){r->cause=e.empty()?"General project actual resolver differs":e;return fail(e,r->cause);}
    r->image=image;if(!code(*r,e)){r->cause=e;return false;}r->ready=true;e.clear();return true;
}
bool original_route(uintptr_t entry,void* receiver,int32_t workers,bool& handled){
    handled=false;auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||!r->replay)return false;auto& replay=*r->replay;
    const auto expected=replay.intent.command==p::Command::Start?0x262ec0:replay.intent.command==p::Command::Commit?0x25fb10:replay.intent.command==p::Command::Workers?0x25ec40:0;
    const bool nested_stat=entry==0x2de2d0&&replay.intent.command==p::Command::Start&&replay.intent.source.kind==2;
    if(entry!=expected&&!nested_stat)return false;handled=true;const uint32_t bit=nested_stat?2:1;
    const auto blocked=[&](const std::string& cause){if(replay.cause.empty()){replay.cause=cause;LOG_ERROR("[GeneralProject] original admission failed action={} cause={}",replay.id,cause);}return false;};
    if(GetCurrentThreadId()!=r->thread)return blocked("General project private original Mainthread differs");
    const auto controller=typed_action_runtime::controller_status();a::Context actual;std::string e;building_mixed_identity::opened_file::CurrentWorld world;
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=replay.id||!a::selected_session().current(actual,e)||actual!=replay.context||!native_open_runtime::read_current_world(world,e)||world.world_epoch!=replay.binding.native_epoch||world.generation!=replay.binding.native_generation)return blocked(e.empty()?"General project private original exact pending Context/Binding differs":e);
    if(replay.entered&bit)return blocked("General project private original already entered once; no replay");
    if(nested_stat&&!(replay.entered&1))return blocked("General project nested statistic purchase lacks its original Pay entry");
    const auto owner=nested_stat?replay.software:entry==0x262ec0?replay.source:replay.lounge;
    if(uintptr_t(receiver)!=owner||(entry==0x25ec40&&workers!=replay.intent.workers)||(nested_stat&&workers!=replay.intent.source.statistic))return blocked("General project private original receiver/worker/statistic value differs");
    replay.entered|=bit;return true;
}
bool admit_bug_birth(const a::Context& c,const a::Binding& b,const std::string& action,uintptr_t caller,bool& owned,std::string& e){
    owned=false;auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||!r->replay||r->replay->intent.command!=p::Command::Commit||caller!=r->image+0x354e8d){e.clear();return false;}
    owned=true;const auto& replay=*r->replay;const auto controller=typed_action_runtime::controller_status();
    if(GetCurrentThreadId()!=r->thread||replay.context!=c||replay.binding!=b||replay.id!=action||!(replay.entered&1)||!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=action)return fail(e,"General project Bug birth lacks exact entered original Commit/Context/Binding/action");
    building_mixed_identity::opened_file::CurrentWorld world;if(!native_open_runtime::read_current_world(world,e))return false;
    return (world.world_epoch==b.native_epoch&&world.generation==b.native_generation)||fail(e,"General project Bug birth current NativeOrigin differs from saved action Binding");
}
bool capture_ui(void* receiver,void* record,bool& handled,bool& accepted){
    handled=false;accepted=false;auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||r->replay||GetCurrentThreadId()!=r->thread)return true;
    const auto controller=typed_action_runtime::controller_status();if(!controller.value("started",false))return true;
    uintptr_t label=0;std::string command,e;const auto seh=command_label(r->image,record,label);
    if(seh||!string(*r,label,command,e)){handled=true;rejected(*r,seh?"General project genuine command label SEH="+std::to_string(seh):e);return false;}
    p::Command type;if(command=="StartTech")type=p::Command::Start;else if(command=="CancelTechConfirm")type=p::Command::Cancel;else if(command=="CommitTech")type=p::Command::Commit;else if(command=="SetTechWorkers")type=p::Command::Workers;else return true;
    handled=true;if(r->pending||r->presentations.size()>=a::history_limit){rejected(*r,"General project retained UI intent capacity reached");return false;}
    Pending pending;pending.intent.command=type;pending.window=uintptr_t(receiver);int32_t tokens=-1;
    if(!at(*r,uintptr_t(record),0x40,tokens,e)||tokens!=(type==p::Command::Workers?1:0)||!window(*r,pending.window,pending.tabs,e)||!code(*r,e)||
       !a::selected_session().current(pending.context,e)||!typed_action_runtime::network_binding(pending.context.snapshot_uuid,pending.binding,e)){rejected(*r,e.empty()?"General project genuine UI argument count differs":e);return false;}
    p::Local local;p::State before;a::Ledger money;if(!snapshot(*r,pending.context,pending.binding,local,before,money,e)){rejected(*r,e);return false;}
    if(type==p::Command::Start){uintptr_t source=0;if(!at(*r,pending.window,0x610,source,e)){rejected(*r,e);return false;}const auto found=std::find(local.catalogue.begin(),local.catalogue.end(),source);if(found==local.catalogue.end()){rejected(*r,"General project actual selected UI source is outside owned catalogue");return false;}pending.intent.source=before.catalogue[size_t(found-local.catalogue.begin())].source;}
    else{if(!before.progress.current){rejected(*r,"General project actual current owning project absent");return false;}pending.intent.source=*before.progress.current;}
    if(type==p::Command::Workers){const auto error=command_workers(r->image,record,pending.intent.workers);if(error){rejected(*r,"General project genuine worker value SEH="+std::to_string(error));return false;}}
    if(!p::before(pending.intent,before,e)||!completed(*r,local.lounge,type,e)||(pending.id=uuid(e)).empty()){rejected(*r,e);return false;}
    r->presentations.emplace(pending.id,pending);r->pending=std::move(pending);++r->captures;accepted=true;return true;
}
bool pending_ui(){const auto* r=published.load(std::memory_order_acquire);return r&&(r->pending.has_value()||r->replay.has_value());}
void tick(){auto* r=published.load(std::memory_order_acquire);if(!r||!r->pending)return;const auto pending=*r->pending;r->pending.reset();a::Context actual;a::Binding binding;std::string e;
    if(!a::selected_session().current(actual,e)||actual!=pending.context||!typed_action_runtime::network_binding(actual.snapshot_uuid,binding,e)||binding!=pending.binding||!typed_action_runtime::submit_general_project(pending.id,pending.intent,e)){r->presentations.erase(pending.id);rejected(*r,e.empty()?"General project retained UI Context/Binding changed":e);return;}++r->submitted;
}
bool retire_lobby(std::string& e){auto* r=published.load(std::memory_order_acquire);if(!r){e.clear();return true;}native::WorldStatus world;if(GetCurrentThreadId()!=r->thread||!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0||r->replay)return fail(e,"General project retirement requires actual idle Title/no replay");r->pending.reset();r->presentations.clear();r->operations.clear();r->cause.clear();e.clear();return true;
}
a::Outcome perform(const a::Binding& b,const a::Request& request,const a::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||request.action!=a::Action::GeneralProject||!p::valid(request.general_project)||r->replay){e="General project runtime/action unavailable";return a::Outcome::RejectedNoEffect;}
    if(r->operations.contains(request.action_uuid)){e="General project original token already attempted; no replay";return a::Outcome::Unknown;}
    if(r->operations.size()>=a::history_limit){auto oldest=r->operations.end();for(auto i=r->operations.begin();i!=r->operations.end();++i)if(i->second.retired&&(oldest==r->operations.end()||i->second.request.host_revision<oldest->second.request.host_revision))oldest=i;
        if(oldest==r->operations.end()){e="General project retained unretired operation capacity reached";return a::Outcome::RejectedNoEffect;}r->operations.erase(oldest);}
    a::Context c;const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid||!a::selected_session().current(c,e)||c.snapshot_uuid!=request.snapshot_uuid||(c.role==a::Role::Client)!=(authority!=nullptr)||!code(*r,e)){if(e.empty())e="General project exact pending Controller/role/Context unavailable";return a::Outcome::RejectedNoEffect;}
    const auto& intent=request.general_project;if(intent.command==p::Command::Commit&&!bug_authority::native_code(e))return a::Outcome::RejectedNoEffect;
    p::Local local;p::State before;a::Ledger money;p::Proof before_proof;
    if(!snapshot(*r,c,b,local,before,money,e)||!p::before(intent,before,e)||!completed(*r,local.lounge,intent.command,e)||!p::proof(intent.source,before,hash,before_proof,e))return a::Outcome::RejectedNoEffect;
    const auto source=local.sources.find(p::key(intent.source));if(source==local.sources.end()){e="General project exact native source pointer missing";return a::Outcome::RejectedNoEffect;}
    if(authority){if(!authority->general_project_before||!authority->general_project_after||money!=authority->before){e="General project Client complete immediate Ledger/Host proof missing";return a::Outcome::RejectedNoEffect;}
        auto compared=before_proof;compared.random=authority->general_project_before->random;
        if(compared!=*authority->general_project_before){e="General project Client complete current ownership/source differs from Host";return a::Outcome::RejectedNoEffect;}
    }
    auto& op=r->operations.try_emplace(request.action_uuid).first->second;op.context=c;op.binding=b;op.request=request;op.local_random=before.random;
    const auto unknown=[&](const std::string& cause){e=op.cause=cause.empty()?"General project original failed without cause":cause;return a::Outcome::Unknown;};
    if(authority&&before.random!=authority->general_project_before->random){op.attempted=true;const auto exception=seed(local.random,before.random,authority->general_project_before->random);
        if(exception)return unknown("General project exact Host RNG adoption SEH="+std::to_string(exception));
        p::Local seeded;p::State actual;a::Ledger cash;if(!snapshot(*r,c,b,seeded,actual,cash,e))return unknown(e);
        auto expected=before;expected.random=authority->general_project_before->random;if(local!=seeded||actual!=expected||cash!=money)return unknown("General project Host RNG adoption changed other actual state");before=std::move(actual);before_proof.random=before.random;
    }
    op.evidence.before=money;op.evidence.general_project_before=before_proof;
    r->replay=Replay{request.action_uuid,c,b,intent,source->second,local.lounge,local.software};op.attempted=true;bool accepted=false;uint32_t exception=0;
    {live::apply_timing::PageReadScope original(live::apply_timing::PageBoundary::OriginalCall);exception=call(r->image,intent.command,local.lounge,source->second,intent.workers,op.calls,accepted);}
    const auto origin_cause=r->replay->cause;const auto expected_entry=intent.command==p::Command::Start&&intent.source.kind==2?3u:1u;op.native_entered=intent.command==p::Command::Cancel||r->replay->entered==expected_entry;r->replay.reset();op.returned=!exception;
    if(!origin_cause.empty()||!op.native_entered)return unknown(origin_cause.empty()?"General project private original entry not admitted":origin_cause);
    if(exception||!accepted)return unknown("General project original transaction failed/returned false SEH="+std::to_string(exception)+"; no replay");
    const uint32_t expected_calls=intent.command==p::Command::Start?3:intent.command==p::Command::Cancel?4:intent.command==p::Command::Commit?8:16;
    if(op.calls!=expected_calls)return unknown("General project original returned-call mask differs");
    p::Local after_local;p::State after;p::Proof after_proof;
    if(!snapshot(*r,c,b,after_local,after,op.evidence.after,e)||!p::transition(intent,before,after,e)||!p::proof(intent.source,after,hash,after_proof,e))return unknown(e);
    if(local.lounge!=after_local.lounge||local.software!=after_local.software||local.tree!=after_local.tree||local.library!=after_local.library||local.store!=after_local.store||local.player!=after_local.player||local.catalogue!=after_local.catalogue||local.techs!=after_local.techs||local.records!=after_local.records||local.packs!=after_local.packs||local.sources!=after_local.sources||local.random!=after_local.random)return unknown("General project original changed actual native owner/catalogue identity");
    if(intent.command==p::Command::Start){if(!after_local.current||(local.current&&after_local.current!=local.current))return unknown("General project original current clone ownership differs");}
    else if(intent.command==p::Command::Workers){if(after_local.current!=local.current)return unknown("General project worker setter replaced native project owner");}
    else if(after_local.current)return unknown("General project owning destructor did not clear native project");
    op.evidence.general_project_after=after_proof;
    if(!p::effect(intent,before_proof,after_proof,e))return unknown(e);
    if(intent.command!=p::Command::Start&&op.evidence.before!=op.evidence.after)return unknown("General project non-purchase command changed complete Ledger");
    if(authority&&(op.evidence.before!=authority->before||op.evidence.after!=authority->after||before_proof!=*authority->general_project_before||after_proof!=*authority->general_project_after))return unknown("General project complete actual original result/RNG/Ledger differs from Host");
    op.confirmed=true;e.clear();return a::Outcome::Confirmed;
}
bool observe(const a::Binding& b,const a::Request& q,a::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r)return fail(e,"General project owner absent");const auto found=r->operations.find(q.action_uuid);
    if(found==r->operations.end()||found->second.binding!=b||found->second.request!=q||!found->second.confirmed)return fail(e,"General project exact confirmed original receipt absent");const auto& op=found->second;
    p::Local local;p::State current;p::Proof proof;a::Ledger money;
    if(!snapshot(*r,op.context,b,local,current,money,e)||!p::proof(q.general_project.source,current,hash,proof,e))return false;
    const auto& after=*op.evidence.general_project_after;
    if(proof.catalogue_sha256!=after.catalogue_sha256||proof.ownership_sha256!=after.ownership_sha256||proof.progress.current!=after.progress.current)return fail(e,"General project confirmed native source/complete ownership changed before retirement");
    out=op.evidence;out.retirement_confirmed=op.retired;e.clear();return true;
}
bool retire(const a::Binding& b,const a::Request& q,std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r)return fail(e,"General project retirement owner absent");
    auto found=r->operations.find(q.action_uuid);if(found==r->operations.end()||found->second.binding!=b||found->second.request!=q||!found->second.confirmed)return fail(e,"General project exact confirmed retirement receipt absent");
    auto actual=std::make_unique<a::Evidence>();if(!observe(b,q,*actual,e)||!presentation(*r,found->second,e)){found->second.cause=e;return false;}found->second.retired=true;e.clear();return true;
}
nlohmann::json status(){const auto* r=published.load(std::memory_order_acquire);if(!r)return {{"ready",false}};auto operations=nlohmann::json::object();
    for(const auto& [id,op]:r->operations)operations[id]={{"attempted",op.attempted},{"native_entered",op.native_entered},{"returned",op.returned},{"confirmed",op.confirmed},{"retired",op.retired},{"original_calls",op.calls},{"ui_attempted",op.ui_attempted},{"ui_returned",op.ui_returned},{"ui_closed",op.ui_closed},{"cause",op.cause},{"intent",p::encode(op.request.general_project)},{"local_before_rng",op.local_random},{"before",op.evidence.general_project_before?p::encode(*op.evidence.general_project_before):nlohmann::json(nullptr)},{"after",op.evidence.general_project_after?p::encode(*op.evidence.general_project_after):nlohmann::json(nullptr)}};
    return {{"ready",r->ready},{"pending",r->pending.has_value()},{"captures",r->captures},{"submitted",r->submitted},{"rejected",r->rejected},{"cause",r->cause},{"operations",std::move(operations)}};
}
bool release_reference_keys(const a::Context& c,const a::Binding& b,std::map<uintptr_t,std::string>& out,std::string& e){
    out.clear();auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready)return fail(e,"Release reference catalogue requires ready General project owner");
    p::ReferenceOwners local;if(!reference_snapshot(*r,c,b,local,e))return false;
    const auto add=[&](uintptr_t object,const char* family,const std::string& key){
        return out.emplace(object,nlohmann::json::array({family,key}).dump()).second||fail(e,"Release complete Tech/Art canonical catalogue aliases a current pointer");
    };
    for(const auto& [key,object]:local.tech_keys)if(!add(object,"tech",key))return false;
    for(const auto& [key,object]:local.pack_keys)if(!add(object,"art",key))return false;
    return add(local.software,"software","");
}
bool capture_progress(const a::Context& c,const a::Binding& b,p::Progress& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready)return fail(e,"General project Frame provider not enabled");uintptr_t lounge=0,current=0,after_lounge=0,after_current=0;p::Progress first;
    if(!progress_once(*r,c,b,first,lounge,current,e)||!progress_once(*r,c,b,out,after_lounge,after_current,e))return false;
    return (first==out&&lounge==after_lounge&&current==after_current)||fail(e,"General project Frame complete source/progress changed during double read");
}
bool apply_progress(const a::Context& c,const a::Binding& b,const p::Progress& target,bool& attempted,uint32_t& attempted_stores,uint32_t& stores,std::string& e){
    attempted=false;attempted_stores=stores=0;auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||c.role!=a::Role::Client||!p::valid(target))return fail(e,"General project Client Frame provider/target unavailable");
    p::Progress before;uintptr_t lounge=0,current=0;if(!progress_once(*r,c,b,before,lounge,current,e))return false;
    if(before.current!=target.current)return fail(e,"General project Frame owning membership requires its ordered original transaction");
    const std::array<size_t,5> offsets{0xb0,0xb4,0xb8,0xbc,0xc4};
    const std::array<int32_t,5> a{std::bit_cast<int32_t>(before.work),before.lines,before.bugs_total,before.bugs_known,before.workers},v{std::bit_cast<int32_t>(target.work),target.lines,target.bugs_total,target.bugs_known,target.workers};
    auto expected=before;
    for(size_t i=0;i<offsets.size();++i){if(a[i]==v[i])continue;p::Progress actual;uintptr_t owner=0,clone=0;if(!progress_once(*r,c,b,actual,owner,clone,e,false)||owner!=lounge||clone!=current||actual!=expected)return fail(e,e.empty()?"General project Frame exact current clone/progress changed before CAS":e);
        attempted=true;++attempted_stores;if(!live::compare_fixed_i32(lounge+offsets[i],a[i],v[i],e))return false;++stores;
        if(i==0)expected.work=target.work;else if(i==1)expected.lines=target.lines;else if(i==2)expected.bugs_total=target.bugs_total;else if(i==3)expected.bugs_known=target.bugs_known;else expected.workers=target.workers;
    }
    p::Progress after;uintptr_t owner=0,clone=0;if(!progress_once(*r,c,b,after,owner,clone,e))return false;
    return (after==target&&owner==lounge&&clone==current)||fail(e,"General project complete actual Client Frame progress differs");
}
}
