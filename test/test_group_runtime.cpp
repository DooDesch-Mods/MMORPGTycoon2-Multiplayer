#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include "typed_action/action_session.h"
#include "group_native.h"
#include "move_contract.h"
#include <atomic>
#include <optional>
namespace runtime_fixture {
namespace action=mt2mp::action_session;
namespace group_reader=mt2mp::game::group_reader;
namespace group_native=mt2mp::game::group_native;
namespace group_transition=mt2mp::game::group_transition;
namespace contract=mt2mp::game::ui_move_runtime::contract;
namespace q=group_reader::q;
using json=nlohmann::json;
using f::need;
uint32_t GetCurrentThreadId(){return 17;}
struct Config {bool is_host=true;static Config& instance(){static Config value;return value;}};
namespace host {struct WorldState {uintptr_t selection=f::selection;uint64_t generation=4;bool operator==(const WorldState&)const=default;};}
struct Sample {host::WorldState world;group_reader::Local local;action::Ledger money;unsigned undo=0;};
struct Runtime {uintptr_t image=f::image;uint32_t thread=17;bool ready=true;f::Memory memory;std::optional<int> pending;};
struct Pending {action::Binding binding;std::string snapshot;action::GroupIntent intent;Sample before;};
struct Operation {action::Binding binding;action::Request request;Sample before,after;action::Evidence evidence;group_native::Receipt native;
    bool confirmed=false,retired=false;std::string cause;};
Runtime fixture_runtime;std::atomic<Runtime*> published{&fixture_runtime};
bool installed=true;std::optional<Pending> captured;std::map<std::string,Operation> operations;uint64_t captures=0;
action::Binding fixture_binding{3,4};Sample fixture_sample;json fixture_controller;
const std::string fixture_snapshot="22222222-2222-4222-8222-222222222222",fixture_id="11111111-1111-4111-8111-111111111111";
unsigned original_calls=0,public_binding_calls=0;std::string failure;
std::string record(unsigned n){return std::string(64,'a')+":world:building:"+std::to_string(n);}
namespace net {std::string current_snapshot_id(){return fixture_snapshot;}}
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool network_binding(const std::string& snapshot,action::Binding& out,std::string& e){++public_binding_calls;
    if(snapshot!=fixture_snapshot||fixture_controller.value("pending",true)||fixture_controller.value("locked",true)){
        e="public network admission requires idle Controller";return false;}out=fixture_binding;return true;}
}
bool world(Runtime&,host::WorldState& out,std::string&){out=fixture_sample.world;return true;}
bool sample(Runtime&,const action::Binding& binding,const std::vector<uintptr_t>&,Sample& out,std::string& e){
    if(binding!=fixture_binding||fixture_sample.world.generation!=binding.native_generation){e="current complete native binding differs";return false;}
    out=fixture_sample;return true;
}
std::vector<uintptr_t> props(const group_reader::Local& local){std::vector<uintptr_t> out;for(const auto& row:local.state.props)out.push_back(local.handles.at(row.key).object);return out;}
bool same_binding(const Sample& a,const Sample& b){return a.world==b.world&&a.local.manager==b.local.manager&&a.local.selection==b.local.selection;}
namespace remove_diagnostics {
struct LocalTarget {bool normal_scenery=false;uintptr_t object=0;};
bool resolve_record_target(const std::string& snapshot,const std::string& key,LocalTarget& out,std::string& e){
    const auto found=fixture_sample.local.handles.find(key);
    if(snapshot!=fixture_snapshot||found==fixture_sample.local.handles.end()||key.starts_with("group:")){e="current canonical Scenery target absent";return false;}
    out={true,found->second.object};return true;
}
}
struct Port {
    Runtime& r;const action::Binding& binding;Sample& initial;Sample after;
    bool ready(const group_reader::Local&,std::string& e){if(failure=="ready"){e="native Selection unavailable";return false;}return true;}
    bool create(const group_reader::Local&,const group_transition::Plan&,group_reader::Handle& out,std::string&){out={0x999900,77};return true;}
    bool select(const std::vector<uintptr_t>&,std::string&){return true;}
    bool original(action::GroupKind kind,std::string& e){
        ++original_calls;if(failure=="original"){e="first original command failed";return false;}
        const auto& intent=operations.at(fixture_id).request.group;need(kind==intent.kind,"whole original kind differs");group_transition::Plan plan;
        need(group_transition::plan(fixture_sample.local.state,intent,plan,e),e);auto& value=fixture_sample.local;
        for(const auto& removed:plan.removed)value.handles.erase(removed);
        std::erase_if(value.state.groups,[&](const auto& row){return std::find(plan.removed.begin(),plan.removed.end(),row.key)!=plan.removed.end();});
        for(auto& prop:value.state.props)if(std::find(plan.added.begin(),plan.added.end(),prop.key)!=plan.added.end())prop.parent=kind==action::GroupKind::Group?plan.target:"";
        if(kind==action::GroupKind::Group){if(plan.create){value.state.groups.push_back({plan.target,67,f::pose(),{}});value.handles[plan.target]={0x999900,77};}
            auto& target=*std::find_if(value.state.groups.begin(),value.state.groups.end(),[&](const auto& row){return row.key==plan.target;});
            for(const auto& key:plan.added)target.members.push_back({key,f::pose()});}
        ++fixture_sample.undo;if(failure=="cash")++fixture_sample.money.cash;return true;
    }
    bool read_after(const group_reader::Local&,group_reader::Local& out,std::string&){after=fixture_sample;out=after.local;return true;}
    bool inverse(const group_reader::Local&,const group_reader::Local&,const group_transition::Plan&,const group_reader::Handle&,uintptr_t& graph,std::string& e){
        if(failure=="inverse"){e="original inverse differs";return false;}graph=0x8888;return true;}
    bool cleanup_empty(const group_reader::Handle&,std::string&){return true;}
    bool restore(const group_reader::Local&,const group_reader::Local&,const group_transition::Plan&,bool,std::string& e){
        if(failure=="restore"){e="original selection restore failed";return false;}return true;}
};
bool remember_history(Runtime&,const Operation& op,std::string& e){if(!op.native.graph){e="source graph absent";return false;}return true;}
#include "group_capture_actual.inc"
#include "group_perform_actual.inc"
action::Request request(){action::Request out;out.action=action::Action::EditGroup;out.peer=2;out.sequence=1;out.action_uuid=fixture_id;
    out.snapshot_uuid=fixture_snapshot;out.canonical_record_key="groups:"+fixture_id;out.group={action::GroupKind::Group,{"group:"+record(1),record(2)}};return out;}
void reset(){
    fixture_runtime={};fixture_runtime.memory=f::setup();fixture_sample={};auto& local=fixture_sample.local;
    local.manager=f::manager;local.selection=f::selection;local.raw=local.filtered={f::group,f::free_prop};
    local.state.groups={{"group:"+record(1),67,f::pose(),{{record(1),f::pose()}}}};
    local.state.props={{record(1),"group:"+record(1),67,f::pose()},{record(2),"",67,f::pose()}};
    local.handles={{"group:"+record(1),{f::group,f::group_uid}},{record(1),{f::scenery,f::member_uid}},{record(2),{f::free_prop,f::free_uid}}};
    fixture_controller={{"started",true},{"locked",false},{"pending",true},{"action_uuid",fixture_id},{"host_revision",uint64_t(1)}};
    operations.clear();captured.reset();captures=0;original_calls=public_binding_calls=0;failure.clear();installed=true;Config::instance().is_host=true;
    const uintptr_t multi=0x90000;auto& m=fixture_runtime.memory;m.put(f::selection+0xc0,multi);m.put(multi,f::image+0x171b260);
    m.put(f::image+0x171b260+0x168,f::image+0x74bc00);m.put(multi+0x45f,uint8_t(1));
}
void run(){std::string e;reset();fixture_controller["pending"]=false;
    need(capture(fixture_runtime,f::selection,0,action::GroupKind::Group,e)&&captured&&captures==1&&!original_calls,e);
    need(!capture(fixture_runtime,f::selection,0,action::GroupKind::Group,e),"duplicate capture admitted");
    auto reject_capture=[&](auto change){reset();fixture_controller["pending"]=false;change();need(!capture(fixture_runtime,f::selection,0,action::GroupKind::Group,e)&&!captured&&!original_calls,"invalid source admitted");};
    reject_capture([]{fixture_controller["pending"]=true;});reject_capture([]{fixture_controller["locked"]=true;});
    reject_capture([]{fixture_runtime.thread=18;});reject_capture([]{fixture_runtime.memory.put(0x90000+0x45f,uint8_t(0));});
    reset();auto r=request();need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Confirmed,e);action::Evidence authority;
    need(observe(fixture_binding,r,authority,e)&&original_calls==1&&public_binding_calls==0,e);
    need(retire(fixture_binding,r,e)&&!retire(fixture_binding,r,e),"retirement repeated");
    need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&original_calls==1,"confirmed command replayed");
    reset();Config::instance().is_host=false;need(perform(fixture_binding,r,&authority,e)==action::Outcome::Confirmed&&original_calls==1&&!public_binding_calls,e);
    auto rejected=[&](auto change){reset();auto req=r;change(req);need(perform(fixture_binding,req,nullptr,e)==action::Outcome::RejectedNoEffect&&!original_calls,"invalid pending command mutated");};
    rejected([](auto&){fixture_controller["pending"]=false;});rejected([](auto&){fixture_controller["action_uuid"]="33333333-3333-4333-8333-333333333333";});
    rejected([](auto&){Config::instance().is_host=false;});rejected([](auto&){fixture_sample.world.generation++;});
    rejected([](auto& req){req.snapshot_uuid="33333333-3333-4333-8333-333333333333";});rejected([](auto& req){req.group.selected[1]=record(99);});
    reset();Config::instance().is_host=false;authority.group_before->random[0]++;need(perform(fixture_binding,r,&authority,e)==action::Outcome::RejectedNoEffect&&!original_calls,"different complete Host prior admitted");
    for(const std::string stage:{"original","inverse","restore","cash"}){reset();failure=stage;
        need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&original_calls==1&&!e.empty(),"partial native effect not Unknown");
        need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&original_calls==1,"partial native command replayed");}
    reset();need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Confirmed,e);fixture_sample.local.handles.at(record(1)).uid++;
    need(!observe(fixture_binding,r,authority,e),"changed local full UID observed confirmed");
}
}
int main()try{runtime_fixture::run();std::cout<<f::checks<<" actual Group capture/pending Host/Client/observe/retirement cases passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
