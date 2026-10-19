#include "typed_action/action_session.h"
#include "multiplayer_world/world_state.h"
#include "region_history_key.h"
#include "building_history_slice/stack_transition.h"
#include <atomic>
#include <iostream>
#include <map>
#include <stdexcept>
namespace action=mt2mp::action_session;namespace world_state=mt2mp::world_state;
using namespace mt2mp::game::building_history_slice;
namespace {
int checks=0,original_calls=0;void need(bool b,const std::string& e){++checks;if(!b)throw std::runtime_error(e);}
const std::string remove_id="11111111-1111-4111-8111-111111111111",fixture_object_key=std::string(64,'a')+":world:building:44",snap="22222222-2222-4222-8222-222222222222";
uint32_t GetCurrentThreadId(){return 7;}
using Original=bool(*)(void*);
namespace witness=mt2mp::build_actions::private_native;
witness::UndoWitness native_stack;witness::GraphWitness native_graph;bool fail_read=false,original_ok=true;uint64_t next_graph=300;
namespace host {struct ProcessMemory {bool copy(uintptr_t address,void* out,size_t length,std::string& e){if(fail_read){e="original manager ReadMemory failed";return false;}if(address==native_stack.receiver+0x3c&&length==1)*static_cast<uint8_t*>(out)=0;else if(address==native_stack.receiver+0x38&&length==4)*static_cast<int32_t*>(out)=0;else {e="unexpected controlled read";return false;}return true;}};}
namespace port {
bool read_undo(host::ProcessMemory&,uintptr_t,witness::UndoWitness& out,std::string& e){if(fail_read){e="original native stack read failed";return false;}out=native_stack;return true;}
bool read_graph(host::ProcessMemory&,uintptr_t,uintptr_t graph,witness::GraphWitness& out,std::string&){out=native_graph;out.graph=graph;return true;}
}
// Qualified witness calls bind only to the controlled read ports.
namespace controlled_witness {using UndoWitness=witness::UndoWitness;using GraphWitness=witness::GraphWitness;using port::read_undo;using port::read_graph;}
struct Graphs {uintptr_t undo=0,redo=0;std::string object_key;};
struct Runtime {uintptr_t image=0x1000;uint32_t thread=7;bool ready=true;Original undo=nullptr,redo=nullptr;std::string snapshot;std::map<std::string,Graphs> buildings;uint64_t ui_queued=0;std::string last_action_uuid,last_target_uuid,last_direction,last_error;};
std::atomic<Runtime*> published{nullptr};
namespace net {std::string current_snapshot_id(){return snap;}}
std::string new_uuid(std::string&){return "33333333-3333-4333-8333-333333333333";}
std::string submitted_key,submitted_target;
namespace typed_action_runtime {bool submit_building_history(action::Action,const std::string&,const std::string& target,const std::string& key,std::string&){submitted_key=key;submitted_target=target;return true;}}
bool undo_native(void*){++original_calls;if(!original_ok)return false;native_stack.undo.pop_back();native_stack.redo.push_back(++next_graph);return true;}
bool redo_native(void*){++original_calls;if(!original_ok)return false;native_stack.redo.pop_back();native_stack.undo.push_back(++next_graph);return true;}
bool call_original(Original original,void* receiver,uint32_t& exception){exception=0;return original(receiver);}
#define witness controlled_witness
#include "graph_history.inc"
#undef witness
void reset(Runtime& runtime){runtime={};runtime.undo=undo_native;runtime.redo=redo_native;runtime.snapshot=snap;published=&runtime;native_stack={};native_stack.receiver=0x2000;native_stack.undo={100};native_graph={};native_graph.locked=true;fail_read=false;original_ok=true;original_calls=0;submitted_key.clear();submitted_target.clear();}
}
void path_graph_case(){
    Runtime runtime;std::string e;reset(runtime);
    need(remember_path(remove_id,100,e),e);
    need(submit_ui(reinterpret_cast<void*>(native_stack.receiver),action::Action::UndoBuilding,e)&&submitted_key=="path:"+remove_id,e);
    need(perform(action::Action::UndoBuilding,remove_id,e)==action::Outcome::Confirmed,e);
    uintptr_t graph=0;need(graph_for(action::Action::RedoBuilding,remove_id,graph,e)&&graph==next_graph,e);
    need(submit_ui(reinterpret_cast<void*>(native_stack.receiver),action::Action::RedoBuilding,e)&&submitted_key=="path:"+remove_id,e);
    need(perform(action::Action::RedoBuilding,remove_id,e)==action::Outcome::Confirmed&&original_calls==2,e);
    published=nullptr;
}
void region_graph_case(){
    Runtime runtime;std::string e;reset(runtime);
    need(remember_region(remove_id,56,100,e),e);
    need(submit_ui(reinterpret_cast<void*>(native_stack.receiver),action::Action::UndoBuilding,e)&&submitted_key=="region:56",e);
    need(perform(action::Action::UndoBuilding,remove_id,e)==action::Outcome::Confirmed,e);
    uintptr_t graph=0;need(graph_for(action::Action::RedoBuilding,remove_id,graph,e)&&graph==next_graph,e);
    need(submit_ui(reinterpret_cast<void*>(native_stack.receiver),action::Action::RedoBuilding,e)&&submitted_key=="region:56",e);
    need(perform(action::Action::RedoBuilding,remove_id,e)==action::Outcome::Confirmed&&original_calls==2,e);
    reset(runtime);need(!remember_region(remove_id,-1,100,e),"negative Region history ID accepted");
    need(!remember_graph(remove_id,"region:056",100,e),"noncanonical Region history key accepted");
    published=nullptr;
}
void selection_graph_case(){
    Runtime runtime;std::string e;reset(runtime);
    need(remember_selection(remove_id,100,e),e);
    need(submit_ui(reinterpret_cast<void*>(native_stack.receiver),action::Action::UndoBuilding,e)&&submitted_key=="selection:"+remove_id,e);
    uintptr_t graph=0;need(graph_for(action::Action::UndoBuilding,remove_id,graph,e)&&graph==100,e);
    need(perform(action::Action::UndoBuilding,remove_id,e)==action::Outcome::Confirmed,e);
    need(graph_for(action::Action::RedoBuilding,remove_id,graph,e)&&graph==next_graph,e);
    need(!graph_for(action::Action::UndoBuilding,remove_id,graph,e)&&graph==0,"stale consumed graph exposed");
    need(submit_ui(reinterpret_cast<void*>(native_stack.receiver),action::Action::RedoBuilding,e)&&submitted_key=="selection:"+remove_id,e);
    need(perform(action::Action::RedoBuilding,remove_id,e)==action::Outcome::Confirmed&&original_calls==2,e);
    reset(runtime);need(!remember_graph(remove_id,"selection:"+snap,100,e),"mismatched group source bound");
    published=nullptr;
}
int main(){try{path_graph_case();region_graph_case();selection_graph_case();Runtime runtime;std::string e;reset(runtime);
need(remember_remove(remove_id,fixture_object_key,100,e),"exact removed graph not retained: "+e);
need(submit_ui(reinterpret_cast<void*>(native_stack.receiver),action::Action::UndoBuilding,e)&&submitted_target==remove_id&&submitted_key==fixture_object_key,"UI did not preserve mutation/key distinction");
need(perform(action::Action::UndoBuilding,remove_id,e)==action::Outcome::Confirmed&&original_calls==1&&runtime.buildings.at(remove_id).object_key==fixture_object_key,"Undo graph transition changed key: "+e);
need(submit_ui(reinterpret_cast<void*>(native_stack.receiver),action::Action::RedoBuilding,e)&&submitted_key==fixture_object_key,"Redo UI lost original key");
need(perform(action::Action::RedoBuilding,remove_id,e)==action::Outcome::Confirmed&&original_calls==2&&runtime.buildings.at(remove_id).object_key==fixture_object_key,"Redo graph transition changed key: "+e);
need(perform(action::Action::RedoBuilding,remove_id,e)==action::Outcome::RejectedNoEffect&&original_calls==2,"same inverse original replayed");
need(!remember_remove(remove_id,fixture_object_key,native_stack.undo.back(),e)&&e.find("duplicate")!=std::string::npos,"duplicate graph binding replaced source");
reset(runtime);need(!remember_remove(remove_id,"bad",100,e)&&runtime.buildings.empty(),"malformed key admitted");
reset(runtime);native_graph.locked=false;need(!remember_remove(remove_id,fixture_object_key,100,e)&&runtime.buildings.empty(),"uncommitted graph admitted");
reset(runtime);native_stack.undo={99};need(!remember_remove(remove_id,fixture_object_key,100,e),"non top Remove graph admitted");
reset(runtime);fail_read=true;need(!remember_remove(remove_id,fixture_object_key,100,e)&&e=="original native stack read failed","original graph cause overwritten");
reset(runtime);need(remember_remove(remove_id,fixture_object_key,100,e),e);original_ok=false;need(perform(action::Action::UndoBuilding,remove_id,e)==action::Outcome::Unknown&&original_calls==1,"failed original acknowledged as no effect");
reset(runtime);need(remember_place(remove_id,100,e)&&runtime.buildings.at(remove_id).object_key=="action:"+remove_id,"old placement registration changed");
std::cout<<"PASS "<<checks<<" actual graph registration/UI/transition assertions; original calls are synthetic\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
