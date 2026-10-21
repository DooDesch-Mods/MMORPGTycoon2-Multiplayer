#include "district_property_ui.h"
#include "district_edit_history.h"
#include "move_contract.cpp"
#include "typed_action/action_session.h"
#include "district_reader.h"
#include <atomic>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace action=mt2mp::action_session;
namespace district_reader=mt2mp::game::district_reader;
namespace district_saved_identity=mt2mp::game::district_saved_identity;
using json=nlohmann::json;
size_t checks=0;
void need(bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);}
uint32_t GetCurrentThreadId(){return 17;}
struct Config {bool is_host=true;static Config& instance(){static Config v;return v;}};
struct Memory {
    int process=0;std::map<uintptr_t,uint8_t> bytes;
    template<class T>void put(uintptr_t p,const T& v){const auto* b=reinterpret_cast<const uint8_t*>(&v);for(size_t i=0;i<sizeof v;++i)bytes[p+i]=b[i];}
    void zero(uintptr_t p,size_t count){for(size_t i=0;i<count;++i)bytes[p+i]=0;}
    bool read(uintptr_t p,void* out,size_t count,std::string& e){auto* b=static_cast<uint8_t*>(out);for(size_t i=0;i<count;++i){auto it=bytes.find(p+i);if(it==bytes.end()){e="fixture missing read "+std::to_string(p+i);return false;}b[i]=it->second;}return true;}
    void text(uintptr_t p,uintptr_t buffer,const std::string& value){put(p,buffer);put(p+8,uint64_t(value.size()));for(size_t i=0;i<value.size();++i)bytes[buffer+i]=uint8_t(value[i]);}
};
constexpr uintptr_t image=0x140000000,map_ptr=0x100000,region_ptr=0x200000,manager_ptr=0x300000,object_ptr=0x400000;
constexpr uintptr_t selection_ptr=0x500000,handler_ptr=0x600000,window_ptr=0x700000;
constexpr uint64_t object_uid=0x700000007;
namespace host {
struct Origin {uint64_t world_epoch=3,generation=4;uintptr_t map=map_ptr;bool operator==(const Origin&)const=default;};
struct WorldState {Origin origin;uintptr_t selection=selection_ptr;};
bool same_world(const Origin& a,const Origin& b){return a==b;}
}
namespace witness {
struct UndoWitness {uintptr_t active_graph=0;std::vector<uintptr_t> undo{0x9999};bool operator==(const UndoWitness&)const=default;};
UndoWitness fixture_undo;
bool read_undo(int,uintptr_t,UndoWitness& out,std::string&){out=fixture_undo;return true;}
}
struct Runtime {uintptr_t image=::image;uint32_t thread=17;bool ready=true;Memory memory;std::optional<int> pending;};
Runtime fixture_runtime;std::atomic<Runtime*> published{&fixture_runtime};
action::Ledger fixture_money;
action::Binding fixture_binding{3,4};
host::WorldState fixture_world;
district_saved_identity::Current fixture_target;
json fixture_controller;
std::string fixture_snapshot="22222222-2222-4222-8222-222222222222";
bool fixture_lookup=true,fixture_world_valid=true;
bool ledger(action::Ledger& out,std::string&){out=fixture_money;return true;}
bool world(Runtime&,host::WorldState& out,std::string& e){if(!fixture_world_valid){e="actual world not available";return false;}out=fixture_world;return true;}
namespace district_saved_identity_runtime {
bool resolve(const std::string& key,district_saved_identity::Current& out,std::string& e){
    if(!fixture_lookup||key!=fixture_target.key){e="current saved District binding differs";return false;}out=fixture_target;return true;
}
bool bind_local(uintptr_t p,uint64_t uid,std::string& key,std::string& e){
    if(!fixture_lookup||p!=fixture_target.row.pointer||uid!=fixture_target.row.uid){e="current District full UID differs";return false;}key=fixture_target.key;return true;
}
}
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool network_binding(const std::string& snapshot,action::Binding& out,std::string& e){
    if(snapshot!=fixture_snapshot||fixture_controller.value("pending",true)||fixture_controller.value("locked",true)){
        e="public network admission requires idle Controller";return false;}out=fixture_binding;return true;
}
}
namespace net {std::string current_snapshot_id(){return fixture_snapshot;}}
bool installed=true;
struct Pending {action::Binding binding;std::string snapshot,key;action::DistrictTextIntent intent;};
struct Operation {
    action::Binding binding;action::Request request;district_saved_identity::Current target;
    action::Evidence evidence;witness::UndoWitness undo;
    bool attempted=false,returned=false,confirmed=false,retired=false;std::string cause;
};
std::optional<Pending> captured;std::map<std::string,Operation> operations;uint64_t captures=0;
std::string desired(const action::DistrictTextIntent& intent){return intent.kind==action::DistrictTextKind::Name&&intent.text.empty()?"{district_unnamed}":intent.text;}
bool fixture_inside=true;uint32_t fixture_contains_code=0;
uint32_t contains_call(uintptr_t base,uintptr_t shape,uintptr_t point,bool& result){
    need(base==image&&shape==object_ptr+0x220&&point==object_ptr+0x344,"native Contains receivers differ");result=fixture_inside;return fixture_contains_code;
}
bool contains(uintptr_t shape,uintptr_t point,bool& result,std::string& e){const auto code=contains_call(image,shape,point,result);if(code)e="original Contains failed";return code==0;}
#include "district_sample_capture_actual.inc"
unsigned originals=0;int mutation=0;
uint32_t invoke(const action::DistrictTextIntent& intent,uintptr_t target,bool& returned){
    ++originals;if(mutation==1)return 0xc0000005;
    fixture_runtime.memory.text(target+(intent.kind==action::DistrictTextKind::Name?0x38:0x2b8),0x900000,desired(intent));
    if(intent.kind==action::DistrictTextKind::Name&&!fixture_inside)fixture_runtime.memory.put(target+0x350,uint8_t(0));
    if(mutation==2)fixture_runtime.memory.put(target+0x218,int32_t(1));
    if(mutation==3)fixture_money.line_counts[0]++;
    if(mutation==4)witness::fixture_undo.undo.push_back(0x8888);
    if(mutation==5)fixture_target.row.uid++;
    returned=true;return 0;
}
#include "district_perform_actual.inc"
action::Request request(){
    action::Request r;r.action=action::Action::EditDistrictText;r.peer=2;r.sequence=1;
    r.action_uuid="11111111-1111-4111-8111-111111111111";r.snapshot_uuid=fixture_snapshot;
    r.canonical_record_key=fixture_target.key;r.district_text.text="Village";return r;
}
void reset(){
    fixture_runtime={};fixture_world={};fixture_money={};witness::fixture_undo={};
    fixture_controller={{"started",true},{"locked",false},{"pending",true},{"action_uuid","11111111-1111-4111-8111-111111111111"}};
    fixture_target={};fixture_target.saved_region=67;fixture_target.key=std::string(64,'a')+":world:district:17";
    fixture_target.row.pointer=object_ptr;fixture_target.row.uid=object_uid;
    operations.clear();captured.reset();captures=0;originals=0;mutation=0;installed=true;Config::instance().is_host=true;fixture_lookup=true;fixture_world_valid=true;
    fixture_inside=true;fixture_contains_code=0;
    auto& m=fixture_runtime.memory;m.zero(object_ptr,0x360);m.put(object_ptr,image+0x16da000);m.put(object_ptr+0x18,object_uid);
    m.put(object_ptr+0x170,map_ptr+0x1c0);m.put(object_ptr+0x178,region_ptr);
    m.text(object_ptr+0x38,0x810000,"Old");m.text(object_ptr+0x2b8,0x820000,"Description");
    std::array<uint32_t,10> pose{};pose[3]=pose[7]=pose[8]=pose[9]=0x3f800000;m.put(object_ptr+0x100,pose);
    m.put(object_ptr+0x238,uintptr_t(0x830000));m.zero(0x830000,24);
    m.put(map_ptr,image+0x1742860);m.put(map_ptr+0x1c0,image+0x174ad20);m.put(map_ptr+0x208,int32_t(0));
    m.put(map_ptr+0x230,image+0x16f04e0);m.put(map_ptr+0x238,uintptr_t(0x840000));m.put(map_ptr+0x240,int32_t(1));m.put(map_ptr+0x244,int32_t(1));
    m.put(map_ptr+0x358,uintptr_t(0x850000));m.put(map_ptr+0x360,int32_t(1));m.put(map_ptr+0x364,int32_t(1));
    m.put(0x840000,region_ptr);m.put(0x850000,region_ptr);m.put(region_ptr,image+0x1787930);m.put(region_ptr+0x70,map_ptr+0x1c0);
    m.put(region_ptr+0x834,int32_t(67));m.put(region_ptr+0x970,manager_ptr);m.put(manager_ptr,image+0x1719080);m.put(manager_ptr+0x60,region_ptr);
    m.put(image+0x13df630,selection_ptr);m.put(selection_ptr,image+0x17205d0);m.put(selection_ptr+0xd1,uint8_t(0));m.put(selection_ptr+0xd3,uint8_t(0));
    m.put(selection_ptr+0x80,object_ptr);m.put(selection_ptr+0xb8,handler_ptr);
    for(size_t offset:{size_t(0x20),size_t(0x38)}){m.put(selection_ptr+offset,image+0x1748c50);m.put(selection_ptr+offset+8,uintptr_t(0x860000+offset));m.put(selection_ptr+offset+16,int32_t(1));m.put(selection_ptr+offset+20,int32_t(1));m.put(0x860000+offset,object_ptr);}
    m.put(handler_ptr,image+0x1731760);m.put(handler_ptr+0x10,window_ptr);m.put(window_ptr,image+0x172bcb0);
    m.put(image+0x172bcb0+0x168,image+0x6cb400);m.put(window_ptr+0x45f,uint8_t(1));m.put(window_ptr+0x4f0,object_ptr);
    m.put(window_ptr+0x4f8,uintptr_t(0x870000));m.put(0x870000,object_ptr+0x20);
}
void reader_checks(){
    reset();std::string e;action::DistrictState state;
    need(district_reader::read(fixture_runtime.memory,image,map_ptr,fixture_target,contains,state,e),e);
    need(state.region==67&&state.name=="Old"&&state.description=="Description"&&state.shape.empty(),"actual reader field mapping differs");
    auto bad=[&](auto edit){reset();edit();need(!district_reader::read(fixture_runtime.memory,image,map_ptr,fixture_target,contains,state,e),"invalid actual District read accepted");};
    bad([]{fixture_runtime.memory.put(object_ptr+0x18,uint64_t(7));});
    bad([]{fixture_runtime.memory.put(object_ptr+0x178,region_ptr+8);});
    bad([]{fixture_runtime.memory.put(manager_ptr+0x60,region_ptr+8);});
    bad([]{fixture_runtime.memory.put(object_ptr+0x218,int32_t(3));});
    bad([]{fixture_runtime.memory.put(object_ptr+0x324,uint8_t(2));});
    bad([]{fixture_runtime.memory.put(object_ptr+0x338,uint32_t(0x7f800000));});
    bad([]{fixture_runtime.memory.put(object_ptr+0x104,uint32_t(0x7fc00000));});
    bad([]{fixture_runtime.memory.put(object_ptr+0x40,uint64_t(4097));});
    bad([]{fixture_runtime.memory.put(object_ptr+0x350,uint8_t(1));fixture_contains_code=0xc0000005;});
    reset();need(district_reader::ui(fixture_runtime.memory,image,selection_ptr,object_ptr,e),e);
    auto ui_bad=[&](auto edit){reset();edit();need(!district_reader::ui(fixture_runtime.memory,image,selection_ptr,object_ptr,e),"invalid actual District UI accepted");};
    ui_bad([]{fixture_runtime.memory.put(selection_ptr+0xd1,uint8_t(1));});
    ui_bad([]{fixture_runtime.memory.put(selection_ptr+0x80,object_ptr+8);});
    ui_bad([]{fixture_runtime.memory.put(window_ptr+0x45f,uint8_t(0));});
    ui_bad([]{fixture_runtime.memory.put(0x870000,object_ptr+0x28);});
    ui_bad([]{fixture_runtime.memory.put(image+0x172bcb0+0x168,image+0x6cb401);});
}
void action_checks(){
    reset();std::string e;auto r=request();fixture_controller["pending"]=false;
    need(capture(fixture_runtime,object_ptr,r.district_text,e)&&captured&&captures==1&&originals==0,e);
    need(!capture(fixture_runtime,object_ptr,r.district_text,e),"duplicate source capture accepted");
    reset();fixture_controller["pending"]=false;auto noop=r.district_text;noop.text="Old";
    need(capture(fixture_runtime,object_ptr,noop,e)&&!captured&&captures==0,"no-op capture submitted");
    reset();need(!capture(fixture_runtime,object_ptr,r.district_text,e),"pending public admission accepted");
    reset();need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Confirmed,e);action::Evidence authority;
    need(originals==1&&observe(fixture_binding,r,authority,e)&&authority.district_after->name=="Village",e);
    need(retire(fixture_binding,r,e)&&!retire(fixture_binding,r,e),"retirement not exactly once");
    need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&originals==1,"confirmed action replayed");
    reset();Config::instance().is_host=false;
    need(perform(fixture_binding,r,&authority,e)==action::Outcome::Confirmed&&originals==1,e);
    reset();auto empty=r;empty.district_text.text="";need(perform(fixture_binding,empty,nullptr,e)==action::Outcome::Confirmed,e);
    need(operations.at(empty.action_uuid).evidence.district_after->name=="{district_unnamed}","empty native Name lost");
    reset();auto desc=r;desc.district_text.kind=action::DistrictTextKind::Description;desc.district_text.text="";
    need(perform(fixture_binding,desc,nullptr,e)==action::Outcome::Confirmed&&operations.at(desc.action_uuid).evidence.district_after->description.empty(),e);
    reset();fixture_runtime.memory.put(object_ptr+0x350,uint8_t(1));fixture_inside=false;
    need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Confirmed,e);
    need(!operations.at(r.action_uuid).evidence.district_before->manual_title_inside&&operations.at(r.action_uuid).evidence.district_after->flags[11]==0,"original out-of-shape title reset lost");
    reset();fixture_runtime.memory.put(object_ptr+0x350,uint8_t(1));fixture_inside=false;
    need(perform(fixture_binding,desc,nullptr,e)==action::Outcome::Confirmed&&operations.at(desc.action_uuid).evidence.district_after->flags[11]==1,"Description incorrectly moved title");
    auto reject=[&](auto edit){reset();auto v=r;edit(v);need(perform(fixture_binding,v,nullptr,e)==action::Outcome::RejectedNoEffect&&originals==0&&operations.empty(),"invalid pending request mutated");};
    reject([](auto& v){v.snapshot_uuid="33333333-3333-4333-8333-333333333333";});
    reject([](auto&){fixture_controller["action_uuid"]="foreign";});
    reject([](auto&){fixture_controller["pending"]=false;});
    reject([](auto&){Config::instance().is_host=false;});
    reject([](auto&){fixture_world.origin.generation++;});
    reject([](auto&){fixture_runtime.memory.put(object_ptr+0x18,uint64_t(7));});
    reject([](auto&){witness::fixture_undo.active_graph=1;});
    reset();Config::instance().is_host=false;auto foreign=authority;foreign.district_before->geo=1;
    need(perform(fixture_binding,r,&foreign,e)==action::Outcome::RejectedNoEffect&&originals==0,"foreign Client complete state accepted");
    for(int failure=1;failure<=5;++failure){reset();mutation=failure;
        need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&originals==1&&!e.empty(),"native partial failure not Unknown");
        need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&originals==1,"Unknown native action replayed");
    }
    reset();need(perform(fixture_binding,r,nullptr,e)==action::Outcome::Confirmed,e);
    fixture_runtime.memory.put(object_ptr+0x328,uint32_t(1));action::Evidence out;
    need(!observe(fixture_binding,r,out,e),"changed complete property accepted at ACK");
}
#include "district_property_read_checks.inc"
#include "district_edit_execution_checks.inc"
int main()try{reader_checks();action_checks();property_read_checks();district_edit_fixture::run();std::cout<<checks<<" actual District reader/capture/Host/Client/Unknown checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
