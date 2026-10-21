#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "object_ui_probe/contract.h"
#include "ui_remove/diagnostic/ui_diagnostic.h"
#include <iostream>
#include <stdexcept>
#include <map>
#include <set>
#include <cstring>
#include <atomic>
namespace contract=mt2mp::game::object_ui_probe;
namespace common=mt2mp::action_session::ui_diagnostic;
int object_checks=0;
void need(bool value,const char* cause){++object_checks;if(!value)throw std::runtime_error(cause);}
namespace actual_command {
struct Job {uint32_t attempted_mask=0,returned_mask=0,exception=0,cleanup_exception=0;bool command_accepted=false;};
#include "object_ui_command.inc"
inline int stage_fault=0,constructs=0,labels=0,commands=0,destroys=0;
inline bool label_result=true,command_result=true;
inline const char* expected_label="Duplicate";
void fault(int stage){if(stage_fault==stage)RaiseException(0xe0010000u+unsigned(stage),0,0,nullptr);}
void construct(void*,const char* label){++constructs;need(std::string(label)==expected_label,"actual original record label preserved");fault(1);}
bool label(const void*,const char* value){++labels;need(std::string(value)==expected_label,"actual original label equality uses exact command");fault(2);return label_result;}
bool invoke(void* window,void*){++commands;need(uintptr_t(window)==0x1234,"actual visible InfoWindow receiver preserved");fault(3);return command_result;}
void destroy(void*){++destroys;fault(4);}
void reset(){stage_fault=constructs=labels=commands=destroys=0;label_result=command_result=true;}
void checks(){
    common::Api api;api.construct=construct;api.label_equals=label;api.command=invoke;api.destroy=destroy;
    for(const char* name:{"Duplicate","Group","Ungroup"}){reset();expected_label=name;Job receipt;
        need(command(&api,0x1234,name,&receipt)==0&&receipt.command_accepted&&receipt.attempted_mask==60&&receipt.returned_mask==60,"actual command invokes and destroys one original Record");
        need(constructs==1&&labels==1&&commands==1&&destroys==1,"actual command has no replay or repeated native mutation");
    }
    reset();expected_label="Duplicate";label_result=false;Job mismatch;
    need(command(&api,0x1234,expected_label,&mismatch)==0&&!mismatch.command_accepted&&commands==0&&destroys==1,"failed genuine label skips UI mutation and still cleans Record");
    reset();command_result=false;Job refused;
    need(command(&api,0x1234,expected_label,&refused)==0&&!refused.command_accepted&&commands==1&&destroys==1,"original command false stays a rejection");
    for(int stage=1;stage<=4;++stage){reset();stage_fault=stage;Job receipt;const auto exception=command(&api,0x1234,expected_label,&receipt);
        need((stage==4?receipt.cleanup_exception:exception)==0xe0010000u+unsigned(stage),"actual original SEH cause preserved");
        need(destroys==(stage==1?0:1)&&commands<2,"fault neither destroys an unconstructed Record nor replays native command");
    }
}
}
namespace actual_preparation {
namespace probe_contract=contract;
namespace action {struct Ledger {int cash=19;bool operator==(const Ledger&)const=default;};}
struct Memory {
    std::map<uintptr_t,std::vector<unsigned char>> fields;
    template<class T>void put(uintptr_t p,const T& value){std::vector<unsigned char> bytes(sizeof value);std::memcpy(bytes.data(),&value,sizeof value);fields[p]=bytes;}
};
namespace group_reader::q {
    template<class T>bool at(Memory& memory,uintptr_t p,size_t offset,T& value,std::string& error){
        const auto found=memory.fields.find(p+offset);if(found==memory.fields.end()||found->second.size()!=sizeof value){error="original field read failed";return false;}
        std::memcpy(&value,found->second.data(),sizeof value);return true;
    }
    bool typed(Memory& memory,uintptr_t image,uintptr_t p,uintptr_t ap,std::string& error){uintptr_t actual=0;
        if(!at(memory,p,0,actual,error))return false;if(actual!=image+ap){error="original class differs";return false;}return true;}
}
struct Runtime {Memory memory;uintptr_t image=0x100000;};
struct Job {
    struct Input {std::string command="Duplicate";} input;
    struct World {uintptr_t selection=0x400000;} world;
    action::Ledger money;
    uint64_t captures=0,submitted=0,rejected=0,revision=7;
    std::set<std::string> prior_operations;
    uint32_t attempted_mask=3,returned_mask=3,exception=0,cleanup_exception=0;
    bool command_accepted=false,preparation_reconciled=false;
    std::string action_uuid,phase="unknown",cause="object_ui_exact_current_selection_differs";
};
namespace duplicate_edit {inline uint64_t captures=0,submitted=0,rejections=0;inline std::map<std::string,int> operations;}
namespace group_edit {inline uint64_t captures=0,submitted=0,rejections=0;inline std::map<std::string,int> operations;}
namespace typed_action_runtime {
    inline contract::Json status;
    contract::Json controller_status(){return status;}
}
inline bool current_ok=true,ledger_ok=true;inline int fresh_current=0,fresh_ledger=0;inline action::Ledger actual_money;
bool current(Runtime&,const Job&,std::string& error){++fresh_current;if(!current_ok){error="original world changed";return false;}return true;}
bool ledger(action::Ledger& out,std::string& error){++fresh_ledger;if(!ledger_ok){error="original ledger failed";return false;}out=actual_money;return true;}
#include "object_ui_preparation.inc"
void setup(Runtime& runtime){
    runtime.memory.fields.clear();runtime.memory.put(runtime.image+0x13df9d0,uintptr_t(0x200000));
    runtime.memory.put(0x200000,runtime.image+0x1786dd0);runtime.memory.put(0x200190,uintptr_t(0x300000));
    runtime.memory.put(0x300120,uint8_t(1));runtime.memory.put(0x4000d1,uint8_t(0));runtime.memory.put(0x4000d3,uint8_t(0));
    duplicate_edit::captures=duplicate_edit::submitted=duplicate_edit::rejections=0;duplicate_edit::operations.clear();
    group_edit::captures=group_edit::submitted=group_edit::rejections=0;group_edit::operations.clear();
    typed_action_runtime::status={{"host_revision",7},{"pending",false},{"locked",false}};
    current_ok=ledger_ok=true;fresh_current=fresh_ledger=0;actual_money={};
}
void checks(){Runtime runtime;std::string error;
    setup(runtime);Job job;need(selection_ready(runtime,job,error),"actual original selection preflight accepts enabled current cursor");
    for(int bad=0;bad<4;++bad){setup(runtime);
        if(bad==0)runtime.memory.put(0x300120,uint8_t(0));
        if(bad==1)runtime.memory.put(0x4000d1,uint8_t(1));
        if(bad==2)runtime.memory.put(0x4000d3,uint8_t(1));
        if(bad==3)runtime.memory.fields.erase(0x300120);
        need(!selection_ready(runtime,job,error)&&!error.empty(),"actual cursor/queued/read failure blocks selection before UI command");
        if(bad==3)need(error=="original field read failed","selection field failure remains visible");
    }
    for(const char* name:{"Duplicate","Group","Ungroup"}){setup(runtime);Job prepared;prepared.input.command=name;
        need(reconcile_preparation(runtime,prepared,error)&&prepared.phase=="rejected"&&prepared.preparation_reconciled,
             "actual returned preparation-only calls are safely terminal after fresh readback");
        need(fresh_current==1&&fresh_ledger==1&&prepared.cause=="object_ui_exact_current_selection_differs",
             "reconciliation verifies fresh world/ledger and retains original failure");
    }
    for(int bad=0;bad<14;++bad){setup(runtime);Job unknown;
        if(bad==0)unknown.attempted_mask|=4;
        if(bad==1)unknown.returned_mask=1;
        if(bad==2)unknown.exception=1;
        if(bad==3)unknown.command_accepted=true;
        if(bad==4)unknown.action_uuid="captured";
        if(bad==5)++duplicate_edit::captures;
        if(bad==6)++duplicate_edit::submitted;
        if(bad==7)++duplicate_edit::rejections;
        if(bad==8)duplicate_edit::operations.emplace("new",1);
        if(bad==9)typed_action_runtime::status["host_revision"]=8;
        if(bad==10)typed_action_runtime::status["pending"]=true;
        if(bad==11)++actual_money.cash;
        if(bad==12)current_ok=false;
        if(bad==13)ledger_ok=false;
        need(!reconcile_preparation(runtime,unknown,error)&&unknown.phase=="unknown"&&!unknown.preparation_reconciled,
             "command uncertainty or changed native/authority readback is never cleared");
        if(bad==12)need(error=="original world changed","fresh World failure cause retained");
        if(bad==13)need(error=="original ledger failed","fresh Ledger failure cause retained");
    }
}
}
namespace actual_observation {
namespace probe_contract=contract;
namespace action {
struct Binding {uint64_t native_epoch=7,native_generation=8;bool operator==(const Binding&)const=default;};
struct Context {std::string snapshot_uuid="shared";int session=1;bool operator==(const Context&)const=default;};
}
namespace net {
inline bool connected_value=true;inline std::string snapshot_value="shared";
namespace session_wire {bool connected(){return connected_value;}}
std::string current_snapshot_id(){return snapshot_value;}
}
namespace typed_action_runtime {
struct State {bool started=true,locked=false,pending=false;};
struct Controller {State state;action::Context context;action::Binding binding;
    State status(){return state;}action::Context bound_context(){return context;}action::Binding bound_binding(){return binding;}};
struct Session {action::Context value;bool okay=true;
    bool current(action::Context& out,std::string& error){if(!okay){error="original Session read failure";return false;}out=value;return true;}};
struct Engine {bool okay=true;int reads=0;
    bool current(const action::Binding&,const std::string&,std::string& error){++reads;if(!okay){error="original native World binding failure";return false;}return true;}};
struct BoundRuntime {uint32_t thread=0;bool lobby_retiring=false;Controller controller;Session session;Engine engine;};
inline std::atomic<BoundRuntime*> published_runtime=nullptr;
#include "object_ui_binding.inc"
}
namespace host {
struct Origin {int owner=1;bool operator==(const Origin&)const=default;};
struct WorldState {Origin origin;uintptr_t selection=101,root=102;};
bool same_world(const Origin& first,const Origin& last){return first==last;}
}
struct Runtime {bool ready=true;uint32_t thread=0;};
struct Job {struct Input{std::string snapshot="shared";}input;action::Binding binding;host::WorldState world;};
inline host::WorldState actual_world;
inline int world_reads=0;inline bool world_ok=true;
bool world(Runtime&,host::WorldState& out,std::string& error){++world_reads;if(!world_ok){error="original object world failure";return false;}out=actual_world;return true;}
#include "object_ui_current.inc"
void checks(){
    Runtime runtime;runtime.thread=GetCurrentThreadId();
    typed_action_runtime::BoundRuntime bound;bound.thread=runtime.thread;
    typed_action_runtime::published_runtime=&bound;Job job;std::string error;action::Binding binding;
    bound.controller.state.pending=true;
    need(current(runtime,job,error)&&bound.engine.reads==1&&world_reads==1,
         "actual object observer accepts its already submitted pending Controller with fresh native owner proof");
    need(!typed_action_runtime::network_binding("shared",binding,error)&&error=="network admission requires idle unlocked Controller",
         "actual original admission still refuses new commands while Controller is pending");
    bound.controller.state.pending=false;need(typed_action_runtime::network_binding("shared",binding,error),"idle original admission remains available");
    for(int bad=0;bad<9;++bad){
        bound={};bound.thread=runtime.thread;world_reads=0;world_ok=true;actual_world={};runtime.ready=true;net::connected_value=true;net::snapshot_value="shared";
        if(bad==0)bound.controller.state.locked=true;
        if(bad==1)bound.controller.state.started=false;
        if(bad==2)bound.lobby_retiring=true;
        if(bad==3)bound.session.value.session=2;
        if(bad==4)bound.controller.binding.native_generation=9;
        if(bad==5)bound.engine.okay=false;
        if(bad==6)actual_world.root=999;
        if(bad==7)net::snapshot_value="foreign";
        if(bad==8)world_ok=false;
        error.clear();need(!current(runtime,job,error)&&!error.empty(),"actual pending observation preserves every Session/binding/World/locked guard");
        if(bad==5)need(error=="original native World binding failure","native owner failure cause retained by actual observation");
        if(bad==8)need(error=="original object world failure","object current-world failure cause retained");
    }
    typed_action_runtime::published_runtime=nullptr;
}
}

namespace actual_rejection {
using json=contract::Json;
namespace probe_contract=contract;
namespace action {
using Binding=actual_observation::action::Binding;
enum class Outcome {Pending=0,Confirmed=1,RejectedNoEffect=2,Unknown=3};
struct Ledger {int cash=100;bool operator==(const Ledger&)const=default;};
}
struct Runtime {};
struct Job {
    struct Input {std::string command="Duplicate",snapshot="shared";std::vector<std::string> keys={"selected"};} input;
    action::Binding binding;action::Ledger money;
    uint64_t captures=5,submitted=4,rejected=1,revision=8;
    uint32_t attempted_mask=511,returned_mask=511,exception=0,cleanup_exception=0;
    bool command_accepted=true;std::set<std::string> prior_operations={"old"};
    std::string action_uuid,phase="unknown",cause="object_ui_observation_timeout_ms>30000_no_replay",reconciliation_cause;
    json result=json::object();
};
inline std::map<std::string,Job> jobs;
namespace duplicate_edit {
inline uint64_t captures=6,submitted=5,rejections=1;
inline std::map<std::string,int> operations;
inline json source;
json status(){return {{"last_submission",source}};}
}
namespace typed_action_runtime {
inline json controller;
inline json changed;
inline int reads=0;
json controller_status(){++reads;return reads>1&&!changed.is_null()?changed:controller;}
}
inline bool owner_ok=true,ledger_ok=true;inline action::Ledger observed_money;
inline int owner_reads=0,money_reads=0;
bool current(Runtime&,const Job&,std::string& error){++owner_reads;if(!owner_ok){error="original current owner proof failed";return false;}return true;}
bool ledger(action::Ledger& out,std::string& error){++money_reads;if(!ledger_ok){error="original current ledger read failed";return false;}out=observed_money;return true;}
#include "object_ui_rejection.inc"
void reset(){
    jobs.clear();jobs.emplace("token",Job{});duplicate_edit::captures=6;duplicate_edit::submitted=5;duplicate_edit::rejections=1;
    duplicate_edit::operations={{"old",1}};
    duplicate_edit::source={{"action_uuid","12345678-1234-4234-8234-123456789abc"},{"snapshot_uuid","shared"},
        {"selected_keys",std::vector<std::string>{"selected"}},{"native_epoch",7},{"native_generation",8},
        {"capture_count",6},{"submission_count",5},{"submitted",true},
        {"target_poses",json::array({json::array({0,0,0,1065353216,10,20,30,1065353216,1065353216,1065353216})})},
        {"cash_position",json::array({40,50,60})}};
    typed_action_runtime::controller={{"pending",false},{"locked",false},{"outcome",2},
        {"action_uuid",duplicate_edit::source.at("action_uuid")},{"host_revision",9},{"cause","host native action: exact source record hash differs"}};
    typed_action_runtime::changed=nullptr;typed_action_runtime::reads=0;
    owner_ok=ledger_ok=true;observed_money={};owner_reads=money_reads=0;
}
void checks(){
    Runtime runtime;std::string error;reset();auto& recovered=jobs.at("token");
    need(unresolved(),"unobserved original command blocks new mutations");
    need(reconcile_rejection(runtime,recovered,error)&&terminal(recovered)&&!unresolved(),
        "exact submitted action plus authenticated no-effect completion releases only this probe");
    need(recovered.action_uuid==duplicate_edit::source.at("action_uuid").get<std::string>()&&recovered.phase=="rejected"&&
         recovered.cause==typed_action_runtime::controller.at("cause").get<std::string>()&&error.empty(),"exact Host rejection cause/UUID retained without claiming confirmation");
    need(owner_reads==1&&money_reads==1&&typed_action_runtime::reads==2,"rejection uses fresh owner/ledger and stable authoritative completion");
    need(recovered.result.at("source_submission")==duplicate_edit::source&&
         recovered.result.at("prior_observation_cause")=="object_ui_observation_timeout_ms>30000_no_replay", "original observer failure and complete native source witness remain retained");
    reset();jobs.at("token").phase="waiting_for_capture_and_host";
    need(reconcile_rejection(runtime,jobs.at("token"),error)&&!unresolved(),"source rejection before timeout becomes terminal directly");
    for(int bad=0;bad<25;++bad){reset();auto& job=jobs.at("token");
        if(bad==0)job.input.command="Group";
        if(bad==1)job.returned_mask=63;
        if(bad==2)job.exception=1;
        if(bad==3)job.cleanup_exception=1;
        if(bad==4)job.command_accepted=false;
        if(bad==5)duplicate_edit::source=nullptr;
        if(bad==6)duplicate_edit::source["submitted"]=false;
        if(bad==7)duplicate_edit::source["snapshot_uuid"]="foreign";
        if(bad==8)duplicate_edit::source["selected_keys"]=json::array({"other"});
        if(bad==9)duplicate_edit::source["native_epoch"]=9;
        if(bad==10)duplicate_edit::source["native_generation"]=9;
        if(bad==11)duplicate_edit::source["capture_count"]=5;
        if(bad==12)duplicate_edit::source["submission_count"]=4;
        if(bad==13)++duplicate_edit::rejections;
        if(bad==14)duplicate_edit::operations.emplace("new",2);
        if(bad==15)typed_action_runtime::controller["pending"]=true;
        if(bad==16)typed_action_runtime::controller["locked"]=true;
        if(bad==17)typed_action_runtime::controller["outcome"]=3;
        if(bad==18)typed_action_runtime::controller["action_uuid"]="other";
        if(bad==19)typed_action_runtime::controller["host_revision"]=10;
        if(bad==20)typed_action_runtime::controller["cause"]="";
        if(bad==21)owner_ok=false;
        if(bad==22)ledger_ok=false;
        if(bad==23)++observed_money.cash;
        if(bad==24){typed_action_runtime::changed=typed_action_runtime::controller;typed_action_runtime::changed["action_uuid"]="changed";}
        error.clear();need(!reconcile_rejection(runtime,job,error)&&unresolved()&&job.phase=="unknown"&&job.action_uuid.empty(),
            "incomplete/foreign/changed/ambiguous native command never clears an unresolved probe");
        if(bad==21)need(error=="original current owner proof failed","original owner failure propagated");
        if(bad==22)need(error=="original current ledger read failed","original ledger failure propagated");
    }
    reset();jobs.at("token").captures=UINT64_MAX;
    need(!reconcile_rejection(runtime,jobs.at("token"),error)&&error=="object_ui_rejection_counter_exhausted"&&unresolved(),"counter wrap cannot fabricate exact-one submission");
}
}
#include "object_ui_late_observation_cases.inc"
int main()try{
    using contract::Json;const std::string id="12345678-1234-4234-8234-123456789abc";std::string error;contract::Input input;
    Json args={{"acceptance_token",id},{"snapshot_uuid",id},{"command","Duplicate"},{"record_keys",Json::array({"action:"+id})},{"expected_world_epoch",7},{"expected_generation",8},{"position",Json::array({1,2,3})}};
    need(contract::parse(args,input,error)&&input.position[2]==3,"actual Duplicate input schema");
    for(const char* command:{"Group","Ungroup"}){auto request=args;request["command"]=command;request.erase("position");need(contract::parse(request,input,error),"actual Group/Ungroup input schema");}
    for(int bad=0;bad<10;++bad){auto request=args;
        if(bad==0)request["acceptance_token"]="wrong";
        if(bad==1)request["snapshot_uuid"]="wrong";
        if(bad==2)request["expected_world_epoch"]=0;
        if(bad==3)request["expected_generation"]=true;
        if(bad==4)request["command"]="Move";
        if(bad==5)request["record_keys"].push_back(request["record_keys"][0]);
        if(bad==6)request["position"][0]=std::numeric_limits<double>::infinity();
        if(bad==7)request["position"][0]=true;
        if(bad==8)request["record_keys"]=Json::array();
        if(bad==9)request["other"]=1;
        need(!contract::parse(request,input,error)&&!error.empty(),"actual malformed request must fail before native effects");
    }
    const contract::Counters before{4,4,0,9},after{5,5,0,10};
    const auto complete=[&](contract::Counters measured,size_t count,bool confirmed,bool retired,bool pending,bool locked,bool same,int outcome,uint64_t elapsed,const std::string& cause){return contract::completion(before,measured,count,confirmed,retired,pending,locked,same,outcome,elapsed,cause,error);};
    need(complete(after,1,true,true,false,false,true,1,1,{})==contract::Completion::Confirmed,"one exact captured/submitted/retired authoritative operation confirms");
    need(complete(before,0,false,false,false,false,false,0,1,{})==contract::Completion::Waiting,"native return without capture is not success");
    need(complete(after,1,true,true,true,false,true,1,1,{})==contract::Completion::Waiting,"pending authoritative completion remains waiting");
    for(int bad=0;bad<10;++bad){auto measured=after;size_t count=1;bool locked=false,same=true;int outcome=1;std::string cause;
        if(bad==0)++measured.captures;
        if(bad==1)--measured.submitted;
        if(bad==2)++measured.rejected;
        if(bad==3)--measured.revision;
        if(bad==4)++measured.revision;
        if(bad==5)count=2;
        if(bad==6)locked=true;
        if(bad==7)same=false;
        if(bad==8)outcome=0;
        if(bad==9)cause="original authoritative failure";
        need(complete(measured,count,true,true,false,locked,same,outcome,1,cause)==contract::Completion::Unknown&&!error.empty(),"unmatched source/authority completion remains Unknown");
        if(bad==9)need(error==cause,"original native failure cause retained");
    }
    need(complete(before,0,false,false,false,false,false,0,30001,{})==contract::Completion::Unknown&&error.find("no_replay")!=std::string::npos,"read-only deadline preserves no-replay Unknown");
    actual_late_observation::checks();actual_rejection::checks();actual_observation::checks();actual_preparation::checks();actual_command::checks();std::cout<<object_checks<<" object UI checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"failure after "<<object_checks<<": "<<error.what()<<'\n';return 1;}
