#define main duplicate_protocol_fixture_main
#include "test_duplicate_protocol.cpp"
#undef main
#include "duplicate_native.h"
#include "duplicate_record_diagnostic.h"
#include "duplicate_records.h"
#include <functional>
#include "move_contract.h"
#include <atomic>
#include <set>
namespace runtime_fixture {
namespace action=mt2mp::action_session;
namespace duplicate_native=mt2mp::game::duplicate_native;
namespace duplicate_records=mt2mp::game::duplicate_records;
namespace contract=mt2mp::game::ui_move_runtime::contract;
namespace child_npc {struct Receipt{};}
using Handle=duplicate_native::Handle;
using json=nlohmann::json;
namespace q {bool fail(std::string& e,const std::string& cause){e=cause;return false;}}
uint32_t GetCurrentThreadId(){return 17;}
struct Config{bool diagnostic_mailbox_enabled=true,is_host=true;static Config& instance(){static Config value;return value;}};
struct Sample {struct World {struct Origin {std::string uuid=fixture_snapshot_uuid;uint64_t world_epoch=3,generation=4;} origin;uintptr_t root=1,selection=2;} world;uintptr_t business=3,release=4,random=5;uint64_t generation=4;std::map<std::string,Handle> handles;std::map<std::string,DuplicateObject> objects;DuplicateState state;Ledger money;int undo=0;json selected_records=json::object();std::map<std::string,duplicate_records::UptimeWitness> uptime;};
struct Runtime {bool ready=true;uint32_t thread=17;int memory=0;uintptr_t image=0;};
struct Operation {Binding binding;Request request;Sample before,after;Evidence evidence;duplicate_native::Receipt native;
    std::map<uint64_t,std::string> created;std::vector<uintptr_t> cash_ops,destroy_ops;std::map<uint64_t,child_npc::Receipt> children;
    bool confirmed=false,retired=false;std::string cause;};
Runtime fixture_runtime;std::atomic<Runtime*> published{&fixture_runtime};
Binding fixture_binding{3,4};Sample fixture_current;Message fixture_message;json fixture_controller;
std::map<std::string,Operation> operations;std::set<std::string> local_sources;
unsigned original_calls=0,public_binding_calls=0;std::string failure;
uint64_t captures=0,submitted=0,rejections=0;std::string cause;
json last_submission=nullptr,last_source_records=nullptr,last_observation=nullptr;
bool tick_binding_ready=false,cleanup_ready=true,connected=true;
std::function<bool(const std::string&,const DuplicateIntent&,std::string&)> submit_hook;
namespace duplicate_reader {struct Capture{};bool cleaned(int&,uintptr_t,const Capture&,std::string& e){return cleanup_ready||q::fail(e,"original Duplicate cleanup failed");}}
struct Pending {Binding binding;std::string id;duplicate_reader::Capture source;DuplicateIntent intent;Sample before;};
std::optional<Pending> captured;std::map<int,int> history_sources,history_operations;
void reject(const std::string& e){++rejections;cause=e;}
void retire_session();
namespace net {std::string current_snapshot_id(){return fixture_snapshot_uuid;}namespace session_wire {bool connected(){return runtime_fixture::connected;}}}
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool network_binding(const std::string&,Binding& out,std::string& e){++public_binding_calls;
    if(tick_binding_ready){out=fixture_binding;return true;}e="public network admission requires idle Controller";return false;}
bool submit_duplicate(const std::string& id,const DuplicateIntent& intent,std::string& e){return submit_hook(id,intent,e);}
}
bool same_binding(const Sample& a,const Sample& b){return a.generation==b.generation;}
bool sample_duplicate(Runtime&,const Binding& binding,const DuplicateIntent*,const std::map<uint64_t,std::string>&,Sample& out,std::string& e){
    if(binding!=fixture_binding||fixture_current.generation!=binding.native_generation)return q::fail(e,"current native generation differs");out=fixture_current;return true;}
namespace remove_diagnostics {
bool admit_created_building(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& e){
    const auto found=fixture_current.handles.find(key);
    return (failure!="admit"&&snapshot==fixture_snapshot_uuid&&found!=fixture_current.handles.end()&&found->second.uid==uid)||q::fail(e,"current created full UID admission failed");}
}
struct Port {
    Runtime& r;const Binding& binding;const Request& request;Sample& before;Sample after;duplicate_native::Receipt& receipt;bool source_peer=false;
    std::map<uint64_t,std::string> created;std::vector<uintptr_t> cash_ops,destroy_ops;std::map<uint64_t,child_npc::Receipt> children;
    bool ready(const DuplicateIntent&,const std::vector<Handle>&,int32_t,std::string& e){return failure!="ready"||q::fail(e,"original ready cause");}
    bool charge(int32_t,std::string&){return true;}
    bool begin_edit(bool& opened,std::string&){opened=false;return true;}
    bool end_edit(std::string&){return true;}
    bool create_graph(uintptr_t& graph,std::string&){graph=900;return true;}
    bool cash_inverse(uintptr_t,int32_t,const std::array<uint32_t,3>&,std::string&){cash_ops.push_back(901);return true;}
    bool duplicate(const Handle&,const DuplicateTarget& row,Handle& out,std::string& e){++original_calls;out={2000,200};created.emplace(200,row.created_keys.front());
        return failure!="original"||q::fail(e,"first original Duplicate cause");}
    bool destroy_inverse(uintptr_t,const Handle&,std::string&){destroy_ops.push_back(902);return true;}
    bool select_created(const std::vector<Handle>&,std::string&){return true;}
    bool release_points(const DuplicateIntent&,const std::vector<Handle>&,std::string&){return true;}
    bool commit_graph(uintptr_t,const std::vector<Handle>&,std::string&){return true;}
    bool readback(const DuplicateIntent&,const std::vector<Handle>&,const duplicate_native::Receipt&,std::string&){
        fixture_current.state=*fixture_message.evidence.duplicate_after;fixture_current.money=fixture_message.evidence.after;
        for(const auto& row:fixture_current.state.created){fixture_current.handles[row.key]={2000,200};fixture_current.objects[row.key]=row;}
        if(failure=="cash")fixture_current.money.cash++;after=fixture_current;return true;}
    bool restore_selection(std::string& e){return failure!="restore"||q::fail(e,"original selection cleanup cause");}
    bool inverse(std::string& e){return failure!="inverse"||q::fail(e,"original inverse differs");}
};
#include "duplicate_source_actual.inc"
#include "duplicate_record_status_actual.inc"
#include "duplicate_tick_actual.inc"
#include "duplicate_retire_session_actual.inc"
bool remember_history(Runtime&,const Operation&,std::string&){return true;}
#include "duplicate_perform_actual.inc"
void reset(){fixture_runtime={};fixture_message=message();fixture_current={};fixture_current.state=*fixture_message.evidence.duplicate_before;fixture_current.money=fixture_message.evidence.before;
    for(const auto& row:fixture_current.state.sources){fixture_current.handles[row.key]={1000,100};fixture_current.objects[row.key]=row;}
    fixture_controller={{"started",true},{"locked",false},{"pending",true},{"action_uuid",fixture_action_uuid},{"host_revision",uint64_t(1)}};
    operations.clear();local_sources.clear();failure.clear();original_calls=public_binding_calls=0;Config::instance().is_host=true;
    captured.reset();last_submission=nullptr;last_source_records=nullptr;last_observation=nullptr;captures=submitted=rejections=0;cause.clear();Config::instance().diagnostic_mailbox_enabled=true;
    tick_binding_ready=false;cleanup_ready=connected=true;submit_hook={};
    fixture_current.selected_records={{fixture_current.state.sources.front().key,{{"state","complete"},{"record_sha",fixture_current.state.sources.front().record_sha}}}};
}
void source_diagnostics(){
    const std::array<const char*,7> fields={"key","record_sha","family","region","pose_bits","members","parent"};
    for(size_t field=0;field<fields.size();++field){reset();auto& row=fixture_current.state.sources.front();
        if(field==0)row.key=key(99);
        if(field==1)row.record_sha=std::string(64,'e');
        if(field==2)row.family=DuplicateFamily::Scenery;
        if(field==3)++row.region;
        if(field==4)row.pose[2]^=1;
        if(field==5)row.members={key(99)};
        if(field==6)row.parent="group:"+key(99);
        std::string e;need(perform(fixture_binding,fixture_message.request,nullptr,e)==Outcome::RejectedNoEffect&&original_calls==0&&operations.empty(),
            "source mismatch must reject before original Duplicate or operation publication");
        need(last_source_records.at("action_uuid")==fixture_action_uuid&&last_source_records.at("snapshot_uuid")==fixture_snapshot_uuid&&
             last_source_records.at("phase")=="authority_source"&&last_source_records.at("records")==fixture_current.selected_records,
             "pre-effect authority rejection lost its exact selected Record witness");
        const std::string marker=" differences=";const auto start=e.find(marker);
        need(e.starts_with("Duplicate current original record/owner/pose differs key="+fixture_message.request.duplicate.rows.front().key)&&start!=std::string::npos,
            "original Duplicate mismatch cause/key must remain visible");
        const std::string context_marker=" context=";const auto context_start=e.find(context_marker);
        need(context_start!=std::string::npos&&context_start<start,"source failure must retain local native identity context");
        const auto context=json::parse(e.substr(context_start+context_marker.size(),start-context_start-context_marker.size()));
        need(context.at("snapshot")==fixture_snapshot_uuid&&context.at("native_epoch")==3&&context.at("native_generation")==4,
            "source diagnosis retains actual snapshot and native epoch/generation");
        need(field==0?context.at("local_uid").is_null():context.at("local_uid")==100,"source diagnosis uses actual keyed local UID without exporting it as wire identity");
        const auto details=json::parse(e.substr(start+marker.size()));
        need(details.size()==1&&details.contains(fields[field]),"actual source comparator must identify precisely the differing field");
        const auto& detail=details.at(fields[field]);need(detail.size()==2&&detail.contains("required")&&detail.contains("actual")&&detail.at("required")!=detail.at("actual"),
            "mismatch diagnosis must retain both exact required and observed values");
        if(field==1)need(detail.at("required")==std::string(64,'b')&&detail.at("actual")==std::string(64,'e'),"record diagnosis preserves hashes without raw Record content");
        if(field==4)need(detail.at("required")==json(fixture_message.request.duplicate.rows.front().before)&&detail.at("actual")==json(row.pose),"pose diagnosis preserves all ten native bit fields");
    }
}
void submission_diagnostics(){
    reset();tick_binding_ready=true;captures=1;
    const auto intent=fixture_message.request.duplicate;
    captured=Pending{fixture_binding,fixture_action_uuid,{},intent,fixture_current};
    submit_hook=[&](const std::string& id,const DuplicateIntent& observed,std::string&){
        need(id==fixture_action_uuid&&observed==intent&&!last_submission.at("submitted").get<bool>()&&
             last_submission.at("action_uuid")==id&&last_submission.at("capture_count")==1,
             "exact local submission metadata was not published before synchronous Controller submission");
        return true;
    };
    tick();need(!captured&&submitted==1&&last_submission.at("submitted")==true&&last_submission.at("submission_count")==1&&
        last_submission.at("snapshot_uuid")==fixture_snapshot_uuid&&last_submission.at("native_epoch")==3&&
        last_submission.at("native_generation")==4&&last_submission.at("selected_keys")==json::array({intent.rows[0].key})&&
        last_submission.at("target_poses")==json::array({intent.rows[0].after})&&last_submission.at("cash_position")==json(intent.cash_position)&&
        last_source_records.at("action_uuid")==fixture_action_uuid&&last_source_records.at("phase")=="local_submit"&&operations.empty(),
        "successful native submission lost exact correlation without an Operation");
    const auto retained=last_submission;tick();need(last_submission==retained&&submitted==1,"idle tick replaced exact last submission");
    retire_session();need(last_submission.is_null()&&last_source_records.is_null(),"retired session retained source diagnostics");
    reset();tick_binding_ready=true;captures=1;Config::instance().diagnostic_mailbox_enabled=false;
    captured=Pending{fixture_binding,fixture_action_uuid,{},intent,fixture_current};
    submit_hook=[](const std::string&,const DuplicateIntent&,std::string&){return true;};
    tick();need(last_source_records.is_null()&&last_submission.at("submitted")==true,
        "disabled Record diagnostics suppressed lightweight exact submission correlation");
    reset();tick_binding_ready=true;captures=1;captured=Pending{fixture_binding,fixture_action_uuid,{},intent,fixture_current};
    submit_hook=[](const std::string&,const DuplicateIntent&,std::string& e){e="original Controller submit failure";return false;};
    tick();need(!last_submission.at("submitted").get<bool>()&&submitted==0&&cause=="original Controller submit failure",
        "failed submission claimed accepted metadata or lost its cause");
    reset();tick_binding_ready=true;captures=1;cleanup_ready=false;captured=Pending{fixture_binding,fixture_action_uuid,{},intent,fixture_current};
    tick();need(last_submission.is_null()&&submitted==0&&cause=="original Duplicate cleanup failed",
        "uncleaned original source fabricated a submitted action");
}
#include "duplicate_uptime_runtime_cases.inc"
void observation_diagnostics(){
    const std::array<const char*,15> fields={"binding","sources","created","untouched_sha","random","release","history",
        "money","handles","sources","sources","sources","sources","sources","sources"};
    for(size_t index=0;index<fields.size();++index){
        reset();std::string e;const auto request=fixture_message.request;
        need(perform(fixture_binding,request,nullptr,e)==Outcome::Confirmed,e);
        auto& op=operations.at(request.action_uuid);
        op.after.selected_records["expected-created"]={{"record_sha",std::string(64,'a')}};
        fixture_current.selected_records["actual-created"]={{"record_sha",std::string(64,'e')}};
        if(index==0)++op.before.generation;
        if(index==1)fixture_current.state.sources.front().record_sha=std::string(64,'e');
        if(index==2)fixture_current.state.created.front().record_sha=std::string(64,'e');
        if(index==3)fixture_current.state.untouched_sha=std::string(64,'e');
        if(index==4)++fixture_current.state.random[0];
        if(index==5)++fixture_current.state.release[0];
        if(index==6)fixture_current.state.history=action::DuplicateHistoryEffects{};
        if(index==7)++fixture_current.money.line_income[6];
        if(index==8)++fixture_current.handles.at(fresh(0)).uid;
        if(index==9)fixture_current.state.sources.front().key=key(99);
        if(index==10)fixture_current.state.sources.front().parent="group:"+key(99);
        if(index==11)fixture_current.state.sources.front().family=DuplicateFamily::Scenery;
        if(index==12)++fixture_current.state.sources.front().region;
        if(index==13)++fixture_current.state.sources.front().pose[5];
        if(index==14)fixture_current.state.sources.front().members={key(99)};
        Evidence observed;need(!observe(fixture_binding,request,observed,e)&&original_calls==1,"observation mismatch accepted or repeated original");
        const auto& details=last_observation.at("differences");
        need(details.size()==1&&details.contains(fields[index]),"post-effect diagnostic did not isolate actual failing field");
        need(last_observation.at("action_uuid")==request.action_uuid&&last_observation.at("snapshot_uuid")==request.snapshot_uuid&&
            last_observation.at("sampled")==true&&last_observation.at("source_cause")==""&&
            last_observation.at("expected_records")==op.after.selected_records&&last_observation.at("current_records")==fixture_current.selected_records,
            "post-effect diagnostic lost exact action or already captured Record witnesses");
        need(e.starts_with("Duplicate complete current result changed before ACK")&&e.find(fields[index])!=std::string::npos&&op.cause==e,
            "original observation cause or diagnostic predicate was lost");
        if(index==1||index==2)need(details.at(fields[index]).at("rows")[0].at("fields").at("record_sha").at("actual")==std::string(64,'e'),
            "post-effect record SHA normalized or omitted");
        if(index==8)need(details.at("handles").at("rows")[0].at("actual_uid")==201&&
            details.at("handles").at("rows")[0].at("same_object")==true,"full UID/pointer comparison lost");
        need(perform(fixture_binding,request,nullptr,e)==Outcome::Unknown&&original_calls==1,"failed observation permitted original replay");
    }
    reset();std::string e;const auto request=fixture_message.request;Evidence observed;
    need(perform(fixture_binding,request,nullptr,e)==Outcome::Confirmed,e);++fixture_current.generation;
    need(!observe(fixture_binding,request,observed,e)&&last_observation.at("sampled")==false&&
         last_observation.at("source_cause")=="current native generation differs"&&last_observation.at("current_records").is_null()&&original_calls==1,
         "failed sample became a value comparison or lost its upstream cause");
    reset();e.clear();need(perform(fixture_binding,request,nullptr,e)==Outcome::Confirmed,e);
    Config::instance().diagnostic_mailbox_enabled=false;++fixture_current.money.cash;
    need(!observe(fixture_binding,request,observed,e)&&last_observation.is_null()&&
         e=="Duplicate complete current result changed before ACK"&&original_calls==1,"disabled diagnostics changed guard or retained payload");
    reset();e.clear();need(perform(fixture_binding,request,nullptr,e)==Outcome::Confirmed,e);
    for(unsigned i=0;i<10;++i){auto row=fixture_current.state.created.front();row.key=key(100+i);fixture_current.state.created.push_back(row);}
    need(!observe(fixture_binding,request,observed,e)&&last_observation.at("differences").at("created").at("differing_rows")==10&&
         last_observation.at("differences").at("created").at("rows").size()==duplicate_records::diagnostic_sources&&
         last_observation.at("differences").at("created").at("truncated")==true&&original_calls==1,"diagnostic row bound changed admission or lost truncation");
    retire_session();need(last_observation.is_null(),"retired session retained observation diagnostics");
}

void run(){observation_diagnostics();submission_diagnostics();source_diagnostics();uptime_runtime_cases();std::string e;reset();const auto request=fixture_message.request;
    need(perform(fixture_binding,request,nullptr,e)==Outcome::Confirmed&&original_calls==1&&!public_binding_calls,e);Evidence authority;
    need(observe(fixture_binding,request,authority,e)&&retire(fixture_binding,request,e)&&!retire(fixture_binding,request,e),e);
    need(perform(fixture_binding,request,nullptr,e)==Outcome::Unknown&&original_calls==1,"confirmed Duplicate replayed");
    reset();Config::instance().is_host=false;need(perform(fixture_binding,request,&authority,e)==Outcome::Confirmed&&original_calls==1&&!public_binding_calls,e);
    auto rejected=[&](auto change){reset();auto req=request;change(req);need(perform(fixture_binding,req,nullptr,e)==Outcome::RejectedNoEffect&&!original_calls,"invalid pending Duplicate mutated");};
    rejected([](auto&){fixture_controller["pending"]=false;});rejected([](auto&){fixture_controller["locked"]=true;});
    rejected([](auto&){fixture_controller["action_uuid"]=fixture_snapshot_uuid;});rejected([](auto&){fixture_runtime.thread=18;});
    rejected([](auto&){Config::instance().is_host=false;});rejected([](auto&){fixture_current.generation++;});
    rejected([](auto& req){req.snapshot_uuid=fixture_action_uuid;});rejected([](auto&){fixture_current.state.sources[0].record_sha=std::string(64,'e');});
    reset();Config::instance().is_host=false;auto foreign=authority;foreign.duplicate_before->random[0]++;need(perform(fixture_binding,request,&foreign,e)==Outcome::RejectedNoEffect&&!original_calls,"different Host prior mutated");
    for(const std::string stage:{"original","restore","cash","admit"}){reset();failure=stage;
        need(perform(fixture_binding,request,nullptr,e)==Outcome::Unknown&&original_calls==1&&!e.empty(),"partial Duplicate not Unknown");
        need(perform(fixture_binding,request,nullptr,e)==Outcome::Unknown&&original_calls==1,"partial Duplicate replayed");}
    reset();need(perform(fixture_binding,request,nullptr,e)==Outcome::Confirmed,e);fixture_current.handles.at(fresh(0)).uid++;
    need(!observe(fixture_binding,request,authority,e),"changed created full UID ACKed");
    reset();need(perform(fixture_binding,request,nullptr,e)==Outcome::Confirmed,e);failure="inverse";need(!observe(fixture_binding,request,authority,e),"changed original graph ACKed");
}
}
int main()try{runtime_fixture::run();std::cout<<"actual Duplicate pending Host/Client/readback/retirement/failure cases passed\n";return 0;}
catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
