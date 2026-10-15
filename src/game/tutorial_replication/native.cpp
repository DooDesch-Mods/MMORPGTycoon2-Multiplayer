#include "developer_staff/action_native.h"
#include "native.h"
#include "lifecycle.h"
#include "owned_scenario_start_entry.h"
#include "owned_advance_entry.h"
#include "owned_sequence_entry.h"
#include "owned_staff_display_entry.h"
#include "mt2mp/core/hash.h"
#include "owned_commit_entry.h"
#include "start_capture.h"
#include "staff_apply.h"
#include "owned_staff_entry.h"
#include "start_apply.h"
#include "start_window.h"
#include "start_presentation.h"
#include "owned_pay_entry.h"
#include <intrin.h>
#include "message_progress.h"
#include "message_progress_contract.h"
#include "startingpoint_native_code.h"
#include "startingpoint_creation_native.h"
#include "live_host_state/native_port.h"
#include "tutorial_development.h"
#include <algorithm>
#include <cmath>
#include "tutorial_observer.h"
#include "native_open_runtime.h"
#include "session_provider.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/game/native_engine.h"
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include "typed_action_runtime.h"
#include <array>
#include <cstring>
#include <set>
#include <stdexcept>

namespace mt2mp::game::tutorial_replication {
namespace {
using World=building_mixed_identity::opened_file::CurrentWorld;
using Call=void(*)(void*);using Update=void(*)(void*,float);
Call original_postload=nullptr,original_command=nullptr,original_save=nullptr,original_tech=nullptr;
Update original_update=nullptr,original_display=nullptr;
Call original_commit=nullptr,original_advance=nullptr;
void(*original_scenario_start)(void*,void*,void*)=nullptr;
bool(*original_pay)(void*)=nullptr;
void(*original_allocate)(void*,int32_t)=nullptr;
placement_ui_probe::ProcessMemory memory;
uintptr_t image=0;uint32_t mainthread=0;
tutorial_sync::OwnedSequenceEntry owned,owned_commit,owned_pay,owned_staff,owned_display,owned_scenario_start,owned_advance;
void allocate_current(void*,int32_t);
bool pay_current(void*);
bool intent_uuid(std::string&,std::string&);
struct CommitReceipt {World origin;uintptr_t tech=0,record=0;std::optional<startingpoint::CommitBugs> bugs;std::string cause;};
CommitReceipt commit_receipt;
bool client_commit_suppressed=false;
std::string client_commit_action,client_commit_cause;
bool client_commit_submitted=false;
bool submit_client_commit(void* lounge,std::string&);
void commit_current(void* lounge);
bool same_world(const World&,const World&);
struct Load {std::string uuid;uint64_t generation=0;bool armed=false,ready=false;std::set<uintptr_t> sequences;bool waiting_postload=false;std::set<uintptr_t> quarantined;} load;
std::string fault;
thread_local uintptr_t scoped_sequence=0;
thread_local bool restoring=false;
thread_local uintptr_t scoped_startingpoint_step=0,scoped_version_step=0;
namespace lifecycle_native {void remember_terminal(uintptr_t);}
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
void block(const std::string& cause){if(fault.empty()){fault=cause;LOG_ERROR("[Tutorial] native operation blocked cause={}",cause);}}
template<class T>bool at(uintptr_t p,size_t offset,T& value,std::string& e){if(!p||p>UINTPTR_MAX-offset)return fail(e,"tutorial native address overflow");return memory.copy(p+offset,&value,sizeof(value),e);}
DWORD store_bytes(uintptr_t p,const void* value,size_t count){__try{std::memcpy(reinterpret_cast<void*>(p),value,count);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
template<class T>bool put(uintptr_t p,size_t offset,const T& value,std::string& e){T readable{};if(!at(p,offset,readable,e))return false;const auto code=store_bytes(p+offset,&value,sizeof(value));return !code||fail(e,"tutorial native store exception="+std::to_string(code));}
bool str(uintptr_t p,size_t offset,std::string& value,std::string& e){uintptr_t data=0;uint64_t size=0;if(!at(p,offset,data,e)||!at(p,offset+8,size,e))return false;if(size>200)return fail(e,"tutorial string exceeds200");std::array<char,201> bytes{};if(!memory.copy(data,bytes.data(),size_t(size)+1,e))return false;if(bytes[size]||std::memchr(bytes.data(),0,size_t(size)))return fail(e,"tutorial native string terminator differs");value.assign(bytes.data(),size_t(size));return true;}
bool supported(const std::string& id){return id=="initial_tutorial"||id=="leveltwo_tutorial";}
bool type(uintptr_t p,uintptr_t rva,std::string& e){uintptr_t actual=0;return at(p,0,actual,e)&&(actual==image+rva||fail(e,"tutorial native vtable differs"));}
bool step_member(uintptr_t sequence,uintptr_t step,std::string& e){uintptr_t data=0;int32_t count=0;if(!at(sequence,0x20,data,e)||!at(sequence,0x28,count,e))return false;if(count<1||count>2048)return fail(e,"tutorial step count outside1..2048");size_t found=0;for(int32_t i=0;i<count;++i){uintptr_t entry=0;if(!at(data,size_t(i)*8,entry,e))return false;if(entry==step)++found;}return found==1||fail(e,"tutorial scoped step is not uniquely owned");}
struct Scope {
    uintptr_t previous,previous_tech,previous_version;bool was_restoring;
    Scope(uintptr_t sequence,bool restore,uintptr_t tech_step=0,uintptr_t version_step=0):previous(scoped_sequence),previous_tech(scoped_startingpoint_step),previous_version(scoped_version_step),was_restoring(restoring){scoped_sequence=sequence;restoring=restore;scoped_startingpoint_step=tech_step;scoped_version_step=version_step;}
    ~Scope(){scoped_sequence=previous;restoring=was_restoring;scoped_startingpoint_step=previous_tech;scoped_version_step=previous_version;}
};
bool skip(void* step,bool save,bool tech){
    if(!scoped_sequence)return false;
    std::string e;const auto p=reinterpret_cast<uintptr_t>(step);
    if(!step_member(scoped_sequence,p,e)){block(e);return true;}
    bool suppress=save||(tech&&restoring);
    if(tech&&!restoring){
        if(p!=scoped_startingpoint_step){block("unadmitted scoped tutorial CompleteTech");return true;}
        suppress=true;
    }
    if(!save&&!tech){std::string command;if(!str(p,0x40,command,e)){block(e);return true;}const auto policy=(p==scoped_version_step&&command=="MessageTo SoftwareStatus TutorialLevelUp")?Start::Skip:command_policy(command,restoring);suppress=policy!=Start::Native;if(policy==Start::Reject)block("unsupported scoped tutorial command: "+command);}
    if(suppress&&!put(p,0x18,uint8_t(1),e))block(e);
    return suppress;
}
void command_start(void* step){if(!skip(step,false,false))original_command(step);}
void save_start(void* step){if(!skip(step,true,false))original_save(step);}
void tech_start(void* step){if(!skip(step,false,true))original_tech(step);}
void sequence_update(void* sequence,float delta){
    const auto p=reinterpret_cast<uintptr_t>(sequence);
    if(load.armed&&(load.sequences.contains(p)||load.quarantined.contains(p))){
        if(GetCurrentThreadId()!=mainthread)block("Client tutorial Update thread differs");
        // The old sequence stays fenced until explicit snapshot/session retirement.
        // Native Instance Update still owns completion/history bookkeeping.
        return;
    }
    original_update(sequence,delta);
    lifecycle_native::remember_terminal(p);
}
void postload(void* instance){
    if(!load.armed){original_postload(instance);return;}
    std::string e,id;uintptr_t sequence=0;const auto p=reinterpret_cast<uintptr_t>(instance);
    if(!str(p,0x18,id,e)){block(e);return;}
    if(!supported(id)){original_postload(instance);return;}
    if(GetCurrentThreadId()!=mainthread||!type(p,0x173ce70,e)||!at(p,0x58,sequence,e)||!type(sequence,0x16dbd80,e)){block(e.empty()?"Client tutorial PostLoad thread/sequence differs":e);return;}
    load.sequences.insert(sequence);Scope scope(sequence,true);original_postload(instance);
}
struct NativeCursor {uintptr_t instance=0,sequence=0,steps=0;Cursor value;};
bool origin(const action_session::Context& c,const action_session::Binding& b,World& w,std::string& e){
    action_session::Context actual;if(!action_session::selected_session().current(actual,e)||actual!=c)return fail(e,e.empty()?"tutorial actual common Context differs":e);
    if(!native_open_runtime::read_current_world(w,e))return false;
    if(GetCurrentThreadId()!=mainthread||w.world_epoch!=b.native_epoch||w.generation!=b.native_generation)return fail(e,"tutorial local Mainthread/native binding differs");
    if(c.role==action_session::Role::Client&&(!load.armed||!load.ready||load.uuid!=w.uuid||load.generation!=w.generation))return fail(e,"tutorial Client joined-load scope differs from local origin");
    if(!fault.empty())return fail(e,fault);return true;
}
#include "lifecycle_owner.inc"
#include "lifecycle_code.inc"
#include "staff_display_owner.inc"
#include "staff_display_values.inc"
#include "staff_display_ui.inc"
#include "staff_display_hook.inc"
bool native_state(const action_session::Context& c,const action_session::Binding& b,State& out,std::vector<NativeCursor>& cursors,std::string& e){
    World w;if(!origin(c,b,w,e))return false;nlohmann::json observed;if(!tutorial_observer::read(observed,e))return false;
    out={};cursors.clear();out.initial_complete=observed.at("initial_tutorial_complete").get<bool>();out.leveltwo_complete=observed.at("leveltwo_tutorial_complete").get<bool>();
    uintptr_t log=0,active=0;int32_t count=0;if(!at(reinterpret_cast<uintptr_t>(w.state),0x1e8,log,e)||!at(log,0x20,active,e)||!at(log,0x28,count,e))return false;
    if(count<0||count>64)return fail(e,"tutorial scenario count outside0..64");
    for(int32_t i=0;i<count;++i){NativeCursor n;std::string id;uintptr_t history=0;int32_t history_count=0;uint8_t complete=0;
        if(!at(active,size_t(i)*8,n.instance,e)||!str(n.instance,0x18,id,e))return false;if(!supported(id))continue;
        n.value.scenario=id;if(!at(n.instance,0x58,n.sequence,e)||!at(n.sequence,0x20,n.steps,e)||!at(n.sequence,0x28,n.value.count,e)||!at(n.sequence,0x50,n.value.next,e)||!at(n.sequence,0x54,complete,e)||!str(n.instance,0x60,n.value.last,e)||!at(n.instance,0x48,history,e)||!at(n.instance,0x50,history_count,e))return false;
        n.value.complete=complete!=0;if(history_count<0||history_count>512)return fail(e,"tutorial history count outside0..512");
        for(int32_t j=0;j<history_count;++j){std::string entry;if(!str(history,size_t(j)*32,entry,e))return false;n.value.history.push_back(std::move(entry));}
        if(c.role==action_session::Role::Client&&load.quarantined.contains(n.sequence))return fail(e,"Client tutorial constructor remains unconfirmed");
        if(c.role==action_session::Role::Client&&!load.sequences.contains(n.sequence))return fail(e,"Client tutorial was not fenced during joined PostLoad");
        message_progress::Binding meter;if(!message_progress::read(memory,image,n.sequence,n.value.next,meter,e))return false;
        n.value.message_progress=meter.values;
        out.active.push_back(n.value);cursors.push_back(std::move(n));
    }
    for(const auto& cursor:out.active)if(cursor.scenario=="initial_tutorial"){
        tutorial_development::StartingpointSnapshot snapshot;nlohmann::json diagnostic;
        if(!tutorial_development::read_startingpoint(snapshot,diagnostic,e,true))return false;
        if(snapshot.origin.game!=w.game||snapshot.origin.mode!=w.mode||snapshot.origin.state!=w.state||snapshot.origin.world!=w.world||snapshot.origin.map!=w.map||snapshot.origin.uuid!=w.uuid||snapshot.origin.sha256!=w.sha256||snapshot.origin.bytes!=w.bytes||snapshot.origin.world_epoch!=w.world_epoch||snapshot.origin.generation!=w.generation)
            return fail(e,"tutorial development full Origin differs during capture");
        out.startingpoint=snapshot.row;
        if(!display_native::capture(c,b,snapshot,out.staff_display,e))return false;
        if(out.startingpoint&&out.startingpoint->deployed&&commit_receipt.bugs&&same_world(w,commit_receipt.origin)&&snapshot.tech==commit_receipt.tech&&snapshot.record==commit_receipt.record)
            out.startingpoint->commit_bugs=commit_receipt.bugs;
    }
    World after;if(!origin(c,b,after,e)||after.game!=w.game||after.mode!=w.mode||after.state!=w.state||after.world!=w.world||after.map!=w.map||after.uuid!=w.uuid||after.sha256!=w.sha256||after.bytes!=w.bytes||after.world_epoch!=w.world_epoch||after.generation!=w.generation)return fail(e,e.empty()?"tutorial origin changed during capture":e);
    if(!lifecycle_native::decorate(w,c,out,e))return false;
    return valid(out,e);
}
#include "lifecycle_hook.inc"
bool interval(const NativeCursor& n,const Cursor& target,const std::optional<startingpoint::Row>& technology,std::string& e){
    if(n.value.count!=target.count||n.value.next>target.next||(n.value.complete&&!target.complete))return fail(e,"tutorial backward/count/completion transition differs");
    std::set<std::string> ids;
    for(int32_t i=0;i<target.count;++i){uintptr_t step=0;std::string id;if(!at(n.steps,size_t(i)*8,step,e)||!str(step,0x20,id,e))return false;if(!id.empty())ids.insert(id);
        if(i<n.value.next||i>=target.next)continue;uintptr_t vt=0;if(!at(step,0,vt,e))return false;
        if(vt==image+0x17206c0){std::string key;if(!str(step,0x40,key,e))return false;
            if(!startingpoint::accepts_complete_tech(n.value.scenario,i,key,technology))return fail(e,"tutorial CompleteTech index="+std::to_string(i)+" requires supported authoritative Startingpoint state");
            continue;
        }
        if(vt==image+0x16fd930){std::string command;if(!str(step,0x40,command,e))return false;if(command_policy(command,false)==Start::Reject&&!(n.value.scenario=="initial_tutorial"&&i==100&&command=="MessageTo SoftwareStatus TutorialLevelUp"))return fail(e,"tutorial command requires unsupported authoritative state: "+command);}
        else if(vt!=image+0x16fda00&&vt!=image+0x17091e0&&vt!=image+0x1725e70&&vt!=image+0x171c5a0&&vt!=image+0x16dc000&&vt!=image+0x16dbf30)return fail(e,"tutorial forward step type unsupported");
    }
    for(const auto& id:target.history)if(!ids.contains(id))return fail(e,"Host tutorial history ID absent from local sequence");
    if(!target.last.empty()&&!ids.contains(target.last))return fail(e,"Host tutorial last ID absent from local sequence");return true;
}
struct BorrowedString {const char* data;uint64_t size;std::array<unsigned char,16> storage{};};
static_assert(sizeof(BorrowedString)==32);
DWORD progress_label_call(uintptr_t meter){
    constexpr char key[]="{tutorial_meter_progress}";const BorrowedString label{key,sizeof(key)-1,{}};
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x840f20)(reinterpret_cast<void*>(meter),&label);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD progress_value_call(uintptr_t meter,const MessageProgress& values){
    __try{reinterpret_cast<void(*)(void*,float,float,float)>(image+0x842ff0)(reinterpret_cast<void*>(meter),values.value,values.minimum,values.maximum);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct ProgressCalls {
    bool label(uintptr_t meter,std::string& e){if(!message_progress::verify_code(memory,image,e))return false;const auto code=progress_label_call(meter);return !code||fail(e,"tutorial progress native SetLabel exception="+std::to_string(code));}
    bool shown(uintptr_t meter,bool visible,std::string& e){return put(meter,0x69,uint8_t(visible),e);}
    bool value(uintptr_t meter,const MessageProgress& values,std::string& e){if(!message_progress::verify_code(memory,image,e))return false;const auto code=progress_value_call(meter,values);return !code||fail(e,"tutorial progress native SetValue exception="+std::to_string(code));}
};
DWORD advance_call(uintptr_t sequence){__try{reinterpret_cast<Call>(image+0x4fceb0)(reinterpret_cast<void*>(sequence));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD reserve_call(uintptr_t instance,int32_t count){__try{reinterpret_cast<void(*)(void*,int32_t)>(image+0xc4f300)(reinterpret_cast<void*>(instance+0x40),count);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD assign_call(uintptr_t destination,const BorrowedString* source){__try{reinterpret_cast<void(*)(void*,const void*)>(image+0x11bfce0)(reinterpret_cast<void*>(destination),source);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
void assign(uintptr_t destination,const std::string& value){const BorrowedString source{value.c_str(),value.size(),{}};const auto code=assign_call(destination,&source);if(code)throw std::runtime_error("tutorial string assign exception="+std::to_string(code));}
bool history(const NativeCursor& n,const Cursor& target,std::string& e){
    const auto code=reserve_call(n.instance,int32_t(target.history.size()));if(code)return fail(e,"tutorial history Reserve exception="+std::to_string(code));
    uintptr_t data=0;int32_t capacity=0;if(!at(n.instance,0x48,data,e)||!at(n.instance,0x54,capacity,e))return false;
    if(capacity<int32_t(target.history.size())||capacity>512)return fail(e,"tutorial native Reserve capacity readback differs");
    for(size_t i=0;i<target.history.size();++i)assign(data+i*32,target.history[i]);
    assign(n.instance+0x60,target.last);return put(n.instance,0x50,int32_t(target.history.size()),e);
}
#include "startingpoint_native.inc"
#include "start_action_native.inc"
#include "start_capture_native.inc"
#include "staff_native.inc"
#include "lifecycle_apply.inc"
}
bool initialize(uintptr_t base,std::string& e){
    if(image)return fail(e,"tutorial hooks already initialized");image=base;
    std::array<unsigned char,16> entry{};if(!memory.copy(image+0x4fc4a0,entry.data(),entry.size(),e)||entry!=tutorial_sync::sequence_entry)return fail(e,e.empty()?"tutorial original Sequence entry differs":e);
    struct Entry {uintptr_t rva;std::array<unsigned char,12> bytes;};
    const Entry entries[]={
        {0x4f5900,{0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x48,0x48,0x8b,0x71,0x58}},
        {0x4fd410,{0x56,0x53,0x48,0x81,0xec,0xa8,0,0,0,0x48,0x8d,0x74}},
        {0x506010,{0x53,0x48,0x83,0xec,0x20,0x48,0x89,0xcb,0xe8,0xe3,0xf5,0x61}},
        {0x4fd870,{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53}},
        {0x4fceb0,{0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xa0,0,0}},
        {0xc4f300,{0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x20,0x48,0x89}},
        {0x11bfce0,{0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x30,0x48,0x89}}};
    for(const auto& expected:entries){std::array<unsigned char,12> actual{};if(!memory.copy(image+expected.rva,actual.data(),actual.size(),e))return false;if(actual!=expected.bytes)return fail(e,"tutorial native entry guard differs RVA="+std::to_string(expected.rva));}
    if(!message_progress::verify_code(memory,image,e))return false;
    std::array<unsigned char,16> commit_bytes{};
    if(!memory.copy(image+0x25fb10,commit_bytes.data(),commit_bytes.size(),e))return false;
    if(commit_bytes!=tutorial_sync::commit_entry)return fail(e,"tutorial original Commit entry differs");
    if(!start_intent::verify_pay(nullptr,image+start_intent::pay_rva,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e)||
       !start_intent::verify_ui(image,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e,developer_staff::action_runtime::verify_entry))return false;
    std::array<unsigned char,16> staff_entry{};
    if(!memory.copy(image+staff::allocate_rva,staff_entry.data(),staff_entry.size(),e)||staff_entry!=staff::allocate_entry)
        return fail(e,e.empty()?"Startingpoint original AllocateTechWorkers entry differs":e);
    if(!display_native::verify_code(e)||!lifecycle_native::verify_code(e))return false;
    owned_scenario_start={image+0x4f3b00,reinterpret_cast<uintptr_t>(&lifecycle_native::scenario_start),0,true,false};
    owned_advance={image+0x4fceb0,reinterpret_cast<uintptr_t>(&lifecycle_native::advance),0,true,false};
    owned_display={image+0x6c6660,reinterpret_cast<uintptr_t>(&display_update),0,true,false};
    owned_staff={image+staff::allocate_rva,reinterpret_cast<uintptr_t>(&allocate_current),0,true,false};
    owned_pay.target=image+start_intent::pay_rva;owned_pay.detour=reinterpret_cast<uintptr_t>(&pay_current);owned_pay.original_verified=true;
    owned_commit.target=image+0x25fb10;owned_commit.detour=reinterpret_cast<uintptr_t>(&commit_current);owned_commit.original_verified=true;
    owned.target=image+0x4fc4a0;owned.detour=reinterpret_cast<uintptr_t>(&sequence_update);owned.original_verified=true;
    struct Hook {uintptr_t rva;void* replacement;void** original;};
    const Hook hooks_to_create[]={
        {0x4f3b00,reinterpret_cast<void*>(&lifecycle_native::scenario_start),reinterpret_cast<void**>(&original_scenario_start)},
        {0x4fceb0,reinterpret_cast<void*>(&lifecycle_native::advance),reinterpret_cast<void**>(&original_advance)},
        {0x6c6660,reinterpret_cast<void*>(&display_update),reinterpret_cast<void**>(&original_display)},
        {staff::allocate_rva,reinterpret_cast<void*>(&allocate_current),reinterpret_cast<void**>(&original_allocate)},
        {start_intent::pay_rva,reinterpret_cast<void*>(&pay_current),reinterpret_cast<void**>(&original_pay)},
        {0x25fb10,reinterpret_cast<void*>(&commit_current),reinterpret_cast<void**>(&original_commit)},
        {0x4fc4a0,reinterpret_cast<void*>(&sequence_update),reinterpret_cast<void**>(&original_update)},
        {0x4f5900,reinterpret_cast<void*>(&postload),reinterpret_cast<void**>(&original_postload)},
        {0x4fd410,reinterpret_cast<void*>(&command_start),reinterpret_cast<void**>(&original_command)},
        {0x506010,reinterpret_cast<void*>(&save_start),reinterpret_cast<void**>(&original_save)},
        {0x4fd870,reinterpret_cast<void*>(&tech_start),reinterpret_cast<void**>(&original_tech)}};
    for(const auto& h:hooks_to_create)if(!hooks::create_hook(reinterpret_cast<void*>(image+h.rva),h.replacement,h.original))return fail(e,"tutorial hook create failed RVA="+std::to_string(h.rva));
    owned_scenario_start.trampoline=reinterpret_cast<uintptr_t>(original_scenario_start);
    owned_advance.trampoline=reinterpret_cast<uintptr_t>(original_advance);
    owned_display.trampoline=reinterpret_cast<uintptr_t>(original_display);
    owned_staff.trampoline=reinterpret_cast<uintptr_t>(original_allocate);
    owned_pay.trampoline=reinterpret_cast<uintptr_t>(original_pay);
    owned.trampoline=reinterpret_cast<uintptr_t>(original_update);owned_commit.trampoline=reinterpret_cast<uintptr_t>(original_commit);return true;
}
bool hooks_enabled(std::string& e){
    owned.enabled=owned_commit.enabled=owned_pay.enabled=owned_staff.enabled=owned_display.enabled=owned_scenario_start.enabled=owned_advance.enabled=true;
    if(!tutorial_sync::verify_owned_scenario_start_entry(owned_scenario_start,owned_scenario_start.target,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e)||!tutorial_sync::verify_owned_advance_entry(owned_advance,owned_advance.target,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e)||!staff_display::verify_owned_display_entry(owned_display,owned_display.target,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e)||!start_intent::verify_pay(&owned_pay,owned_pay.target,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e)||!verify_sequence_entry(owned.target,e)||!tutorial_sync::verify_owned_commit_entry(owned_commit,owned_commit.target,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e)||!staff::verify_owned_allocate(owned_staff,owned_staff.target,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e)){
        owned.enabled=owned_commit.enabled=owned_pay.enabled=owned_staff.enabled=owned_display.enabled=owned_scenario_start.enabled=owned_advance.enabled=false;return false;
    }return true;
}
bool verify_pay_entry(uintptr_t address,std::string& e){
    uintptr_t base=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(base,thread,sha,e))return false;
    if(base!=image||thread!=GetCurrentThreadId())return fail(e,"tutorial Pay verifier requires actual Mainthread/image");
    return start_intent::verify_pay(owned_pay.enabled?&owned_pay:nullptr,address,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e);
}
bool verify_commit_entry(uintptr_t address,std::string& e){
    uintptr_t base=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(base,thread,sha,e))return false;
    if(base!=image||thread!=GetCurrentThreadId())return fail(e,"tutorial Commit verifier requires actual Mainthread/image");
    return tutorial_sync::verify_owned_commit_entry(owned_commit,address,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e);
}
bool verify_sequence_entry(uintptr_t address,std::string& e){return tutorial_sync::verify_owned_sequence_entry(owned,address,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e);}
bool arm_client_load(const std::string& uuid,uint64_t generation,std::string& e){
    if(!owned.enabled||!generation||uuid.empty())return fail(e,"tutorial Client load requires enabled hooks and exact origin");
    display_native::clear();lifecycle_native::clear();pending_start_presentation.reset();client_start_receipt={};client_start_presentation_cause.clear();client_staff_receipt={};
    commit_receipt={};client_commit_suppressed=false;client_commit_action.clear();client_commit_cause.clear();client_commit_submitted=false;load={uuid,generation,true,false,{}};mainthread=GetCurrentThreadId();fault.clear();return true;
}
bool finish_client_load(bool& pending,std::string& e){
    pending=false;
    if(!load.armed||!fault.empty())return fail(e,fault.empty()?"tutorial Client load not armed":fault);
    if(GetCurrentThreadId()!=mainthread)return fail(e,"tutorial Client load completion thread differs");
    World before;if(!native_open_runtime::read_current_world(before,e))return false;
    if(before.uuid!=load.uuid||before.generation!=load.generation)return fail(e,"tutorial loaded local origin differs");
    nlohmann::json observed;if(!tutorial_observer::read(observed,e))return false;
    uintptr_t log=0,active=0;int32_t count=0;uint8_t postloaded=0;
    if(!at(reinterpret_cast<uintptr_t>(before.state),0x1e8,log,e)||!at(log,0x60,postloaded,e))return false;
    if(postloaded>1)return fail(e,"tutorial ScenarioLog PostLoad byte invalid");
    if(postloaded){
        if(!at(log,0x20,active,e)||!at(log,0x28,count,e))return false;
        if(count<0||count>64)return fail(e,"tutorial scenario count outside0..64 during load completion");
        for(int32_t i=0;i<count;++i){uintptr_t instance=0,sequence=0;std::string id;
            if(!at(active,size_t(i)*8,instance,e)||!str(instance,0x18,id,e))return false;
            if(!supported(id))continue;
            if(!type(instance,0x173ce70,e)||!at(instance,0x58,sequence,e)||!type(sequence,0x16dbd80,e))return false;
            if(!load.sequences.contains(sequence))return fail(e,"Client tutorial was not fenced during joined PostLoad after ScenarioLog completion: "+id);
        }
    }
    World after;if(!native_open_runtime::read_current_world(after,e))return false;
    if(before.game!=after.game||before.mode!=after.mode||before.state!=after.state||before.world!=after.world||before.map!=after.map||
       before.uuid!=after.uuid||before.generation!=after.generation||before.world_epoch!=after.world_epoch||before.sha256!=after.sha256||before.bytes!=after.bytes)
        return fail(e,"tutorial origin changed during load completion");
    uint8_t after_postloaded=0;uintptr_t after_log=0;
    if(!at(reinterpret_cast<uintptr_t>(after.state),0x1e8,after_log,e)||after_log!=log)return fail(e,e.empty()?"tutorial ScenarioLog owner changed during load completion":e);
    if(!at(log,0x60,after_postloaded,e))return false;
    if(after_postloaded!=postloaded)return fail(e,"tutorial ScenarioLog PostLoad byte changed during load completion");
    if(!postloaded){
        load.ready=false;pending=true;
        if(!load.waiting_postload){load.waiting_postload=true;LOG_INFO("[Tutorial] Client load pending native ScenarioLog PostLoad");}
        e.clear();return true;
    }
    if(load.waiting_postload)LOG_INFO("[Tutorial] Client load recovered native ScenarioLog PostLoad; tutorial fences confirmed");
    load.waiting_postload=false;load.ready=true;e.clear();return true;
}
void invalidate(const std::string& cause){if(load.ready)LOG_INFO("[Tutorial] Client admission invalidated; native fence retained cause={}",cause);load.ready=false;display_native::clear();lifecycle_native::clear();}
void retire(const std::string& cause){if(load.armed)LOG_INFO("[Tutorial] Client scope retired cause={}",cause);load={};fault.clear();display_native::clear();lifecycle_native::clear();pending_start_presentation.reset();client_start_receipt={};client_start_presentation_cause.clear();client_staff_receipt={};commit_receipt={};client_commit_suppressed=false;client_commit_action.clear();client_commit_cause.clear();client_commit_submitted=false;}
bool capture(const action_session::Context& c,const action_session::Binding& b,State& out,std::string& e){if(c.role!=action_session::Role::Host)return fail(e,"tutorial capture requires Host");if(!mainthread)mainthread=GetCurrentThreadId();std::vector<NativeCursor> cursors;return native_state(c,b,out,cursors,e);}
bool read_progress(nlohmann::json& out,std::string& e){
    out=nullptr;
    try{
        action_session::Context context;if(!action_session::selected_session().current(context,e))return false;
        World before;if(!native_open_runtime::read_current_world(before,e))return false;
        const action_session::Binding binding{before.world_epoch,before.generation};
        State state;std::vector<NativeCursor> cursors;if(!native_state(context,binding,state,cursors,e))return false;
        World after;if(!native_open_runtime::read_current_world(after,e))return false;
        if(before.game!=after.game||before.mode!=after.mode||before.state!=after.state||before.world!=after.world||before.map!=after.map||
           before.uuid!=after.uuid||before.generation!=after.generation||before.world_epoch!=after.world_epoch||before.sha256!=after.sha256||before.bytes!=after.bytes)
            return fail(e,"tutorial progress observation local origin changed");
        out={{"schema",1},{"staff_display",display_native::status()},{"mainthread_confirmed",true},{"lifetime_confirmed",true},
             {"snapshot_uuid",before.uuid},{"world_epoch",before.world_epoch},{"world_generation",before.generation},
             {"client_sequence_fenced",context.role==action_session::Role::Client},{"tutorial",encode(state)},
             {"client_staff_suppressed",client_staff_receipt.suppressed},{"client_staff_submitted",client_staff_receipt.submitted},
             {"client_staff_action_uuid",client_staff_receipt.action_uuid},{"client_staff_cause",client_staff_receipt.cause},{"client_staff_desired_workers",client_staff_receipt.desired},
             {"client_start_suppressed",client_start_receipt.suppressed},
             {"client_start_submitted",client_start_receipt.submitted},{"client_start_action_uuid",client_start_receipt.action_uuid},
             {"client_start_cause",client_start_receipt.cause},
             {"client_start_presentation_cause",client_start_presentation_cause},
             {"client_commit_suppressed",client_commit_suppressed},
             {"client_commit_cause",client_commit_cause},{"client_commit_action_uuid",client_commit_action},{"client_commit_submitted",client_commit_submitted},
             {"host_commit_receipt_cause",commit_receipt.cause}};
        e.clear();return true;
    }catch(const std::exception& cause){out=nullptr;return fail(e,std::string("tutorial progress read: ")+cause.what());}
}
bool preflight(const action_session::Context& c,const action_session::Binding& b,const State& target,std::string& e){
    if(!message_progress::verify_code(memory,image,e))return false;
    if(c.role!=action_session::Role::Client||!valid(target,e))return fail(e,e.empty()?"tutorial apply requires Client":e);
    State local;std::vector<NativeCursor> native;if(!native_state(c,b,local,native,e))return false;
    lifecycle::Plan plan;std::optional<startingpoint::Row> local_technology;
    if(!lifecycle_native::prepare(c,b,local,target,plan,local_technology,e))return false;
    return display_native::admit(c,b,target.staff_display,e,false);
}
bool apply(const action_session::Context& c,const action_session::Binding& b,const State& target,bool& attempted,bool& unknown,std::string& e){
    attempted=unknown=false;if(!preflight(c,b,target,e))return false;
    State before;std::vector<NativeCursor> native;if(!native_state(c,b,before,native,e))return false;if(before==target)return true;
    try{
        World world;if(!origin(c,b,world,e))throw std::runtime_error(e);
        lifecycle::Plan plan;std::optional<startingpoint::Row> local_technology;
        if(!lifecycle_native::prepare(c,b,before,target,plan,local_technology,e))throw std::runtime_error(e);
        const auto target_technology=lifecycle::technology(target);StartingpointCalls calls{c,b,{}};
        if(!startingpoint::apply(local_technology,target_technology,calls,attempted,unknown,e))throw std::runtime_error(e);
        if(local_technology&&target_technology&&!local_technology->deployed&&target_technology->deployed)
            commit_receipt={calls.before.origin,calls.before.tech,calls.before.record,target_technology->commit_bugs,{}};
        for(const auto& operation:plan.operations){
            bool success=false;
            if(operation.kind==lifecycle::Kind::Create)success=lifecycle_native::create(c,b,world,operation.index,target,attempted,e);
            else if(operation.kind==lifecycle::Kind::Retire)success=lifecycle_native::complete(c,b,world,operation.index,target,attempted,e);
            else success=lifecycle_native::forward(c,b,world,operation.index,operation.next,target,false,attempted,e);
            if(!success)throw std::runtime_error(e);
        }
        if(!lifecycle_native::bind_cache(world,c,e))throw std::runtime_error(e);
        // Metadata is adopted only after native starts/removals and all owner
        // readbacks. It never supplies a world completion flag or version value.
        auto& proof=lifecycle_native::cache->proof;
        proof.terminal=target.terminal;proof.terminal_startingpoint=target.terminal_startingpoint;
        proof.terminal_version=target.terminal_version;proof.lifecycle_order=target.lifecycle_order;proof.leveltwo_initial_next=target.leveltwo_initial_next;
        if(!display_native::admit(c,b,target.staff_display,e,true))throw std::runtime_error(e);
        State observed;std::vector<NativeCursor> rows;
        if(!native_state(c,b,observed,rows,e)||observed!=target)throw std::runtime_error(e.empty()?"tutorial full cursor/history/lifecycle readback differs":e);
        return true;
    }catch(const std::exception& x){unknown=attempted;e=std::string("tutorial native Apply failed: ")+x.what();block(e);return false;}
}
#include "startingpoint_commit_intent.inc"
}

namespace mt2mp::game::tutorial_replication {
bool set_startingpoint_workers(const action_session::Context& c,const action_session::Binding& b,
    const startingpoint::Row& expected,int32_t desired,bool& attempted,std::string& e){
    StaffCalls calls{c,b};return staff::apply(expected,desired,calls,attempted,e);
}
bool start_startingpoint(const action_session::Context& c,const action_session::Binding& b,
    const action_session::Ledger& before,const startingpoint::Row& row,
    action_session::Ledger& after,startingpoint::Row& result,bool& attempted,std::string& e){
    bool unknown=false;return run_start_action(c,b,before,row,nullptr,nullptr,after,result,attempted,unknown,e);
}
bool apply_startingpoint_start(const action_session::Context& c,const action_session::Binding& b,const std::string& action,
    const action_session::Ledger& before,const action_session::Ledger& after,
    const startingpoint::Row& row,const startingpoint::Row& result,bool& attempted,bool& unknown,std::string& e){
    action_session::Ledger measured;startingpoint::Row actual;
    if(!run_start_action(c,b,before,row,&after,&result,measured,actual,attempted,unknown,e))return false;
    bool presentation_attempted=false;std::string cause;
    if(!finish_start_presentation(action,c,b,presentation_attempted,cause)){
        client_start_presentation_cause=cause;
        if(presentation_attempted){unknown=true;e=cause;return false;}
        LOG_WARN("[Tutorial] Startingpoint StartTech presentation skipped cause={}",cause);
    }else client_start_presentation_cause.clear();
    return true;
}
}

namespace mt2mp::game::tutorial_replication {
bool invalidate_developer_pool(const action_session::Context& c,const action_session::Binding& b,std::string& e){
    World world;if(!origin(c,b,world,e))return false;
    display_native::clear();e.clear();return true;
}
}
