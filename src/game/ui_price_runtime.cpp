#include "ui_price_runtime.h"
#include "typed_action_runtime.h"
#include "live_host_state/native_port.h"
#include "session_provider.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/core/logger.h"
#include <Windows.h>
#include "mt2mp/hooks/minhook_backend.h"
#include <bcrypt.h>
#include <intrin.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>
#include <optional>

namespace mt2mp::game::typed_action_runtime {
bool network_binding(const std::string&,action_session::Binding&,std::string&);
bool submit_prices(const std::string&,const action_session::PriceIntent&,std::string&);
}
namespace mt2mp::game::ui_price_runtime {
namespace {
namespace action=action_session;
using SetFee=void(__fastcall*)(void*,int32_t);
struct Pending {
    action::PriceIntent intent;
    action::Binding binding;
    std::string snapshot;
    uint64_t epoch=0;
};
struct Operation {
    action::Binding binding;
    action::Request request;
    action::Evidence evidence;
    uint8_t attempted=0,returned=0;
    bool confirmed=false,retired=false;
    std::string cause;
};
struct UiAttempt {
    int32_t purchase=0,subscription=0;
    uint64_t captured_count=0;
    bool attempted=false,returned=false,accepted=false,captured=false,submitted=false;
    std::string action_uuid,cause;
};
struct Runtime {
    uint32_t thread=GetCurrentThreadId();
    uintptr_t image=0;
    bool install_attempted=false;
    std::string install_cause;
    std::atomic<bool> ready{false};
    std::atomic<SetFee> purchase{nullptr},subscription{nullptr};
    placement_ui_probe::ProcessMemory memory;
    std::optional<Pending> pending;
    std::map<std::string,Operation> operations;
    std::map<std::string,UiAttempt> ui_attempts;
    std::string diagnostic_pending;
    uint64_t captured=0,submitted=0,rejected=0,forwarded=0,skipped=0,coalesced=0;
    std::string last_action,cause;
};
std::atomic<Runtime*> published{nullptr};
bool fail(std::string& e,const std::string& why){e=why;return false;}
template<class T>bool at(Runtime& r,uintptr_t object,size_t offset,T& value,std::string& e){
    if(!object||object>UINTPTR_MAX-offset)return fail(e,"price native UI address invalid");
    return r.memory.copy(object+offset,&value,sizeof value,e);
}
struct NativeString {const char* data;size_t length;unsigned char rest[40]{};};
static_assert(sizeof(NativeString)==0x38);
bool native_text(Runtime& r,uintptr_t object,std::string& value,std::string& e){
    NativeString native{};if(!at(r,object,0,native,e)||!native.data||native.length>32)
        return fail(e,e.empty()?"price native pane value length invalid":e);
    value.resize(native.length);
    return !native.length||r.memory.copy(uintptr_t(native.data),value.data(),native.length,e);
}
uint32_t find_window(uintptr_t image,const char* id,void*& window,bool& constructed,bool& cleaned){
    alignas(8) unsigned char name[32]{};uint32_t seh=0;
    __try {
        reinterpret_cast<void(*)(void*,const char*)>(image+0x49e090)(name,id);constructed=true;
        window=reinterpret_cast<void*(*)(const void*)>(image+0x81aa90)(name);
    }__except(EXCEPTION_EXECUTE_HANDLER){seh=GetExceptionCode();}
    if(constructed){__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(name);cleaned=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){if(!seh)seh=GetExceptionCode();}}
    return seh;
}
uint32_t find_property_pane(uintptr_t image,void* window,const char* id,void*& pane,bool& constructed,bool& cleaned){
    alignas(8) unsigned char name[32]{};uint32_t seh=0;
    __try {
        reinterpret_cast<void(*)(void*,const char*)>(image+0x49e090)(name,id);constructed=true;
        pane=reinterpret_cast<void*(*)(void*,const void*)>(image+0xc360e0)(window,name);
    }__except(EXCEPTION_EXECUTE_HANDLER){seh=GetExceptionCode();}
    if(constructed){__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(name);cleaned=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){if(!seh)seh=GetExceptionCode();}}
    return seh;
}
uint32_t registry_mutex(uintptr_t image,bool lock) noexcept {
    __try {
        reinterpret_cast<void(*)(void*)>(image+(lock?0xa0b0b0:0xa0b0c0))(
            reinterpret_cast<void*>(image+0x1a3c440));
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool registered_overview(Runtime& r,void* window,const char* id,std::string& e){
    const size_t length=std::strlen(id);
    if(!window||length==0||length>31)return fail(e,"price Overview registry identity invalid");
    const auto locked=registry_mutex(r.image,true);
    if(locked)return fail(e,"price WindowRegistry Lock SEH="+std::to_string(locked));
    uintptr_t windows=0;int32_t count=0;int matches=0,owners=0;bool valid=true;
    if(!at(r,r.image,0x1a3c478,windows,e)||!at(r,r.image,0x1a3c480,count,e)||
       count<1||count>4096||!windows){
        valid=false;if(e.empty())e="price WindowRegistry global array/count invalid";
    }
    for(int32_t i=0;valid&&i<count;++i){
        uintptr_t item=0,data=0;size_t name_length=0;
        if(!at(r,windows,size_t(i)*8,item,e)||!item){valid=false;break;}
        if(!at(r,item,0x1b8,name_length,e)){valid=false;break;}
        if(name_length!=length)continue;
        char name[32]{};
        if(!at(r,item,0x1b0,data,e)||!data||!r.memory.copy(data,name,length,e)){
            valid=false;break;
        }
        if(std::memcmp(name,id,length)==0){++matches;if(item==uintptr_t(window))++owners;}
    }
    const auto unlocked=registry_mutex(r.image,false);
    if(unlocked)return fail(e,"price WindowRegistry Unlock SEH="+std::to_string(unlocked));
    if(!valid)return fail(e,e.empty()?"price WindowRegistry entry unreadable":e);
    if(matches!=1||owners!=1)
        return fail(e,"price Overview WindowRegistry name/pointer not unique: names="+
                      std::to_string(matches)+" owners="+std::to_string(owners));
    e.clear();return true;
}
struct UiCall {bool value1=false,value2=false,record=false,called=false,returned=false,accepted=false,cleaned=false;};
uint32_t invoke_ui(uintptr_t image,void* window,void* purchase,void* subscription,
                   const NativeString* first,const NativeString* second,void* record,UiCall& receipt){
    uint32_t seh=0;
    __try {
        reinterpret_cast<void(*)(void*,const void*)>(image+0x785290)(purchase,first);receipt.value1=true;
        reinterpret_cast<void(*)(void*,const void*)>(image+0x785290)(subscription,second);receipt.value2=true;
        reinterpret_cast<void(*)(void*,const char*)>(image+0x967370)(record,"UpdateSaleValues");receipt.record=true;
        receipt.called=true;
        receipt.accepted=reinterpret_cast<bool(*)(void*,void*)>(image+0x76bd40)(window,record);
        receipt.returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){seh=GetExceptionCode();}
    if(receipt.record){__try{reinterpret_cast<void(*)(void*)>(image+0x9676a0)(record);receipt.cleaned=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){if(!seh)seh=GetExceptionCode();}}
    return seh;
}
uint32_t show_window(uintptr_t image,void* window,bool visible){
    __try{reinterpret_cast<void(*)(void*,bool,bool)>(image+0x81ec20)(window,visible,false);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
void reject(Runtime& r,const std::string& e){++r.rejected;if(r.cause!=e){r.cause=e;LOG_ERROR("[UIPrice] Rejected cause={}",e);}}
bool read(const action::Binding& binding,const std::string& snapshot,action::PriceState& price,
          action::Ledger& money,std::string& e) {
    auto* r=published.load(std::memory_order_acquire);
    if(!r||GetCurrentThreadId()!=r->thread)return fail(e,"price read requires installed Mainthread owner");
    action::Context context;
    if(!action::selected_session().current(context,e)||context.snapshot_uuid!=snapshot)return fail(e,e.empty()?"price actual Session/snapshot differs":e);
    live_host_state::NativePort reader(context,binding,nullptr);live_host_state::Pin first,second;
    action::PriceState repeated;
    if(!reader.read(first,e)||!r->memory.copy(first.business+0x40,&price.purchase,sizeof price.purchase,e)||
       !r->memory.copy(first.business+0x44,&price.subscription,sizeof price.subscription,e)||
       !reader.read(second,e)||
       !r->memory.copy(second.business+0x40,&repeated.purchase,sizeof repeated.purchase,e)||
       !r->memory.copy(second.business+0x44,&repeated.subscription,sizeof repeated.subscription,e)||
       first!=second||price!=repeated)return fail(e,e.empty()?"price native Business owner, fee or Ledger changed during readback":e);
    money=live_host_state::ledger(first.values);e.clear();return true;
}
std::string uuid(std::string& e) {
    unsigned char bytes[16];const auto result=BCryptGenRandom(nullptr,bytes,sizeof bytes,BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(result){e="price UUID NTSTATUS="+std::to_string(result);return {};}
    bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;char out[37];
    std::snprintf(out,sizeof out,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
    e.clear();return out;
}
void capture(uint8_t field,void* receiver,int32_t value,uintptr_t caller) {
    auto* r=published.load(std::memory_order_acquire);
    const auto original=(field==1?r->purchase:r->subscription).load(std::memory_order_acquire);
    const uintptr_t expected=r->image+(field==1?0x76c085:0x76c101);
    if(caller!=expected||!net::session_wire::connected()){++r->forwarded;original(receiver,value);return;}
    std::string e;const auto controller=typed_action_runtime::status();
    if(GetCurrentThreadId()!=r->thread||!r->ready.load(std::memory_order_acquire)||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)) {
        reject(*r,"price UI requires ready idle bound Controller");return;
    }
    action::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!typed_action_runtime::network_binding(snapshot,binding,e)){reject(*r,e);return;}
    action::Context context;
    if(!action::selected_session().current(context,e)){reject(*r,e);return;}
    live_host_state::NativePort reader(context,binding,nullptr);live_host_state::Pin owner;
    if(!reader.read(owner,e)||uintptr_t(receiver)!=owner.business){reject(*r,e.empty()?"price UI Business receiver differs from bound owner":e);return;}
    if(!r->pending)r->pending=Pending{{},binding,snapshot,net::current_world_epoch()};
    auto& pending=*r->pending;
    if(pending.binding!=binding||pending.snapshot!=snapshot||pending.epoch!=net::current_world_epoch()){
        reject(*r,"price pending UI command changed bound World or snapshot");return;
    }
    // The original setter only stores the value, so later UI input in this frame replaces an unsent value.
    if(pending.intent.fields&field)++r->coalesced;
    pending.intent.fields|=field;
    if(field==1)pending.intent.purchase=value;else pending.intent.subscription=value;
    ++r->captured;
}
void purchase_entry(void* receiver,int32_t value){capture(1,receiver,value,uintptr_t(_ReturnAddress()));}
void subscription_entry(void* receiver,int32_t value){capture(2,receiver,value,uintptr_t(_ReturnAddress()));}
uint32_t call(SetFee fn,void* business,int32_t value) noexcept {
    __try{fn(business,value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool enable(std::string& e) {
    auto* r=published.load(std::memory_order_acquire);
    if(!r){r=new Runtime;published.store(r,std::memory_order_release);}
    switch(detail::hook_gate(r->install_attempted,r->ready.load(std::memory_order_acquire),
                             GetCurrentThreadId()==r->thread)){
    case detail::HookGate::WrongThread:return fail(e,"price hooks require bound Mainthread");
    case detail::HookGate::Reuse:
        if(!r->purchase.load(std::memory_order_acquire)||!r->subscription.load(std::memory_order_acquire))
            return fail(e,"price ready hooks lack original setter trampolines");
        e.clear();return true;
    case detail::HookGate::FailedAttempt:
        return fail(e,"price hook installation failed earlier; no retry: "+r->install_cause);
    case detail::HookGate::FirstInstall:break;
    }
    r->install_attempted=true;uint32_t thread=0;std::string sha;
    struct KeepInstallCause {Runtime& runtime;std::string& error;
        ~KeepInstallCause(){if(!error.empty())runtime.install_cause=error;}
    } keep{*r,e};
    if(!native::read_resolver_receipt(r->image,thread,sha,e)||
       sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"||thread!=r->thread)
        return fail(e,"price hooks exact EXE/Mainthread differs: "+e);
    constexpr unsigned char purchase_body[]{0x89,0x51,0x40,0xc3};
    constexpr unsigned char subscription_body[]{0x89,0x51,0x44,0xc3};
    constexpr unsigned char purchase_call[]{0xe8,0x5b,0x8c,0x8e,0xff};
    constexpr unsigned char subscription_call[]{0xe8,0xef,0x8b,0x8e,0xff};
    const struct {uintptr_t address;const unsigned char* expected;size_t size;} checks[]{
        {0x54ce0,purchase_body,sizeof purchase_body},{0x54cf0,subscription_body,sizeof subscription_body},
        {0x76c080,purchase_call,sizeof purchase_call},{0x76c0fc,subscription_call,sizeof subscription_call}};
    for(const auto& item:checks){unsigned char bytes[5]{};
        if(!r->memory.copy(r->image+item.address,bytes,item.size,e)||std::memcmp(bytes,item.expected,item.size))
            return fail(e,e.empty()?"price exact native setter/UI callsite bytes differ":e);
    }
    SetFee purchase=nullptr,subscription=nullptr;
    const auto first=MH_CreateHook(reinterpret_cast<void*>(r->image+0x54ce0),reinterpret_cast<void*>(&purchase_entry),reinterpret_cast<void**>(&purchase));
    if(first!=MH_OK||!purchase)return fail(e,"price Purchase MinHook create status="+std::to_string(first));
    r->purchase.store(purchase,std::memory_order_release);
    const auto second=MH_CreateHook(reinterpret_cast<void*>(r->image+0x54cf0),reinterpret_cast<void*>(&subscription_entry),reinterpret_cast<void**>(&subscription));
    if(second!=MH_OK||!subscription)return fail(e,"price Subscription MinHook create status="+std::to_string(second));
    r->subscription.store(subscription,std::memory_order_release);
    const auto enabled_purchase=MH_EnableHook(reinterpret_cast<void*>(r->image+0x54ce0));
    if(enabled_purchase!=MH_OK)return fail(e,"price Purchase MinHook enable status="+std::to_string(enabled_purchase));
    const auto enabled_subscription=MH_EnableHook(reinterpret_cast<void*>(r->image+0x54cf0));
    if(enabled_subscription!=MH_OK)return fail(e,"price Subscription MinHook enable status="+std::to_string(enabled_subscription));
    r->ready.store(true,std::memory_order_release);r->install_cause.clear();e.clear();return true;
}
void tick() {
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||!r->pending)return;
    auto pending=std::move(*r->pending);r->pending.reset();std::string e;
    UiAttempt* ui=nullptr;
    if(!r->diagnostic_pending.empty()){
        ui=&r->ui_attempts.at(r->diagnostic_pending);r->diagnostic_pending.clear();
    }
    if(pending.snapshot!=net::current_snapshot_id()||pending.epoch!=net::current_world_epoch()){
        e="price pending UI command World or snapshot changed";if(ui)ui->cause=e;reject(*r,e);return;
    }
    action::PriceState before;action::Ledger money;
    if(!read(pending.binding,pending.snapshot,before,money,e)){if(ui)ui->cause=e;reject(*r,e);return;}
    const auto desired=action::PriceState{(pending.intent.fields&1)?pending.intent.purchase:before.purchase,
                                          (pending.intent.fields&2)?pending.intent.subscription:before.subscription};
    if(desired==before){++r->skipped;if(ui)ui->cause="price UI desired values unchanged";return;}
    auto id=uuid(e);if(id.empty()){if(ui)ui->cause=e;reject(*r,e);return;}
    r->last_action=id;
    if(ui)ui->action_uuid=id;
    if(!typed_action_runtime::submit_prices(id,pending.intent,e)){if(ui)ui->cause=e;reject(*r,e);return;}
    if(ui)ui->submitted=true;
    ++r->submitted;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,
                        const action::Evidence* authority,std::string& e) {
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||request.action!=action::Action::SetPrices){e="price native runtime unavailable or action differs";return action::Outcome::RejectedNoEffect;}
    if(r->operations.contains(request.action_uuid)){e="price action token already attempted; no replay";return action::Outcome::Unknown;}
    const auto controller=typed_action_runtime::status();
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){
        e="price action lacks exact pending Controller request";return action::Outcome::RejectedNoEffect;
    }
    action::PriceState before;action::Ledger money;
    if(!read(binding,request.snapshot_uuid,before,money,e))return action::Outcome::RejectedNoEffect;
    if(authority&&(before!=authority->price_before||money!=authority->before)){
        e="price Client full native before state differs from Host";return action::Outcome::RejectedNoEffect;
    }
    const auto expected=action::PriceState{(request.price.fields&1)?request.price.purchase:before.purchase,
                                            (request.price.fields&2)?request.price.subscription:before.subscription};
    if(expected==before||(authority&&(expected!=authority->price_after||authority->after!=money))){
        e="price requested/Host result has no exact native change";return action::Outcome::RejectedNoEffect;
    }
    action::Context context;if(!action::selected_session().current(context,e))return action::Outcome::RejectedNoEffect;
    live_host_state::NativePort reader(context,binding,nullptr);live_host_state::Pin owner;
    if(!reader.read(owner,e))return action::Outcome::RejectedNoEffect;
    auto& operation=r->operations.emplace(request.action_uuid,Operation{binding,request}).first->second;
    operation.evidence.before=money;operation.evidence.price_before=before;
    if(request.price.fields&1){operation.attempted|=1;
        const auto seh=call(r->purchase.load(std::memory_order_acquire),reinterpret_cast<void*>(owner.business),request.price.purchase);
        if(seh){e=operation.cause="price Purchase original SEH="+std::to_string(seh);return action::Outcome::Unknown;}operation.returned|=1;
    }
    if(request.price.fields&2){operation.attempted|=2;
        const auto seh=call(r->subscription.load(std::memory_order_acquire),reinterpret_cast<void*>(owner.business),request.price.subscription);
        if(seh){e=operation.cause="price Subscription original SEH="+std::to_string(seh);return action::Outcome::Unknown;}operation.returned|=2;
    }
    if(!read(binding,request.snapshot_uuid,operation.evidence.price_after,operation.evidence.after,e)){
        operation.cause=e;return action::Outcome::Unknown;
    }
    if(operation.returned!=request.price.fields||operation.evidence.price_after!=expected||
       operation.evidence.after!=money||
       (authority&&(operation.evidence.price_before!=authority->price_before||
                   operation.evidence.price_after!=authority->price_after||
                   operation.evidence.before!=authority->before||operation.evidence.after!=authority->after))) {
        e=operation.cause="price original result, full Ledger or Host evidence differs";return action::Outcome::Unknown;
    }
    operation.confirmed=true;e.clear();return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e) {
    auto* r=published.load(std::memory_order_acquire);if(!r)return fail(e,"price observation runtime absent");
    const auto found=r->operations.find(request.action_uuid);
    if(found==r->operations.end()||found->second.binding!=binding||found->second.request!=request||
       !found->second.confirmed)return fail(e,"price observation lacks exact confirmed operation");
    action::PriceState price;action::Ledger money;
    if(!read(binding,request.snapshot_uuid,price,money,e))return false;
    if(price!=found->second.evidence.price_after||money!=found->second.evidence.after)
        return fail(e,"price native readback or full Ledger changed after result");
    out=found->second.evidence;out.retirement_confirmed=found->second.retired;e.clear();return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& e) {
    action::Evidence actual;if(!observe(binding,request,actual,e))return false;
    auto* r=published.load(std::memory_order_acquire);auto& operation=r->operations.at(request.action_uuid);
    if(operation.retired)return fail(e,"price receipt already retired");
    operation.retired=true;e.clear();return true;
}
bool query(nlohmann::json& out,std::string& e) {
    const auto snapshot=net::current_snapshot_id();action::Binding binding;
    if(!typed_action_runtime::network_binding(snapshot,binding,e))return false;
    action::PriceState price;action::Ledger money;
    if(!read(binding,snapshot,price,money,e))return false;
    out={{"purchase_fee",price.purchase},{"subscription_fee",price.subscription},
        {"snapshot_uuid",snapshot},{"world_epoch",binding.native_epoch},
        {"world_generation",binding.native_generation},
        {"full_ledger",{{"cash",money.cash},{"flat_day",money.flat_day},
            {"record_present",money.record_present},{"record_day",money.record_day},
            {"record_cash",money.record_cash},{"record_income",money.record_income},
            {"record_expense",money.record_expense},{"line_counts",money.line_counts},
            {"line_income",money.line_income}}},
        {"ui_price",status()}};
    e.clear();return true;
}
bool native_ui(const std::string& token,int32_t purchase_fee,int32_t subscription_fee,nlohmann::json& out,std::string& e){
    out=nlohmann::json::object();auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||GetCurrentThreadId()!=r->thread||!net::session_wire::connected())
        return fail(e,"price native UI requires connected Mainthread and enabled hooks");
    if(token.empty()||token.size()>64||token.find('\0')!=std::string::npos||purchase_fee<0||subscription_fee<0)
        return fail(e,"price native UI diagnostic token or positive-integer prices invalid");
    if(r->ui_attempts.contains(token))return fail(e,"price native UI token already attempted; inspect receipt before retry");
    if(r->ui_attempts.size()>=64)return fail(e,"price native UI token capacity reached");
    out["diagnostic_uuid"]=token;out["attempted"]=false;
    const auto controller=typed_action_runtime::status();
    if(!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||r->pending||
       !r->diagnostic_pending.empty())return fail(e,"price native UI requires idle bound Controller");
    const auto snapshot=net::current_snapshot_id();action::Binding binding;
    if(!typed_action_runtime::network_binding(snapshot,binding,e))return false;
    action::PriceState before;action::Ledger ledger;
    if(!read(binding,snapshot,before,ledger,e))return false;
    if(before.purchase==purchase_fee&&before.subscription==subscription_fee)
        return fail(e,"price native UI requested values unchanged");
    uintptr_t game=0,mode=0,active=0,ap=0;
    if(!at(*r,r->image,0x13df8f0,game,e)||!at(*r,game,0,ap,e)||ap!=r->image+0x1743740||
       !at(*r,game,0x358,mode,e)||!at(*r,game,0x28,active,e)||mode!=active||
       !at(*r,mode,0,ap,e)||ap!=r->image+0x16e6730)
        return fail(e,e.empty()?"price native UI active game mode owner differs":e);
    void* window=nullptr;bool constructed=false,cleaned=false;
    const auto lookup=find_window(r->image,"report_overview",window,constructed,cleaned);
    if(lookup||!cleaned||!window)return fail(e,"price actual Overview registry lookup/cleanup failed SEH="+std::to_string(lookup));
    if(!registered_overview(*r,window,"report_overview",e))return false;
    uint8_t visible=0;uintptr_t pane1=0,pane2=0,view1=0,view2=0,owner1=0,owner2=0;
    if(!at(*r,uintptr_t(window),0,ap,e))return false;
    const bool overview_class=ap==r->image+0x17142d0;
    if(!at(*r,uintptr_t(window),0x45f,visible,e))return false;
    out["pane_guard"]={{"overview_class",overview_class},
                       {"overview_vtable_rva",ap>=r->image&&ap-r->image<0x2000000
                           ?int64_t(ap-r->image):int64_t(-1)},
                       {"expected_overview_vtable_rva",0x17142d0},
                       {"visibility_valid",visible<=1}};
    if(!overview_class||visible>1)return fail(e,"price Overview class or visibility differs before UI command");
    if(!at(*r,uintptr_t(window),0x588,pane1,e)||!at(*r,uintptr_t(window),0x598,pane2,e))return false;
    out["pane_guard"]["purchase_field_present"]=pane1!=0;
    out["pane_guard"]["subscription_field_present"]=pane2!=0;
    out["pane_guard"]["fee_fields_distinct"]=pane1!=pane2;
    if(!pane1||!pane2||pane1==pane2)return fail(e,"price Overview original fee fields absent or aliased");
    void* found1=nullptr;void* found2=nullptr;bool made1=false,made2=false,cleaned1=false,cleaned2=false;
    const auto first_lookup=find_property_pane(r->image,window,"sales_purchase_price",found1,made1,cleaned1);
    if(first_lookup||!made1||!cleaned1)
        return fail(e,"price original Purchase FindChild/cleanup SEH="+std::to_string(first_lookup));
    const auto second_lookup=find_property_pane(r->image,window,"sales_subscription_price",found2,made2,cleaned2);
    if(second_lookup||!made2||!cleaned2)
        return fail(e,"price original Subscription FindChild/cleanup SEH="+std::to_string(second_lookup));
    const bool purchase_child=found1==reinterpret_cast<void*>(pane1);
    const bool subscription_child=found2==reinterpret_cast<void*>(pane2);
    out["pane_guard"]["purchase_typed_child"]=purchase_child;
    out["pane_guard"]["subscription_typed_child"]=subscription_child;
    if(!purchase_child||!subscription_child)
        return fail(e,"price Overview original typed fee child differs before UI command");
    if(!at(*r,pane1,0x278,view1,e)||!at(*r,pane2,0x278,view2,e)||
       !at(*r,pane1,0x280,owner1,e)||!at(*r,pane2,0x280,owner2,e))return false;
    out["pane_guard"]["purchase_layout_view_present"]=view1!=0;
    out["pane_guard"]["subscription_layout_view_present"]=view2!=0;
    out["pane_guard"]["purchase_layout_owner_present"]=owner1!=0;
    out["pane_guard"]["subscription_layout_owner_present"]=owner2!=0;
    out["pane_guard"]["purchase_layout_owner_matches"]=!owner1||owner1==uintptr_t(window);
    out["pane_guard"]["subscription_layout_owner_matches"]=!owner2||owner2==uintptr_t(window);
    if((owner1&&owner1!=uintptr_t(window))||(owner2&&owner2!=uintptr_t(window)))
        return fail(e,"price Overview original typed fee child has different Layout window");
    auto& attempt=r->ui_attempts.emplace(token,UiAttempt{purchase_fee,subscription_fee}).first->second;
    attempt.attempted=true;out["diagnostic_uuid"]=token;out["attempted"]=true;
    out["window_confirmed"]=true;out["initially_visible"]=bool(visible);
    if(!visible){const auto shown=show_window(r->image,window,true);uint8_t now=0;
        if(shown||!at(*r,uintptr_t(window),0x45f,now,e)||now!=1){
            attempt.cause="price Overview native Show/readback failed SEH="+std::to_string(shown);
            out["cause"]=attempt.cause;return fail(e,attempt.cause);
        }
        uintptr_t current1=0,current2=0;
        if(!at(*r,uintptr_t(window),0x588,current1,e)||!at(*r,uintptr_t(window),0x598,current2,e)||
           current1!=pane1||current2!=pane2){
            const auto hidden=show_window(r->image,window,false);
            attempt.cause="price Overview pane ownership changed after Show hide="+std::to_string(hidden);
            out["cause"]=attempt.cause;return fail(e,attempt.cause);
        }
    }
    const std::string first=std::to_string(purchase_fee),second=std::to_string(subscription_fee);
    const NativeString native1{first.data(),first.size(),{}},native2{second.data(),second.size(),{}};
    alignas(8) unsigned char record[0x78]{};UiCall receipt;const auto captured_before=r->captured;
    const auto seh=invoke_ui(r->image,window,reinterpret_cast<void*>(pane1),reinterpret_cast<void*>(pane2),
                             &native1,&native2,record,receipt);
    attempt.returned=receipt.returned;attempt.accepted=receipt.accepted;
    attempt.captured_count=r->captured-captured_before;
    attempt.captured=attempt.captured_count==2&&r->pending&&
        r->pending->intent==action::PriceIntent{3,purchase_fee,subscription_fee};
    out["values_set"]={{"purchase",receipt.value1},{"subscription",receipt.value2}};
    out["command_attempted"]=receipt.called;out["command_returned"]=receipt.returned;
    out["command_accepted"]=receipt.accepted;out["record_cleaned"]=receipt.cleaned;
    out["captured_count"]=attempt.captured_count;out["captured_once_per_field"]=attempt.captured;
    out["native_exception"]=seh;
    uint32_t hidden=0;if(!visible)hidden=show_window(r->image,window,false);
    uint8_t final_visible=255;const bool visibility_read=at(*r,uintptr_t(window),0x45f,final_visible,e);
    out["visibility_restored"]=visibility_read&&!hidden&&final_visible==visible;
    if(seh||hidden||!receipt.value1||!receipt.value2||!receipt.called||!receipt.returned||
       !receipt.accepted||!receipt.cleaned||!visibility_read||final_visible!=visible){
        r->pending.reset();
        attempt.cause="price Overview original UI command/cleanup failed SEH="+std::to_string(seh)+
            " hide="+std::to_string(hidden);out["cause"]=attempt.cause;return fail(e,attempt.cause);
    }
    std::string observed1,observed2;
    if(!native_text(*r,pane1+0x350,observed1,e)||!native_text(*r,pane2+0x350,observed2,e)){
        r->pending.reset();attempt.cause=e;out["cause"]=e;return false;
    }
    out["pane_values_confirmed"]=observed1==first&&observed2==second;
    if(!attempt.captured||observed1!=first||observed2!=second){
        r->pending.reset();
        attempt.cause="price Overview values or two setter captures differ";out["cause"]=attempt.cause;return fail(e,attempt.cause);
    }
    action::PriceState after;action::Ledger after_ledger;
    if(!read(binding,snapshot,after,after_ledger,e)){r->pending.reset();attempt.cause=e;out["cause"]=e;return false;}
    out["native_state_unchanged"]=after==before&&after_ledger==ledger;
    if(after!=before||after_ledger!=ledger){
        r->pending.reset();
        attempt.cause="price Overview changed fees or Ledger before Host authority";out["cause"]=attempt.cause;return fail(e,attempt.cause);
    }
    r->diagnostic_pending=token;out["pending_intent"]=true;e.clear();return true;
}
nlohmann::json status() {
    auto* r=published.load(std::memory_order_acquire);if(!r)return {{"initialized",false}};
    if(GetCurrentThreadId()!=r->thread)return {{"initialized",true},{"error","price status requires Mainthread"}};
    nlohmann::json operations=nlohmann::json::array();
    for(const auto& [id,operation]:r->operations)operations.push_back({
        {"action_uuid",id},{"fields",operation.request.price.fields},
        {"attempted",operation.attempted},{"returned",operation.returned},
        {"confirmed",operation.confirmed},{"retired",operation.retired},
        {"before_purchase",operation.evidence.price_before.purchase},
        {"after_purchase",operation.evidence.price_after.purchase},
        {"before_subscription",operation.evidence.price_before.subscription},
        {"after_subscription",operation.evidence.price_after.subscription},
        {"ledger_unchanged",operation.evidence.before==operation.evidence.after},
        {"cause",operation.cause}});
    nlohmann::json ui_attempts=nlohmann::json::array();
    for(const auto& [token,attempt]:r->ui_attempts){
        const auto found=r->operations.find(attempt.action_uuid);
        const bool confirmed=attempt.submitted&&found!=r->operations.end()&&found->second.confirmed&&found->second.retired;
        ui_attempts.push_back({{"diagnostic_uuid",token},{"purchase_fee",attempt.purchase},
            {"subscription_fee",attempt.subscription},{"attempted",attempt.attempted},
            {"returned",attempt.returned},{"accepted",attempt.accepted},{"captured",attempt.captured},
            {"captured_count",attempt.captured_count},
            {"submitted",attempt.submitted},{"confirmed",confirmed},{"action_uuid",attempt.action_uuid},
            {"cause",attempt.cause}});
    }
    return {{"initialized",true},{"enabled",bool(r->ready)},{"pending_ui",r->pending.has_value()},
        {"install_attempted",r->install_attempted},{"install_cause",r->install_cause},
        {"captured",r->captured},{"submitted",r->submitted},{"rejected",r->rejected},
        {"forwarded",r->forwarded},{"skipped",r->skipped},{"coalesced",r->coalesced},
        {"last_action_uuid",r->last_action},
        {"cause",r->cause},{"operations",operations},{"ui_attempts",ui_attempts}};
}
}
