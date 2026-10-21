namespace prop_text {
namespace {
struct Text {const char* data=nullptr;size_t size=0;std::array<uint8_t,16> storage{};};
using Setter=void(__fastcall*)(void*,const Text*);
Setter building_name=nullptr,building_description=nullptr;
bool installed=false,attempted=false;
std::string install_cause,cause;
struct Pending {action::Binding binding;std::string snapshot,key;action::PropTextIntent intent;};
struct Operation {
    action::Binding binding;action::Request request;remove_diagnostics::LocalTarget target;
    action::Evidence evidence;witness::UndoWitness undo;
    prop_text_dungeon::Owners dungeon_owners{};
    std::optional<dungeon_gizmo_identity::Member> gizmo;
    std::array<uint64_t,4> gizmo_random{};
    bool attempted=false,returned=false,confirmed=false,retired=false;
    std::string cause;
};
std::optional<Pending> captured;
std::map<std::string,Operation> operations;
uint64_t captures=0,submits=0,rejections=0;
struct UiMemory final:action::ui_capture::Memory {
    Runtime& runtime;explicit UiMemory(Runtime& r):runtime(r){}
    bool copy(uintptr_t p,void* out,size_t n,std::string& e)override{return runtime.memory.read(p,out,n,e);}
    bool region(uintptr_t p,action::ui_capture::Region& out,std::string& e)override{
        out={};MEMORY_BASIC_INFORMATION actual{};
        if(VirtualQuery(reinterpret_cast<const void*>(p),&actual,sizeof actual)!=sizeof actual){
            e="Prop text VirtualQuery failed Win32="+std::to_string(GetLastError());return false;}
        const auto protection=actual.Protect&0xff;
        out={uintptr_t(actual.AllocationBase),actual.State==MEM_COMMIT,
            !(actual.Protect&PAGE_GUARD)&&(protection==PAGE_EXECUTE||protection==PAGE_EXECUTE_READ||
                protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY),actual.Type==MEM_IMAGE,
            protection==PAGE_READWRITE||protection==PAGE_WRITECOPY||protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY};return true;
    }
};
void reject(const std::string& e){++rejections;if(cause!=e){cause=e;LOG_ERROR("[UIPropText] {}",e);}}
bool text(Runtime& r,uintptr_t at,std::string& out,std::string& e){
    Text value;if(!r.memory.read(at,&value,sizeof value,e))return false;
    if(value.size>4096||(value.size&&(!value.data||uintptr_t(value.data)>UINTPTR_MAX-value.size))){e="Prop text native string span invalid";return false;}
    out.resize(value.size);return !value.size||r.memory.read(uintptr_t(value.data),out.data(),value.size,e);
}
bool current_object(Runtime& r,const remove_diagnostics::LocalTarget& target,host::WorldState& w,native::BuildingState& building,std::string& e){
    if(target.normal_building||target.normal_scenery)return current_prop(r,target,w,building,e);
    host::NativeOrigin origin;host::LookupApi api;
    if(!host::resolve_lookup(r.memory.process,r.image,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",api,e))return false;
    host::Reader reader(r.memory.process,origin,r.image,r.thread,api);prop_lease::Prop prop;
    if(!reader.current(w,true,e)||!reader.prop(reinterpret_cast<void*>(target.object),target.uid,prop,e)||
       !scenery_kiss::read_action_prop(target.uid,building,e))return false;
    if(w.origin.world_epoch!=target.world_epoch||w.origin.generation!=target.world_generation||building.prop.uid!=target.uid||
       building.type!=target.type||building.variant!=target.variant||prop.class_addresspoint!=r.image+0x16d93c0||
       building.dungeon_membership||building.dungeon_instance_membership){e="Prop text current root Dungeon Building class/full UID/type/generation differs";return false;}
    return true;
}
bool read(Runtime& r,const remove_diagnostics::LocalTarget& target,host::WorldState& w,
          action::PropTextState& out,action::Ledger& money,witness::UndoWitness& undo,std::string& e,prop_text_dungeon::Owners* local_owners=nullptr){
    native::BuildingState native;
    if(!current_object(r,target,w,native,e)||!ledger(money,e)||!witness::read_undo(r.memory.process,r.image,undo,e))return false;
    if(undo.active_graph){e="Prop text requires idle Undo";return false;}
    out.object=world_state::building_state(native);out.scenery=target.normal_scenery;
    if(!text(r,target.object+0x38,out.name,e))return false;
    prop_text_dungeon::Owners owners{};
    if(!out.scenery){
        auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& error){
            if(!base||base>UINTPTR_MAX-offset){error="Prop text Dungeon address invalid";return false;}return r.memory.read(base+offset,&value,sizeof value,error);};
        auto string=[&](uintptr_t address,std::string& value,std::string& error){return text(r,address,value,error);};
        if(!prop_text_dungeon::read(at,string,r.image,w.root,target.object,owners,out.dungeon_name,out.dungeon_region_name,e))return false;
        out.dungeon=owners[0]!=0;
        if(!target.normal_building&&!out.dungeon){e="Prop text non-normal Building lacks reciprocal Dungeon owner";return false;}
        if(!text(r,target.object+0x7b0,out.description,e))return false;
    }
    if(local_owners)*local_owners=owners;
    host::WorldState after;if(!world(r,after,e)||!host::same_world(w.origin,after.origin)){if(e.empty())e="Prop text World changed during complete text read";return false;}
    return true;
}
bool capture(Runtime& r,uintptr_t object,action::PropTextIntent intent,std::string& e){
    const auto controller=typed_action_runtime::controller_status();
    if(!installed||!r.ready||GetCurrentThreadId()!=r.thread||captured||r.pending||
       !controller.value("started",false)||controller.value("pending",true)||controller.value("locked",true)){
        e="Prop text capture requires ready idle Mainthread Controller";return false;}
    if(!action::valid_prop_text(intent)){e="Prop text input invalid";return false;}
    const auto snapshot=net::current_snapshot_id();action::Binding binding;
    if(!typed_action_runtime::network_binding(snapshot,binding,e))return false;
    host::WorldState w;if(!world(r,w,e))return false;
    remove_diagnostics::LocalTarget target;
    if(!remove_diagnostics::resolve_local_target(w.selection,object,target,e))return false;
    if(target.snapshot_id!=snapshot||target.world_epoch!=binding.native_epoch||target.world_generation!=binding.native_generation){
        e="Prop text capture current native binding differs";return false;}
    action::PropTextState before;action::Ledger money;witness::UndoWitness undo;
    if(!read(r,target,w,before,money,undo,e))return false;
    if(intent.kind==action::PropTextKind::Description&&before.scenery){e="Scenery has no Building description";return false;}
    UiMemory memory(r);action::ui_diagnostic::Window window;
    action::ui_diagnostic::Context context{r.image,w.selection,object,target.uid,target.world_epoch,target.world_generation,
        "bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",
        before.scenery?action::ui_diagnostic::Family::Scenery:action::ui_diagnostic::Family::Building};
    if(!action::ui_diagnostic::read_live_window(memory,context,window,e))return false;
    if((intent.kind==action::PropTextKind::Name?before.name:before.description)==intent.text)return true;
    captured=Pending{binding,snapshot,target.record_key,std::move(intent)};++captures;return true;
}
void building_name_entry(void* object,const Text* value){
    auto* r=published.load(std::memory_order_acquire);
    if(!net::session_wire::connected()||!r||uintptr_t(_ReturnAddress())!=r->image+0x628dea){building_name(object,value);return;}
    try{action::PropTextIntent intent;std::string e;
        if(!text(*r,uintptr_t(value),intent.text,e)||!capture(*r,uintptr_t(object),std::move(intent),e))reject(e);
    }catch(const std::exception& x){reject(std::string("Building Rename capture: ")+x.what());}
}
void building_description_entry(void* object,const Text* value){
    auto* r=published.load(std::memory_order_acquire);
    if(!net::session_wire::connected()||!r||uintptr_t(_ReturnAddress())!=r->image+0x66de80){building_description(object,value);return;}
    try{action::PropTextIntent intent;intent.kind=action::PropTextKind::Description;std::string e;
        if(!text(*r,uintptr_t(value),intent.text,e)||!capture(*r,uintptr_t(object),std::move(intent),e))reject(e);
    }catch(const std::exception& x){reject(std::string("Building description capture: ")+x.what());}
}
uint32_t invoke(Runtime& r,const action::PropTextIntent& intent,const remove_diagnostics::LocalTarget& target,bool& returned){
    Text value{intent.text.data(),intent.text.size(),{}};
    __try{
        if(intent.kind==action::PropTextKind::Description)building_description(reinterpret_cast<void*>(target.object),&value);
        else if(!target.normal_scenery)building_name(reinterpret_cast<void*>(target.object),&value);
        else reinterpret_cast<Setter>(r.image+0x4cf710)(reinterpret_cast<void*>(target.object),&value);
        returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
#include "gizmo_prop_text_runtime.inc"
}
bool capture_rename(uintptr_t object,const void* native_text,bool& handled,std::string& e){
    handled=false;auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready){e="Prop text UI runtime unavailable";return false;}
    uintptr_t ap=0;if(!r->memory.read(object,&ap,sizeof ap,e))return false;
    if(ap==r->image+0x1749ab0){handled=true;return capture_gizmo(*r,object,native_text,e);}
    if(!scenery_kiss::can_move_rva(r->image,ap))return true;
    host::WorldState w;if(!world(*r,w,e))return false;
    remove_diagnostics::LocalTarget target;
    if(!remove_diagnostics::resolve_local_target(w.selection,object,target,e))return false;
    if(!target.normal_scenery&&!target.normal_building){e="Prop text selected class has no current typed adapter";return false;}
    handled=true;action::PropTextIntent intent;
    return text(*r,uintptr_t(native_text),intent.text,e)&&capture(*r,object,std::move(intent),e);
}
bool enable(std::string& e){
    if(installed)return true;
    if(attempted){e="Prop text previous hook install failed: "+install_cause;return false;}attempted=true;
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->image||r->thread!=GetCurrentThreadId()){e=install_cause="Prop text hook requires bound Mainthread";return false;}
    struct Hook {uintptr_t rva;void* detour;void** original;};
    const Hook hooks[]{
        {0x402bf0,reinterpret_cast<void*>(&building_name_entry),reinterpret_cast<void**>(&building_name)},
        {0x406290,reinterpret_cast<void*>(&building_description_entry),reinterpret_cast<void**>(&building_description)}};
    for(const auto& h:hooks){const auto result=MH_CreateHook(reinterpret_cast<void*>(r->image+h.rva),h.detour,h.original);
        if(result!=MH_OK||!*h.original){e=install_cause="Prop text hook create RVA="+std::to_string(h.rva)+" status="+std::to_string(result)+" "+mt2mp::hooks::framework::process_backend().error();return false;}}
    for(const auto& h:hooks){const auto result=MH_EnableHook(reinterpret_cast<void*>(r->image+h.rva));
        if(result!=MH_OK){e=install_cause="Prop text hook enable RVA="+std::to_string(h.rva)+" status="+std::to_string(result)+" "+mt2mp::hooks::framework::process_backend().error();return false;}}
    installed=true;return true;
}
bool pending(){return bool(captured);}
void tick(){
    if(!captured)return;auto value=std::move(*captured);captured.reset();std::string e;action::Binding binding;
    if(!typed_action_runtime::network_binding(value.snapshot,binding,e)||binding!=value.binding){reject(e.empty()?"Prop text pending native origin changed":e);return;}
    const auto id=uuid(e);if(id.empty()||!typed_action_runtime::submit_prop_text(id,value.key,value.intent,e)){reject(e);return;}++submits;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    if(!request.prop_text.gizmo_scene.empty())return perform_gizmo(binding,request,authority,e);
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||!installed||r->thread!=GetCurrentThreadId()||request.action!=action::Action::EditPropText||!action::valid_prop_text(request.prop_text)){
        e="Prop text runtime/request unavailable";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){e="Prop text action already attempted; no replay";return action::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||
       controller.value("action_uuid",std::string{})!=request.action_uuid||bool(authority)==Config::instance().is_host){
        e="Prop text exact pending Controller/role differs";return action::Outcome::RejectedNoEffect;}
    remove_diagnostics::LocalTarget target;
    if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.canonical_record_key,target,e))return action::Outcome::RejectedNoEffect;
    if(target.world_epoch!=binding.native_epoch||target.world_generation!=binding.native_generation){e="Prop text pending native generation differs";return action::Outcome::RejectedNoEffect;}
    host::WorldState w;action::PropTextState before;action::Ledger money;witness::UndoWitness undo;prop_text_dungeon::Owners owners{};
    if(!read(*r,target,w,before,money,undo,e,&owners))return action::Outcome::RejectedNoEffect;
    if((request.prop_text.kind==action::PropTextKind::Name?before.name:before.description)==request.prop_text.text||
       (before.scenery&&request.prop_text.kind==action::PropTextKind::Description)){
        e="Prop text target already has requested value or has no description";return action::Outcome::RejectedNoEffect;}
    if(authority&&(!action::valid_prop_text_effect(request,*authority)||authority->prop_text_before!=before||authority->before!=money)){
        e="Prop text Client before complete object/text/Ledger differs from Host";return action::Outcome::RejectedNoEffect;}
    auto& op=operations.emplace(request.action_uuid,Operation{binding,request,target}).first->second;
    op.evidence.before=money;op.evidence.prop_text_before=before;op.undo=undo;op.dungeon_owners=owners;
    const auto unknown=[&](const std::string& why){e=op.cause=why;return action::Outcome::Unknown;};
    op.attempted=true;const auto seh=invoke(*r,request.prop_text,target,op.returned);
    if(seh||!op.returned)return unknown("Prop text original setter failed SEH="+std::to_string(seh));
    action::PropTextState after;action::Ledger after_money;witness::UndoWitness after_undo;host::WorldState after_world;
    prop_text_dungeon::Owners after_owners{};
    if(!read(*r,target,after_world,after,after_money,after_undo,e,&after_owners))return unknown(e);
    if(!host::same_world(w.origin,after_world.origin)||undo!=after_undo||owners!=after_owners)return unknown("Prop text original World/Undo/Dungeon owners changed");
    op.evidence.after=after_money;op.evidence.prop_text_after=after;
    if(!action::valid_prop_text_effect(request,op.evidence)||
       (authority&&(authority->prop_text_after!=after||authority->after!=after_money)))return unknown("Prop text original full after state differs from requested/Host result");
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    const auto found=operations.find(request.action_uuid);auto* r=published.load(std::memory_order_acquire);
    if(!r||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed){e="Prop text observation lacks exact confirmed operation";return false;}
    const auto& op=found->second;if(op.gizmo)return observe_gizmo(op,out,e);host::WorldState w;action::PropTextState current;action::Ledger money;witness::UndoWitness undo;
    prop_text_dungeon::Owners owners{};
    if(!read(*r,op.target,w,current,money,undo,e,&owners))return false;
    if(current!=op.evidence.prop_text_after||money!=op.evidence.after||undo!=op.undo||owners!=op.dungeon_owners){e="Prop text current object/UID/World/Undo/Ledger/Dungeon differs from confirmed result";return false;}
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence current;if(!observe(binding,request,current,e))return false;
    auto& op=operations.at(request.action_uuid);if(op.retired){e="Prop text operation already retired";return false;}op.retired=true;return true;
}
json status(){
    json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"attempted",op.attempted},{"returned",op.returned},
        {"confirmed",op.confirmed},{"retired",op.retired},{"local_uid",op.gizmo?op.gizmo->uid:op.target.uid},{"key",op.request.canonical_record_key},{"cause",op.cause}};
    return {{"enabled",installed},{"pending",pending()},{"captures",captures},{"submitted",submits},{"rejected",rejections},{"cause",cause},{"operations",std::move(rows)}};
}
}
