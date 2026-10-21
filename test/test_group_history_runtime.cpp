#define main group_protocol_fixture_main
#include "test_group_protocol.cpp"
#undef main
#include "group_reader.h"
#include "group_history_reader.h"
#include "move_contract.h"
#include <atomic>
namespace gh=mt2mp::game::group_history;
namespace history_fixture {
namespace action=mt2mp::action_session;
namespace group_reader=mt2mp::game::group_reader;
namespace group_history=mt2mp::game::group_history;
namespace q=group_reader::q;
namespace contract=mt2mp::game::ui_move_runtime::contract;
using json=nlohmann::json;
struct World {uint64_t generation=4;bool operator==(const World&)const=default;};
struct Sample {World world;group_reader::Local local;action::Ledger money;unsigned undo=0;std::map<std::string,std::string> group_names;};
struct Runtime {bool ready=true;uint32_t thread=17;};
struct NativeReceipt{uintptr_t graph=1;};
struct Operation {action::Binding binding;action::Request request;Sample before,after;bool confirmed=true,retired=true;NativeReceipt native;};
#include "group_history_structs_actual.inc"
Runtime fixture_runtime;std::atomic<Runtime*> published{&fixture_runtime};bool installed=true;
struct Config {bool is_host=true;static Config& instance(){static Config c;return c;}};
uint32_t GetCurrentThreadId(){return 17;}
std::map<std::string,Operation> operations;
const action::Binding fixture_binding{3,4};Sample fixture_state;json fixture_controller;
std::string fault;unsigned original_calls=0,restores=0,public_binding_calls=0;uintptr_t graph_current=1;
namespace net {std::string current_snapshot_id(){return fixture_snapshot_uuid;}}
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool network_binding(const std::string&,action::Binding&,std::string& e){++public_binding_calls;e="public idle admission rejects pending";return false;}
}
std::vector<uintptr_t> props(const group_reader::Local& local){std::vector<uintptr_t> result;for(const auto& row:local.state.props)result.push_back(local.handles.at(row.key).object);return result;}
bool same_binding(const Sample& a,const Sample& b){return a.world==b.world&&a.local.manager==b.local.manager;}
bool sample(Runtime&,const action::Binding& binding,const std::vector<uintptr_t>&,Sample& out,std::string& e){
    if(binding!=fixture_binding||fixture_state.world.generation!=binding.native_generation){e="full current world binding differs";return false;}
    if(fault=="after-read"&&original_calls){e="read failed after original";return false;}out=fixture_state;return true;}
bool find(Runtime&,uint64_t uid,uintptr_t& object,std::string&){object=0;for(const auto& [key_value,h]:fixture_state.local.handles){(void)key_value;if(h.uid==uid)object=h.object;}return true;}
std::vector<group_history::Node> history_values(const std::vector<group_history::LocalNode>& rows){std::vector<group_history::Node> result;for(const auto& row:rows)result.push_back(row.value);return result;}
bool history_nodes(Runtime&,const Sample&,uintptr_t,const HistorySource&,std::vector<group_history::LocalNode>& out,std::string& e){
    if(fault=="graph-before"&&!original_calls){e="first exact node owner failure";return false;}
    const auto& op=operations.at(fixture_action_uuid);const auto& group=op.after.local.state.groups[0];
    group_history::Node node{graph_current==1?gh::Kind::Remove:gh::Kind::Add,group.key,group.region,group.pose,{key(1),key(2)}};
    if(fault=="inverse-members"&&original_calls)std::reverse(node.members.begin(),node.members.end());
    if(fault=="inverse-pose"&&original_calls)node.saved_pose[4]=1;
    out={{node,44,77,"Group"}};return true;
}
struct Port{Runtime& r;const action::Binding& binding;Sample& initial;
    bool ready(const group_reader::Local&,std::string& e){if(fault=="ready"){e="original Selection not ready";return false;}return true;}};
bool history_ready(Port& port,const Sample& before,std::string& e){return port.ready(before.local,e);}
bool history_restore(Port&,const Sample&,const Sample&,std::string& e){++restores;if(fault=="restore"||fault=="original-restore"){e="selection restore failed";return false;}return true;}
#include "group_history_identity_actual.inc"
namespace building_history_slice {
bool graph_for(action::Action,const std::string& source,uintptr_t& graph,std::string& e){
    if(source!=fixture_action_uuid||fault=="stack"){e="exact native graph is not stack top";return false;}graph=graph_current;return true;}
action::Outcome perform(action::Action direction,const std::string&,std::string& e){
    if(fault=="bridge-reject"){e="original history queue not ready";return action::Outcome::RejectedNoEffect;}
    ++original_calls;if(fault=="original"||fault=="original-restore"){e="first original graph failure";return action::Outcome::Unknown;}
    auto& source=operations.at(fixture_action_uuid);fixture_state=direction==action::Action::UndoBuilding?source.before:source.after;
    if(direction==action::Action::RedoBuilding)for(auto& [key_value,name]:fixture_state.group_names){
        (void)key_value;name=history_sources.at(fixture_action_uuid).known.at(77).restore_name;}
    graph_current=direction==action::Action::UndoBuilding?2:1;fixture_state.undo=unsigned(graph_current);
    if(fault=="cash")++fixture_state.money.cash;
    if(fault=="member-uid")fixture_state.local.handles.at(key(1)).uid++;
    if(fault=="pose")fixture_state.local.state.props[0].pose[4]=1;
    if(fault=="random")fixture_state.local.state.random[0]=1;
    return action::Outcome::Confirmed;
}
}
#include "group_history_perform_actual.inc"
Sample local_sample(const GroupState& state){Sample result;result.local.state=state;result.local.manager=12;result.undo=1;uintptr_t object=100;
    for(const auto& p:state.props){result.local.handles[p.key]={object,uint64_t(object)};object+=10;}
    for(const auto& g:state.groups){result.local.handles[g.key]={900,77};result.group_names[g.key]="Group";}return result;}
action::Request request(action::Action direction=action::Action::UndoBuilding){action::Request r;r.action=direction;r.peer=2;r.sequence=1;
    r.action_uuid="33333333-3333-4333-8333-333333333333";r.snapshot_uuid=fixture_snapshot_uuid;r.target_action_uuid=fixture_action_uuid;r.canonical_record_key="groups:"+fixture_action_uuid;return r;}
void reset(){fixture_runtime={};installed=true;operations.clear();history_sources.clear();history_operations.clear();original_calls=restores=public_binding_calls=0;graph_current=1;fault.clear();Config::instance().is_host=true;
    const auto prior=before(),after=apply(prior,{GroupKind::Group,{key(1),key(2)}});auto& source=operations[fixture_action_uuid];
    source.binding=fixture_binding;source.request.action=Action::EditGroup;source.request.action_uuid=fixture_action_uuid;source.request.snapshot_uuid=fixture_snapshot_uuid;
    source.before=local_sample(prior);source.after=local_sample(after);fixture_state=source.after;
    auto& remembered=history_sources[fixture_action_uuid];const auto& g=after.groups[0];remembered.known[77]={77,g,"Group","Group"};
    remembered.undo_nodes={{gh::Kind::Remove,g.key,g.region,g.pose,{key(1),key(2)}}};
    const auto r=request();fixture_controller={{"started",true},{"pending",true},{"locked",false},{"action_uuid",r.action_uuid},{"host_revision",uint64_t(2)}};
}
void run(){std::string e;reset();auto req=request();need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Confirmed,e);Evidence authority;
    need(observe_history(fixture_binding,req,authority,e)&&original_calls==1&&restores==1&&!public_binding_calls,e);
    need(retire_history(fixture_binding,req,e)&&!retire_history(fixture_binding,req,e),"history retired twice");
    need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Unknown&&original_calls==1,"confirmed graph replayed");
    auto redo=request(Action::RedoBuilding);redo.action_uuid="44444444-4444-4444-8444-444444444444";fixture_controller["action_uuid"]=redo.action_uuid;fixture_controller["host_revision"]=uint64_t(3);
    need(perform_history(fixture_binding,redo,nullptr,e)==Outcome::Confirmed&&original_calls==2,e);
    need(fixture_state.local.handles.at("group:"+key(1)).uid==77,"Redo lost original restored Group full UID");
    reset();const auto fixture_group_key="group:"+key(1);
    operations.at(fixture_action_uuid).after.group_names[fixture_group_key]="Backing property";
    fixture_state=operations.at(fixture_action_uuid).after;
    history_sources.at(fixture_action_uuid).known.at(77).name="Backing property";
    history_sources.at(fixture_action_uuid).known.at(77).restore_name="Native display label";
    need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Confirmed&&original_calls==1,e);
    need(observe_history(fixture_binding,req,authority,e)&&retire_history(fixture_binding,req,e),e);
    fixture_controller["action_uuid"]=redo.action_uuid;fixture_controller["host_revision"]=uint64_t(3);
    need(perform_history(fixture_binding,redo,nullptr,e)==Outcome::Confirmed&&original_calls==2,e);
    need(fixture_state.group_names.at(fixture_group_key)=="Native display label","original recreate name was not retained");
    need(observe_history(fixture_binding,redo,authority,e)&&retire_history(fixture_binding,redo,e),e);
    auto fixture_recreated=fixture_state;auto fixture_absent=operations.at(fixture_action_uuid).before;
    const auto& fixture_source=history_sources.at(fixture_action_uuid);
    fixture_recreated.group_names[fixture_group_key]="Backing property";
    need(!history_identity(fixture_runtime,fixture_absent,fixture_recreated,fixture_source,e),"recreated Group accepted backing name instead of saved native name");
    fixture_recreated=fixture_state;auto fixture_survivor=fixture_state;fixture_survivor.group_names[fixture_group_key]="Changed property";
    need(!history_identity(fixture_runtime,fixture_recreated,fixture_survivor,fixture_source,e),"surviving Group backing name drift accepted");
    reset();need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Confirmed&&observe_history(fixture_binding,req,authority,e),e);
    reset();Config::instance().is_host=false;need(perform_history(fixture_binding,req,&authority,e)==Outcome::Confirmed&&original_calls==1&&!public_binding_calls,e);
    auto rejected=[&](auto change){reset();auto r=req;change(r);need(perform_history(fixture_binding,r,nullptr,e)==Outcome::RejectedNoEffect&&!original_calls,"invalid Group history request mutated");};
    rejected([](auto&){fixture_controller["pending"]=false;});rejected([](auto&){fixture_controller["locked"]=true;});
    rejected([](auto&){fixture_runtime.thread=18;});rejected([](auto&){Config::instance().is_host=false;});
    rejected([](auto&){fixture_state.world.generation++;});rejected([](auto& r){r.canonical_record_key="groups:"+r.action_uuid;});
    rejected([](auto&){operations.at(fixture_action_uuid).retired=false;});rejected([](auto&){history_sources.clear();});
    for(const std::string stage:{"graph-before","stack","ready","bridge-reject"})rejected([&](auto&){fault=stage;});
    reset();Config::instance().is_host=false;auto bad=authority;bad.group_before->random[0]=1;
    need(perform_history(fixture_binding,req,&bad,e)==Outcome::RejectedNoEffect&&!original_calls,"Host complete prior drift admitted");
    for(const std::string stage:{"original","original-restore","after-read","cash","member-uid","pose","random","inverse-members","inverse-pose","restore"}){
        reset();fault=stage;need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Unknown&&original_calls==1&&!e.empty(),"partial original graph not Unknown");
        if(stage=="original-restore")need(e.starts_with("first original graph failure; cleanup:"),"first original cause overwritten by cleanup");
        const auto calls=original_calls;need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Unknown&&original_calls==calls,"failed graph replayed");}
    reset();need(perform_history(fixture_binding,req,nullptr,e)==Outcome::Confirmed,e);fixture_state.local.state.random[0]++;
    need(!observe_history(fixture_binding,req,authority,e),"changed complete result ACKed");
}
}
int main()try{history_fixture::run();std::cout<<"Actual Group history pending Host/Client/whole graph/inverse/full UID/ACK/cause/no-replay cases passed\n";return 0;}
catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
