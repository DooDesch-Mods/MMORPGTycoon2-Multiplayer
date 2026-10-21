namespace build_actions=mt2mp::build_actions;
namespace scenery_receipt_fixture {
inline constexpr uintptr_t graph=0x600,image=0x8000,object=0x1000;
unsigned original_updates=0,graph_reads=0,undo_reads=0,cleanup_returns=0,remembered=0,world_reads=0;
bool graph_locked=true,cleanup_ok=true,history_ok=true,world_stable=true;
std::vector<std::string> errors;
}
namespace host {
struct ProcessMemory:build_actions::private_native::Memory {
    bool copy(uintptr_t address,void* out,size_t size,std::string& error)override{
        if(address!=scenery_receipt_fixture::object+0x18||size!=sizeof(uint64_t)){
            error="fixture unexpected native Scenery UID read";return false;
        }
        const uint64_t uid=500;std::memcpy(out,&uid,sizeof uid);return true;
    }
};
namespace opened {
struct CurrentWorld {void* world=nullptr;uint64_t world_epoch=0,generation=0;};
}
bool same_world(const opened::CurrentWorld& a,const opened::CurrentWorld& b){
    return a.world==b.world&&a.world_epoch==b.world_epoch&&a.generation==b.generation;
}
}
namespace placement_kiss {
#include "construction_scenery_effect.inc"
}
namespace scenery_kiss {
using State=action::BuildingState;
#include "construction_scenery_guards.inc"
struct PrivateScenery {
    unsigned stage=0,exception=0,cleanup_exception=0;
    bool update_attempted=false,update_returned=false,cursor_dtor_attempted=false,cursor_dtor_returned=false,
         options_dtor_attempted=false,options_dtor_returned=false;
};
struct NativeCatalog {
    NativeCatalog(host::ProcessMemory&,uintptr_t selected_image,uint32_t thread){
        fixture::need(selected_image==scenery_receipt_fixture::image&&thread==1,"producer native catalogue binding changed");
    }
    bool resolve(const State& value,int32_t& category,int32_t& variant,std::string& error){
        if(value.type!="cactus"||value.variant!="p_ext_treecactus_01.vmb"){
            error="actual Scenery catalogue fixture resource differs";return false;
        }
        category=7;variant=123;return true;
    }
};
bool create_permission(host::ProcessMemory&,uintptr_t selected_image,uintptr_t shard,const State&,int32_t category,
                       uintptr_t& manager,int32_t& cost,std::string& error){
    if(selected_image!=scenery_receipt_fixture::image||shard!=0x20||category!=7){error="actual Scenery permission owner differs";return false;}
    manager=0x30;cost=10;return true;
}
bool validate_ui_routes(uintptr_t selected_image,const std::array<uintptr_t,6>& originals,
                        const std::array<uintptr_t,6>& entries,std::string& error){
    if(selected_image!=scenery_receipt_fixture::image||originals!=std::array<uintptr_t,6>{1,2,3,4,5,6}||
       std::any_of(entries.begin(),entries.end(),[](uintptr_t entry){return entry==0;})){
        error="actual Scenery owned entry routes differ";return false;
    }
    return true;
}
}
namespace mt2mp::build_actions::private_native {
#include "construction_scenery_undo.inc"
bool read_undo(Memory&,uintptr_t selected_image,UndoWitness& out,std::string& error){
    if(selected_image!=scenery_receipt_fixture::image){error="actual Undo owner differs";return false;}
    out={};out.receiver=0x70;
    if(scenery_receipt_fixture::undo_reads++)out.undo.push_back(scenery_receipt_fixture::graph);
    return true;
}
bool read_graph(Memory&,uintptr_t selected_image,uintptr_t selected_graph,GraphWitness& out,std::string& error){
    ++scenery_receipt_fixture::graph_reads;
    if(selected_image!=scenery_receipt_fixture::image||selected_graph!=scenery_receipt_fixture::graph){error="actual original Undo graph receiver differs";return false;}
    out={};out.graph=selected_graph;out.nodes=out.operations=1;out.locked=scenery_receipt_fixture::graph_locked;return true;
}
}
namespace native {
bool read_simulation_state(SimulationState& out,std::string&){out.cash=scenery_receipt_fixture::original_updates?90:100;return true;}
}
namespace ui_build_runtime {
struct Runtime;
struct SceneCalls {
    Runtime* owner=nullptr;
    bool prepare(const action::BuildingState&,int32_t,int32_t,void*,scenery_kiss::PrivateScenery&,std::string&);
    bool update_once(scenery_kiss::PrivateScenery&,std::string&);
    bool destroy_once(scenery_kiss::PrivateScenery&,std::string&);
};
struct Scope {
    bool scenery=false;int32_t category=0,variant=0,cost=0;void* manager=nullptr;void* building=nullptr;
    std::array<float,3> position{};uintptr_t graph=0;std::string cause;
    uint32_t purchase_attempted=0,purchase_returned=0,add_attempted=0,add_returned=0,undo_attempted=0,undo_returned=0;
};
struct Runtime {
    uintptr_t image=scenery_receipt_fixture::image;uint32_t thread=1;
    std::array<uintptr_t,6> scene_originals{1,2,3,4,5,6};
    Scope* scope=nullptr;bool replaying=false;SceneCalls scene;
    std::map<std::string,json> receipts;
    std::map<std::string,scenery_kiss::PrivateScenery> held_scenery;
};
bool SceneCalls::prepare(const action::BuildingState& value,int32_t category,int32_t variant,void* shard,
                         scenery_kiss::PrivateScenery& owned,std::string& error){
    if(!owner||!owner->scope||!owner->replaying||owned.stage||category!=7||variant!=123||shard!=reinterpret_cast<void*>(uintptr_t(0x20))||value!=fixture::building){
        error="actual original Scenery prepare binding differs";return false;
    }
    owned.stage=16;return true;
}
bool SceneCalls::update_once(scenery_kiss::PrivateScenery& owned,std::string& error){
    if(!owner||!owner->scope||owned.stage!=16||owned.update_attempted){error="actual original Scenery Update replay rejected";return false;}
    ++scenery_receipt_fixture::original_updates;
    owned.update_attempted=owned.update_returned=true;owned.stage=18;
    auto& active=*owner->scope;
    active.building=reinterpret_cast<void*>(scenery_receipt_fixture::object);active.graph=scenery_receipt_fixture::graph;
    active.purchase_attempted=active.purchase_returned=active.add_attempted=active.add_returned=active.undo_attempted=active.undo_returned=1;
    return true;
}
bool SceneCalls::destroy_once(scenery_kiss::PrivateScenery& owned,std::string& error){
    if(owned.stage!=18||!owned.update_returned||owned.cursor_dtor_attempted){error="actual original Scenery cleanup replay rejected";return false;}
    owned.cursor_dtor_attempted=true;
    if(!scenery_receipt_fixture::cleanup_ok){error="actual original Scenery cleanup failure";owned.cleanup_exception=11;return false;}
    owned.cursor_dtor_returned=owned.options_dtor_attempted=owned.options_dtor_returned=true;owned.stage=19;
    ++scenery_receipt_fixture::cleanup_returns;return true;
}
bool read_world(host::opened::CurrentWorld& out,void*& shard,std::string&){
    out.world=reinterpret_cast<void*>(uintptr_t(0x10));out.world_epoch=7;out.generation=9;shard=reinterpret_cast<void*>(uintptr_t(0x20));
    if(scenery_receipt_fixture::world_reads++&&!scenery_receipt_fixture::world_stable)++out.generation;
    return true;
}
void scene_update_entry(){}void scene_purchase_entry(){}void scene_add_entry(){}
void scene_press_entry(){}void erase_press_entry(){}void scene_release_entry(){}
#pragma push_macro("LOG_ERROR")
#undef LOG_ERROR
#define LOG_ERROR(format,action,cause) scenery_receipt_fixture::errors.push_back(cause)
#include "construction_scenery_place.inc"
#pragma pop_macro("LOG_ERROR")
}
namespace scenery_receipt_fixture {
ui_build_runtime::Runtime producer;
void reset(){
    original_updates=graph_reads=undo_reads=cleanup_returns=remembered=world_reads=0;
    graph_locked=cleanup_ok=history_ok=world_stable=true;
    errors.clear();
    producer={};producer.scene.owner=&producer;
}
}
