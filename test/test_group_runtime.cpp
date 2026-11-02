#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include "typed_action/action_session.h"
#include "group_native.h"
#include "move_contract.h"
#include <atomic>
#include <optional>
namespace mt2mp::action_session {
struct TestSession { Context context; bool current(Context& out,std::string&) {out=context;return true;} };
TestSession& selected_session(){static TestSession value;return value;}
namespace tick_commands::authority {
bool enabled=false,matched=true;
bool active(){return enabled;}
bool matches(const Context&,const Binding&,const Request&,std::string& e){if(!enabled||!matched){e="scope mismatch";return false;}return true;}
}}
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
struct Config {bool is_host=true,diagnostic_mailbox_enabled=false;static Config& instance(){static Config value;return value;}};
namespace host {struct WorldState {uintptr_t selection=f::selection;uint64_t generation=4;bool operator==(const WorldState&)const=default;};}
struct Sample {host::WorldState world;group_reader::Local local;action::Ledger money;unsigned undo=0;uintptr_t random_owner=0;};
struct Runtime {uintptr_t image=f::image;uint32_t thread=17;bool ready=true;f::Memory memory;std::optional<int> pending;};
struct Pending {action::Binding binding;std::string snapshot;action::GroupIntent intent;Sample before;json diagnostic_context;};
struct Operation {action::Binding binding;action::Request request;Sample before,after;action::Evidence evidence;group_native::Receipt native;
    bool confirmed=false,retired=false;std::string cause;bool ordered=false;action::Context ordered_context;};
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
bool resolve_record_targets(const std::string& snapshot,const std::vector<std::string>& keys,std::vector<LocalTarget>& out,std::string& e){
    out.clear();for(const auto& key:keys){const auto found=fixture_sample.local.handles.find(key);
        if(snapshot!=fixture_snapshot||found==fixture_sample.local.handles.end()||key.starts_with("group:")){e="current canonical Scenery target absent";return false;}
        out.push_back({true,found->second.object});}return true;
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
        ++fixture_sample.undo;if(failure=="cash")++fixture_sample.money.cash;if(failure=="random")++fixture_sample.local.state.random[0];return true;
    }
    bool read_after(const group_reader::Local&,group_reader::Local& out,std::string&){after=fixture_sample;out=after.local;return true;}
    bool inverse(const group_reader::Local&,const group_reader::Local&,const group_transition::Plan&,const group_reader::Handle&,uintptr_t& graph,std::string& e){
        if(failure=="inverse"){e="original inverse differs";return false;}graph=0x8888;return true;}
    bool cleanup_empty(const group_reader::Handle&,std::string&){return true;}
    bool restore(const group_reader::Local&,const group_reader::Local&,const group_transition::Plan&,bool,std::string& e){
        if(failure=="restore"){e="original selection restore failed";return false;}return true;}
};
bool remember_history(Runtime&,const Operation& op,std::string& e){if(!op.native.graph){e="source graph absent";return false;}return true;}
json group_source_context(const std::string&,const action::Binding&,const Sample&){return nullptr;}
#include "group_capture_actual.inc"
#include "group_scope_actual.inc"
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
void run(){std::string e;reset();
    for(bool host:{true,false}){reset();Config::instance().is_host=host;fixture_controller["ordered"]=true;
        action::tick_commands::authority::enabled=true;action::tick_commands::authority::matched=true;
        action::selected_session().context.role=host?action::Role::Host:action::Role::Client;auto r=request();
        need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Confirmed&&original_calls==1,e);action::Evidence evidence;
        need(observe(fixture_binding,r,evidence,e)&&retire(fixture_binding,r,e),e);
        action::tick_commands::authority::enabled=false;need(!observe(fixture_binding,r,evidence,e),"route substitution accepted");}
    reset();fixture_controller["ordered"]=true;need(perform(fixture_binding,request(),nullptr,e)==action::Outcome::RejectedNoEffect&&!original_calls,"missing consumption accepted");
    action::tick_commands::authority::enabled=true;action::tick_commands::authority::matched=false;
    need(perform(fixture_binding,request(),nullptr,e)==action::Outcome::RejectedNoEffect&&!original_calls,"foreign consumed request accepted");
    action::tick_commands::authority::enabled=false;action::tick_commands::authority::matched=true;reset();fixture_controller["pending"]=false;
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
    reset();fixture_controller["ordered"]=true;action::tick_commands::authority::enabled=true;action::tick_commands::authority::matched=true;
    action::selected_session().context.role=action::Role::Host;fixture_sample.local.state.random={9,8,7,6};
    need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Confirmed&&operations.at(fixture_id).evidence.group_before->random==std::array<uint64_t,4>{9,8,7,6},
         "ordered execution records fresh RNG baseline");
    reset();fixture_controller["ordered"]=true;action::tick_commands::authority::enabled=true;failure="random";
    need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&original_calls==1,"ordered native execution RNG mutation remains Unknown");
    action::tick_commands::authority::enabled=false;
}
}
namespace source_difference_fixture {
using json=nlohmann::json;using f::need;namespace action=mt2mp::action_session;namespace group_reader=mt2mp::game::group_reader;
namespace witness {struct UndoWitness {uintptr_t receiver=0,active_graph=0;std::vector<uintptr_t> undo,redo;bool operator==(const UndoWitness&)const=default;};}
namespace host {struct WorldState {int origin=0;uintptr_t root=1,selection=2;};bool same_world(int a,int b){return a==b;}}
struct Sample {host::WorldState world;group_reader::Local local;action::Ledger money;witness::UndoWitness undo;uintptr_t random_owner=3;};
bool same_binding(const Sample& a,const Sample& b){return host::same_world(a.world.origin,b.world.origin)&&a.world.root==b.world.root&&a.world.selection==b.world.selection&&a.local.manager==b.local.manager&&a.random_owner==b.random_owner;}
#include "group_source_difference_actual.inc"
struct Pending {action::Binding binding;std::string snapshot;action::GroupIntent intent;Sample before;json diagnostic_context;};
struct Runtime{};Runtime runtime;std::atomic<Runtime*> published{&runtime};std::optional<Pending> captured;Sample observed;json last_source_difference;bool sample_ok=true;
struct Config {bool diagnostic_mailbox_enabled=false;static Config& instance(){static Config value;return value;}};
namespace net {namespace session_wire {bool connected(){return true;}}}
namespace typed_action_runtime {
action::Binding current_binding{1,2};unsigned submissions=0;bool ordered=false;action::GroupIntent submitted_intent;
json controller_status(){return {{"ordered",ordered}};}
bool network_binding(const std::string&,action::Binding& out,std::string&){out=current_binding;return true;}
bool submit_group(const std::string&,const action::GroupIntent& intent,std::string&){++submissions;submitted_intent=intent;return true;}
}
std::string rejected;unsigned submitted=0;
void reject(const std::string& e){rejected=e;}
void retire_session(){}
std::vector<uintptr_t> props(const group_reader::Local&){return {};}
bool sample(Runtime&,const action::Binding&,const std::vector<uintptr_t>&,Sample& out,std::string& error){if(!sample_ok){error="actual source read failed";return false;}out=observed;return true;}
json group_source_context(const std::string&,const action::Binding&,const Sample&){return nullptr;}
std::string uuid(std::string&){return "fresh";}
#include "group_tick_actual.inc"
void run(){Sample before;action::GroupProp prop;prop.key="prop";prop.region=67;before.local.state.props.push_back(prop);
    before.local.handles["prop"]={10,11};before.undo.undo={77};
    for(int changed=0;changed<8;++changed){observed=before;typed_action_runtime::ordered=false;typed_action_runtime::current_binding={1,2};typed_action_runtime::submissions=submitted=0;rejected.clear();
        if(changed==0)observed.world.root=9;
        if(changed==1)observed.local.state.groups.push_back({});
        if(changed==2)observed.local.state.props[0].pose[0]=1;
        if(changed==3)observed.local.state.random[1]=4;
        if(changed==4)observed.local.handles["prop"].uid=12;
        if(changed==5)observed.money.cash=2;
        if(changed==6)observed.undo.undo.push_back(78);
        if(changed==7)typed_action_runtime::current_binding.native_epoch=9;
        captured=Pending{{1,2},"snapshot",{},before,nullptr};tick();
        const char* field=changed==0||changed==7?"binding":changed==1?"groups":changed==2?"props":changed==3?"global_rng":changed==4?"handles":changed==5?"ledger":"undo";
        const auto& details=changed==7?last_source_difference:last_source_difference.at("differences");
        need(details.contains(field)&&!rejected.empty()&&!captured&&submitted==0&&typed_action_runtime::submissions==0,"actual tick rejects and identifies changed source component before submission");
        if(changed==2)need(details.at("props").at("rows").at(0).at("after").at("pose").at(0)==1,"changed prop pose bits retained");
        if(changed==3)need(details.at("global_rng").at("before").at(1)==0&&details.at("global_rng").at("after").at(1)==4,"changed global RNG words retained");
        if(changed==4)need(details.at("handles").at("rows").at(0).at("before_uid")==11&&details.at("handles").at("rows").at(0).at("after_uid")==12,"changed full UID retained");
        if(changed==5)need(details.at("ledger").at("before").at("cash")==0&&details.at("ledger").at("after").at("cash")==2,"changed Ledger values retained");
        if(changed==6)need(details.at("undo").at("undo_changes").at("rows").at(0).at("after")==78,"changed Undo graph identity retained");
        if(changed==7)need(details.at("binding").at("before_epoch")==1&&details.at("binding").at("after_epoch")==9,"changed native epoch retained");
    }
    observed=before;observed.local.state.random[0]=1;observed.money.cash=4;typed_action_runtime::current_binding={1,2};
    captured=Pending{{1,2},"snapshot",{},before,nullptr};tick();
    need(last_source_difference.at("differences").contains("global_rng")&&last_source_difference.at("differences").contains("ledger"),"actual tick retains simultaneous changed components");
    observed=before;rejected.clear();captured=Pending{{1,2},"snapshot",{},before,nullptr};tick();
    need(rejected.empty()&&submitted==1&&typed_action_runtime::submissions==1,"unchanged source follows original submission path once");
    for(auto kind:{action::GroupKind::Group,action::GroupKind::Ungroup}){
        runtime_fixture::reset();runtime_fixture::fixture_controller["pending"]=false;std::string e;
        need(runtime_fixture::capture(runtime_fixture::fixture_runtime,f::selection,0,kind,e),"actual UI capture accepted");
        const auto original=*runtime_fixture::captured;
        Sample captured_sample;captured_sample.local=original.before.local;captured_sample.money=original.before.money;
        captured_sample.local.state.random={18038251647744536283ull,5539200321442195391ull,1651901221901139748ull,6918384378640680782ull};
        observed=captured_sample;observed.local.state.random={15458754130589060650ull,11558264680628827712ull,10234522170607243775ull,18166999296775747814ull};
        typed_action_runtime::current_binding=original.binding;typed_action_runtime::ordered=true;typed_action_runtime::submissions=submitted=0;rejected.clear();
        captured=Pending{original.binding,original.snapshot,original.intent,captured_sample,nullptr};const auto rng=observed.local.state.random;tick();tick();
        need(rejected.empty()&&submitted==1&&typed_action_runtime::submissions==1&&typed_action_runtime::submitted_intent==original.intent&&observed.local.state.random==rng,
             "actual ordered capture and tick submit original intent once despite cross-boundary RNG advancement");
        typed_action_runtime::ordered=false;typed_action_runtime::submissions=submitted=0;rejected.clear();
        captured=Pending{original.binding,original.snapshot,original.intent,captured_sample,nullptr};tick();
        need(!rejected.empty()&&submitted==0&&typed_action_runtime::submissions==0,"legacy capture retains complete RNG equality");
    }
    for(int changed=0;changed<6;++changed){observed=before;typed_action_runtime::ordered=true;typed_action_runtime::current_binding={1,2};typed_action_runtime::submissions=submitted=0;rejected.clear();
        if(changed==0)observed.world.root=9;if(changed==1)observed.local.state.groups.push_back({});
        if(changed==2)observed.local.state.props[0].pose[0]=1;if(changed==3)observed.local.handles["prop"].uid=12;
        if(changed==4)observed.money.cash=2;if(changed==5)observed.undo.undo.push_back(78);
        captured=Pending{{1,2},"snapshot",{},before,nullptr};tick();
        need(!rejected.empty()&&submitted==0&&typed_action_runtime::submissions==0,"ordered capture still rejects structural, identity, Ledger and Undo changes");}
    sample_ok=false;typed_action_runtime::submissions=submitted=0;rejected.clear();captured=Pending{{1,2},"snapshot",{},before,nullptr};tick();
    need(rejected=="actual source read failed"&&submitted==0&&typed_action_runtime::submissions==0,"source sampling failure remains visible and cannot submit");sample_ok=true;
}
}
int main()try{runtime_fixture::run();source_difference_fixture::run();std::cout<<f::checks<<" actual Group capture/pending Host/Client/observe/retirement cases passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
