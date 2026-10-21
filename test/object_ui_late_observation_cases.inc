namespace actual_late_observation {
using json=contract::Json;
namespace probe_contract=contract;
namespace action {
using Binding=actual_observation::action::Binding;
enum class GroupKind {Group,Ungroup};
}
struct Runtime {uint32_t thread=7;};
struct Job {
    struct Input {std::string token="token",snapshot="shared",command="Duplicate";std::vector<std::string> keys={"selected"};} input;
    action::Binding binding;
    std::set<std::string> prior_operations={"old"};
    uint64_t captures=4,submitted=4,rejected=0,revision=4,started_at=0;
    std::string action_uuid="current",phase="unknown",cause="object_ui_observation_timeout_ms>30000_no_replay";
    uint32_t attempted_mask=511,returned_mask=511,exception=0,cleanup_exception=0;
    bool command_accepted=true,preparation_reconciled=false,observation_reconciled=false;
    std::string reconciliation_cause,prior_observation_cause;
    json result=json::object();
};
struct Operation {
    struct Request {
        std::string snapshot_uuid="shared";
        struct Target {std::string key="selected";};
        struct Duplicate {std::vector<Target> rows={{}};} duplicate;
        struct Group {action::GroupKind kind=action::GroupKind::Group;std::vector<std::string> selected={"selected"};} group;
    } request;
    action::Binding binding;
    bool confirmed=true,retired=true;
    std::string cause;
    struct After {
        struct Birth {std::string key="created";int family=2;std::vector<std::string> members={"member-a","member-b"};};
        struct State {std::vector<Birth> created={{}};} state;
        struct Member {std::string key="selected";};
        struct Group {std::string key="group:created";std::vector<Member> members={{}};};
        struct Local {struct State {std::vector<Group> groups={{}};} state;} local;
    } after;
};
namespace duplicate_edit {
uint64_t captures=5,submitted=5,rejections=0;
std::map<std::string,Operation> operations;
std::string cause;
json status(){json values=json::object();for(const auto& [id,value]:operations)
    values[id]={{"original_calls",1},{"confirmed",value.confirmed},{"retired",value.retired},{"selection_restored",true}};
    return {{"operations",values}};}
}
namespace group_edit {
uint64_t captures=5,submitted=5,rejections=0;
std::map<std::string,Operation> operations;
std::string cause;
json status(){json values=json::object();for(const auto& [id,value]:operations)
    values[id]={{"original_calls",1},{"confirmed",value.confirmed},{"retired",value.retired},{"selection_restored",true}};
    return {{"operations",values}};}
}
namespace typed_action_runtime {
json fixture_controller;
unsigned fixture_reads=0;
json controller_status(){++fixture_reads;return fixture_controller;}
}
std::map<std::string,Job> jobs;
std::atomic<Runtime*> published{nullptr};
uint64_t fixture_now=60001;
unsigned fixture_owner_reads=0,fixture_stop_logs=0,fixture_recovery_logs=0;
bool fixture_owner_ok=true;
uint32_t GetCurrentThreadId(){return 7;}
uint64_t GetTickCount64(){return fixture_now;}
bool current(Runtime&,const Job&,std::string& error){++fixture_owner_reads;
    if(!fixture_owner_ok){error="original exact World binding changed";return false;}return true;}
bool reconcile_rejection(Runtime&,Job&,std::string&){return false;}
bool reconcile_preparation(Runtime&,Job&,std::string&){return false;}
template<class... Values>void fixture_stop_log(const char*,const Values&...){++fixture_stop_logs;}
template<class... Values>void fixture_recovery_log(const char*,const Values&...){++fixture_recovery_logs;}
#define LOG_ERROR(...) fixture_stop_log(__VA_ARGS__)
#define LOG_INFO(...) fixture_recovery_log(__VA_ARGS__)
#include "object_ui_read.inc"
#undef LOG_INFO
#undef LOG_ERROR

void reset(Runtime& runtime,const std::string& command="Duplicate"){
    jobs={{"token",Job{}}};auto& prepared=jobs.at("token");prepared.input.command=command;
    if(command!="Duplicate")prepared.attempted_mask=prepared.returned_mask=63;
    Operation operation;if(command=="Ungroup")operation.request.group.kind=action::GroupKind::Ungroup;
    duplicate_edit::operations={{"old",Operation{}},{"current",operation}};group_edit::operations=duplicate_edit::operations;
    duplicate_edit::captures=duplicate_edit::submitted=group_edit::captures=group_edit::submitted=5;
    duplicate_edit::rejections=group_edit::rejections=0;duplicate_edit::cause.clear();group_edit::cause.clear();
    typed_action_runtime::fixture_controller={{"pending",false},{"locked",false},{"outcome",1},{"action_uuid","current"},{"host_revision",5}};
    typed_action_runtime::fixture_reads=0;fixture_owner_reads=fixture_stop_logs=fixture_recovery_logs=0;
    fixture_owner_ok=true;runtime.thread=7;published=&runtime;fixture_now=60001;
}
void checks(){
    Runtime runtime;json receipt;std::string error;const json token={{"acceptance_token","token"}};
    const auto prior_native_commands=actual_command::commands;
    for(const std::string command:{"Duplicate","Group","Ungroup"}){
        reset(runtime,command);
        need(read(token,receipt,error)&&receipt.at("confirmed").get<bool>()&&receipt.at("observation_reconciled").get<bool>()&&
             receipt.at("prior_observation_cause")=="object_ui_observation_timeout_ms>30000_no_replay"&&receipt.at("cause")=="",
             "late exact native/authority completion reconciles only observation and retains original timeout");
        need(jobs.at("token").action_uuid=="current"&&fixture_owner_reads==1&&typed_action_runtime::fixture_reads==1&&fixture_recovery_logs==1,
             "late completion uses one fresh current proof and exact Controller result");
        need(read(token,receipt,error)&&fixture_owner_reads==1&&fixture_recovery_logs==1,
             "terminal repeated reads do not repeat observation or original native commands");
    }
    reset(runtime);jobs.at("token").phase="waiting_for_capture_and_host";jobs.at("token").cause.clear();fixture_now=1;
    need(read(token,receipt,error)&&receipt.at("confirmed").get<bool>()&&!receipt.at("observation_reconciled").get<bool>(),
         "ordinary timely completion keeps existing behavior");
    reset(runtime);jobs.at("token").cause="original native effect is unknown";
    need(read(token,receipt,error)&&receipt.at("unknown").get<bool>()&&fixture_owner_reads==0&&typed_action_runtime::fixture_reads==0,
         "a non-timeout Unknown never re-enters successful completion observation");
    reset(runtime);typed_action_runtime::fixture_controller["pending"]=true;
    need(read(token,receipt,error)&&receipt.at("unknown").get<bool>()&&receipt.at("cause")=="object_ui_observation_timeout_ms>30000_no_replay"&&fixture_stop_logs==0,
         "still-pending timeout remains blocked without repeated timeout logs");
    typed_action_runtime::fixture_controller["pending"]=false;
    need(read(token,receipt,error)&&receipt.at("confirmed").get<bool>(),"a later exact completion can resolve the same original token without a new command");
    for(int bad=0;bad<21;++bad){reset(runtime);auto& prepared=jobs.at("token");auto& operation=duplicate_edit::operations.at("current");
        if(bad==0)prepared.returned_mask=63;
        if(bad==1)prepared.attempted_mask=63;
        if(bad==2)prepared.exception=1;
        if(bad==3)prepared.cleanup_exception=1;
        if(bad==4)prepared.command_accepted=false;
        if(bad==5)fixture_owner_ok=false;
        if(bad==6)prepared.action_uuid="another";
        if(bad==7)operation.request.snapshot_uuid="foreign";
        if(bad==8)operation.request.duplicate.rows[0].key="other";
        if(bad==9)++operation.binding.native_generation;
        if(bad==10)++duplicate_edit::captures;
        if(bad==11)--duplicate_edit::submitted;
        if(bad==12)++duplicate_edit::rejections;
        if(bad==13)duplicate_edit::operations.emplace("another",Operation{});
        if(bad==14)operation.confirmed=false;
        if(bad==15)operation.retired=false;
        if(bad==16)operation.cause="original Duplicate failed";
        if(bad==17)typed_action_runtime::fixture_controller["locked"]=true;
        if(bad==18)typed_action_runtime::fixture_controller["action_uuid"]="other";
        if(bad==19)typed_action_runtime::fixture_controller["host_revision"]=6;
        if(bad==20)typed_action_runtime::fixture_controller["outcome"]=3;
        need(read(token,receipt,error)&&receipt.at("unknown").get<bool>()&&!receipt.at("confirmed").get<bool>()&&
             receipt.at("prior_observation_cause")=="object_ui_observation_timeout_ms>30000_no_replay",
             "foreign, ambiguous, incomplete or failed authority cannot resolve late observation");
        if(bad==5)need(receipt.at("cause")=="original exact World binding changed","fresh owner failure remains distinct from original timeout");
        if(bad==6)need(prepared.action_uuid=="another","late observation does not overwrite the already-correlated action identity");
        if(bad==16)need(receipt.at("cause")=="original Duplicate failed","native failure cause remains visible beside original timeout");
    }
    reset(runtime);need(!read({{"acceptance_token","other"}},receipt,error)&&error=="object_ui_token_unknown"&&fixture_owner_reads==0,
        "a foreign token cannot observe or release this retained job");
    reset(runtime);runtime.thread=9;need(!read(token,receipt,error)&&error=="object_ui_receipt_requires_bound_Mainthread",
        "late observation still requires the exact bound main thread");
    need(actual_command::commands==prior_native_commands,"all actual observation paths made zero native command calls");
    published=nullptr;
}
}
