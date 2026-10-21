#include "native_bridge.h"
#include "dungeon_held_graph.h"
#include "region_history_key.h"
#include "stack_transition.h"
#include "multiplayer_world/world_state.h"
#include "typed_action_runtime.h"
#include "session_provider.h"
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/core/logger.h"
#include <Windows.h>
#include "mt2mp/hooks/minhook_backend.h"
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <map>

namespace mt2mp::game::building_history_slice {
namespace {
namespace action=action_session;
namespace witness=build_actions::private_native;
namespace host=build_actions::private_native::host;
using Original=bool(__fastcall*)(void*);
struct Graphs {uintptr_t undo=0,redo=0;std::string object_key;};
struct Runtime {
    uintptr_t image=0;
    uint32_t thread=0;
    bool ready=false;
    Original undo=nullptr,redo=nullptr;
    std::string snapshot;
    std::map<std::string,Graphs> buildings;
    uint64_t ui_calls=0,ui_queued=0;
    std::string last_action_uuid,last_target_uuid,last_direction,last_error;
};
std::atomic<Runtime*> published{nullptr};

bool bind_current_snapshot(Runtime& runtime,std::string& error) {
    const auto current=net::current_snapshot_id();
    if(current.empty()){error="building history current snapshot missing";return false;}
    if(runtime.snapshot!=current){
        runtime.buildings.clear();runtime.snapshot=current;
        runtime.last_action_uuid.clear();runtime.last_target_uuid.clear();runtime.last_direction.clear();runtime.last_error.clear();
    }
    error.clear();return true;
}

bool pinned_prologue(host::ProcessMemory& memory,uintptr_t image,uintptr_t rva,
                     const std::array<uint8_t,32>& expected,std::string& error) {
    std::array<uint8_t,32> actual{};
    if(!memory.copy(image+rva,actual.data(),actual.size(),error))return false;
    if(actual!=expected){error="building_history_native_prologue_differs rva="+std::to_string(rva);return false;}
    return true;
}

std::string new_uuid(std::string& error) {
    unsigned char bytes[16];
    const auto status=BCryptGenRandom(nullptr,bytes,sizeof bytes,BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(status!=0){error="building history BCryptGenRandom NTSTATUS="+std::to_string(status);return {};}
    bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;char value[37];
    std::snprintf(value,sizeof value,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
    return value;
}

bool submit_ui(void* receiver,action::Action direction,std::string& error) {
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime||!runtime->ready||runtime->thread!=GetCurrentThreadId()) {
        error="building history UI hook owner/Mainthread differs";return false;
    }
    if(!bind_current_snapshot(*runtime,error))return false;
    host::ProcessMemory memory;witness::UndoWitness state;
    if(!witness::read_undo(memory,runtime->image,state,error))return false;
    if(state.receiver!=uintptr_t(receiver)||state.active_graph) {
        error="building history UI receiver or active graph differs";return false;
    }
    const auto& stack=direction==action::Action::UndoBuilding?state.undo:state.redo;
    if(stack.empty()){error="building history UI stack empty";return false;}
    const auto top=stack.back();std::string target;
    for(const auto& [id,graphs]:runtime->buildings) {
        if((direction==action::Action::UndoBuilding?graphs.undo:graphs.redo)!=top)continue;
        if(!target.empty()){error="building history UI target graph ambiguous";return false;}
        target=id;
    }
    if(target.empty()){error="building history UI stack top has no confirmed shared action";return false;}
    const auto id=new_uuid(error);
    if(id.empty())return false;
    if(!typed_action_runtime::submit_building_history(direction,id,target,runtime->buildings.at(target).object_key,error))return false;
    ++runtime->ui_queued;runtime->last_action_uuid=id;runtime->last_target_uuid=target;
    runtime->last_direction=direction==action::Action::UndoBuilding?"undo":"redo";
    runtime->last_error.clear();return true;
}

bool __fastcall undo_entry(void* receiver) {
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime)return false;
    if(!net::session_wire::connected()||!typed_action_runtime::status().value("started",false))
        return runtime->undo?runtime->undo(receiver):false;
    if(!runtime->ready){LOG_ERROR("[BuildingHistory] Undo hook incomplete in connected session");return false;}
    // UI intent only. Host ordering and peer ACK occur before any original call.
    std::string error;
    ++runtime->ui_calls;
    if(!submit_ui(receiver,action::Action::UndoBuilding,error)){
        runtime->last_error=error;LOG_ERROR("[BuildingHistory] Undo UI intent rejected cause={}",error);
    }
    return false;
}
bool __fastcall redo_entry(void* receiver) {
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime)return false;
    if(!net::session_wire::connected()||!typed_action_runtime::status().value("started",false))
        return runtime->redo?runtime->redo(receiver):false;
    if(!runtime->ready){LOG_ERROR("[BuildingHistory] Redo hook incomplete in connected session");return false;}
    std::string error;
    ++runtime->ui_calls;
    if(!submit_ui(receiver,action::Action::RedoBuilding,error)){
        runtime->last_error=error;LOG_ERROR("[BuildingHistory] Redo UI intent rejected cause={}",error);
    }
    return false;
}

bool call_original(Original original,void* receiver,uint32_t& exception) {
    exception=0;
    __try {return original(receiver);}
    __except(EXCEPTION_EXECUTE_HANDLER) {exception=GetExceptionCode();return false;}
}
}

bool enable(uintptr_t image,uint32_t main_thread,std::string& error) {
    if(!image||!main_thread||GetCurrentThreadId()!=main_thread){error="building history hook owner/thread invalid";return false;}
    if(auto* existing=published.load(std::memory_order_acquire)) {
        if(!existing->ready||existing->image!=image||existing->thread!=main_thread){
            error="building history existing hook owner or installation differs";return false;
        }
        uintptr_t actual_image=0;uint32_t actual_thread=0;std::string actual_hash;
        if(!native::read_resolver_receipt(actual_image,actual_thread,actual_hash,error)||
           actual_image!=image||actual_thread!=main_thread||
           actual_hash!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"){
            if(error.empty())error="building history repeated enable owner differs";return false;
        }
        return bind_current_snapshot(*existing,error);
    }
    uintptr_t measured_image=0;uint32_t measured_thread=0;std::string measured;
    if(!native::read_resolver_receipt(measured_image,measured_thread,measured,error)||
       measured_image!=image||measured_thread!=main_thread||
       measured!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6") {
        if(error.empty())error="building history resolver owner differs";return false;
    }
    host::ProcessMemory memory;
    constexpr std::array<uint8_t,32> undo_bytes={0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x68,0x0f,0xb6,0x41,0x3c,0x48,0x89,0xcb,0x84,0xc0,0x75,0x25,0x8b,0x51,0x18,0x85,0xd2};
    constexpr std::array<uint8_t,32> redo_bytes={0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x78,0x0f,0xb6,0x41,0x3c,0x48,0x89,0xcb,0x84,0xc0,0x75,0x25,0x8b,0x49,0x30,0x85,0xc9};
    if(!pinned_prologue(memory,image,0x53aef0,undo_bytes,error)||
       !pinned_prologue(memory,image,0x53ab70,redo_bytes,error))return false;
    auto* runtime=new Runtime;runtime->image=image;runtime->thread=main_thread;
    if(!bind_current_snapshot(*runtime,error)){delete runtime;return false;}
    if(!hooks::create_hook(reinterpret_cast<void*>(image+0x53aef0),reinterpret_cast<void*>(&undo_entry),reinterpret_cast<void**>(&runtime->undo))) {
        error="building history Undo CreateHook failed";delete runtime;return false;
    }
    if(!hooks::create_hook(reinterpret_cast<void*>(image+0x53ab70),reinterpret_cast<void*>(&redo_entry),reinterpret_cast<void**>(&runtime->redo))) {
        MH_RemoveHook(reinterpret_cast<void*>(image+0x53aef0));
        error="building history Redo CreateHook failed";delete runtime;return false;
    }
    published.store(runtime,std::memory_order_release);
    for(const auto rva:{0x53aef0,0x53ab70}) {
        const auto result=MH_EnableHook(reinterpret_cast<void*>(image+rva));
        if(result!=MH_OK){
            MH_DisableHook(reinterpret_cast<void*>(image+0x53aef0));
            MH_DisableHook(reinterpret_cast<void*>(image+0x53ab70));
            error="building history EnableHook failed: "+std::string(MH_StatusToString(result));return false;
        }
    }
    runtime->ready=true;error.clear();return true;
}

bool read_ui(nlohmann::json& out,std::string& error) {
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime||!runtime->ready||runtime->thread!=GetCurrentThreadId()){
        error="building history read requires enabled bound Mainthread";return false;
    }
    if(!bind_current_snapshot(*runtime,error))return false;
    host::ProcessMemory memory;witness::UndoWitness stack;
    if(!witness::read_undo(memory,runtime->image,stack,error))return false;
    const auto target=[&](uintptr_t graph,bool undo){
        std::string found;
        if(!graph)return found;
        for(const auto& [id,entry]:runtime->buildings){
            if((undo?entry.undo:entry.redo)!=graph)continue;
            if(!found.empty())return std::string{};
            found=id;
        }
        return found;
    };
    const auto controller=typed_action_runtime::status();
    out={{"snapshot_uuid",runtime->snapshot},{"undo_depth",stack.undo.size()},
         {"redo_depth",stack.redo.size()},{"active_graph",stack.active_graph!=0},
         {"undo_target_action_uuid",target(stack.undo.empty()?0:stack.undo.back(),true)},
         {"redo_target_action_uuid",target(stack.redo.empty()?0:stack.redo.back(),false)},
         {"ui_hook_calls",runtime->ui_calls},{"ui_intents_queued",runtime->ui_queued},
         {"last_action_uuid",runtime->last_action_uuid},{"last_target_action_uuid",runtime->last_target_uuid},
         {"last_direction",runtime->last_direction},{"last_error",runtime->last_error},
         {"controller",{{"started",controller.value("started",false)},
                        {"locked",controller.value("locked",false)},
                        {"pending",controller.value("pending",false)},
                        {"failed",controller.value("failed",uint64_t{})},
                        {"host_revision",controller.value("host_revision",uint64_t{})},
                        {"action_uuid",controller.value("action_uuid",std::string{})},
                        {"outcome",controller.value("outcome",0)}}}};
    error.clear();return true;
}

bool invoke_ui(const nlohmann::json& args,nlohmann::json& out,std::string& error) {
    out=nlohmann::json::object();
    if(!args.is_object()||args.size()!=4||!args.contains("direction")||
       !args.contains("snapshot_uuid")||!args.contains("expected_host_revision")||
       !args.contains("target_action_uuid")||!args.at("direction").is_string()||
       !args.at("snapshot_uuid").is_string()||!args.at("expected_host_revision").is_number_unsigned()||
       !args.at("target_action_uuid").is_string()){
        error="building history UI requires exact direction, snapshot, revision and target UUID";return false;
    }
    const auto direction=args.at("direction").get<std::string>();
    if(direction!="undo"&&direction!="redo"){
        error="building history UI direction invalid";return false;
    }
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime||!runtime->ready||runtime->thread!=GetCurrentThreadId()||!net::session_wire::connected()){
        error="building history UI hook/session/Mainthread unavailable";return false;
    }
    if(!bind_current_snapshot(*runtime,error))return false;
    action::Context context;
    if(runtime->snapshot!=args.at("snapshot_uuid").get<std::string>()||
       !action::selected_session().current(context,error)||context.snapshot_uuid!=runtime->snapshot){
        if(error.empty())error="building history UI current Session/snapshot differs";return false;
    }
    const auto controller=typed_action_runtime::status();
    if(!controller.value("started",false)||controller.value("pending",false)||controller.value("locked",false)||
       controller.value("lobby_retiring",false)||controller.value("failed",uint64_t{})||
       controller.value("host_revision",uint64_t{})!=args.at("expected_host_revision").get<uint64_t>()){
        error="building history UI Controller not idle, clean or at expected revision";return false;
    }
    host::ProcessMemory memory;witness::UndoWitness before;
    if(!witness::read_undo(memory,runtime->image,before,error))return false;
    const auto& source=direction=="undo"?before.undo:before.redo;
    const auto found=runtime->buildings.find(args.at("target_action_uuid").get<std::string>());
    if(before.active_graph||source.empty()||found==runtime->buildings.end()||
       (direction=="undo"?found->second.undo:found->second.redo)!=source.back()){
        error="building history UI target is not exact current native stack top";return false;
    }
    const auto calls=runtime->ui_calls,queued=runtime->ui_queued;
    uint32_t exception=0;
    const bool native_result=call_original(reinterpret_cast<Original>(runtime->image+(direction=="undo"?0x53aef0:0x53ab70)),
                                           reinterpret_cast<void*>(before.receiver),exception);
    if(exception||native_result||runtime->ui_calls!=calls+1||runtime->ui_queued!=queued+1||
       runtime->last_target_uuid!=args.at("target_action_uuid").get<std::string>()||
       runtime->last_direction!=direction){
        error="building history UI entry did not capture exactly one intent exception="+std::to_string(exception)+
              " cause="+runtime->last_error;
        return false;
    }
    out={{"state","queued"},{"action_uuid",runtime->last_action_uuid},
         {"target_action_uuid",runtime->last_target_uuid},{"direction",direction},
         {"ui_hook_calls",runtime->ui_calls},{"ui_intents_queued",runtime->ui_queued},
         {"controller",typed_action_runtime::status()}};
    error.clear();return true;
}

static bool remember_graph(const std::string& original,const std::string& object_key,uintptr_t graph,std::string& error) {
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime||!runtime->ready||runtime->thread!=GetCurrentThreadId()||original.empty()||(!world_state::valid_object_key(object_key)&&object_key!="modular:"+original&&object_key!="district-edit:"+original&&object_key!="district-stroke:"+original&&object_key!="zone-edit:"+original&&object_key!="group-object:"+original&&object_key!="groups:"+original&&object_key!="duplicate:"+original&&object_key!="scenery-stroke:"+original&&object_key!="selection:"+original&&object_key!="path:"+original&&object_key!="terrain:"+original&&object_key!="dungeon-tiles:"+original&&object_key!="dungeon-input:"+original&&object_key!="npc-history:"+original&&object_key!="monster-history:"+original&&object_key!="quests:"+original&&object_key!="network:"+original&&!mt2mp::game::region_history_key::parse(object_key))||!graph) {
        error="building history mutation graph binding invalid";return false;
    }
    if(!bind_current_snapshot(*runtime,error))return false;
    if(runtime->buildings.contains(original)){error="building history duplicate mutation graph binding";return false;}
    host::ProcessMemory memory;witness::UndoWitness state;witness::GraphWitness checked;
    if(!witness::read_undo(memory,runtime->image,state,error))return false;
    if(state.active_graph||state.undo.empty()||state.undo.back()!=graph) {
        error="building history mutation graph not current Undo top";return false;
    }
    if(!witness::read_graph(memory,runtime->image,graph,checked,error))return false;
    if(!checked.locked||checked.pending_delete){error="building history mutation graph not drained/locked";return false;}
    runtime->buildings.emplace(original,Graphs{graph,0,object_key});error.clear();return true;
}
bool remember_place(const std::string& original,uintptr_t graph,std::string& error) {return remember_graph(original,"action:"+original,graph,error);}
bool remember_remove(const std::string& original,const std::string& key,uintptr_t graph,std::string& error) {return remember_graph(original,key,graph,error);}
bool remember_move(const std::string& original,const std::string& key,uintptr_t graph,std::string& error) {return remember_graph(original,key,graph,error);}

bool remember_region(const std::string& original,int32_t id,uintptr_t graph,std::string& error){
    if(id<0){error="Region history ID is negative";return false;}
    return remember_graph(original,"region:"+std::to_string(id),graph,error);
}
bool remember_terrain(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"terrain:"+original,graph,error);}
bool remember_dungeon_tiles(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"dungeon-tiles:"+original,graph,error);}
bool remember_npc(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"npc-history:"+original,graph,error);}
bool remember_monster(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"monster-history:"+original,graph,error);}
bool remember_dungeon_input(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"dungeon-input:"+original,graph,error);}
bool remember_quest(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"quests:"+original,graph,error);}
bool remember_network(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"network:"+original,graph,error);}
bool remember_path(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"path:"+original,graph,error);}
bool remember_duplicate(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"duplicate:"+original,graph,error);}
bool remember_scenery(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"scenery-stroke:"+original,graph,error);}
bool remember_group(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"groups:"+original,graph,error);}
bool remember_zone_edit(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"zone-edit:"+original,graph,error);}
bool remember_district_stroke(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"district-stroke:"+original,graph,error);}
bool remember_modular(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"modular:"+original,graph,error);}
bool remember_district(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"district-edit:"+original,graph,error);}
bool remember_group_object(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"group-object:"+original,graph,error);}
bool remember_selection(const std::string& original,uintptr_t graph,std::string& error){return remember_graph(original,"selection:"+original,graph,error);}
bool graph_for(action::Action direction,const std::string& original,uintptr_t& graph,std::string& error){
    graph=0;auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime||!runtime->ready||runtime->thread!=GetCurrentThreadId()||
       (direction!=action::Action::UndoBuilding&&direction!=action::Action::RedoBuilding)){
        error="history graph lookup requires bound Mainthread and direction";return false;}
    if(!bind_current_snapshot(*runtime,error))return false;
    const auto found=runtime->buildings.find(original);
    if(found==runtime->buildings.end()){error="history graph source absent";return false;}
    const auto expected=direction==action::Action::UndoBuilding?found->second.undo:found->second.redo;
    host::ProcessMemory memory;witness::UndoWitness undo;witness::GraphWitness checked;
    if(!witness::read_undo(memory,runtime->image,undo,error))return false;
    const auto& stack=direction==action::Action::UndoBuilding?undo.undo:undo.redo;
    if(undo.active_graph||!expected||stack.empty()||stack.back()!=expected){error="history graph source is not exact native stack top";return false;}
    if(!witness::read_graph(memory,runtime->image,expected,checked,error))return false;
    if(!checked.locked||checked.pending_delete){error="history graph source is not drained/locked";return false;}
    graph=expected;error.clear();return true;
}

bool retained_dungeon_node(action::Action direction,const std::string& original,
                          uintptr_t node,bool& present,std::string& error){
    present=false;auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime||!runtime->ready||runtime->thread!=GetCurrentThreadId()||!node||
       (direction!=action::Action::UndoBuilding&&direction!=action::Action::RedoBuilding)){
        error="Held Dungeon graph lookup requires its bound Mainthread and direction";return false;
    }
    if(!bind_current_snapshot(*runtime,error))return false;
    const auto found=runtime->buildings.find(original);
    if(found==runtime->buildings.end()){error="Held Dungeon graph source action absent";return false;}
    const auto graph=direction==action::Action::UndoBuilding?found->second.undo:found->second.redo;
    host::ProcessMemory memory;witness::UndoWitness before,after;witness::GraphWitness checked,final_graph;
    if(!witness::read_undo(memory,runtime->image,before,error))return false;
    if(before.active_graph){error="Held Dungeon graph lookup encountered an active original graph";return false;}
    const auto& stack=direction==action::Action::UndoBuilding?before.undo:before.redo;
    // AddUndo can retire old history. Do not read a node after its owning graph
    // leaves the original stack, even if a later allocation reused its address.
    if(!graph||std::find(stack.begin(),stack.end(),graph)==stack.end()){error.clear();return true;}
    if(std::count(stack.begin(),stack.end(),graph)!=1||
       !witness::read_graph(memory,runtime->image,graph,checked,error)||!checked.locked||checked.pending_delete){
        if(error.empty())error="Held Dungeon graph is not unique, locked and drained";return false;
    }
    auto at=[&](uintptr_t pointer,size_t offset,auto& value,std::string& cause){
        if(!pointer||pointer>UINTPTR_MAX-offset||pointer+offset>UINTPTR_MAX-sizeof(value)){
            cause="Held Dungeon graph field span invalid";return false;
        }
        return memory.copy(pointer+offset,&value,sizeof value,cause);
    };
    if(!dungeon_held_graph::member(at,runtime->image,graph,node,error)||
       !witness::read_graph(memory,runtime->image,graph,final_graph,error)||
       !witness::read_undo(memory,runtime->image,after,error))return false;
    if(before!=after||checked!=final_graph){error="Held Dungeon original graph membership changed during lookup";return false;}
    present=true;error.clear();return true;
}

action::Outcome perform(action::Action direction,const std::string& original,std::string& error) {
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime||!runtime->ready||runtime->thread!=GetCurrentThreadId()) {
        error="building history original requires bound Mainthread";return action::Outcome::RejectedNoEffect;
    }
    if(!bind_current_snapshot(*runtime,error))return action::Outcome::RejectedNoEffect;
    if(direction!=action::Action::UndoBuilding&&direction!=action::Action::RedoBuilding) {
        error="building history direction invalid";return action::Outcome::RejectedNoEffect;
    }
    const auto found=runtime->buildings.find(original);
    if(found==runtime->buildings.end()){error="building history original action graph missing";return action::Outcome::RejectedNoEffect;}
    const auto expected=direction==action::Action::UndoBuilding?found->second.undo:found->second.redo;
    host::ProcessMemory memory;witness::UndoWitness before;
    if(!witness::read_undo(memory,runtime->image,before,error))return action::Outcome::RejectedNoEffect;
    const auto& source=direction==action::Action::UndoBuilding?before.undo:before.redo;
    if(before.active_graph||!expected||source.empty()||source.back()!=expected) {
        error="building history exact local target is not stack top";return action::Outcome::RejectedNoEffect;
    }
    uint8_t disabled=1;int32_t delay=0;
    if(!memory.copy(before.receiver+0x3c,&disabled,1,error)||!memory.copy(before.receiver+0x38,&delay,4,error)||disabled||delay>0) {
        if(error.empty())error="building history native manager disabled or delayed";
        return action::Outcome::RejectedNoEffect;
    }
    const auto original_fn=direction==action::Action::UndoBuilding?runtime->undo:runtime->redo;
    if(!original_fn){error="building history original trampoline missing";return action::Outcome::RejectedNoEffect;}
    uint32_t exception=0;
    const bool native_result=call_original(original_fn,reinterpret_cast<void*>(before.receiver),exception);
    witness::UndoWitness after;witness::GraphWitness inverse;
    const auto& other=direction==action::Action::UndoBuilding?after.redo:after.undo;
    if(exception||!native_result||!witness::read_undo(memory,runtime->image,after,error)||other.empty()||
       !witness::read_graph(memory,runtime->image,other.back(),inverse,error)||
       !transition(direction==action::Action::UndoBuilding?Direction::Undo:Direction::Redo,expected,before,after,inverse,error)) {
        if(error.empty())error="building history native original/stack readback failed exception="+std::to_string(exception);
        return action::Outcome::Unknown;
    }
    if(direction==action::Action::UndoBuilding){found->second.undo=0;found->second.redo=inverse.graph;}
    else {found->second.undo=inverse.graph;found->second.redo=0;}
    error.clear();return action::Outcome::Confirmed;
}
}
