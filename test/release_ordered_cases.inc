namespace release_ordered_fixture {
namespace real=mt2mp::action_session;
namespace a {
using namespace mt2mp::action_session;
struct Selected {Context value;bool available=true;bool current(Context& out,std::string& e){if(!available){e="session unavailable";return false;}out=value;return true;}};
inline Selected selected;inline Selected& selected_session(){return selected;}
namespace tick_commands::authority {
inline bool use_real=false,enabled=false,valid=false;inline Context context;inline Binding binding;inline Request request;
inline bool active(){return use_real?real::tick_commands::authority::active():enabled;}
inline bool matches(const Context& c,const Binding& b,const Request& q,std::string& e){if(use_real)return real::tick_commands::authority::matches(c,b,q,e);if(!enabled||!valid||c!=context||b!=binding||q!=request){e="consumed authority differs";return false;}return true;}
}
}
struct Config {bool is_host=true;static Config& instance(){static Config value;return value;}};
namespace typed_action_runtime {
inline nlohmann::json controller;inline a::Binding binding;inline bool ordered=true,available=true,inside_original=false;inline int submitted=0;inline p::Intent intent;
inline nlohmann::json controller_status(){return controller;}
inline bool original_go_admission(bool& inside,std::string&){inside=inside_original;return true;}
inline bool network_binding_with_mode(const std::string&,a::Binding& out,std::string& e,bool* mode){if(!available){e="binding unavailable";return false;}out=binding;*mode=ordered;return true;}
inline bool submit_release_edit(const std::string&,const p::Intent& value,std::string&){++submitted;intent=value;return true;}
}
struct Pending {a::Context context;a::Binding binding;p::Intent intent;std::string id;uintptr_t window=0;bool ordered=false;};
struct Operation {a::Context context;a::Binding binding;a::Request request;a::Evidence evidence;std::optional<p::Local> ordered_local;std::optional<p::State> ordered_after;bool attempted=false,returned=false,confirmed=false,retired=false,ui_attempted=false,ui_returned=false,ordered=false;uint32_t calls=0,default_steps=0;std::string cause;};
using UI=bool(*)(void*,void*);struct Route{UI original=nullptr;};
struct Runtime {uint32_t thread=GetCurrentThreadId();uintptr_t image=10,presentation_window=0;bool ready=true,unknown=false;std::optional<Pending> pending;std::map<std::string,Pending> presentations;std::map<std::string,Operation> operations;std::array<Route,3> routes;uint64_t captures=0,submitted=0,rejected=0;std::string cause;};
inline std::atomic<Runtime*> published;inline p::State state;inline p::Local owner;inline a::Ledger money;inline bool readable=true,native_fault=false,drift=false;inline int native_calls=0;inline float selected_clock=0;
inline bool fail(std::string& e,const std::string& why){e=why;return false;}inline void rejected(Runtime& r,const std::string& e){++r.rejected;r.cause=e;}
namespace live {
struct Pin {float selected=0;};struct NativePort{NativePort(const a::Context&,const a::Binding&,void*){}bool read(Pin& p,std::string&){p.selected=selected_clock;return true;}};
namespace apply_timing {enum class PageBoundary{OriginalCall};struct PageReadScope{explicit PageReadScope(PageBoundary){}};}
}
namespace native {inline bool read_resolver_receipt(uintptr_t& image,uint32_t& thread,std::string& sha,std::string&){image=10;thread=GetCurrentThreadId();sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";return true;}}
#include "release_context_actual.inc"
inline bool sample(Runtime& r,const a::Context& c,const a::Binding& b,p::Local& l,p::State& s,a::Ledger& out,std::string& e,bool ordered=false){live::Pin pin;if(!readable)return fail(e,"native owner unavailable");if(!context(r,c,b,pin,e,ordered))return false;l=owner;s=state;out=money;return true;}
inline bool code(Runtime&,std::string&){return true;}inline bool window(Runtime&,uintptr_t target,size_t index,std::string& e){return (target==1&&index==1)||fail(e,"planner owner differs");}
inline uint32_t command_label(uintptr_t,void* record,uintptr_t& out){out=uintptr_t(record);return 0;}
inline bool string(Runtime&,uintptr_t address,std::string& out,std::string&){out=*reinterpret_cast<std::string*>(address);return true;}
template<class T>bool at(Runtime&,uintptr_t,size_t,T& out,std::string&){out={};return true;}
inline uint32_t token_text(uintptr_t,void*,int,uintptr_t&){return 1;}inline uint32_t pane_text(uintptr_t,uintptr_t,uintptr_t&){return 1;}inline bool child(Runtime&,uintptr_t,uintptr_t,std::string&){return false;}
inline bool cycle_capture(Runtime&,size_t,void*,void*,const std::string&,bool& handled,bool&,std::string&){handled=false;return true;}
inline std::string uuid(std::string&){return "20000000-0000-0000-0000-000000000002";}
inline bool defaults(Runtime&,uintptr_t,p::Defaults& out,Operation&,std::string&){out=::defaults;return true;}
namespace n {
struct String{explicit String(const std::string&){};};
inline uint32_t set_type(uintptr_t,uintptr_t,const std::array<int32_t,3>& next,int32_t type,uint32_t& calls){++native_calls;calls=1;if(native_fault)return 123;state.planned=next;state.type=type;if(drift)++state.allocated_funds;return 0;}
inline uint32_t assign(uintptr_t,uintptr_t,const String&){return 0;}inline uint32_t custom(uintptr_t,bool,bool){return 0;}inline uint32_t reset(uintptr_t,uintptr_t,bool){return 0;}
}
inline a::Outcome cycle_perform(Runtime&,const a::Binding&,const a::Request&,const a::Evidence*,std::string&){return a::Outcome::RejectedNoEffect;}
inline bool cycle_observe(Runtime&,const a::Binding&,const a::Request&,a::Evidence&,std::string&){return false;}
struct CycleOp{bool retired=false;};struct Cycle{std::map<std::string,CycleOp> operations;std::optional<int> pending;std::string finalize_id;} cycle_runtime;
namespace management_runtime {inline bool pending_ui(){return false;}}
inline bool presentation(Runtime&,Operation& op,std::string&){op.ui_attempted=op.ui_returned=true;return true;}
inline bool original_ui(void*,void*){return false;}
#include "release_capture_actual.inc"
#include "release_ui_entry_actual.inc"
#include "release_flush_actual.inc"
#include "release_read_ordered_actual.inc"
#include "release_perform_actual.inc"
#include "release_observe_actual.inc"
#include "release_retire_action_actual.inc"
inline void reset(Runtime& r,bool host=true){r=Runtime{};r.routes[1].original=original_ui;published.store(&r);state=initial();owner={};owner.manager=2;owner.release=3;owner.software=4;owner.random=5;money={};money.cash=money.record_cash=100000;money.record_present=1;readable=true;native_fault=drift=false;native_calls=0;selected_clock=1;
    a::selected.value={77,host?1u:2u,1,3,host?a::Role::Host:a::Role::Client,snapshot};a::selected.available=true;Config::instance().is_host=host;typed_action_runtime::binding={3,9};typed_action_runtime::ordered=true;typed_action_runtime::available=true;typed_action_runtime::submitted=0;typed_action_runtime::controller={{"started",true},{"locked",false},{"pending",false},{"ordered",true}};a::tick_commands::authority::use_real=false;a::tick_commands::authority::enabled=false;}
inline a::Request request(){a::Request q;q.action=a::Action::ReleaseEdit;q.peer=2;q.sequence=1;q.action_uuid=id;q.snapshot_uuid=snapshot;q.canonical_record_key="release:editing";q.release_edit.command=p::Command::SetType;q.release_edit.type=2;return q;}
inline void authority(const a::Request& q){typed_action_runtime::controller["pending"]=true;typed_action_runtime::controller["action_uuid"]=q.action_uuid;a::tick_commands::authority::enabled=a::tick_commands::authority::valid=true;a::tick_commands::authority::context=a::selected.value;a::tick_commands::authority::binding=typed_action_runtime::binding;a::tick_commands::authority::request=q;}
struct Session:real::Session{bool current(real::Context& out,std::string& e)override{return a::selected.current(out,e);}};
struct Engine:real::Engine {bool fresh=true;bool bind(const std::string&,real::Binding& out,std::string&)override{out=typed_action_runtime::binding;return true;}bool current(const real::Binding& b,const std::string& s,std::string& e)override{return (fresh&&b==typed_action_runtime::binding&&s==snapshot)||fail(e,"engine binding stale");}bool ledger(const real::Binding&,real::Ledger& out,std::string&)override{out=money;return true;}real::Outcome execute_remove(const real::Binding&,const real::Request&,std::string&)override{return real::Outcome::RejectedNoEffect;}real::Outcome apply_remove(const real::Binding&,const real::Request&,const real::Evidence&,std::string&)override{return real::Outcome::RejectedNoEffect;}bool observe_remove(const real::Binding&,const real::Request&,real::Evidence&,std::string&)override{return false;}bool retire(const real::Binding&,const real::Request&,std::string&)override{return true;}};
inline void consumed(bool host_role){namespace tick=real::tick_commands;Runtime r;reset(r,host_role);std::string label="SetMinor";need(ui_entry(1,reinterpret_cast<void*>(1),&label)&&r.pending&&native_calls==0,"actual type UI capture");auto q=request();authority(q);a::tick_commands::authority::use_real=true;
    tick::Config hc;hc.context=a::selected.value;hc.context.local_peer=1;hc.context.role=real::Role::Host;tick::Config lc;lc.context=a::selected.value;auto validate=[](const tick::Input&,std::string&){return true;};tick::Queue host(hc,validate),client(lc,validate);tick::Input input{77,snapshot,q};tick::Assigned assigned;tick::Batch batch;tick::Receipt receipt;std::string e;
    need(host.offer(2,input,assigned,e)==tick::Admission::Accepted&&host.seal(batch,e),"release scope offer");if(!host_role)need(client.admit(1,batch,e)==tick::Admission::Accepted,"release client admission");auto& queue=host_role?host:client;Session session;Engine engine;auto current=[](std::string&){return true;};auto advance=[](uint64_t,std::string&){return real::Outcome::Confirmed;};
    need(queue.consume(1,current,[&](const tick::Assigned& command,std::string& cause){auto result=tick::authority::run(queue,session,engine,typed_action_runtime::binding,command,[&](std::string& why){auto wrong=q;wrong.action_uuid="wrong";need(!tick::authority::matches(a::selected.value,typed_action_runtime::binding,wrong,why),"release wrong UUID authorized");auto bad=command;++bad.order;real::Context actual;uint64_t tick_value=0;need(!queue.current_execution(bad,actual,tick_value,why),"release wrong order authorized");
        need(perform(typed_action_runtime::binding,q,nullptr,why)==a::Outcome::Confirmed&&native_calls==1&&state.type==2,"actual ordered SetType original");a::Evidence out;engine.fresh=false;need(!observe(typed_action_runtime::binding,q,out,why),"release stale authority observed");engine.fresh=true;need(observe(typed_action_runtime::binding,q,out,why)&&retire(typed_action_runtime::binding,q,why),"release actual observation/retirement");return real::Outcome::Confirmed;},cause);if(result==real::Outcome::Confirmed&&!queue.bind_evidence(command,std::string(64,'b'),cause))return real::Outcome::Unknown;return result;},advance,receipt,e),"release actual authority queue execution");
    need(!tick::authority::matches(a::selected.value,typed_action_runtime::binding,q,e),"release authority escaped scope");}
inline void run(){Runtime r;std::string e;reset(r);nlohmann::json observed;need(!read_ordered_state(r,observed,e)&&e=="Release ordered read requires actual original Go scope","Release read admitted outside original Go");typed_action_runtime::inside_original=true;e.clear();need(read_ordered_state(r,observed,e)&&observed.at("read_only")==true&&observed.at("state").at("type")==1&&observed.at("state").at("allocated_funds")==0&&native_calls==0,"actual ordered Release read");typed_action_runtime::ordered=false;need(!read_ordered_state(r,observed,e),"legacy Release ordered read admitted");typed_action_runtime::ordered=true;typed_action_runtime::inside_original=false;std::string label="SetMinor";need(ui_entry(1,reinterpret_cast<void*>(1),&label)&&r.pending&&native_calls==0,"actual release capture did not retain intent");need(!read_ordered_state(r,observed,e),"Release read admitted retained UI input");flush_pending(r,true);need(!r.pending&&typed_action_runtime::submitted==1&&typed_action_runtime::intent.type==2&&native_calls==0,"actual release held flush");flush_pending(r,true);need(typed_action_runtime::submitted==1,"release flush replay");
    reset(r);state.type=2;label="SetHotfix";need(ui_entry(1,reinterpret_cast<void*>(1),&label)&&r.pending&&r.pending->intent.type==1&&native_calls==0,"actual hotfix UI capture");flush_pending(r,true);need(typed_action_runtime::submitted==1&&typed_action_runtime::intent.type==1,"actual hotfix held flush");label="SetMinor";
    for(int fault=0;fault<3;++fault){reset(r);need(ui_entry(1,reinterpret_cast<void*>(1),&label),"release capture setup");if(fault==0)++a::selected.value.session;else if(fault==1)++typed_action_runtime::binding.native_generation;else typed_action_runtime::ordered=false;flush_pending(r,true);need(!typed_action_runtime::submitted&&r.rejected==1,"stale release capture submitted");}
    reset(r);label="SetCustomTitle";need(!ui_entry(1,reinterpret_cast<void*>(1),&label)&&!r.pending,"unsupported ordered release edit admitted");
    for(int fault=0;fault<5;++fault){reset(r);auto q=request();authority(q);a::Evidence host;const a::Evidence* evidence=nullptr;if(fault==0)evidence=&host;else if(fault==1)a::tick_commands::authority::request.action_uuid="wrong";else if(fault==2)Config::instance().is_host=false;else if(fault==3)readable=false;else q.release_edit.command=p::Command::UseCustom;need(perform(typed_action_runtime::binding,q,evidence,e)==a::Outcome::RejectedNoEffect&&native_calls==0,"invalid ordered release reached original");}
    reset(r);auto missing=request();typed_action_runtime::controller["pending"]=true;typed_action_runtime::controller["action_uuid"]=missing.action_uuid;need(perform(typed_action_runtime::binding,missing,nullptr,e)==a::Outcome::RejectedNoEffect&&native_calls==0,"ordered release without consumed scope reached original");
    for(int fault=0;fault<2;++fault){reset(r);auto q=request();authority(q);if(fault==0)native_fault=true;else drift=true;need(perform(typed_action_runtime::binding,q,nullptr,e)==a::Outcome::Unknown&&native_calls==1,"failed release original confirmed");need(perform(typed_action_runtime::binding,q,nullptr,e)!=a::Outcome::Confirmed&&native_calls==1,"release original replayed");}
    for(int fault=0;fault<5;++fault){reset(r);auto q=request();authority(q);need(perform(typed_action_runtime::binding,q,nullptr,e)==a::Outcome::Confirmed,"release observation drift setup");if(fault==0)++state.random[0];else if(fault==1)++state.allocated_funds;else if(fault==2)++owner.release;else if(fault==3)++money.line_income[19];else typed_action_runtime::controller["ordered"]=false;a::Evidence out;need(!observe(typed_action_runtime::binding,q,out,e)&&!retire(typed_action_runtime::binding,q,e),"changed release owner/state/Ledger/mode retired");}
    reset(r,false);typed_action_runtime::controller["ordered"]=false;typed_action_runtime::ordered=false;selected_clock=1;auto q=request();authority(q);a::tick_commands::authority::enabled=false;a::Evidence host;need(perform(typed_action_runtime::binding,q,&host,e)==a::Outcome::RejectedNoEffect&&native_calls==0,"legacy Client Clock0 guard weakened");
    for(bool is_host:{true,false}){reset(r,is_host);typed_action_runtime::controller["ordered"]=false;typed_action_runtime::ordered=false;selected_clock=0;q=request();authority(q);a::tick_commands::authority::enabled=false;bool confirmation=false;const auto next=apply(q.release_edit,state,confirmation);a::Evidence evidence;evidence.before=evidence.after=money;evidence.release_edit_before=proof(state);evidence.release_edit_after=proof(next);need(perform(typed_action_runtime::binding,q,is_host?nullptr:&evidence,e)==a::Outcome::Confirmed&&native_calls==1,"legacy Release role/evidence changed");}
    consumed(true);consumed(false);
}
}
