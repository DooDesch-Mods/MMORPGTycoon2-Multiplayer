#include "developer_staff/read.h"
#include "developer_staff/probe.h"
#include "developer_staff/shared_state.h"
#include "developer_staff/frame_apply.h"
#include "developer_staff/construction_native.h"
#include "developer_staff/ui_capture.h"
#include "developer_staff/owned_ui_entry.h"
#include "session_provider.h"
#include "live_host_state/state.h"
#include <atomic>
#include <map>
#include <iostream>
#include <stdexcept>
#include <regex>
namespace action=mt2mp::action_session;
namespace staff=mt2mp::game::developer_staff;
namespace fixture {
unsigned checks=0,ui_calls=0,count_calls=0,reads=0;
unsigned construction_preflights=0,construction_applies=0;bool jobs_ok=true,construction_ok=true,construction_partial=false;
bool code_ok=true,window_ok=true,before_ok=true,after_ok=true,immediate_ok=true;
bool ui_accepted=true,ui_returned=true,ui_clean=true,ui_label=true,ui_effect=true;
bool wrong_owner=false,wrong_immediate_actor=false,wrong_final_actor=false,count_fault=false;
bool role_ok=true,session_ok=true;
bool display_ok=true,carried=false,scalar_ledger_change=false;unsigned scalar_calls=0,fail_scalar=0,display_calls=0;
bool update_ok=true,network_ok=true,show_visible=false,ui_unexpected_effect=false;
std::string cause;action::Context context;action::Binding binding{7,9};
staff::Capture current;staff::PoolState pool;
nlohmann::json controller;
void need(bool ok,const char* e){++checks;if(!ok)throw std::runtime_error(e);}
struct Session final:action::Session {bool current(action::Context& out,std::string& e)override{if(!session_ok){e="actual session unavailable";return false;}out=context;return true;}} session;
}
namespace latch {using ApplyReceipt=mt2mp::game::live_host_state::ApplyReceipt;
#include "developer_failure_latch.inc"
}
uint32_t GetCurrentThreadId(){return 1;}
namespace mt2mp {struct Config {bool local_test=true;static Config& instance(){static Config value;return value;}};}
namespace mt2mp::action_session {Session& selected_session(){return fixture::session;}}
namespace mt2mp::game::typed_action_runtime {
nlohmann::json controller_status(){return fixture::controller;}
bool construction_jobs(const action::Context&,const action::Binding&,std::vector<staff::construction::LocalJob>& out,std::string& e){out.clear();if(!fixture::jobs_ok){e="actual local Job owner changed";return false;}return true;}
bool network_binding(const std::string&,action::Binding& out,std::string& e){if(!fixture::network_ok){e="original ready binding failure";return false;}out=fixture::binding;return true;}
}
namespace mt2mp::game::hud_time_runtime {bool verify_window_show_hook(uintptr_t,std::string&){return true;}}
namespace mt2mp::game::tutorial_replication {
bool invalidate_developer_pool(const action::Context&,const action::Binding&,std::string& e){++fixture::display_calls;if(!fixture::display_ok)e="actual display Origin changed";return fixture::display_ok;}
}
namespace mt2mp::game::developer_staff {
uint32_t original_show(uintptr_t,uintptr_t){++fixture::ui_calls;fixture::show_visible=true;return 0;}
bool authorize_ui_update(Capture& out,std::string& e){if(!fixture::update_ok){e="original owned GameUpdate failure";return false;}out=fixture::current;return true;}
nlohmann::json encode(const Capture& c){return {{"count",c.state.business_count},{"cash",c.pin.ledger.cash}};}
bool same_origin(const Origin& a,const Origin& b){return a.uuid==b.uuid&&a.world_epoch==b.world_epoch&&a.generation==b.generation;}
bool read_process(Capture& out,std::string& e){if(!fixture::immediate_ok){e="actual immediate read failed";return false;}out=fixture::current;return true;}
namespace scalar {
bool apply(const Read&,uintptr_t,uintptr_t actor,Field field,int32_t value,Receipt& receipt,std::string& e){
    ++fixture::scalar_calls;receipt.attempted=true;
    if(fixture::scalar_calls==fixture::fail_scalar){e="original State/Active property cleanup failure";return false;}
    const auto slot=size_t((actor-0x10000)/0x1000);auto& row=fixture::pool.rows.at(slot);auto& native=fixture::current.state.rows.at(slot);
    if(field==Field::State)row.state=native.state=value;else row.active=native.active=value!=0;
    if(fixture::scalar_ledger_change)++fixture::current.pin.ledger.line_counts[19];
    return true;
}
}
namespace construction {
bool preflight(const action::Context&,const action::Binding&,const Capture&,const PoolState&,const std::vector<LocalJob>&,std::string& e){++fixture::construction_preflights;if(!fixture::construction_ok)e="actual construction preflight cause";return fixture::construction_ok;}
bool apply(const action::Context&,const action::Binding&,const Capture&,const PoolState& target,const std::vector<LocalJob>&,bool& attempted,bool& unknown,std::string& e){++fixture::construction_applies;attempted=true;unknown=fixture::construction_partial;if(unknown){e="actual construction partial native effect";return false;}fixture::pool=target;return true;}
}
namespace action_runtime {
using json=nlohmann::json;
#include "developer_pending.inc"
#include "developer_operation.inc"
struct Memory {bool copy(uintptr_t,void* out,size_t n,std::string&){if(n==sizeof(uintptr_t))*static_cast<uintptr_t*>(out)=fixture::carried?1:0;if(n==1)*static_cast<uint8_t*>(out)=fixture::show_visible?1:0;return true;}};
struct Runtime {bool ready=true;uintptr_t image=0x140000000;uint32_t thread=1;Memory memory;std::map<std::string,Operation> operations;std::atomic<initial_release::UiCommand> original{nullptr};bool frame_locked=false;std::string frame_cause;uint64_t frame_fields=0;std::map<std::string,nlohmann::json> ui_proofs;bool ui_proof_locked=false;std::string ui_proof_cause;std::optional<Pending> pending;uint64_t captured=0;UiEntryOwner owner;IntentReceipt intent;};
std::atomic<Runtime*> published{nullptr};
bool read(Runtime&,const action::Context&,const action::Binding&,Capture& out,PoolState& state,std::string& e){
    ++fixture::reads;if((fixture::reads==1&&!fixture::before_ok)||(fixture::reads>1&&!fixture::after_ok)){e="actual pool read failed";return false;}
    out=fixture::current;state=fixture::pool;return true;
}
bool full_code(Runtime&,std::string& e){if(!fixture::code_ok)e="actual code owner differs";return fixture::code_ok;}
bool fail(std::string& e,const std::string& why){e=why;return false;}
#include "developer_frame_calls.inc"
template<class Copy>bool read_window(Copy&&,const Capture&,uintptr_t& out,std::string& e){if(!fixture::window_ok){e="actual DevelopmentWindow owner differs";return false;}out=0x5000;return true;}
uint32_t invoke_ui(initial_release::UiCommand original,uintptr_t,uintptr_t,bool hire,NativeReceipt* receipt){
    ++fixture::ui_calls;receipt->attempted=true;receipt->returned=fixture::ui_returned;receipt->accepted=fixture::ui_accepted;
    receipt->label_confirmed=fixture::ui_label;receipt->cleanup_confirmed=fixture::ui_clean;
    auto* owner=published.load();if(reinterpret_cast<uintptr_t>(original)==owner->owner.target){
        if(fixture::ui_effect){owner->pending=Pending{hire,"33333333-3333-4333-8333-333333333333",fixture::context,fixture::binding,fixture::current.pin.origin};++owner->captured;}
        if(fixture::ui_unexpected_effect)--fixture::current.pin.ledger.cash;
        return 0;
    }
    if(fixture::ui_effect){fixture::current.state.business_count+=hire?1:-1;action::Ledger after;std::string e;
        if(!expected_payment(hire,fixture::current.pin.ledger,after,e))throw std::runtime_error(e);fixture::current.pin.ledger=after;}
    if(fixture::wrong_owner)++fixture::current.local.lounge;
    if(fixture::wrong_immediate_actor)fixture::current.state.rows[49].position[0]=1;
    return 0;
}
uint32_t invoke_count(uintptr_t,uintptr_t,int32_t count){
    ++fixture::count_calls;if(fixture::count_fault)return 0xc0000005;
    fixture::current.state.lounge_count=count;fixture::pool.count=count;
    for(size_t i=0;i<50;++i){auto& r=fixture::current.state.rows[i];auto& p=fixture::pool.rows[i];
        r.employed=p.employed=i<size_t(count);if(r.employed)r.active=p.active=true;}
    if(fixture::wrong_final_actor)fixture::pool.rows[49].position[0]=1;
    return 0;
}
#include "developer_perform.inc"
#include "developer_adopt_frame.inc"
#include "developer_ui_fixture.inc"
}
}
namespace {
namespace runtime=staff::action_runtime;
constexpr const char* id="11111111-1111-4111-8111-111111111111";
constexpr const char* snapshot="22222222-2222-4222-8222-222222222222";
void reset(runtime::Runtime& owner,bool hire=true){
    using namespace fixture;ui_calls=count_calls=reads=0;construction_preflights=construction_applies=0;jobs_ok=construction_ok=true;construction_partial=false;code_ok=window_ok=before_ok=after_ok=immediate_ok=true;
    ui_accepted=ui_returned=ui_clean=ui_label=ui_effect=true;wrong_owner=wrong_immediate_actor=wrong_final_actor=count_fault=false;
    session_ok=role_ok=display_ok=true;carried=scalar_ledger_change=false;scalar_calls=fail_scalar=display_calls=0;cause.clear();context={};context.role=action::Role::Host;context.snapshot_uuid=snapshot;
    controller={{"pending",true},{"action_uuid",id}};current={};pool={};pool.count=hire?1:2;
    current.local.lounge=0x3000;current.state.business_count=current.state.lounge_count=pool.count;
    current.pin.origin.uuid=snapshot;current.pin.origin.world_epoch=7;current.pin.origin.generation=9;
    auto& l=current.pin.ledger;l.cash=l.record_cash=93640;l.record_present=1;l.record_day=l.flat_day=0;
    for(int32_t i=0;i<50;++i){staff::Row r;r.id=49-i;r.slot=i;r.employed=r.active=i<pool.count;current.state.rows.push_back(r);
        pool.rows.push_back({r.id,r.state,r.employed,r.active,r.on_map,r.job_link,r.position});pool.indoor.push_back(r.id);current.local.developers.push_back(0x10000+uintptr_t(i)*0x1000);}
    update_ok=network_ok=true;show_visible=ui_unexpected_effect=false;mt2mp::Config::instance().local_test=true;
    owner.operations.clear();owner.ready=true;owner.frame_locked=false;owner.frame_cause.clear();owner.frame_fields=0;owner.thread=1;owner.owner.target=owner.image+0x6c7b70;owner.pending.reset();owner.captured=0;owner.intent={};owner.ui_proofs.clear();owner.ui_proof_locked=false;owner.ui_proof_cause.clear();runtime::published=&owner;
}
action::Request request(bool hire=true){action::Request r;r.action=hire?action::Action::HireDeveloper:action::Action::FireDeveloper;r.action_uuid=id;r.snapshot_uuid=snapshot;r.canonical_record_key="staff:developers";return r;}
action::Outcome run(bool hire=true,const action::Evidence* host=nullptr){return runtime::perform(fixture::binding,request(hire),host,fixture::cause);}
void unchanged(){fixture::need(!fixture::ui_calls&&!fixture::count_calls,"preflight rejection performed native work");}
void unknown_once(runtime::Runtime& owner,bool hire=true,const action::Evidence* host=nullptr){
    using namespace fixture;need(run(hire,host)==action::Outcome::Unknown&&!cause.empty(),"partial native attempt did not preserve Unknown/cause");
    const auto calls=ui_calls;need(calls==1,"partial action did not perform exactly one original UI call");
    need(run(hire,host)==action::Outcome::Unknown&&ui_calls==calls&&owner.operations.size()==1,"partial action replayed native effects");
}
}
int main(){try{
    using namespace fixture;runtime::Runtime owner;
    for(bool hire:{true,false}){
        reset(owner,hire);need(run(hire)==action::Outcome::Confirmed&&ui_calls==1&&count_calls==1,"native original Hire/Fire body did not confirm exact once");
        const auto evidence=owner.operations.at(id).evidence;need(evidence.developers_before&&evidence.developers_after,"confirmed action lost full native pools");
        need(hire?evidence.after.cash==93140:evidence.after.cash==93640,"native action payment differs");
        need(run(hire)==action::Outcome::Unknown&&ui_calls==1&&count_calls==1,"confirmed UUID replayed original UI or SetDevCount");
        reset(owner,hire);context.role=action::Role::Client;need(run(hire,&evidence)==action::Outcome::Confirmed&&ui_calls==1&&count_calls==1,"Client authoritative effect did not use the actual native body");
        reset(owner,hire);context.role=action::Role::Client;auto bad=evidence;bad.before.line_counts[19]=1;
        need(run(hire,&bad)==action::Outcome::RejectedNoEffect,"Client mismatched complete BEFORE ledger reached native work");unchanged();
        reset(owner,hire);context.role=action::Role::Client;bad=evidence;bad.developers_before->rows[49].active=true;
        need(run(hire,&bad)==action::Outcome::RejectedNoEffect,"Client mismatched BEFORE pool reached native work");unchanged();
        reset(owner,hire);context.role=action::Role::Client;bad=evidence;bad.after.line_counts[19]=1;unknown_once(owner,hire,&bad);
    }
    for(int failure=0;failure<6;++failure){reset(owner);
        if(failure==0)controller["pending"]=false;if(failure==1)controller["action_uuid"]="stale";
        if(failure==2)session_ok=false;if(failure==3)before_ok=false;if(failure==4)code_ok=false;if(failure==5)window_ok=false;
        need(run()==action::Outcome::RejectedNoEffect&&!cause.empty(),"actual preflight failure was hidden");unchanged();}
    reset(owner);current.pin.ledger.cash=current.pin.ledger.record_cash=499;need(run()==action::Outcome::RejectedNoEffect,"unaffordable Hire reached Pay");unchanged();
    for(int failure=0;failure<10;++failure){reset(owner);
        if(failure==0)ui_effect=false;if(failure==1)ui_accepted=false;if(failure==2)ui_returned=false;if(failure==3)ui_clean=false;if(failure==4)ui_label=false;
        if(failure==5)immediate_ok=false;if(failure==6)wrong_owner=true;if(failure==7)wrong_immediate_actor=true;if(failure==8)count_fault=true;if(failure==9)wrong_final_actor=true;
        unknown_once(owner);if(failure<8)need(!count_calls,"unconfirmed original UI effect was force-projected by SetDevCount");}
    reset(owner);after_ok=false;unknown_once(owner);need(count_calls==1,"post-effect read failure did not retain original count attempt");
    reset(owner);display_ok=false;unknown_once(owner);need(count_calls==1&&display_calls==1,"action cache ownership failure lost original effect/Unknown");
    bool attempted=false,unknown=false;
    reset(owner);context.role=action::Role::Client;auto target=pool;target.rows[0].state=6;target.rows[1].active=true;
    need(runtime::preflight_frame(context,binding,target,cause)&&!scalar_calls,"frame preflight performed a native property");
    need(runtime::adopt_frame(context,binding,target,attempted,unknown,cause)&&attempted&&!unknown&&scalar_calls==2&&display_calls==1&&owner.frame_fields==2,"actual frame adopter did not use original scalar fields with full readback");
    need(runtime::adopt_frame(context,binding,target,attempted,unknown,cause)&&!attempted&&scalar_calls==2&&display_calls==1,"duplicate full50 frame replayed original properties");
    for(int failure=0;failure<4;++failure){reset(owner);context.role=action::Role::Client;target=pool;target.rows[0].state=6;
        if(failure==0)carried=true;if(failure==1)target.rows[0].job_link=true;if(failure==2)target.rows[0].state=4;if(failure==3)context.role=action::Role::Host;
        need(!runtime::adopt_frame(context,binding,target,attempted,unknown,cause)&&!attempted&&!unknown&&!scalar_calls&&!owner.frame_locked,"native frame owner/non-indoor preflight performed effects");}
    for(int failure=0;failure<3;++failure){reset(owner);context.role=action::Role::Client;target=pool;target.rows[0].state=7;
        if(failure==0)fail_scalar=1;if(failure==1)scalar_ledger_change=true;if(failure==2)display_ok=false;
        need(!runtime::adopt_frame(context,binding,target,attempted,unknown,cause)&&attempted&&unknown&&owner.frame_locked&&!cause.empty(),"actual frame partial failure was not locked Unknown");
        const auto original=cause;need(!runtime::adopt_frame(context,binding,target,attempted,unknown,cause)&&!attempted&&scalar_calls==1&&cause==original,"locked frame replayed a native property or replaced original cause");
        need(!runtime::preflight_frame(context,binding,target,cause)&&cause==original,"locked frame preflight hid its retained original cause");}
    const auto construction_target=[&](){auto x=pool;staff::construction::State extension;for(const auto& row:x.rows)extension.actors.push_back({row.id,-1,{0,0,0,1},{1,1,1}});extension.jobs.push_back({"action:11111111-1111-4111-8111-111111111111",true,false,-1});x.construction=extension;return x;};
    reset(owner);context.role=action::Role::Client;target=construction_target();
    need(runtime::preflight_frame(context,binding,target,cause)&&construction_preflights==1&&!scalar_calls,"construction preflight missed native route");
    need(runtime::adopt_frame(context,binding,target,attempted,unknown,cause)&&attempted&&!unknown&&construction_applies==1&&display_calls==1&&!scalar_calls,"construction apply missed native route or used scalar fallback");
    auto dropped=pool;dropped.construction.reset();need(!runtime::adopt_frame(context,binding,dropped,attempted,unknown,cause)&&!attempted&&construction_applies==1,"retained construction silently dropped");
    reset(owner);context.role=action::Role::Client;target=construction_target();jobs_ok=false;
    need(!runtime::adopt_frame(context,binding,target,attempted,unknown,cause)&&!attempted&&!construction_applies&&cause=="actual local Job owner changed","binding failure lost original cause");
    reset(owner);context.role=action::Role::Client;target=construction_target();construction_partial=true;
    need(!runtime::adopt_frame(context,binding,target,attempted,unknown,cause)&&attempted&&unknown&&owner.frame_locked,"construction partial effect did not lock");
    const auto construction_cause=cause;need(!runtime::adopt_frame(context,binding,target,attempted,unknown,cause)&&!attempted&&construction_applies==1&&cause==construction_cause,"construction Unknown replayed or cause replaced");
    reset(owner);context.role=action::Role::Client;target=construction_target();display_ok=false;
    need(!runtime::adopt_frame(context,binding,target,attempted,unknown,cause)&&attempted&&unknown&&owner.frame_locked&&construction_applies==1,"construction cache failure not Unknown");
    const auto ui_args=[](const std::string& command="Hire"){return nlohmann::json{{"command",command},{"token",id},{"native_origin_uuid",snapshot},{"epoch",uint64_t(7)},{"generation",uint64_t(9)},{"expected_count",1}};};
    const auto ui_ready=[&](){reset(owner);context.role=action::Role::Client;controller={{"initialized",true},{"started",true},{"pending",false},{"locked",false}};};
    nlohmann::json ui;
    for(const auto* command:{"Show","Hire","Fire"}){ui_ready();const auto before=current.state;
        need(runtime::ui_fixture(ui_args(command),ui,cause)&&ui.at("admitted")==true&&ui.at("admission_only")==true&&ui.at("execution_ack")==false&&ui.at("unknown")==false&&ui_calls==1&&!count_calls&&current.state==before,
            "fixed live original UI entry did not prove admission without native gameplay");
        need(command==std::string("Show")?ui.at("shown")==true:ui.at("captured")==true&&ui.at("action_uuid")=="33333333-3333-4333-8333-333333333333","fixed UI receipt lost shown/capture UUID distinction");
        need(!runtime::ui_fixture(ui_args(command),ui,cause)&&ui.at("replay_rejected")==true&&ui_calls==1,"fixed UI token replayed the original entry");}
    for(int guard=0;guard<13;++guard){ui_ready();auto arguments=ui_args();
        if(guard==0)owner.thread=2;if(guard==1)update_ok=false;if(guard==2)controller["pending"]=true;if(guard==3)controller["locked"]=true;
        if(guard==4)network_ok=false;if(guard==5)arguments["epoch"]=uint64_t(8);if(guard==6)arguments["expected_count"]=0;if(guard==7)arguments["generation"]=true;
        if(guard==8)code_ok=false;if(guard==9)window_ok=false;if(guard==10)controller["initialized"]=false;
        if(guard==11){owner.frame_locked=true;owner.frame_cause="original frame property Unknown";}if(guard==12)mt2mp::Config::instance().local_test=false;
        need(!runtime::ui_fixture(arguments,ui,cause)&&!ui_calls&&!count_calls&&owner.ui_proofs.empty()&&!cause.empty(),"fixed UI owner/Controller/Origin guard reached an original entry");
        if(guard==11)need(cause=="original frame property Unknown","fixed UI replaced the original frame Unknown cause");}
    for(int failure=0;failure<3;++failure){ui_ready();if(failure==0)ui_clean=false;if(failure==1)ui_unexpected_effect=true;if(failure==2)ui_effect=false;
        need(!runtime::ui_fixture(ui_args(),ui,cause)&&ui.at("unknown")==true&&ui.at("execution_ack")==false&&owner.ui_proof_locked&&ui_calls==1&&!cause.empty(),"uncertain UI entry produced a false ACK or lost its cause");
        const auto original=cause;auto another=ui_args();another["token"]="44444444-4444-4444-8444-444444444444";
        need(!runtime::ui_fixture(another,ui,cause)&&ui_calls==1&&cause==original,"UI Unknown allowed another native entry or replaced original cause");}
    for(int phase=0;phase<5;++phase){bool changed=phase!=0,completed=phase==1,locked=false;latch::ApplyReceipt receipt;receipt.confirmed=true;std::string error;
        if(phase==2)error="original later tutorial preflight failure";if(phase==3)receipt.cause="original later Day failure";
        {latch::DeveloperFailureLatch actual{changed,completed,locked,receipt,error};}
        if(phase<2)need(!locked&&!receipt.unknown&&receipt.confirmed,"no-effect or completed frame was falsely locked Unknown");
        else{need(locked&&receipt.attempted&&receipt.unknown&&!receipt.confirmed&&!receipt.cause.empty(),"later frame failure after original property produced false confirmed success");
            if(phase==2)need(receipt.cause==error,"later tutorial failure cause was overwritten");if(phase==3)need(receipt.cause=="original later Day failure","existing Day cause was overwritten");}}
    std::cout<<"PASS "<<checks<<" actual native action boundary assertions\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
