#define main duplicate_protocol_fixture_main
#include "test_duplicate_protocol.cpp"
#undef main
#include "duplicate_native.h"
#include "move_contract.h"
#include <atomic>
#include <set>
namespace runtime_fixture {
namespace action=mt2mp::action_session;
namespace duplicate_native=mt2mp::game::duplicate_native;
namespace contract=mt2mp::game::ui_move_runtime::contract;
namespace child_npc {struct Receipt{};}
using Handle=duplicate_native::Handle;
using json=nlohmann::json;
namespace q {bool fail(std::string& e,const std::string& cause){e=cause;return false;}}
uint32_t GetCurrentThreadId(){return 17;}
struct Config{bool is_host=true;static Config& instance(){static Config value;return value;}};
struct Sample {uint64_t generation=4;std::map<std::string,Handle> handles;std::map<std::string,DuplicateObject> objects;DuplicateState state;Ledger money;};
struct Runtime {bool ready=true;uint32_t thread=17;};
struct Operation {Binding binding;Request request;Sample before,after;Evidence evidence;duplicate_native::Receipt native;
    std::map<uint64_t,std::string> created;std::vector<uintptr_t> cash_ops,destroy_ops;std::map<uint64_t,child_npc::Receipt> children;
    bool confirmed=false,retired=false;std::string cause;};
Runtime fixture_runtime;std::atomic<Runtime*> published{&fixture_runtime};
Binding fixture_binding{3,4};Sample fixture_current;Message fixture_message;json fixture_controller;
std::map<std::string,Operation> operations;std::set<std::string> local_sources;
unsigned original_calls=0,public_binding_calls=0;std::string failure;
namespace net {std::string current_snapshot_id(){return fixture_snapshot_uuid;}}
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool network_binding(const std::string&,Binding&,std::string& e){++public_binding_calls;e="public network admission requires idle Controller";return false;}
}
bool same_binding(const Sample& a,const Sample& b){return a.generation==b.generation;}
bool sample(Runtime&,const Binding& binding,const DuplicateIntent*,const std::map<uint64_t,std::string>&,Sample& out,std::string& e){
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
bool remember_history(Runtime&,const Operation&,std::string&){return true;}
#include "duplicate_perform_actual.inc"
void reset(){fixture_runtime={};fixture_message=message();fixture_current={};fixture_current.state=*fixture_message.evidence.duplicate_before;fixture_current.money=fixture_message.evidence.before;
    for(const auto& row:fixture_current.state.sources){fixture_current.handles[row.key]={1000,100};fixture_current.objects[row.key]=row;}
    fixture_controller={{"started",true},{"locked",false},{"pending",true},{"action_uuid",fixture_action_uuid},{"host_revision",uint64_t(1)}};
    operations.clear();local_sources.clear();failure.clear();original_calls=public_binding_calls=0;Config::instance().is_host=true;}
void run(){std::string e;reset();const auto request=fixture_message.request;
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
