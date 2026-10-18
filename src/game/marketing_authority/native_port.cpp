#include "native_port.h"
#include "notice_runtime.h"
#include "notice_hook.h"
#include "readback.h"
#include "records.h"
#include "clock_guard.h"
#include "native_pins.h"
#include "subscriber_lifecycle/append_calls.h"
#include "capture_guard.h"
#include "subscriber_lifecycle/native_calls.h"
#include "subscriber_lifecycle/save_scope.h"
#include "native_open_runtime.h"
#include "session_provider.h"
#include "live_host_state/native_port.h"
#include "live_host_state/economic_fence.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/game/native_engine.h"
#include <bit>
#include <cmath>
#include <Windows.h>

namespace mt2mp::game::marketing_authority {
namespace {
namespace local=mt2mp::subscriber_lifecycle;
namespace stage=local::first_append;
namespace live=live_host_state;
using World=building_mixed_identity::opened_file::CurrentWorld;
class ProcessMemory final:public Memory {public:bool read(uintptr_t p,void* out,size_t n,std::string& e)override{
    if(!p||p>UINTPTR_MAX-n)return fail(e,"memory extent invalid");SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),out,n,&copied)||copied!=n)return fail(e,"ReadProcessMemory error="+std::to_string(GetLastError()));return true;
}} memory;
template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& e){if(!p||p>UINTPTR_MAX-offset)return fail(e,"field extent overflow");return memory.read(p+offset,&out,sizeof out,e);}
bool addresspoint(uintptr_t p,uintptr_t image,uintptr_t rva,std::string& e){uintptr_t v=0;return at(p,0,v,e)&&(v==image+rva||fail(e,"addresspoint differs"));}

struct Pin {World world;uintptr_t image=0;uint32_t thread=0;Snapshot snapshot;native::SimulationState finance;native::SoftwareState software;};

Json owner_fields(const Pin& p){const auto& w=p.world;const auto& s=p.snapshot;
    return {{"game",uintptr_t(w.game)},{"mode",uintptr_t(w.mode)},{"state",uintptr_t(w.state)},{"world",uintptr_t(w.world)},{"map",uintptr_t(w.map)},
        {"origin_uuid",w.uuid},{"origin_sha256",w.sha256},{"world_epoch",w.world_epoch},{"generation",w.generation},
        {"image",p.image},{"thread",p.thread},{"marketing",s.marketing},{"clock",s.clock},{"hud",s.hud},{"window",s.window},{"width_bits",s.width},{"height_bits",s.height}};
}
bool same_guards(const Pin& required,const Pin& measured,const char* phase,bool scope_zero,std::string& e){
    if(!guard_equal(phase,"owners",owner_fields(required),owner_fields(measured),e)||
       !guard_equal(phase,"finance/Software",finance_fields(required.finance,required.software),finance_fields(measured.finance,measured.software),e))return false;
    return clock_unchanged({required.finance.clock.flat_day,required.finance.clock.seconds,required.finance.clock.time_scale,required.finance.clock.pause_scale,required.finance.clock.paused},
        {measured.finance.clock.flat_day,measured.finance.clock.seconds,measured.finance.clock.time_scale,measured.finance.clock.pause_scale,measured.finance.clock.paused},scope_zero,e);
}

bool schedulers(const Pin& p,bool held,std::string& e){
    for(const auto& row:{std::array<uintptr_t,4>{0x13df340,0x16fdaf0,0x68,0xf8},{0x13df350,0x16fdc40,0x150,0x28},{0x13df4a0,0x1709650,0x70,0x68}}){
        uintptr_t singleton=0,owner=0;uint8_t paused=2;
        if(!at(p.image,row[0],singleton,e)||!at(uintptr_t(p.world.game),row[2],owner,e)||singleton!=owner||!addresspoint(owner,p.image,row[1],e)||!at(owner,row[3],paused,e))return fail(e,e.empty()?"Game scheduler owner differs":e);
        if(paused!=uint8_t(held))return fail(e,"scheduler pause measured="+std::to_string(paused)+" required="+std::to_string(held));
        if(held&&row[0]==0x13df340){int32_t running=-1,waiting=-1;if(!at(owner,0xa8,running,e)||!at(owner,0x90,waiting,e))return false;if(running||waiting)return fail(e,"TaskManager not drained running="+std::to_string(running)+" waiting="+std::to_string(waiting));}
    }return true;
}
bool read_once(const action_session::Context& c,const action_session::Binding& b,const live::Frame& f,bool held,Pin& p,std::string& e){
    p={};std::string hash;action_session::Context current;
    if(!native::read_resolver_receipt(p.image,p.thread,hash,e)||!action_session::selected_session().current(current,e)||!native_open_runtime::read_current_world(p.world,e))return false;
    if(hash!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"||p.image!=uintptr_t(GetModuleHandleW(nullptr))||p.thread!=GetCurrentThreadId()||current!=c||p.world.world_epoch!=b.native_epoch||p.world.generation!=b.native_generation||!p.world.world||!p.world.state||net::current_world_epoch()!=f.epoch||f.session!=c.session||f.snapshot!=c.snapshot_uuid)return fail(e,"current session/build/thread/world/Frame binding differs");
    native::WorldStatus status;if(!native::read_world_status(status,e))return false;
    if(status.active_mode_index!=3||status.in_game_state!=4||!status.world_present||status.quitting||status.title_transition_pending||status.map_task_pending||status.edit_operations||status.saves_in_progress!=(held?1:0))return fail(e,"World effects guard measured saves="+std::to_string(status.saves_in_progress)+" required="+std::to_string(held));
    if(!addresspoint(uintptr_t(p.world.state),p.image,0x16de920,e)||!read_snapshot(memory,p.image,uintptr_t(p.world.state),uintptr_t(p.world.mode),p.snapshot,e)||!native::read_simulation_state(p.finance,e)||!native::read_software_state(p.software,e)||!schedulers(p,held,e))return false;
    if(p.finance.clock.flat_day!=f.values.calendar[3]||p.finance.clock.seconds!=f.values.calendar[4])return fail(e,"Marketing read requires already authoritative Day/time");
    if(c.role==action_session::Role::Client&&(p.finance.clock.time_scale!=0||!p.finance.clock.paused))return fail(e,"Client native Clock0 required");
    return true;
}
bool read(const action_session::Context& c,const action_session::Binding& b,const live::Frame& f,bool held,Pin& p,std::string& e){
    Pin a;if(!read_once(c,b,f,held,a,e)||!read_once(c,b,f,held,p,e))return false;
    return same_guards(a,p,"double read",false,e)&&guard_equal("double read","Marketing/panes",report(a.snapshot),report(p.snapshot),e);
}
enum class Op {Allocate,AwardConstruct,AwardLoad,AwardAdd,CampaignStart,CampaignLoad,ContextConstruct,ContextDestroy,Rebuild,PurgeAwards,AbortCampaign,RestorePause};
struct Call {Op op;uintptr_t image=0,target=0,record=0,context=0,result=0;int32_t type=0,duration=0;float pause=0;bool accepted=false;};
uint32_t invoke(Call* c){__try{void* p=reinterpret_cast<void*>(c->target);
    switch(c->op){
    case Op::Allocate:c->result=uintptr_t(reinterpret_cast<void*(*)(size_t)>(c->image+0xa02670)(0xa8));break;
    case Op::AwardConstruct:reinterpret_cast<void(*)(void*)>(c->image+0x51a9b0)(p);break;
    case Op::AwardLoad:reinterpret_cast<void(*)(void*,void*,void*)>(c->image+0xceabb0)(p,reinterpret_cast<void*>(c->record),reinterpret_cast<void*>(c->context));break;
    case Op::AwardAdd:reinterpret_cast<void(*)(void*,void*)>(c->image+0xb5d8d0)(p,reinterpret_cast<void*>(c->result));break;
    case Op::CampaignStart:c->accepted=reinterpret_cast<bool(*)(void*,int32_t,int32_t)>(c->image+0x51c640)(p,c->type,c->duration);break;
    case Op::CampaignLoad:reinterpret_cast<void(*)(void*,void*,void*)>(c->image+0xc7cf80)(p,reinterpret_cast<void*>(c->record),reinterpret_cast<void*>(c->context));break;
    case Op::ContextConstruct:reinterpret_cast<void(*)(void*)>(c->image+0x89e940)(p);break;
    case Op::ContextDestroy:reinterpret_cast<void(*)(void*)>(c->image+0x89e480)(p);break;
    case Op::Rebuild:reinterpret_cast<void(*)(void*)>(c->image+0x663880)(p);break;
    case Op::PurgeAwards:reinterpret_cast<void(*)(void*)>(c->image+0x51b780)(p);break;
    case Op::AbortCampaign:c->accepted=reinterpret_cast<bool(*)(void*,int32_t)>(c->image+0x51c510)(p,c->type);break;
    case Op::RestorePause:reinterpret_cast<void(*)(void*,float)>(c->image+0x259b70)(p,c->pause);break;
    }
}__except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;}
struct Resources {stage::NativeTree tree;stage::DecodeReceipt decode;uintptr_t context=0,award=0;std::unique_ptr<Resources> next;};
std::unique_ptr<Resources> retained;
struct Work {
    action_session::Context context;action_session::Binding binding;live::Frame frame;Pin before;
    State target;Plan plan;stage::DecodeApi records;std::unique_ptr<Resources> resources=std::make_unique<Resources>();
    Json receipt={{"calls",Json::array()}};bool attempted=false,unknown=false;
};
bool call(Work& w,Call& c,const char* name,std::string& e){w.attempted=true;Json r={{"name",name},{"attempted",true},{"returned",false}};
    try{const auto seh=invoke(&c);r["seh"]=seh;if(seh){w.unknown=true;w.receipt["calls"].push_back(r);return fail(e,std::string(name)+" SEH="+std::to_string(seh));}}
    catch(const std::exception& x){w.unknown=true;w.receipt["calls"].push_back(r);return fail(e,std::string(name)+" C++="+x.what());}
    catch(...){w.unknown=true;w.receipt["calls"].push_back(r);return fail(e,std::string(name)+" nonstandard exception");}
    r["returned"]=true;r["accepted"]=c.accepted;w.receipt["calls"].push_back(std::move(r));return true;
}
bool unchanged(Work& w,Pin& p,bool held,std::string& e){
    if(!read(w.context,w.binding,w.frame,held,p,e))return false;
    w.receipt["last_guard_observation"]={{"phase",held?"held":"closed"},{"owners",owner_fields(p)},{"finance",finance_fields(p.finance,p.software)}};
    return same_guards(w.before,p,held?"held":"closed",true,e);
}
bool pins(uintptr_t image,std::string& e){
    for(const auto& pin:native_pins){if(pin.rva==0x518290){if(!notice_hook::verify(image,e))return false;continue;}std::vector<unsigned char> bytes(pin.size);if(!memory.read(image+pin.rva,bytes.data(),bytes.size(),e))return false;if(mt2mp::sha256(bytes)!=pin.sha)return fail(e,"body SHA differs RVA="+std::to_string(pin.rva));}
    for(const auto& row:{std::array<uintptr_t,2>{0x13cfe20,0x98},{0x13cfde0,0x9c}}){uintptr_t offset=0;if(!at(image+row[0],0x38,offset,e)||offset!=row[1])return fail(e,"buzz native descriptor offset differs");}
    for(const auto& pair:{std::pair<uintptr_t,uintptr_t>{0x1749880+0x70,0xceabb0},{0x1749880+0x80,0xb63070},{0x16e3d50+0x70,0xc7cf80},{0x16e3d50+0x80,0x5199e0}}){uintptr_t target=0;if(!at(image,pair.first,target,e)||target!=image+pair.second)return fail(e,"typed Load/Resolve slot differs");}
    return true;
}
bool cleanup_record(Work& w,std::string& e){return stage::destroy_records(w.records,w.resources->tree,w.resources->decode,e);}
uintptr_t find_owner(const Snapshot& s,const Json& row,bool award){const auto& values=s.values.at(award?"awards":"campaigns");const auto key=award?award_key(row):campaign_key(row);
    for(size_t i=0;i<values.size();++i)if((award?award_key(values[i]):campaign_key(values[i]))==key)return (award?s.awards:s.campaigns).at(i);return 0;
}
bool load(Work& w,uintptr_t owner,const Json& row,bool award,std::string& e){auto& r=*w.resources;const auto image=w.before.image;Pin p;
    if(!unchanged(w,p,true,e))return false;
    if(!award&&!current_campaign(row,p.finance.clock.flat_day))return fail(e,"Campaign pre-Load current day guard failed");
    if(!stage::decode_marketing_record(w.records,memory,award?stage::MarketingRecordKind::Award:stage::MarketingRecordKind::Campaign,records(row,award),r.tree,r.decode,e))return false;
    Call c{Op::ContextConstruct,image,uintptr_t(&r.context)};if(!call(w,c,"context_construct",e))return false;
    c={award?Op::AwardLoad:Op::CampaignLoad,image,owner,r.tree.root,uintptr_t(&r.context)};if(!call(w,c,award?"award_load":"campaign_load",e))return false;
    if(!unchanged(w,p,true,e))return false;
    if(!award){const auto& values=p.snapshot.values.at("campaigns");const auto found=std::find_if(values.begin(),values.end(),[&](const Json& r){return campaign_key(r)==campaign_key(row);});
        if(found==values.end()||*found!=row||!current_campaign(*found,p.finance.clock.flat_day))return fail(e,"actual Campaign pre-Resolve fields/current-day guard differs");}
    c={Op::ContextDestroy,image,uintptr_t(&r.context)};if(!call(w,c,"context_resolve_destroy",e))return false;r.context=0;
    if(!cleanup_record(w,e))return false;r.decode={};return true;
}


bool apply_body(Work& w,std::string& e){Pin p;if(!unchanged(w,p,true,e))return false;if(!guard_equal("held entry","Marketing/panes",report(w.before.snapshot),report(p.snapshot),e))return false;const auto image=p.image;
    for(auto type:w.plan.abort_campaigns){Call c{Op::AbortCampaign,image,p.snapshot.marketing};c.type=type;if(!call(w,c,"abort_campaign",e)||!c.accepted)return fail(e,e.empty()?"original AbortAdCampaign rejected":e);if(!unchanged(w,p,true,e))return false;}
    if(w.plan.purge_awards){Call c{Op::PurgeAwards,image,p.snapshot.marketing};if(!call(w,c,"purge_expired_awards",e)||!unchanged(w,p,true,e))return false;}
    for(bool award:{true,false}){
        const auto& rows=w.target.values.at(award?"awards":"campaigns");
        for(size_t i=0;i<rows.size();++i){const auto& row=rows[i];uintptr_t owner=find_owner(p.snapshot,row,award);
            if(owner){const auto& current=p.snapshot.values.at(award?"awards":"campaigns");const auto key=award?award_key(row):campaign_key(row);
                const auto found=std::find_if(current.begin(),current.end(),[&](const Json& r){return (award?award_key(r):campaign_key(r))==key;});if(*found==row)continue;
                if(!load(w,owner,row,award,e))return false;
            }else if(award){Call c{Op::Allocate,image};if(!call(w,c,"award_allocate",e)||!c.result)return fail(e,e.empty()?"Award allocation returned null":e);owner=c.result;w.resources->award=owner;
                c={Op::AwardConstruct,image,owner};if(!call(w,c,"award_construct",e)||!load(w,owner,row,true,e))return false;
                c={Op::AwardAdd,image,p.snapshot.marketing+0x38};c.result=owner;if(!call(w,c,"award_add_direct_pointer",e))return false;
            }else{Call c{Op::CampaignStart,image,p.snapshot.marketing};c.type=integer(row.at("type"));c.duration=integer(row.at("duration"));
                const auto prior=p.snapshot.campaigns;if(!call(w,c,"campaign_start",e)||!c.accepted)return fail(e,e.empty()?"StartAdCampaign rejected":e);
                if(!unchanged(w,p,true,e))return false;owner=find_owner(p.snapshot,row,false);
                if(!owner||std::find(prior.begin(),prior.end(),owner)!=prior.end()||p.snapshot.campaigns.size()!=prior.size()+1)return fail(e,"StartAdCampaign fresh owner handoff differs");
                if(!load(w,owner,row,false,e))return false;
            }
            if(!unchanged(w,p,true,e)||find_owner(p.snapshot,row,award)!=owner)return fail(e,e.empty()?"typed row ownership readback differs":e);
            if(award&&w.resources->award==owner)w.resources->award=0;
        }
    }
    for(const auto& field:{std::pair{"positiveBuzz",uintptr_t(0x13cfe20)},std::pair{"negativeBuzz",uintptr_t(0x13cfde0)}}){if(p.snapshot.values.at(field.first)==w.target.values.at(field.first))continue;
        auto& r=*w.resources;if(!stage::decode_integer_record(w.records,memory,field.first,integer(w.target.values.at(field.first)),r.tree,r.decode,e))return false;
        local::CallReceipt receipt;w.attempted=true;if(!local::load_scalar_property(reinterpret_cast<local::PropertyLoad>(image+0xd2cb70),reinterpret_cast<void*>(image+field.second),reinterpret_cast<void*>(p.snapshot.marketing),reinterpret_cast<void*>(r.tree.root),receipt,e))return false;
        w.receipt["calls"].push_back({{"name",field.first},{"attempted",receipt.attempted},{"returned",receipt.returned},{"seh",receipt.seh}});
        if(!cleanup_record(w,e))return false;r.decode={};if(!unchanged(w,p,true,e))return false;
    }
    if(p.snapshot.values!=w.target.values)return fail(e,"complete represented Marketing readback differs before pane rebuild");
    Call c{Op::Rebuild,image,p.snapshot.window};if(!call(w,c,"boost_rebuild",e)||!unchanged(w,p,true,e))return false;
    if(p.snapshot.values!=w.target.values||!panes_match(p.snapshot,e))return fail(e,e.empty()?"complete Marketing/pane readback differs":e);
    // Verify every retained portable identity still points to the original native allocation.
    for(bool award:{true,false})for(const auto& row:w.before.snapshot.values.at(award?"awards":"campaigns")){
        const auto owner=find_owner(p.snapshot,row,award);if(owner&&owner!=find_owner(w.before.snapshot,row,award))return fail(e,"retained owner was replaced");
    }
    w.receipt["after"]=report(p.snapshot);return true;
}
bool body(void* value,std::string& e){auto& w=*static_cast<Work*>(value);return apply_body(w,e);}
bool exit_guard(void* value,std::string& e){auto& w=*static_cast<Work*>(value);Pin p;return unchanged(w,p,true,e);}
bool execute(Work& w,std::string& e){
    w.receipt["origin"]={{"session",w.frame.session},{"epoch",w.frame.epoch},{"source_generation",w.frame.source_generation},{"snapshot_uuid",w.frame.snapshot},{"revision",w.frame.revision},{"local_epoch",w.binding.native_epoch},{"local_generation",w.binding.native_generation}};
    w.receipt["before"]=report(w.before.snapshot);w.receipt["target"]=w.target.values;
    w.receipt["guard_before"]={{"owners",owner_fields(w.before)},{"finance",finance_fields(w.before.finance,w.before.software)}};
    local::SaveScopeReceipt scope;const local::SaveScopeApi api{reinterpret_cast<local::SaveScopeCall>(w.before.image+0x4a03a0),reinterpret_cast<local::SaveScopeCall>(w.before.image+0x49f6f0)};
    bool okay=false;try{okay=local::with_save_scope(api,w.before.world.mode,body,exit_guard,&w,scope,e);}catch(const std::exception& x){e=std::string("Marketing native scope C++: ")+x.what();}catch(...){e="Marketing native scope nonstandard exception";}
    w.attempted=w.attempted||scope.enter_attempted;w.unknown=w.unknown||scope.unknown;
    w.receipt["save_scope"]={{"enter_returned",scope.enter_returned},{"leave_returned",scope.leave_returned},{"exit_guard",scope.exit_guard_confirmed},{"unknown",scope.unknown},{"body_cause",scope.body_cause},{"cleanup_cause",scope.cleanup_cause}};
    if(okay){Pin p;okay=unchanged(w,p,false,e);
        if(okay&&std::bit_cast<uint32_t>(w.before.finance.clock.pause_scale)!=std::bit_cast<uint32_t>(p.finance.clock.pause_scale)){
            if(w.before.finance.clock.pause_scale!=1||p.finance.clock.pause_scale!=0)okay=fail(e,"original pause restoration branch not admitted");
            else{Call c{Op::RestorePause,w.before.image,w.before.snapshot.clock};c.pause=w.before.finance.clock.pause_scale;okay=call(w,c,"restore_original_pause",e)&&read(w.context,w.binding,w.frame,false,p,e)&&same_guards(w.before,p,"restored",false,e);}
        }
        if(okay){okay=p.snapshot.values==w.target.values&&panes_match(p.snapshot,e);if(!okay&&e.empty())e="Marketing closed readback differs";}
    }
    if(!okay){w.unknown=w.unknown||w.attempted;if(w.unknown){w.resources->next=std::move(retained);retained=std::move(w.resources);}}
    w.receipt["attempted"]=w.attempted;w.receipt["confirmed"]=okay;w.receipt["unknown"]=w.unknown;w.receipt["cause"]=okay?"":e;return okay;
}
}
struct NativePort::Impl {bool unknown=false;Json receipt=nullptr;notice::Runtime notices;};
NativePort::NativePort():impl_(std::make_unique<Impl>()){}
NativePort::~NativePort()=default;
bool NativePort::bind(const action_session::Context& c,const action_session::Binding& b,uint64_t epoch,std::string& e){if(impl_->unknown)return fail(e,"cannot rebind uncertain Marketing world");return impl_->notices.bind(c,b,epoch,e);}
bool NativePort::suspend(std::string& e){return impl_->notices.suspend(e);}
bool NativePort::sent(const live::Frame& f,std::string& e){return impl_->notices.sent(f,e);}
bool NativePort::acknowledge(uint64_t peer,uint64_t revision,std::string& e){return impl_->notices.acknowledge(peer,revision,e);}
bool NativePort::admit(const live::Frame& f,const live::Frame* prior,std::string& e){return impl_->notices.admit(f,prior,e);}
bool NativePort::capture(const action_session::Context& c,const action_session::Binding& b,const live::Frame& frame,State& out,std::string& e){
    out={};if(impl_->unknown)return fail(e,"capture locked after uncertain call; no replay");if(c.role!=action_session::Role::Host)return fail(e,"capture requires Host");
    Pin observed;
    if(!read(c,b,frame,false,observed,e)||!pins(observed.image,e)||
       !valid(State{observed.snapshot.values},frame.values.calendar[3],e)||!host_frame_matches(frame,observed.finance,e)){
        impl_->receipt={{"operation","host_capture"},{"attempted",false},{"confirmed",false},{"unknown",false},{"read_only",true},{"cause",e}};return false;
    }
    // Typed Save only reconfirms this pinned schema. Its SaveScope advances shards,
    // so Host publication reads the admitted fields without invoking game methods.
    out={observed.snapshot.values};notice::Batch batch;if(!impl_->notices.capture(frame,batch,e))return false;out.notices=std::move(batch);
    impl_->receipt={{"operation","host_capture"},{"revision",frame.revision},{"attempted",false},{"confirmed",true},{"unknown",false},{"read_only",true},
        {"owners",owner_fields(observed)},{"finance",finance_fields(observed.finance,observed.software)},{"values",out.values}};
    e.clear();return true;
}
bool NativePort::apply(const action_session::Context& c,const action_session::Binding& b,const live::Frame& frame,live::MarketingReceipt& receipt,std::string& e){
    receipt={};if(impl_->unknown)return fail(e,"Client locked after uncertain call; no replay");if(c.role!=action_session::Role::Client||!frame.marketing)return fail(e,"Client requires complete Marketing component");
    Work w;w.context=c;w.binding=b;w.frame=frame;w.target=*frame.marketing;
    live::NativePort reader(c,b,&live::economic_fence());live::Pin fence;
    if(!reader.read(fence,e)||!live::economic_fence().read_current_fence(fence,e)||!read(c,b,frame,false,w.before,e)||!plan(State{w.before.snapshot.values},w.target,frame.values.calendar[3],w.plan,e))return false;
    const auto complete_notices=[&](live::MarketingReceipt& result){
        live::MarketingReceipt notice_receipt;
        const bool okay=impl_->notices.apply(frame,notice_receipt,e);
        result.attempted=result.attempted||notice_receipt.attempted;
        result.confirmed=okay&&notice_receipt.confirmed;
        result.unknown=result.unknown||notice_receipt.unknown;
        if(!okay){result.cause=e;impl_->unknown=result.unknown;return false;}
        if(notice_receipt.attempted){
            Pin after;
            if(!read(c,b,frame,false,after,e)||!same_guards(w.before,after,"after notice",false,e)||
               after.snapshot.values!=w.target.values||!panes_match(after.snapshot,e)||
               !reader.read(fence,e)||!live::economic_fence().read_current_fence(fence,e)){
                if(e.empty())e="Marketing state changed across message handoff";
                result.confirmed=false;result.unknown=true;result.cause=e;impl_->unknown=true;return false;
            }
        }
        return true;
    };
    if(!w.plan.changed()){if(!panes_match(w.before.snapshot,e))return false;return complete_notices(receipt);}
    if(!pins(w.before.image,e)||!stage::resolve_record_api(memory,w.before.image,w.records,e))return false;
    bool okay=execute(w,e);receipt={w.attempted,okay,w.unknown,e};impl_->receipt=w.receipt;impl_->unknown=w.unknown;
    if(okay&&(!reader.read(fence,e)||!live::economic_fence().read_current_fence(fence,e))){receipt.confirmed=false;receipt.unknown=true;receipt.cause=e;impl_->unknown=true;okay=false;}
    if(okay)okay=complete_notices(receipt);
    return okay;
}
Json NativePort::status()const{return {{"unknown",impl_->unknown},{"last",impl_->receipt},{"campaign_notices",impl_->notices.status()}};}
}
