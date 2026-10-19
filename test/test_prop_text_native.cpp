#include "typed_action/action_session.h"
#include "remove_diagnostics.h"
#include "prop_text_dungeon.h"
namespace prop_text_dungeon=mt2mp::game::prop_text_dungeon;
#include <atomic>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace action=mt2mp::action_session;
namespace remove_diagnostics=mt2mp::game::remove_diagnostics;
using json=nlohmann::json;
void need(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
uint32_t GetCurrentThreadId(){return 17;}
struct Config {bool is_host=true;static Config& instance(){static Config v;return v;}};
struct Text {const char* data=nullptr;size_t size=0;std::array<uint8_t,16> storage{};};
namespace host {
struct Origin {uint64_t epoch=3,generation=4;bool operator==(const Origin&)const=default;};
struct WorldState {Origin origin;uintptr_t selection=0x2222,root=0x3333;};
bool same_world(const Origin& a,const Origin& b){return a==b;}
}
namespace native {struct BuildingState {action::BuildingState value;};}
namespace world_state {action::BuildingState building_state(const native::BuildingState& v){return v.value;}}
namespace witness {
struct UndoWitness {uintptr_t active_graph=0;std::vector<uintptr_t> undo{0x9999};bool operator==(const UndoWitness&)const=default;};
UndoWitness fixture_undo;
bool read_undo(int,uintptr_t,UndoWitness& out,std::string&){out=fixture_undo;return true;}
}
struct Memory {
    int process=0;std::map<uintptr_t,std::vector<uint8_t>> values;
    template<class T>void put(uintptr_t p,const T& v){const auto* b=reinterpret_cast<const uint8_t*>(&v);values[p]={b,b+sizeof v};}
    bool read(uintptr_t p,void* out,size_t n,std::string& e){auto it=values.find(p);if(it==values.end()||it->second.size()!=n){e="fixture exact memory read absent";return false;}std::memcpy(out,it->second.data(),n);return true;}
};
struct Runtime {uintptr_t image=0x140000000;uint32_t thread=17;bool ready=true;Memory memory;std::optional<int> pending;};
Runtime fixture_runtime;std::atomic<Runtime*> published{&fixture_runtime};
action::Ledger fixture_money;
action::Binding fixture_binding{3,4};
host::WorldState fixture_world;
remove_diagnostics::LocalTarget fixture_target;
action::BuildingState fixture_object;
json fixture_controller;
std::string fixture_snapshot="22222222-2222-4222-8222-222222222222";
bool fixture_lookup=true,fixture_window=true;
bool ledger(action::Ledger& out,std::string&){out=fixture_money;return true;}
bool world(Runtime&,host::WorldState& out,std::string&){out=fixture_world;return true;}
bool current_prop(Runtime&,const remove_diagnostics::LocalTarget& target,host::WorldState& out,native::BuildingState& value,std::string& e){
    if(!fixture_lookup||target.uid!=fixture_target.uid||target.object!=fixture_target.object||target.world_epoch!=fixture_world.origin.epoch||
       target.world_generation!=fixture_world.origin.generation){e="current full UID/generation differs";return false;}
    out=fixture_world;value.value=fixture_object;return true;
}
namespace mt2mp::game::remove_diagnostics {
bool resolve_local_target(uintptr_t manager,uintptr_t object,LocalTarget& out,std::string& e){
    if(manager!=fixture_world.selection||object!=fixture_target.object){e="local selected target differs";return false;}out=fixture_target;return true;
}
bool resolve_record_target(const std::string& snapshot,const std::string& key,LocalTarget& out,std::string& e){
    if(snapshot!=fixture_snapshot||key!=fixture_target.record_key){e="canonical target differs";return false;}out=fixture_target;return true;
}
}
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool network_binding(const std::string& snapshot,action::Binding& out,std::string& e){
    if(snapshot!=fixture_snapshot||fixture_controller.value("pending",true)||fixture_controller.value("locked",true)){
        e="actual public admission requires idle Controller";return false;}out=fixture_binding;return true;
}
}
namespace net {std::string current_snapshot_id(){return fixture_snapshot;}}
namespace action_ui {
struct Window {};
enum class Family {Building,Scenery};
struct Context {uintptr_t image,manager,target;uint64_t uid,epoch,generation;std::string sha;Family family;};
template<class M>bool read_live_window(M&,const Context&,Window&,std::string& e){if(!fixture_window)e="live typed window differs";return fixture_window;}
}
namespace mt2mp::action_session {namespace ui_diagnostic=::action_ui;}
struct UiMemory {explicit UiMemory(Runtime&){};};
bool installed=true;
struct Pending {action::Binding binding;std::string snapshot,key;action::PropTextIntent intent;};
struct Operation {
    action::Binding binding;action::Request request;remove_diagnostics::LocalTarget target;
    action::Evidence evidence;witness::UndoWitness undo;prop_text_dungeon::Owners dungeon_owners{};
    bool attempted=false,returned=false,confirmed=false,retired=false;std::string cause;
};
std::optional<Pending> captured;std::map<std::string,Operation> operations;uint64_t captures=0;
bool current_object(Runtime& r,const remove_diagnostics::LocalTarget& target,host::WorldState& out,native::BuildingState& value,std::string& e){return current_prop(r,target,out,value,e);}
#include "prop_text_string_actual.inc"
#include "prop_text_read_actual.inc"
#include "prop_text_capture_actual.inc"
std::string fixture_name="Old name",fixture_description="Old description",fixture_dungeon_name="Dungeon old",fixture_region_name="Region old";
bool fixture_dungeon=false,fixture_skip_region=false;
void strings(){
    auto& memory=fixture_runtime.memory;const auto object=fixture_target.object;
    Text a{fixture_name.data(),fixture_name.size(),{}},b{fixture_description.data(),fixture_description.size(),{}};
    memory.put(object+0x38,a);memory.put(object+0x7b0,b);memory.put(object+0xa38,fixture_dungeon?uintptr_t(0x20000):0);
    if(fixture_dungeon){const auto image=fixture_runtime.image;memory.put(object,image+0x16d93c0);memory.put(object+0x170,fixture_world.root);
        memory.put(0x20000,image+0x16c0560);memory.put(0x206f8,object);memory.put(0x20048,uintptr_t(0x30000));memory.put(0x20050,uintptr_t(0x40000));
        memory.put(0x30000,image+0x174ad20);memory.put(0x30048,int32_t(1));memory.put(0x30018,uintptr_t(0x20000));
        memory.put(0x40000,image+0x1787930);memory.put(0x40070,uintptr_t(0x30000));
        Text c{fixture_dungeon_name.data(),fixture_dungeon_name.size(),{}},d{fixture_region_name.data(),fixture_region_name.size(),{}};
        memory.put(0x20028,c);memory.put(0x40810,d);memory.values[uintptr_t(c.data)]={c.data,c.data+c.size};memory.values[uintptr_t(d.data)]={d.data,d.data+d.size};
    }
    memory.values[uintptr_t(a.data)]={a.data,a.data+a.size};memory.values[uintptr_t(b.data)]={b.data,b.data+b.size};
}
unsigned fixture_calls=0;bool fixture_corrupt=false;uint32_t fixture_exception=0;
uint32_t invoke(Runtime&,const action::PropTextIntent& intent,const remove_diagnostics::LocalTarget&,bool& returned){
    ++fixture_calls;
    if(intent.kind==action::PropTextKind::Name){fixture_name=intent.text;if(fixture_dungeon){fixture_dungeon_name=intent.text;if(!fixture_skip_region)fixture_region_name=intent.text;}}else fixture_description=intent.text;
    strings();if(fixture_corrupt)++fixture_money.cash;returned=fixture_exception==0;return fixture_exception;
}
#include "prop_text_perform_actual.inc"
action::Request request(){
    action::Request r;r.action=action::Action::EditPropText;r.action_uuid="11111111-1111-4111-8111-111111111111";
    r.snapshot_uuid=fixture_snapshot;r.canonical_record_key="action:33333333-3333-4333-8333-333333333333";
    r.prop_text={action::PropTextKind::Name,"New name"};return r;
}
void reset(){
    fixture_runtime=Runtime{};fixture_name="Old name";fixture_description="Old description";fixture_money={};fixture_binding={3,4};fixture_world={};
    fixture_target={};fixture_target.object=0x10000;fixture_target.uid=0x100000001;fixture_target.manager=fixture_world.selection;
    fixture_target.world_epoch=3;fixture_target.world_generation=4;fixture_target.snapshot_id=fixture_snapshot;
    fixture_target.record_key=request().canonical_record_key;fixture_target.normal_building=true;
    fixture_object={};fixture_object.type="SmallInn";fixture_object.variant="variant";fixture_object.alive=true;
    fixture_controller={{"started",true},{"pending",false},{"locked",false},{"action_uuid",request().action_uuid}};
    fixture_lookup=fixture_window=true;fixture_dungeon=fixture_skip_region=false;fixture_dungeon_name="Dungeon old";fixture_region_name="Region old";fixture_calls=0;fixture_exception=0;fixture_corrupt=false;Config::instance().is_host=true;
    witness::fixture_undo={};operations.clear();captured.reset();captures=0;strings();
}
int main(){try{unsigned count=0;std::string e;auto r=request();
    reset();need(capture(fixture_runtime,fixture_target.object,r.prop_text,e),e);need(captured&&captures==1&&fixture_calls==0,"capture must suppress setter");++count;
    reset();auto noop=r.prop_text;noop.text=fixture_name;need(capture(fixture_runtime,fixture_target.object,noop,e)&&!captured,"unchanged notification queues no action");++count;
    for(int fault=0;fault<5;++fault){reset();if(fault==0)fixture_controller["pending"]=true;if(fault==1)fixture_window=false;
        if(fault==2)fixture_lookup=false;if(fault==3)witness::fixture_undo.active_graph=1;if(fault==4)fixture_target.world_generation=5;
        need(!capture(fixture_runtime,fixture_target.object,r.prop_text,e)&&!captured&&fixture_calls==0,"capture guard skipped");++count;}
    reset();fixture_controller["pending"]=true;need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Confirmed,e);need(fixture_calls==1,"Host setter once");
    action::Evidence host_result;need(observe(fixture_binding,r,host_result,e),e);need(retire(fixture_binding,r,e),e);need(observe(fixture_binding,r,host_result,e)&&host_result.retirement_confirmed,e);++count;
    need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&fixture_calls==1,"duplicate must not replay");++count;
    reset();Config::instance().is_host=false;fixture_controller["pending"]=true;need(perform(fixture_binding,r,&host_result,e)==action::Outcome::Confirmed,e);need(fixture_calls==1,"Client setter once while pending");++count;
    reset();fixture_controller["pending"]=true;need(perform(fixture_binding,r,&host_result,e)==action::Outcome::RejectedNoEffect&&fixture_calls==0,"wrong Host authority role");++count;
    reset();fixture_controller["pending"]=true;fixture_corrupt=true;need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&fixture_calls==1,"partial effect must remain Unknown");++count;
    reset();fixture_controller["pending"]=true;fixture_exception=5;need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&fixture_calls==1,"original exception must remain Unknown");++count;
    reset();fixture_controller["pending"]=true;fixture_runtime.memory.put(fixture_target.object+0xa38,uintptr_t(0x7777));need(perform(fixture_binding,r,nullptr,e)==action::Outcome::RejectedNoEffect&&fixture_calls==0,"Unbound Dungeon rejected before mutation");++count;
    reset();fixture_dungeon=true;fixture_target.normal_building=false;strings();fixture_controller["pending"]=true;
    need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Confirmed&&fixture_calls==1&&fixture_dungeon_name==r.prop_text.text&&fixture_region_name==r.prop_text.text,e);++count;
    reset();fixture_dungeon=true;fixture_target.normal_building=false;strings();fixture_controller["pending"]=true;fixture_skip_region=true;
    need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&fixture_calls==1,"Missing original Dungeon Region name effect accepted");++count;
    reset();fixture_dungeon=true;strings();fixture_runtime.memory.put(0x206f8,uintptr_t(0x123));fixture_controller["pending"]=true;
    need(perform(fixture_binding,r,nullptr,e)==action::Outcome::RejectedNoEffect&&fixture_calls==0,"Foreign Dungeon exterior owner accepted");++count;
    reset();fixture_dungeon=true;strings();fixture_runtime.memory.put(0x40070,uintptr_t(0x123));fixture_controller["pending"]=true;
    need(perform(fixture_binding,r,nullptr,e)==action::Outcome::RejectedNoEffect&&fixture_calls==0,"Foreign Dungeon Region Shard accepted");++count;
    reset();r.prop_text={action::PropTextKind::Description,""};fixture_controller["pending"]=true;need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Confirmed&&fixture_description.empty(),e);++count;
    std::cout<<count<<" actual Prop capture/read/perform/observe/retire cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
