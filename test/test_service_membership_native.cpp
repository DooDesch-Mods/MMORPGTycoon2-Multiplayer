#include "typed_action/action_session.h"
#include "remove_diagnostics.h"
#include "service_membership_fixture.h"
#include <atomic>
#include <iostream>
namespace action=mt2mp::action_session;namespace service_membership_reader=mt2mp::service_membership_reader;
namespace remove_diagnostics=mt2mp::game::remove_diagnostics;
using service_fixture::need;using json=nlohmann::json;
struct Config {bool is_host=true;static Config& instance(){static Config value;return value;}};
uint32_t GetCurrentThreadId(){return 17;}
namespace host {
struct Origin {uint64_t epoch=3,generation=4;uintptr_t state=0xb0000;bool operator==(const Origin&)const=default;};
struct WorldState {Origin origin;uintptr_t selection=0x1111;};
bool same_world(const Origin& a,const Origin& b){return a==b;}
}
namespace witness {struct UndoWitness {uintptr_t active_graph=0;std::vector<uintptr_t> undo{0x999};bool operator==(const UndoWitness&)const=default;};}
struct Runtime {uintptr_t image=service_fixture::image;uint32_t thread=17;bool ready=true;service_fixture::Memory memory;std::optional<int> pending;};
Runtime runtime;std::atomic<Runtime*> published{&runtime};
const std::string fixture_snapshot="22222222-2222-4222-8222-222222222222",fixture_key="action:33333333-3333-4333-8333-333333333333",id="11111111-1111-4111-8111-111111111111";
action::Binding fixture_binding{3,4};
json fixture_controller;host::WorldState current_world;action::PropTextState current_object;action::Ledger current_money;witness::UndoWitness current_undo;
remove_diagnostics::LocalTarget fixture_target;bool window_ok=true,origin_ok=true;uintptr_t selected_window=0x7777;int original_calls=0,fault_mode=0;
bool world(Runtime&,host::WorldState& out,std::string& e){if(!origin_ok){e="fixture NativeOrigin unavailable";return false;}out=current_world;return true;}
namespace mt2mp::game::remove_diagnostics {
bool resolve_record_target(const std::string& snapshot_value,const std::string& key_value,LocalTarget& out,std::string& e){
    if(snapshot_value!=fixture_snapshot||key_value!=fixture_key){e="fixture exact canonical fixture_target absent";return false;}out=fixture_target;return true;}
bool resolve_local_target(uintptr_t,uintptr_t object,LocalTarget& out,std::string& e){if(object!=fixture_target.object){e="fixture foreign local fixture_target";return false;}out=fixture_target;return true;}
}
namespace net {std::string current_snapshot_id(){return fixture_snapshot;}}
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool network_binding(const std::string& wanted,action::Binding& out,std::string& e){if(wanted!=fixture_snapshot||fixture_controller.value("pending",true)||fixture_controller.value("locked",true)){e="network admission requires idle unlocked Controller";return false;}out=fixture_binding;return true;}
}
namespace prop_text {
bool pending(){return false;}
struct UiMemory {explicit UiMemory(Runtime&){};};
bool text(Runtime& r,uintptr_t at,std::string& out,std::string& e){return r.memory.text(at,out,e);}
bool read(Runtime&,const remove_diagnostics::LocalTarget&,host::WorldState& w,action::PropTextState& out,action::Ledger& money,witness::UndoWitness& undo,std::string& e){
    if(!origin_ok){e="fixture current Prop full UID rejected";return false;}w=current_world;out=current_object;money=current_money;undo=current_undo;return true;}
}
namespace boss_phase {bool pending(){return false;}}
namespace action_ui {
enum class Family {Building};
struct Window {uintptr_t window=0;};
struct Context {uintptr_t image,selection,target;uint64_t uid,epoch,generation;std::string sha;Family family;};
bool read_live_window(prop_text::UiMemory&,const Context& context,Window& out,std::string& e){if(!window_ok||context.target!=fixture_target.object||context.uid!=fixture_target.uid){e="fixture exact selected Window differs";return false;}out.window=selected_window;return true;}
}
namespace mt2mp::action_session {namespace ui_diagnostic=::action_ui;}
bool installed=true;uint64_t captures=0;
struct Sample {host::WorldState world;remove_diagnostics::LocalTarget target;action::PropTextState object;action::ServiceMembershipState value;service_membership_reader::Local local;action::Ledger money;witness::UndoWitness undo;uintptr_t random_owner=0;std::array<uint64_t,4> random{};};
struct Pending {action::Binding binding;std::string snapshot,key;action::ServiceMembershipIntent intent;};
struct Operation {action::Binding binding;action::Request request;Sample before,after;action::Evidence evidence;bool attempted=false,returned=false,confirmed=false,retired=false;std::string cause;};
std::optional<Pending> captured;std::map<std::string,Operation> operations;
#include "service_sample_capture_actual.inc"
uint32_t invoke(Runtime& r,const action::ServiceMembershipIntent& intent,const Sample& before,bool& returned){
    ++original_calls;if(fault_mode==1)return 5;
    service_fixture::mutate(r.memory,intent,before.value,before.local);returned=true;
    if(fault_mode==2)current_money.cash++;if(fault_mode==3)r.memory.put(0xa0000,std::array<uint64_t,4>{8,2,3,4});
    if(fault_mode==4)r.memory.put(service_fixture::service+0x1000+0x18,uint64_t(0x999999999));
    return fault_mode==5?5:0;
}
#include "service_unchanged_actual.inc"
#include "service_perform_actual.inc"
void reset(){runtime={};runtime.memory=service_fixture::memory();published=&runtime;fixture_controller={{"started",true},{"locked",false},{"pending",false},{"action_uuid",id}};
    fixture_target={};fixture_target.object=service_fixture::building;fixture_target.uid=0x123456789;fixture_target.world_epoch=3;fixture_target.world_generation=4;fixture_target.snapshot_id=fixture_snapshot;fixture_target.record_key=fixture_key;fixture_target.normal_building=true;
    current_world={};current_object={};current_object.object.type="SmallInn";current_object.object.variant="Base";current_object.object.alive=true;current_object.name="Inn";current_money={};current_undo={};
    window_ok=origin_ok=installed=true;captured.reset();operations.clear();original_calls=fault_mode=0;Config::instance().is_host=true;
}
action::Request request(action::ServiceMembershipIntent intent){action::Request r;r.action=action::Action::EditServiceMembership;r.peer=1;r.sequence=1;r.action_uuid=id;r.snapshot_uuid=fixture_snapshot;r.canonical_record_key=fixture_key;r.service_membership=intent;return r;}
int main(){try{unsigned cases=0;std::string e;reset();
    need(capture(runtime,fixture_target.object,selected_window,0,service_fixture::character,action::ServiceMembershipKind::AddSpawn,e),e);need(captured&&captured->intent.character==0&&original_calls==0,"capture before original allocation");++cases;
    reset();need(capture(runtime,fixture_target.object,0,service_fixture::service+0x1000,0,action::ServiceMembershipKind::Remove,e),e);need(captured&&captured->intent.ordinal==1,"exact owned source ordinal");++cases;
    auto bad_capture=[&](auto change){reset();change();need(!capture(runtime,fixture_target.object,selected_window,0,0,action::ServiceMembershipKind::AddSpawn,e),"bad capture accepted");need(!captured&&original_calls==0,"rejected source mutated");++cases;};
    bad_capture([]{fixture_controller["pending"]=true;});bad_capture([]{fixture_controller["locked"]=true;});bad_capture([]{window_ok=false;});bad_capture([]{fixture_target.world_generation++;});bad_capture([]{runtime.thread++;});
    bad_capture([]{runtime.memory.put(0xb0000+0x168,uintptr_t(0xdead));});bad_capture([]{runtime.memory.put(service_fixture::image+0x164b250,uintptr_t(0));});
    for(auto intent:{action::ServiceMembershipIntent{action::ServiceMembershipKind::AddSpawn,-1,-1},action::ServiceMembershipIntent{action::ServiceMembershipKind::AddSpawn,0,-1},action::ServiceMembershipIntent{action::ServiceMembershipKind::Remove,-1,1}}){reset();const auto r=request(intent);fixture_controller["pending"]=true;
        need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Confirmed,e);need(original_calls==1,"Host original once");action::Evidence authority;need(observe(fixture_binding,r,authority,e),e);need(retire(fixture_binding,r,e),e);++cases;
        need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&original_calls==1,"Host replay blocked");++cases;
        reset();Config::instance().is_host=false;fixture_controller["pending"]=true;need(perform(fixture_binding,r,&authority,e)==action::Outcome::Confirmed,e);need(original_calls==1,"Client original once");action::Evidence actual;need(observe(fixture_binding,r,actual,e)&&actual.service_membership_after==authority.service_membership_after,e);++cases;
        current_money.cash++;need(!observe(fixture_binding,r,actual,e),"ACK accepted changed ledger");++cases;
        reset();Config::instance().is_host=false;fixture_controller["pending"]=true;auto wrong=authority;wrong.service_membership_before->rows[0].price[1]++;
        need(perform(fixture_binding,r,&wrong,e)==action::Outcome::RejectedNoEffect&&original_calls==0&&operations.empty(),"Client stale before mutated");++cases;
    }
    for(int fault=1;fault<=5;++fault){reset();fault_mode=fault;fixture_controller["pending"]=true;const auto r=request({action::ServiceMembershipKind::AddSpawn,-1,-1});
        need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&original_calls==1,"partial mutation not Unknown");need(!operations.at(id).confirmed&&!operations.at(id).cause.empty(),"failure cause missing");need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&original_calls==1,"partial replay occurred");++cases;}
    reset();fixture_controller["pending"]=true;fixture_controller["action_uuid"]="wrong";need(perform(fixture_binding,request({}),nullptr,e)==action::Outcome::RejectedNoEffect&&original_calls==0,"wrong pending UUID accepted");++cases;
    reset();fixture_controller["pending"]=true;Config::instance().is_host=false;need(perform(fixture_binding,request({}),nullptr,e)==action::Outcome::RejectedNoEffect&&original_calls==0,"Client authority absent accepted");++cases;
    reset();fixture_controller["pending"]=true;
    const auto character_count=int32_t(action::max_payload/(4096*2)+2);
    runtime.memory.array(service_fixture::manager,0x80,service_fixture::character_data,character_count,character_count);
    for(int32_t i=0;i<character_count;++i){const uintptr_t character=i?0x100000+uintptr_t(i)*0x1000:service_fixture::character;
        runtime.memory.put(service_fixture::character_data+size_t(i)*8,character);
        runtime.memory.put(character,service_fixture::image+0x1705990);runtime.memory.put(character+0x18,uint64_t(0x100000001)+i);
        runtime.memory.put(character+0x2c,int32_t(0));runtime.memory.put(character+0x30,i);runtime.memory.put(character+0x490,uint8_t(0));runtime.memory.strings[character+0xb0]=std::string(4096,'c');}
    need(size_t(character_count)*4096*2>action::max_payload,"oversize fixture must exceed the hard packet limit");
    need(perform(fixture_binding,request({}),nullptr,e)==action::Outcome::RejectedNoEffect&&original_calls==0&&operations.empty()&&e=="action payload exceeds "+std::to_string(action::max_payload)+" bytes","oversized result discovered after mutation or rejected for another cause: "+e);++cases;
    std::cout<<cases<<" actual Service sample/capture/perform/ACK cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
