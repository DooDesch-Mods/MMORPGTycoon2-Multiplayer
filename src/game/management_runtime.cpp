#include "district_edit_runtime.h"
#include "management_runtime.h"
#include "management_live_port.h"
#include "dungeon_gizmo_runtime.h"
#include "world_zone_runtime.h"
#include "toon_thought_lifetime.h"
#include "character_calendar_native.h"
#include "service_live_reader.h"
#include "general_project_runtime.h"
#include "subscriber_lifecycle/save_scope.h"
#include "gm_ticket_publication.h"
#include "gm_ticket_reference.h"
#include "gm_ticket_native.h"
#include "gm_policy_native.h"
#include "game_rules_native.h"
#include "intervention_census.h"
#include "intervention_records.h"
#include "intervention_native.h"
#include "intervention_actor_live.h"
#include "intervention_actor_native.h"
#include "control_input/control_runtime.h"
#include "tutorial_replication/native.h"
#include <algorithm>
#include <cmath>
#include "release_ui_owner.h"
#include <bit>
#include <functional>
#include <cstring>
#include "typed_action_runtime.h"
#include "session_provider.h"
#include "native_open_runtime.h"
#include "live_host_state/native_port.h"
#include "live_host_state/economic_fence.h"
#include "live_host_state/memory_query_timing.h"
#include "placement_ui_probe/ui_probe.h"
#include "marketing_authority/readback.h"
#include "remove_diagnostics.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/hooks/minhook_backend.h"
#include "mt2mp/net/session_wire.h"
#include <windows.h>
#include <bcrypt.h>
#include <intrin.h>
#include <atomic>
#include <map>
#include <optional>
#include <set>
#include <cstdio>

namespace mt2mp::game::management_runtime {
namespace {
namespace a=action_session;namespace live=live_host_state;
using Void=void(*)(void*);using Funds=bool(*)(void*,int32_t);
using Ui=bool(*)(void*,void*);
using GmUpdate=void(*)(void*,float);
struct GmRoute {uintptr_t rva;size_t bytes,stolen;const char* sha;GmUpdate original=nullptr;release_ui_owner::Owner owner;};
using ServicePrice=void(*)(void*,const void*);using ServiceSync=void(*)(void*,bool);
struct ServiceOwner {std::string key;uintptr_t object=0;uint64_t uid=0;service_membership_reader::Local local;a::ServiceMembershipState sampled;building_uptime::Native uptime;bool operator==(const ServiceOwner&)const=default;};
struct Local {std::vector<ServiceOwner> services;std::map<uintptr_t,std::string> service_destinations;live::Pin pin;uintptr_t software=0,release=0,current_release=0,lounge=0,marketing=0,service_building=0,service_object=0;uint64_t service_uid=0;std::vector<uintptr_t> service_rows,gm_rows;std::vector<uint64_t> gm_uids;int32_t lounge_count=0;};
struct Pending {a::Context context;a::Binding binding;a::ManagementIntent intent;std::string id;};
struct Receipt {a::Binding binding;a::Request request;a::Evidence evidence;bool attempted=false,returned=false,confirmed=false,retired=false;std::string cause;};
struct Runtime {
    DWORD thread=GetCurrentThreadId();uintptr_t image=0;bool attempted=false,ready=false,replay=false;
    Void hire=nullptr,fire=nullptr,statistics=nullptr,toggle_release=nullptr;Funds funds=nullptr,purchase_statistic=nullptr;
    Ui overview=nullptr;
    ServicePrice service_price=nullptr;ServiceSync service_sync=nullptr;
    placement_ui_probe::ProcessMemory memory;std::optional<Pending> pending;std::map<std::string,Receipt> operations;
    uint64_t captures=0,submitted=0,rejected=0;std::string cause,install_cause;
    std::array<GmRoute,2> gm_routes{{{0x3adae0,240,5,"9098ff645372542d40e5b75a5490516c5e340d4eb47bc7dbcdd3817deac772c9"},{0x3ad520,1344,11,"5bc044e0425b35c55ddbca5ce914b0994a0341628acf4652dcc5afa88cff711b"}}};
    bool gm_client=false,gm_suspended=true,gm_failed=false;std::string gm_cause;
    uintptr_t gm_root=0;std::optional<a::Context> gm_context;std::optional<a::Binding> gm_binding;
};
std::atomic<Runtime*> published{nullptr};
bool intervention_subscribers(Runtime&,subscriber_intervention::State&,subscriber_intervention::Owners&,std::string&);
bool intervention_adopt_subscribers(Runtime&,const a::Context&,const a::Binding&,const subscriber_intervention::State&,live::ManagementReceipt&,std::string&);
bool intervention_enable(Runtime&,std::string&);bool intervention_pending();void intervention_reset();void intervention_tick(Runtime&);
nlohmann::json intervention_status();
a::Outcome intervention_perform(const a::Binding&,const a::Request&,const a::Evidence*,std::string&);
bool intervention_observe(const a::Binding&,const a::Request&,a::Evidence&,std::string&);bool intervention_retire(const a::Binding&,const a::Request&,std::string&);
bool rules_enable(Runtime&,std::string&);bool rules_pending();void rules_reset();void rules_tick(Runtime&);
bool rules_values(Runtime&,game_rules::State&,game_rules::Pin&,std::string&);bool rules_routes(Runtime&,std::string&);
bool rules_adopt(Runtime&,const game_rules::State&,live::ManagementReceipt&,std::string&);
bool rules_checkbox(Runtime&,uintptr_t,bool,bool&,std::string&);
a::Outcome rules_perform(const a::Binding&,const a::Request&,const a::Evidence*,std::string&);
bool rules_observe(const a::Binding&,const a::Request&,a::Evidence&,std::string&);bool rules_retire(const a::Binding&,const a::Request&,std::string&);
bool policy_enable(Runtime&,std::string&);bool policy_pending();void policy_reset();void policy_tick(Runtime&);
bool policy_values(Runtime&,gm_policy::State&,uintptr_t&,std::string&);bool policy_route_code(Runtime&,std::string&);
bool policy_adopt(Runtime&,const gm_policy::State&,live::ManagementReceipt&,std::string&);
a::Outcome policy_perform(const a::Binding&,const a::Request&,const a::Evidence*,std::string&);
bool policy_observe(const a::Binding&,const a::Request&,a::Evidence&,std::string&);bool policy_retire(const a::Binding&,const a::Request&,std::string&);
bool fail(std::string& e,const std::string& s){e=s;return false;}
template<class T>bool at(Runtime& r,uintptr_t p,size_t offset,T& value,std::string& e){
    if(!p||p>UINTPTR_MAX-offset)return fail(e,"Management native field extent invalid");return r.memory.copy(p+offset,&value,sizeof value,e);
}
bool ap(Runtime& r,uintptr_t p,uintptr_t expected,std::string& e){uintptr_t actual=0;return at(r,p,0,actual,e)&&(actual==r.image+expected||fail(e,"Management actual native class differs required="+std::to_string(expected)));}
bool text(Runtime& r,uintptr_t string,std::string& out,size_t limit,std::string& e){
    uintptr_t bytes=0;size_t length=0;if(!at(r,string,0,bytes,e)||!at(r,string,8,length,e))return false;
    if(!bytes||length>limit||bytes>UINTPTR_MAX-length)return fail(e,"Management GNU string extent invalid");out.resize(length);
    if(length&&!r.memory.copy(bytes,out.data(),length,e))return false;char end=1;
    return at(r,bytes,length,end,e)&&(!end&&out.find('\0')==std::string::npos||fail(e,"Management GNU string terminator differs"));
}
bool loan_rows(Runtime& r,const Local& local,a::ManagementState& state,std::string& e){
    uintptr_t analytics=0;if(!at(r,r.image,0x13df280,analytics,e)||!ap(r,analytics,0x16dd3b0,e))return false;
    int32_t eligibility=0;if(!at(r,analytics,0x44,eligibility,e))return false;
    for(bool offer:{false,true}){
        uintptr_t data=0;int32_t count=0,capacity=0;size_t base=offer?0xc0:0x90;
        if(!at(r,local.pin.business,base+8,data,e)||!at(r,local.pin.business,base+0x10,count,e)||!at(r,local.pin.business,base+0x14,capacity,e))return false;
        if(count<0||count>1024||capacity<count||capacity>1000000||(!data&&count))return fail(e,"Management actual Loan/Offer array invalid");
        std::set<uintptr_t> owners;std::set<std::string> keys;
        for(int32_t i=0;i<count;++i){uintptr_t owner=0;if(!at(r,data,size_t(i)*8,owner,e)||!ap(r,owner,offer?0x16df370:0x17438e0,e))return false;
            if(!owners.insert(owner).second)return fail(e,"Management Loan/Offer native owner duplicated");
            a::ManagementLoan loan;a::ManagementOffer quote;auto& names=offer?quote.text:loan.text;
            for(size_t n=0;n<3;++n)if(!text(r,owner+0x18+n*0x20,names[n],n?2048:256,e))return false;
            if(!keys.insert(names[0]).second)return fail(e,"Management Loan/Offer key duplicated");
            if(offer){for(size_t n=0;n<3;++n)if(!at(r,owner,0x78+n*4,quote.amounts[n],e))return false;
                if(!at(r,owner,0x84,quote.eligibility,e))return false;quote.available=eligibility>=quote.eligibility;state.offers.push_back(std::move(quote));}
            else {for(size_t n=0;n<4;++n)if(!at(r,owner,0x78+n*4,loan.amounts[n],e))return false;state.loans.push_back(std::move(loan));}
        }
    }
    return true;
}
struct MarketingMemory:subscriber_lifecycle::Memory {
    Runtime& runtime;explicit MarketingMemory(Runtime& r):runtime(r){}
    bool read(uintptr_t p,void* out,size_t size,std::string& e)override{return runtime.memory.copy(p,out,size,e);}
};
bool read_slots(Runtime& r,uintptr_t software,std::array<int32_t,3>& out,std::string& e){
    const auto collection=software+0xb8;uintptr_t data=0;int32_t count=0,capacity=0;
    if(!ap(r,software+0x98,0x1786ef0,e)||!ap(r,collection,0x173a6f0,e)||!at(r,collection,0x140,data,e)||!at(r,collection,0x148,count,e)||!at(r,collection,0x14c,capacity,e)||count<3||count>4096||capacity<count||capacity>1000000||!data||!r.memory.copy(data,out.data(),sizeof out,e))return fail(e,e.empty()?"Management native Character max-slot array invalid":e);return true;
}
uint32_t uid_lookup(uintptr_t image,uint64_t uid,uintptr_t& result)noexcept{
    __try{result=uintptr_t(reinterpret_cast<void*(*)(uint64_t)>(image+0x47ce20)(uid));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}

#include "toon_thought_lifetime.inc"
#include "building_uptime_native.inc"
bool read_services(Runtime&,const a::Context&,const a::Binding&,const a::ManagementIntent&,Local&,a::ManagementState&,std::string&);
bool read_employment(Runtime& r,Local& local,int32_t& count,std::vector<std::array<uint8_t,2>>& flags,std::string& e){
    uintptr_t data=0;int32_t size=0,capacity=0;flags.clear();
    if(!at(r,r.image,0x13df5e0,local.lounge,e)||!ap(r,local.lounge,0x171e600,e)||!at(r,local.lounge,0xd0,count,e)||!at(r,local.lounge,0x40,data,e)||!at(r,local.lounge,0x48,size,e)||!at(r,local.lounge,0x4c,capacity,e))return false;
    if(count<0||count>50||(size!=0&&size!=50)||capacity<size||capacity>4096||(!data&&size))return fail(e,"Management native Lounge generated pool/count bounds differ");
    local.gm_rows.resize(size);if(size&&!r.memory.copy(data,local.gm_rows.data(),size*sizeof(uintptr_t),e))return false;
    std::set<uintptr_t> unique;std::set<uint64_t> uids;local.gm_uids.clear();for(int32_t n=0;n<size;++n){const auto object=local.gm_rows[n];int32_t slot=-1;uint64_t uid=0;std::array<uint8_t,2> pair{};
        if(!unique.insert(object).second||!ap(r,object,0x16e5070,e)||!at(r,object,0x18,uid,e)||!uid||!uids.insert(uid).second||!at(r,object,0x4ac,slot,e)||slot!=n||!at(r,object,0x4c4,pair,e)||pair[0]!=uint8_t(n<count)||pair[1]>1)return fail(e,e.empty()?"Management GameMaster exact full-UID/fixed-slot owner/flags differ":e);
        uintptr_t registered=0;const auto seh=uid_lookup(r.image,uid,registered);if(seh||registered!=object)return fail(e,"Management GameMaster FindUID/current pointer differs SEH="+std::to_string(seh));flags.push_back(pair);local.gm_uids.push_back(uid);}
    return true;
}
bool once(Runtime& r,const a::Context& c,const a::Binding& b,Local& local,a::ManagementState& state,a::Ledger& ledger,std::string& e,const a::ManagementIntent* intent){
    state={};local={};a::Context actual;if(GetCurrentThreadId()!=r.thread||!a::selected_session().current(actual,e)||actual!=c)return fail(e,e.empty()?"Management immutable Session/Mainthread differs":e);
    live::NativePort port(c,b,nullptr);if(!port.read(local.pin,e)||local.pin.image!=r.image)return fail(e,e.empty()?"Management actual bound World/image differs":e);
    if(!at(r,r.image,0x13df500,local.software,e)||!ap(r,local.software,0x1715450,e)||
       !at(r,r.image,0x13df4f0,local.release,e)||!ap(r,local.release,0x1714ed0,e)||
       !at(r,r.image,0x13df5e0,local.lounge,e)||!ap(r,local.lounge,0x171e600,e)||
       !at(r,local.lounge,0xd0,local.lounge_count,e)||local.lounge_count<0||local.lounge_count>50||
       !at(r,local.pin.business,0x50,state.game_masters,e)||!at(r,local.release,0xb8,state.release_funds,e)||
       !at(r,local.software,0x31c,state.statistics,e)||!at(r,local.software,0x350,state.spent,e)||
       !at(r,local.software,0x344,state.purchased,e)||!at(r,local.software,0x37c,state.statistics_points,e)||
       !at(r,local.software,0x378,state.purchased_points,e)||!loan_rows(r,local,state,e))return false;
    if(!at(r,local.software,0x88,state.software_version,e)||!at(r,local.release,0x38,local.current_release,e)||!read_slots(r,local.software,state.character_slots,e)||!read_employment(r,local,state.lounge_employed,state.gm_employment,e))return false;
    state.release_present=local.current_release!=0;
    if(state.release_present&&(!ap(r,local.current_release,0x16c12b0,e)||!at(r,local.current_release,0x50,state.planned_version,e)||!at(r,local.current_release,0x60,state.release_type,e)))return false;
    building_mixed_identity::opened_file::CurrentWorld world;if(!native_open_runtime::read_current_world(world,e)||world.world_epoch!=b.native_epoch||world.generation!=b.native_generation)return fail(e,e.empty()?"Management Marketing current NativeOrigin differs":e);
    MarketingMemory memory(r);marketing_authority::Snapshot marketing;
    if(!marketing_authority::read_snapshot(memory,r.image,uintptr_t(world.state),uintptr_t(world.mode),marketing,e))return false;
    local.marketing=marketing.marketing;state.marketing=marketing.values;
    for(size_t n=0;n<9;++n)if(!at(r,r.image,0x1a39bc0+n*0xd0,state.campaign_costs[n],e))return false;
    if(intent&&!read_services(r,c,b,*intent,local,state,e))return false;
    ledger=live::ledger(local.pin.values);return a::management_valid(state)||fail(e,"Management native state outside typed domain");
}
bool read(Runtime& r,const a::Context& c,const a::Binding& b,Local& local,a::ManagementState& state,a::Ledger& ledger,std::string& e,const a::ManagementIntent* intent=nullptr){
    Local previous;a::ManagementState before;a::Ledger money;if(!once(r,c,b,previous,before,money,e,intent)||!once(r,c,b,local,state,ledger,e,intent))return false;
    if(previous.pin!=local.pin||previous.software!=local.software||previous.release!=local.release||previous.current_release!=local.current_release||previous.lounge!=local.lounge||previous.lounge_count!=local.lounge_count||previous.marketing!=local.marketing||previous.service_uid!=local.service_uid||previous.service_building!=local.service_building||previous.service_object!=local.service_object||previous.service_rows!=local.service_rows||previous.gm_rows!=local.gm_rows||previous.gm_uids!=local.gm_uids||before!=state||money!=ledger)return fail(e,"Management native roots/state/Ledger changed during double read");e.clear();return true;
}
bool expected_money(const a::ManagementIntent& intent,const a::ManagementState& state,const a::Ledger& before,a::Ledger& after,std::string& e){
    return a::management_ledger(intent,state,before,after)||fail(e,"Management native payment amount/Ledger invalid");
}
std::string uuid(std::string& e){unsigned char b[16]{};auto code=BCryptGenRandom(nullptr,b,sizeof b,BCRYPT_USE_SYSTEM_PREFERRED_RNG);if(code){e="Management UUID NTSTATUS="+std::to_string(code);return {};}
    b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;char out[37];std::snprintf(out,sizeof out,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);return out;}
void reject(Runtime& r,const std::string& e){++r.rejected;if(r.cause!=e){r.cause=e;LOG_ERROR("[Management] Rejected cause={}",e);}}
std::string hook_cause(){
#ifdef MT2MP_FRAMEWORK_PLUGIN
    return hooks::framework::process_backend().error();
#else
    return {};
#endif
}
bool capture(Runtime& r,void* receiver,const a::ManagementIntent& intent,uintptr_t expected_owner){
    std::string e;const auto controller=typed_action_runtime::controller_status();a::Context c;a::Binding b;
    if(!r.ready||r.replay||GetCurrentThreadId()!=r.thread||r.pending||policy_pending()||rules_pending()||intervention_pending()||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||
       !a::selected_session().current(c,e)||!typed_action_runtime::network_binding(c.snapshot_uuid,b,e)){reject(r,e.empty()?"Management normal UI requires idle bound Controller":e);return false;}
    Local local;a::ManagementState state;a::Ledger money;
    if(!a::management_valid(intent)||!read(r,c,b,local,state,money,e,&intent)||uintptr_t(receiver)!=expected_owner||(local.service_object&&local.service_object!=uintptr_t(receiver))){reject(r,e.empty()?"Management original receiver or intent differs":e);return false;}
    auto id=uuid(e);if(id.empty()){reject(r,e);return false;}r.pending=Pending{c,b,intent,id};++r.captures;return true;
}
#include "management_services.inc"
void gm(bool hire,void* receiver,uintptr_t caller){auto* r=published.load(std::memory_order_acquire);const auto original=hire?r->hire:r->fire;
    if(r->replay||!net::session_wire::connected()||caller!=r->image+(hire?0x6ea687:0x6ea620)){original(receiver);return;}
    uintptr_t business=0;std::string e;if(!at(*r,r->image,0x13df220,business,e)){reject(*r,e);return;}
    a::ManagementIntent intent;intent.op=hire?a::ManagementOp::HireGameMaster:a::ManagementOp::FireGameMaster;capture(*r,receiver,intent,business);
}
void hire_entry(void* receiver){gm(true,receiver,uintptr_t(_ReturnAddress()));}
void fire_entry(void* receiver){gm(false,receiver,uintptr_t(_ReturnAddress()));}
bool funds_entry(void* receiver,int32_t amount){auto* r=published.load(std::memory_order_acquire);
    if(r->replay||!net::session_wire::connected()||uintptr_t(_ReturnAddress())!=r->image+0x79db50)return r->funds(receiver,amount);
    uintptr_t owner=0;std::string e;if(!at(*r,r->image,0x13df4f0,owner,e)){reject(*r,e);return false;}
    a::ManagementIntent intent;intent.op=a::ManagementOp::AllocateReleaseFunds;intent.value=amount;return capture(*r,receiver,intent,owner);
}
void statistics_entry(void* receiver){auto* r=published.load(std::memory_order_acquire);
    if(r->replay||!net::session_wire::connected()){r->statistics(receiver);return;}
    std::string e;if(!ap(*r,uintptr_t(receiver),0x17157a0,e)){reject(*r,e);return;}
    a::ManagementIntent intent;intent.op=a::ManagementOp::SpendStatistics;
    if(!at(*r,uintptr_t(receiver),0x34c,intent.statistics,e)||!capture(*r,receiver,intent,uintptr_t(receiver))){if(!e.empty())reject(*r,e);return;}
    // Apply normally consumes these UI allocations. The ordered action owns only
    // the Software effect, while the source view consumes its captured input once.
    std::array<int32_t,10> current{};if(!at(*r,uintptr_t(receiver),0x34c,current,e)||current!=intent.statistics){reject(*r,e.empty()?"Management Statistics view changed before input consumption":e);r->pending.reset();return;}
    SIZE_T written=0;std::array<int32_t,10> clear{};
    if(!WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(uintptr_t(receiver)+0x34c),clear.data(),sizeof clear,&written)||written!=sizeof clear){reject(*r,"Management Statistics UI input consumption failed; no automatic replay");r->pending.reset();}
}
uint32_t campaign_command(uintptr_t image,void* record,bool& matches)noexcept{
    __try{matches=reinterpret_cast<bool(*)(const void*,const char*)>(image+0x1113d80)(record,"StartAdCampaign");return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t campaign_type(uintptr_t image,uintptr_t choice,int32_t& type)noexcept{
    __try{const auto value=reinterpret_cast<void*(*)(void*)>(image+0x6ab190)(reinterpret_cast<void*>(choice));type=reinterpret_cast<int32_t(*)(void*)>(image+0x5165d0)(value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool overview_entry(void* receiver,void* record){auto* r=published.load(std::memory_order_acquire);
    if(r->replay||!net::session_wire::connected())return r->overview(receiver,record);
    bool matches=false;const auto command_error=campaign_command(r->image,record,matches);
    if(command_error){reject(*r,"Management Overview command SEH="+std::to_string(command_error));return true;}
    if(!matches)return r->overview(receiver,record);
    std::string e;uintptr_t choice=0;if(!ap(*r,uintptr_t(receiver),0x17142d0,e)||!at(*r,uintptr_t(receiver),0x5c8,choice,e)||!choice){reject(*r,e.empty()?"Management original advertisement selector absent":e);return true;}
    a::ManagementIntent intent;intent.op=a::ManagementOp::StartCampaign;const auto fault=campaign_type(r->image,choice,intent.value);
    if(fault){reject(*r,"Management original advertisement type read SEH="+std::to_string(fault));return true;}
    capture(*r,receiver,intent,uintptr_t(receiver));return true;
}
void toggle_release_entry(void* receiver){auto* r=published.load(std::memory_order_acquire);const auto caller=uintptr_t(_ReturnAddress());
    if(r->replay||!net::session_wire::connected()||(caller!=r->image+0x79bba3&&caller!=r->image+0x7ff991)){r->toggle_release(receiver);return;}
    std::string e;uintptr_t manager=0,release=0;if(!at(*r,r->image,0x13df4f0,manager,e)||!at(*r,manager,0x38,release,e)||!release){reject(*r,e.empty()?"Management original current release absent":e);return;}
    a::ManagementIntent intent;intent.op=a::ManagementOp::ToggleReleaseKind;capture(*r,receiver,intent,release);
}
bool purchase_statistic_entry(void* receiver,int32_t ordinal){auto* r=published.load(std::memory_order_acquire);
    bool project_handled=false;const bool project_allowed=general_project_runtime::original_route(0x2de2d0,receiver,ordinal,project_handled);
    if(project_handled)return project_allowed?r->purchase_statistic(receiver,ordinal):false;
    if(r->replay||!net::session_wire::connected())return r->purchase_statistic(receiver,ordinal);
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||uintptr_t(_ReturnAddress())!=r->image+0x6c7e02)return r->purchase_statistic(receiver,ordinal);
    reject(*r,"Statistics project reached Pay without its before-effect Development UI capture");return false;
}
struct Call {Runtime* runtime=nullptr;a::ManagementIntent intent;Local local;bool accepted=false,returned=false,price_constructed=false,price_cleaned=false;alignas(8) unsigned char price[0x20]{};};
uint32_t original(Call& c){uint32_t seh=0;auto& r=*c.runtime;__try{
    switch(c.intent.op){
    case a::ManagementOp::HireGameMaster:r.hire(reinterpret_cast<void*>(c.local.pin.business));c.accepted=true;break;
    case a::ManagementOp::FireGameMaster:r.fire(reinterpret_cast<void*>(c.local.pin.business));c.accepted=true;break;
    case a::ManagementOp::AllocateReleaseFunds:c.accepted=r.funds(reinterpret_cast<void*>(c.local.release),c.intent.value);break;
    case a::ManagementOp::SpendStatistics:for(int32_t n=0;n<10;++n)reinterpret_cast<void(*)(void*,int32_t,int32_t)>(r.image+0x2dc900)(reinterpret_cast<void*>(c.local.software),n,c.intent.statistics[n]);c.accepted=true;break;
    case a::ManagementOp::TakeLoan:case a::ManagementOp::RepayLoan:{
        const struct {const char* data;size_t length;unsigned char rest[16];} key{c.intent.key.data(),c.intent.key.size(),{}};
        c.accepted=reinterpret_cast<bool(*)(void*,const void*)>(r.image+(c.intent.op==a::ManagementOp::TakeLoan?0x555a0:0x57620))(reinterpret_cast<void*>(c.local.pin.business),&key);break;
    }
    case a::ManagementOp::StartCampaign:{
        const auto daily=reinterpret_cast<int32_t(*)(int32_t)>(r.image+0x516510)(c.intent.value);
        if(daily>=0&&daily<=INT32_MAX/7&&reinterpret_cast<bool(*)(void*,int32_t)>(r.image+0x51c5e0)(reinterpret_cast<void*>(c.local.marketing),c.intent.value)&&
           reinterpret_cast<bool(*)(void*,int32_t,int32_t)>(r.image+0x54d80)(reinterpret_cast<void*>(c.local.pin.business),daily*7,17))
            c.accepted=reinterpret_cast<bool(*)(void*,int32_t,int32_t)>(r.image+0x51c640)(reinterpret_cast<void*>(c.local.marketing),c.intent.value,7);
        break;
    }
    case a::ManagementOp::ToggleReleaseKind:r.toggle_release(reinterpret_cast<void*>(c.local.current_release));c.accepted=true;break;
    case a::ManagementOp::SetServicePrice:
        reinterpret_cast<void(*)(void*,int32_t,int32_t)>(r.image+0x86d260)(c.price,c.intent.currency,c.intent.value);c.price_constructed=true;
        r.service_price(reinterpret_cast<void*>(c.local.service_object),c.price);c.accepted=true;break;
    case a::ManagementOp::SetServiceSync:r.service_sync(reinterpret_cast<void*>(c.local.service_object),c.intent.value!=0);c.accepted=true;break;
    case a::ManagementOp::PurchaseStatistic:c.accepted=r.purchase_statistic(reinterpret_cast<void*>(c.local.software),c.intent.value);break;
    }c.returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){seh=GetExceptionCode();}
    if(c.price_constructed){__try{reinterpret_cast<void(*)(void*)>(r.image+0x89da30)(c.price);c.price_cleaned=true;}__except(EXCEPTION_EXECUTE_HANDLER){if(!seh)seh=GetExceptionCode();}}
    return seh;
}
#include "gm_ticket_hooks.inc"
}
bool capture_loan(bool repay,void* receiver,const void* key,uintptr_t caller,bool& accepted){
    auto* r=published.load(std::memory_order_acquire);if(!r||r->replay||!net::session_wire::connected()||caller!=r->image+(repay?0x7336fd:0x7336b3))return false;
    std::string e;uintptr_t owner=0;a::ManagementIntent intent;intent.op=repay?a::ManagementOp::RepayLoan:a::ManagementOp::TakeLoan;
    accepted=false;if(!at(*r,r->image,0x13df220,owner,e)||!text(*r,uintptr_t(key),intent.key,256,e)){reject(*r,e);return true;}
    accepted=capture(*r,receiver,intent,owner);return true;
}
bool enable(std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r){r=new Runtime;published.store(r,std::memory_order_release);}
    if(GetCurrentThreadId()!=r->thread)return fail(e,"Management hook requires bound Mainthread");if(r->ready){e.clear();return true;}
    if(r->attempted)return fail(e,"Management hook installation failed earlier; no retry: "+r->install_cause);r->attempted=true;
    struct Keep {Runtime& r;std::string& e;~Keep(){if(!e.empty())r.install_cause=e;}} keep{*r,e};
    uint32_t thread=0;std::string sha;if(!native::read_resolver_receipt(r->image,thread,sha,e)||thread!=r->thread||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"Management exact EXE/Mainthread differs: "+e);
    std::vector<unsigned char> lookup_body(176);if(!r->memory.copy(r->image+0x47ce20,lookup_body.data(),lookup_body.size(),e)||mt2mp::sha256(lookup_body)!="de46f415bfe91761b32588eb0578aee73c58a18f4fe9b2f40ce3060ac30ff1f7")return fail(e,e.empty()?"Management original FindUID body differs":e);
    struct Hook {uintptr_t rva;size_t size;const char* sha;void* entry;void** original;};
    const Hook hooks[]{
        {0x56940,304,"7c7d686227b2264218cc445425c6f390458d1aaebaba7312e71e42e32dce95c9",reinterpret_cast<void*>(&hire_entry),reinterpret_cast<void**>(&r->hire)},
        {0x56a70,304,"dc180bfe9808d320f2c64b5e02d4a7aad5aaca55fb8768b150ebdae2c568e20f",reinterpret_cast<void*>(&fire_entry),reinterpret_cast<void**>(&r->fire)},
        {0x4ec880,128,"fc03d6d85d9c83f1acf14b2c8cdef6570663cc902f22c311987e4170c5baeb1b",reinterpret_cast<void*>(&funds_entry),reinterpret_cast<void**>(&r->funds)},
        {0x7d2a40,1024,"f41f346ad13cd53dce37f79878dbe9911dfe9718e2d4305cba2f05ba2375fb85",reinterpret_cast<void*>(&statistics_entry),reinterpret_cast<void**>(&r->statistics)},
        {0x76bd40,1296,"d28dbc0bfd6cc611b65cabf76028840aa9d2ac5572ed6fc577e23fd27bedb9a4",reinterpret_cast<void*>(&overview_entry),reinterpret_cast<void**>(&r->overview)},
        {0x4dee30,208,"5d33c47d75253e22c7b1c42c40d03d67b966c375a4a9140ebf8969aaac318c9c",reinterpret_cast<void*>(&toggle_release_entry),reinterpret_cast<void**>(&r->toggle_release)},
        {0x244f50,176,"d54f74e4bc4380a076b4542f478ba4e2a34e6a2f1c6246d0d7d5b081259b72e5",reinterpret_cast<void*>(&service_price_entry),reinterpret_cast<void**>(&r->service_price)},
        {0x245000,128,"7a2f496335333a05ecdd3d25cd5db54f1ebf50a8ea885a39c20c4ed3e39d0663",reinterpret_cast<void*>(&service_sync_entry),reinterpret_cast<void**>(&r->service_sync)},
        {0x2de2d0,144,"0057c30d5bba210c0105ed3f99d246885541d32e3ca9473da5a31b28d543088e",reinterpret_cast<void*>(&purchase_statistic_entry),reinterpret_cast<void**>(&r->purchase_statistic)}};
    for(const auto& h:hooks){std::vector<unsigned char> body(h.size);if(!r->memory.copy(r->image+h.rva,body.data(),body.size(),e)||mt2mp::sha256(body)!=h.sha)return fail(e,e.empty()?"Management original body differs RVA="+std::to_string(h.rva):e);}
    for(const auto& h:hooks){const auto created=MH_CreateHook(reinterpret_cast<void*>(r->image+h.rva),h.entry,h.original);if(created!=MH_OK||!*h.original)return fail(e,"Management hook create failed RVA="+std::to_string(h.rva)+" status="+std::to_string(created)+" original="+hook_cause());
        const auto enabled=MH_EnableHook(reinterpret_cast<void*>(r->image+h.rva));if(enabled!=MH_OK)return fail(e,"Management hook enable failed RVA="+std::to_string(h.rva)+" status="+std::to_string(enabled)+" original="+hook_cause());}
    if(!gm_enable(*r,e)||!policy_enable(*r,e)||!rules_enable(*r,e)||!intervention_enable(*r,e)||!interventions::actors::live::enable(e)||!control_input::runtime::enable(e)||!thought_native::enable(*r,e)||!uptime_native::enable(*r,e))return false;r->ready=true;e.clear();return true;
}
bool pending_ui(){auto* r=published.load(std::memory_order_acquire);return r&&(r->pending.has_value()||policy_pending()||rules_pending()||intervention_pending()||control_input::runtime::pending()||r->replay);}
bool retire_lobby(std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r){e.clear();return true;}
    native::WorldStatus world;if(GetCurrentThreadId()!=r->thread||!native::read_world_status(world,e))return fail(e,e.empty()?"Management lobby retirement requires Mainthread":e);
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0||r->replay)return fail(e,"Management lobby retirement requires actual idle Title and no replay");
    if(!control_input::runtime::retire_lobby(e))return false;
    intervention_reset();rules_reset();policy_reset();r->pending.reset();r->operations.clear();r->cause.clear();r->gm_client=false;r->gm_suspended=true;r->gm_failed=false;r->gm_cause.clear();r->gm_context.reset();r->gm_binding.reset();r->gm_root=0;e.clear();return true;
}
void tick(){auto* r=published.load(std::memory_order_acquire);if(!r)return;control_input::runtime::tick();intervention_tick(*r);rules_tick(*r);policy_tick(*r);if(!r->pending)return;const auto pending=*r->pending;r->pending.reset();std::string e;
    a::Context c;a::Binding b;if(!a::selected_session().current(c,e)||c!=pending.context||!typed_action_runtime::network_binding(c.snapshot_uuid,b,e)||b!=pending.binding||!typed_action_runtime::submit_management(pending.id,pending.intent,e)){reject(*r,e.empty()?"Management captured native binding changed":e);return;}++r->submitted;
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* host,std::string& e){
    if(request.action==a::Action::Intervention)return interventions::control_command(request.intervention.command)?control_input::runtime::perform(binding,request,host,e):intervention_perform(binding,request,host,e);
    if(request.action==a::Action::GameRules)return rules_perform(binding,request,host,e);
    if(request.action==a::Action::GmPolicy)return policy_perform(binding,request,host,e);
    auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||request.action!=a::Action::ManagementCommand||!a::management_valid(request.management)){e="Management runtime/action unavailable";return a::Outcome::RejectedNoEffect;}
    if(r->operations.contains(request.action_uuid)){e="Management token already attempted; no replay";return a::Outcome::Unknown;}
    if(r->operations.size()>=a::history_limit){auto oldest=r->operations.end();for(auto it=r->operations.begin();it!=r->operations.end();++it)
        if(it->second.retired&&(oldest==r->operations.end()||it->second.request.host_revision<oldest->second.request.host_revision))oldest=it;
        if(oldest==r->operations.end()){e="Management retained unretired original capacity reached";return a::Outcome::RejectedNoEffect;}r->operations.erase(oldest);}
    const auto controller=typed_action_runtime::controller_status();a::Context c;
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid||!a::selected_session().current(c,e)||c.snapshot_uuid!=request.snapshot_uuid||(c.role==a::Role::Client)!=(host!=nullptr)){if(e.empty())e="Management requires exact pending Controller/role/snapshot";return a::Outcome::RejectedNoEffect;}
    Local local;a::ManagementState state;a::Ledger money,expected;
    if(!read(*r,c,binding,local,state,money,e,&request.management)||!expected_money(request.management,state,money,expected,e))return a::Outcome::RejectedNoEffect;
    if(a::management_transition(request.management,state,state)){e="Management requested native values are unchanged";return a::Outcome::RejectedNoEffect;}
    if(request.management.op==a::ManagementOp::StartCampaign){
        const auto& campaigns=state.marketing.at("campaigns");bool duplicate=false;for(const auto& row:campaigns)duplicate|=row.at("type")==request.management.value;
        if(campaigns.size()>=4||duplicate){e="Management original advertisement capacity/type prerequisite failed";return a::Outcome::RejectedNoEffect;}
    }
    if(request.management.op==a::ManagementOp::SetServiceSync&&request.management.value&&!state.services->rows[size_t(request.management.service_ordinal)].allow_sync){e="Management original service prototype disables price sync";return a::Outcome::RejectedNoEffect;}
    if(host&&(!host->management_before||!host->management_after||state!=*host->management_before||money!=host->before||expected!=host->after)){e="Management Client actual before/Ledger differs from Host";return a::Outcome::RejectedNoEffect;}
    if((request.management.op==a::ManagementOp::HireGameMaster&&local.lounge_count>=50)||(request.management.op==a::ManagementOp::FireGameMaster&&local.lounge_count<=0)){e="Management original Lounge employment prerequisite failed";return a::Outcome::RejectedNoEffect;}
    auto& op=r->operations.emplace(request.action_uuid,Receipt{binding,request}).first->second;op.evidence.before=money;op.evidence.management_before=state;
    Call call{r,request.management,local};r->replay=true;op.attempted=true;const auto seh=original(call);r->replay=false;op.returned=call.returned;
    auto unknown=[&](const std::string& why){e=op.cause=why;return a::Outcome::Unknown;};
    if(seh||!call.returned||!call.accepted)return unknown("Management original failed/returned false SEH="+std::to_string(seh)+"; no replay");
    a::ManagementState after;Local final;if(!read(*r,c,binding,final,after,op.evidence.after,e,&request.management))return unknown(e);
    if(local.service_uid!=final.service_uid||local.service_building!=final.service_building||local.service_object!=final.service_object||local.service_rows!=final.service_rows)return unknown("Management original service identity/membership changed");
    op.evidence.management_after=after;
    if(!a::management_transition(request.management,state,after)||op.evidence.after!=expected||(host&&after!=*host->management_after))return unknown("Management original state or full Ledger differs from intent/Host");
    op.confirmed=true;e.clear();return a::Outcome::Confirmed;
}
bool observe(const a::Binding& b,const a::Request& q,a::Evidence& out,std::string& e){
    if(q.action==a::Action::Intervention)return interventions::control_command(q.intervention.command)?control_input::runtime::observe(b,q,out,e):intervention_observe(b,q,out,e);
    if(q.action==a::Action::GameRules)return rules_observe(b,q,out,e);
    if(q.action==a::Action::GmPolicy)return policy_observe(b,q,out,e);
    auto* r=published.load(std::memory_order_acquire);if(!r)return fail(e,"Management owner absent");auto found=r->operations.find(q.action_uuid);
    if(found==r->operations.end()||found->second.binding!=b||found->second.request!=q||!found->second.confirmed)return fail(e,"Management exact confirmed original receipt absent");
    a::Context c;Local local;a::ManagementState state;a::Ledger ledger;if(!a::selected_session().current(c,e)||c.snapshot_uuid!=q.snapshot_uuid||!read(*r,c,b,local,state,ledger,e,&q.management))return false;
    auto& op=found->second;if(state!=*op.evidence.management_after||ledger!=op.evidence.after)return fail(e,"Management actual state/Ledger differs from original receipt");out=op.evidence;out.retirement_confirmed=op.retired;e.clear();return true;
}
bool retire(const a::Binding& b,const a::Request& q,std::string& e){if(q.action==a::Action::Intervention)return interventions::control_command(q.intervention.command)?control_input::runtime::retire(b,q,e):intervention_retire(b,q,e);if(q.action==a::Action::GameRules)return rules_retire(b,q,e);if(q.action==a::Action::GmPolicy)return policy_retire(b,q,e);a::Evidence actual;if(!observe(b,q,actual,e))return false;published.load()->operations.at(q.action_uuid).retired=true;e.clear();return true;}
nlohmann::json status(){auto* r=published.load(std::memory_order_acquire);if(!r)return {{"ready",false}};nlohmann::json operations=nlohmann::json::object();for(const auto& [id,v]:r->operations)operations[id]={{"attempted",v.attempted},{"returned",v.returned},{"confirmed",v.confirmed},{"retired",v.retired},{"cause",v.cause},
    {"intent",a::management_intent_observation(v.request.management)},
    {"before",v.evidence.management_before?a::management_state_observation(*v.evidence.management_before):nlohmann::json(nullptr)},
    {"after",v.evidence.management_after?a::management_state_observation(*v.evidence.management_after):nlohmann::json(nullptr)},
    {"ledger_before",a::management_ledger_observation(v.evidence.before)},{"ledger_after",a::management_ledger_observation(v.evidence.after)}};
    return {{"ready",r->ready},{"pending",r->pending.has_value()},{"captures",r->captures},{"submitted",r->submitted},{"rejected",r->rejected},{"cause",r->cause},{"operations",operations},{"interventions",intervention_status()},{"building_uptime",{{"suppressed",uptime_native::suppressed},{"forwarded",uptime_native::forwarded},{"failed",uptime_native::failed},{"cause",uptime_native::fault}}}};}
#include "management_live_port.inc"
#include "gm_policy_runtime.inc"
#include "game_rules_runtime.inc"
#include "intervention_runtime.inc"
#include "intervention_subscriber_live.inc"
bool intervention_replaying(){const auto* r=published.load(std::memory_order_acquire);return control_input::runtime::replaying()||(r&&GetCurrentThreadId()==r->thread&&r->replay&&interventions_runtime.original_active);}
}
