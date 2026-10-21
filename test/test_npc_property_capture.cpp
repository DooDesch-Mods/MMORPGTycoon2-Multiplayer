#include "typed_action/action_session.h"
#include "ui_remove/ui_capture.h"
#include "npc_census.h"
#include "npc_target_key.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <Windows.h>
#define GetCurrentThreadId fixture_thread_id
using json=nlohmann::json;
namespace action=mt2mp::action_session;
namespace ui=action::ui_capture;
namespace npc_census=mt2mp::npc_census;
namespace npc_target_key=mt2mp::game::npc_target_key;
struct Memory {
    std::map<uintptr_t,std::vector<uint8_t>> blocks;
    template<class T>void set(uintptr_t address,const T& value){auto& b=blocks[address];b.resize(sizeof value);std::memcpy(b.data(),&value,sizeof value);}
};
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
template<class T>bool field(Memory& m,uintptr_t base,size_t offset,T& value,std::string& error){
    if(base==0x1000&&offset==0x18){if constexpr(sizeof(T)==8){uint64_t uid=77;std::memcpy(&value,&uid,8);return true;}}
    auto found=m.blocks.find(base+offset);if(found==m.blocks.end()||found->second.size()!=sizeof value)return fail(error,"unreadable field");
    std::memcpy(&value,found->second.data(),sizeof value);return true;
}
struct PropertyPending {action::NpcPropertyIntent intent;action::Binding binding;std::string fixture_snapshot,key;};
std::optional<PropertyPending> property_pending;
struct Runtime {bool ready=true,pending=false,capture_failed=false;uint32_t thread=5;uintptr_t image=0x140000000;uint64_t captures=0;} fixture_npc_state;
uint32_t GetCurrentThreadId(){return 5;}
const std::string fixture_target="33333333-3333-4333-8333-333333333333";
const std::string fixture_snapshot="22222222-2222-4222-8222-222222222222";
namespace net {std::string current_snapshot_id(){return fixture_snapshot;}}
json fixture_controller={{"started",true},{"locked",false},{"pending",false}};
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool network_binding(const std::string& id,action::Binding& binding,std::string& error){
    if(id!=fixture_snapshot)return fail(error,"fixture_snapshot differs");binding.native_epoch=2;binding.native_generation=3;return true;}
}
bool local_target_key(uintptr_t object,uint64_t uid,std::string& key,std::string& error){
    if(object!=0x1000||uid!=77)return fail(error,"local fixture_target differs");key=fixture_target;return true;}
uintptr_t actual_object=0x1000;uint64_t actual_uid=77;bool window_visible=true,undo_active=false;
bool target_binding(const std::string&,const action::Binding& b,const std::string& id,ui::LocalBinding& local,
                    uintptr_t& region,npc_census::State&,std::string&){
    local={0x5000,actual_object,actual_uid,b.native_epoch,b.native_generation,id,"action:"+fixture_target,false,false,true};region=0x2000;return true;}
namespace action_ui_fixture {struct Window{};}
namespace mt2mp::action_session::ui_diagnostic {using Window=action_ui_fixture::Window;}
bool live_npc_window(Memory&,uintptr_t,const ui::LocalBinding&,action::ui_diagnostic::Window&,json&,std::string& error){
    return window_visible||fail(error,"window hidden");}
namespace undo {struct UndoWitness{uintptr_t active_graph=0;};}
action::NpcPropertyState old_state;
bool property_read(Memory&,uintptr_t,uint64_t,uintptr_t,action::NpcPropertyState& out,action::Ledger&,
                   undo::UndoWitness& witness,npc_census::State&,json&,std::string&){out=old_state;witness.active_graph=undo_active;return true;}
// The extracted production bodies refer to their module runtime by this name.
#define state fixture_npc_state
#include "npc_property_fields_actual.inc"
#include "npc_property_capture_actual.inc"
#include "npc_property_level_request_actual.inc"
#include "npc_property_census_actual.inc"
struct PropertyString {const char* data=nullptr;size_t size=0;std::array<uint8_t,16> storage{};};
using PropertyRename=void(__fastcall*)(void*,const PropertyString*);
using PropertyOnline=bool(__fastcall*)(void*,int32_t);
using PropertyCommand=bool(__fastcall*)(void*,const PropertyString*);
PropertyRename property_rename=nullptr;PropertyOnline property_online=nullptr;PropertyCommand property_command=nullptr;
#include "npc_property_call_actual.inc"
#undef state
unsigned original_calls=0;void* original_object=nullptr;std::string original_command;
bool __fastcall command_fixture(void* object,const PropertyString* command){
    ++original_calls;original_object=object;original_command.assign(command->data,command->size);return true;}
void need(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
void reset(){property_pending.reset();fixture_npc_state={};fixture_controller={{"started",true},{"locked",false},{"pending",false}};
    actual_object=0x1000;actual_uid=77;window_visible=true;undo_active=false;old_state={};old_state.actor.created.name="Old";old_state.online=2;}
int main(){try{unsigned count=0;std::string error;action::NpcPropertyIntent intent;intent.name="New";
    reset();need(property_capture(0x1000,intent,error),error);need(property_pending&&property_pending->key=="action:"+fixture_target&&fixture_npc_state.captures==1,"exact capture missing");++count;
    need(!property_capture(0x1000,intent,error),"second pending capture accepted");++count;
    auto reject=[&](auto mutate){reset();mutate();need(!property_capture(0x1000,intent,error),"capture guard bypassed");need(!property_pending&&!fixture_npc_state.captures,"rejected capture changed fixture_npc_state");++count;};
    reject([]{fixture_controller["pending"]=true;});reject([]{fixture_controller["locked"]=true;});reject([]{fixture_npc_state.thread=6;});
    reject([]{actual_uid=78;});reject([]{actual_object=0x1008;});reject([]{window_visible=false;});
    reject([]{undo_active=true;});reject([]{old_state.actor.created.name="New";});
    reset();intent.kind=action::NpcPropertyKind::OnlineState;intent.name.clear();intent.online=0;
    need(property_capture(0x1000,intent,error),error);++count;
    Memory memory;memory.set(0x1838,int32_t{1});memory.set(0x16c0,uint8_t{1});memory.set(0x16b0,std::array<float,4>{1,0.5f,0.25f,1});
    memory.set(0x1178,uintptr_t{0x2000});memory.set(0x1890,int32_t{5});memory.set(0x2838,int32_t{1});memory.set(0x283c,int32_t{10});
    action::NpcPropertyState value;need(property_fields(memory,0x1000,value,error)&&value.online==1&&value.force_tint,error);++count;
    memory.set(0x16c0,uint8_t{0});need(!property_fields(memory,0x1000,value,error),"online/tint mismatch accepted");++count;
    npc_census::State before;before.manager=1;before.next_local_tag=7;npc_census::Row row;row.local_uid=77;row.name="Old";before.rows={row};
    auto after=before;intent.kind=action::NpcPropertyKind::Name;intent.name="New";after.rows[0].name="New";
    need(property_census(before,after,77,intent,error),error);++count;
    after.rows[0].level=2;need(!property_census(before,after,77,intent,error),"unrelated level changed");++count;
    after=before;after.rows[0].name="New";after.next_local_tag++;need(!property_census(before,after,77,intent,error),"counter changed");++count;
    for(auto kind:{action::NpcPropertyKind::RaiseLevel,action::NpcPropertyKind::LowerLevel}){
        reset();old_state.command_level=5;old_state.region_min=1;old_state.region_max=10;
        const auto caller=fixture_npc_state.image+(kind==action::NpcPropertyKind::RaiseLevel?0x759ceb:0x75a0f8);
        const std::string text=kind==action::NpcPropertyKind::RaiseLevel?"Raise Level":"Lower Level";
        need(property_level_request(caller,text,intent,error)&&intent.kind==kind,error);++count;
        need(property_capture(0x1000,intent,error)&&property_pending->intent==intent,error);++count;
        property_pending.reset();old_state.command_level=kind==action::NpcPropertyKind::RaiseLevel?10:1;
        need(!property_capture(0x1000,intent,error),"level edge captured");++count;
        need(!property_level_request(caller,"Destroy",intent,error),"arbitrary command captured");++count;
        need(!property_level_request(caller+1,text,intent,error),"foreign caller captured");++count;
        property_command=&command_fixture;original_calls=0;bool returned=false;
        need(property_call(intent,0x1000,returned)==0&&returned&&original_calls==1&&original_object==reinterpret_cast<void*>(0x1000)&&original_command==text,"original command ABI/text differs");++count;
        after=before;need(property_census(before,after,77,intent,error),error);++count;
        ++after.rows[0].level;need(!property_census(before,after,77,intent,error),"synthetic actor level change accepted");++count;
    }
    memory.set(0x16c0,uint8_t{1});memory.set(0x1890,int32_t{11});
    need(property_fields(memory,0x1000,value,error)&&value.command_level==11,error);++count;
    memory.set(0x1890,int32_t{-1});
    need(!property_fields(memory,0x1000,value,error)&&error.find("level=-1 min=1 max=10")!=std::string::npos,"negative command level not diagnosed");++count;
    for(auto kind:{action::NpcPropertyKind::RaiseLevel,action::NpcPropertyKind::LowerLevel}){
        for(int32_t prior:{0,12}){
            reset();old_state.command_level=prior;old_state.region_min=1;old_state.region_max=10;
            action::NpcPropertyIntent clamped;clamped.kind=kind;
            need(property_capture(0x1000,clamped,error),error);++count;
            need(action::npc_property_level(old_state,kind)==(prior==0?1:10),"native level clamp after Region edit differs");++count;
        }
    }
    std::cout<<count<<" actual NPC property capture/read/census cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
