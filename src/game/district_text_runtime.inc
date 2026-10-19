namespace district_text {
namespace {
using Text=prop_text::Text;
using Setter=void(__fastcall*)(void*,const Text*);
Setter set_name=nullptr,set_description=nullptr;
bool installed=false,install_attempted=false;std::string install_cause,cause;
struct Pending {action::Binding binding;std::string snapshot,key;action::DistrictTextIntent intent;};
struct Operation {
    action::Binding binding;action::Request request;district_saved_identity::Current target;
    action::Evidence evidence;witness::UndoWitness undo;
    bool attempted=false,returned=false,confirmed=false,retired=false;std::string cause;
};
std::optional<Pending> captured;std::map<std::string,Operation> operations;
uint64_t captures=0,submitted=0,rejections=0;
void reject(const std::string& error){++rejections;if(cause!=error){cause=error;LOG_ERROR("[DistrictText] {}",error);}}
std::string desired(const action::DistrictTextIntent& intent){return intent.kind==action::DistrictTextKind::Name&&intent.text.empty()?"{district_unnamed}":intent.text;}
uint32_t contains_call(uintptr_t image,uintptr_t shape,uintptr_t point,bool& result){
    __try{result=reinterpret_cast<bool(__fastcall*)(const void*,const void*)>(image+0xf0dc0)(reinterpret_cast<const void*>(shape),reinterpret_cast<const void*>(point));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool sample(Runtime& r,const action::Binding& binding,const district_saved_identity::Current& target,
            action::DistrictState& out,action::Ledger& money,witness::UndoWitness& undo,std::string& error){
    host::WorldState before,after;district_saved_identity::Current actual;
    if(!world(r,before,error)||before.origin.world_epoch!=binding.native_epoch||before.origin.generation!=binding.native_generation)
        return district_saved_identity::fail(error,error.empty()?"District request native world binding differs":error);
    if(!district_saved_identity_runtime::resolve(target.key,actual,error))return false;
    if(actual.saved_region!=target.saved_region||actual.row.pointer!=target.row.pointer||actual.row.uid!=target.row.uid)
        return district_saved_identity::fail(error,"District saved key changed current full UID/owner");
    auto contains=[&](uintptr_t shape,uintptr_t point,bool& result,std::string& why){
        const auto code=contains_call(r.image,shape,point,result);
        if(code){why="District original title containment failed SEH="+std::to_string(code);return false;}return true;};
    if(!district_reader::read(r.memory,r.image,uintptr_t(before.origin.map),actual,contains,out,error)||!ledger(money,error)||
       !witness::read_undo(r.memory.process,r.image,undo,error))return false;
    if(undo.active_graph)return district_saved_identity::fail(error,"District text requires idle native Undo");
    if(!world(r,after,error)||!host::same_world(before.origin,after.origin))
        return district_saved_identity::fail(error,error.empty()?"District world changed during complete read":error);
    return true;
}
bool capture(Runtime& r,uintptr_t object,action::DistrictTextIntent intent,std::string& error){
    const auto controller=typed_action_runtime::controller_status();
    if(!installed||!r.ready||GetCurrentThreadId()!=r.thread||captured||r.pending||!action::valid_district_text(intent)||
       !controller.value("started",false)||controller.value("pending",true)||controller.value("locked",true)){
        error="District text capture requires ready idle Mainthread Controller and valid intent";return false;}
    action::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!typed_action_runtime::network_binding(snapshot,binding,error))return false;
    host::WorldState w;uint64_t uid=0;std::string key;
    if(!world(r,w,error)||!district_saved_identity::at(r.memory,object,0x18,uid,error)||
       !district_saved_identity_runtime::bind_local(object,uid,key,error)||!district_reader::ui(r.memory,r.image,w.selection,object,error))return false;
    district_saved_identity::Current target;if(!district_saved_identity_runtime::resolve(key,target,error))return false;
    action::DistrictState before;action::Ledger money;witness::UndoWitness undo;
    if(!sample(r,binding,target,before,money,undo,error))return false;
    if((intent.kind==action::DistrictTextKind::Name?before.name:before.description)==desired(intent))return true;
    captured=Pending{binding,snapshot,key,std::move(intent)};++captures;return true;
}
void name_entry(void* object,const Text* value){
    auto* r=published.load(std::memory_order_acquire);
    if(!net::session_wire::connected()||!r||uintptr_t(_ReturnAddress())!=r->image+0x6cb6af){set_name(object,value);return;}
    try{action::DistrictTextIntent intent;std::string error;
        if(!prop_text::text(*r,uintptr_t(value),intent.text,error)||!capture(*r,uintptr_t(object),std::move(intent),error))reject(error);
    }catch(const std::exception& x){reject(std::string("District Name capture: ")+x.what());}
}
void description_entry(void* object,const Text* value){
    auto* r=published.load(std::memory_order_acquire);
    if(!net::session_wire::connected()||!r||uintptr_t(_ReturnAddress())!=r->image+0x6cb852){set_description(object,value);return;}
    try{action::DistrictTextIntent intent;intent.kind=action::DistrictTextKind::Description;std::string error;
        if(!prop_text::text(*r,uintptr_t(value),intent.text,error)||!capture(*r,uintptr_t(object),std::move(intent),error))reject(error);
    }catch(const std::exception& x){reject(std::string("District Description capture: ")+x.what());}
}
uint32_t invoke(const action::DistrictTextIntent& intent,uintptr_t object,bool& returned){
    Text value{intent.text.data(),intent.text.size(),{}};
    __try{(intent.kind==action::DistrictTextKind::Name?set_name:set_description)(reinterpret_cast<void*>(object),&value);returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool enable(std::string& error){
    if(installed)return true;
    if(install_attempted){error="District text previous hook install failed: "+install_cause;return false;}install_attempted=true;
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->image||r->thread!=GetCurrentThreadId()){error=install_cause="District text hook requires bound Mainthread";return false;}
    constexpr std::array<uint8_t,16> expected{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x68};
    std::array<uint8_t,16> actual{};
    if(!r->memory.read(r->image+0xf0dc0,actual.data(),actual.size(),error)||actual!=expected){
        install_cause=error=error.empty()?"District original title containment code differs":error;return false;}
    struct Hook{uintptr_t rva;void* entry;void** original;};
    const Hook hooks[]{{0x418360,reinterpret_cast<void*>(&name_entry),reinterpret_cast<void**>(&set_name)},
        {0x4173b0,reinterpret_cast<void*>(&description_entry),reinterpret_cast<void**>(&set_description)}};
    for(const auto& hook:hooks){const auto result=MH_CreateHook(reinterpret_cast<void*>(r->image+hook.rva),hook.entry,hook.original);
        if(result!=MH_OK||!*hook.original){error=install_cause="District text hook create RVA="+std::to_string(hook.rva)+" status="+std::to_string(result)+" "+mt2mp::hooks::framework::process_backend().error();return false;}}
    for(const auto& hook:hooks){const auto result=MH_EnableHook(reinterpret_cast<void*>(r->image+hook.rva));
        if(result!=MH_OK){error=install_cause="District text hook enable RVA="+std::to_string(hook.rva)+" status="+std::to_string(result)+" "+mt2mp::hooks::framework::process_backend().error();return false;}}
    installed=true;return true;
}
bool pending(){return bool(captured);}
void retire_session(){captured.reset();operations.clear();cause.clear();}
void tick(){
    if(!net::session_wire::connected()){if(captured)reject("District pending source cancelled after session retirement");retire_session();return;}
    if(!captured)return;auto value=std::move(*captured);captured.reset();std::string error;action::Binding binding;
    if(!typed_action_runtime::network_binding(value.snapshot,binding,error)||binding!=value.binding){reject(error.empty()?"District pending native origin changed":error);return;}
    const auto id=uuid(error);if(id.empty()||!typed_action_runtime::submit_district_text(id,value.key,value.intent,error)){reject(error);return;}++submitted;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& error){
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||!installed||r->thread!=GetCurrentThreadId()||request.action!=action::Action::EditDistrictText||!action::valid_district_text(request.district_text)){
        error="District text runtime/request unavailable";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="District text already attempted; no replay";return action::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||
       controller.value("action_uuid",std::string{})!=request.action_uuid||bool(authority)==Config::instance().is_host){
        error="District text exact pending Controller/role differs";return action::Outcome::RejectedNoEffect;}
    if(request.snapshot_uuid!=net::current_snapshot_id()){
        error="District text request snapshot differs from current session";return action::Outcome::RejectedNoEffect;}
    district_saved_identity::Current target;
    if(!district_saved_identity_runtime::resolve(request.canonical_record_key,target,error))return action::Outcome::RejectedNoEffect;
    action::DistrictState before;action::Ledger money;witness::UndoWitness undo;
    if(!sample(*r,binding,target,before,money,undo,error))return action::Outcome::RejectedNoEffect;
    if((request.district_text.kind==action::DistrictTextKind::Name?before.name:before.description)==desired(request.district_text)){
        error="District text target already has requested value";return action::Outcome::RejectedNoEffect;}
    if(authority&&(!action::valid_district_effect(request,*authority)||authority->district_before!=before||authority->before!=money)){
        error="District Client complete before state/Ledger differs from Host";return action::Outcome::RejectedNoEffect;}
    if(!action::district_wire_fits(request,before,money,error))return action::Outcome::RejectedNoEffect;
    auto& op=operations.emplace(request.action_uuid,Operation{binding,request,target}).first->second;
    op.evidence.before=money;op.evidence.district_before=before;op.undo=undo;
    auto unknown=[&](const std::string& why){error=op.cause=why;return action::Outcome::Unknown;};
    op.attempted=true;const auto seh=invoke(request.district_text,target.row.pointer,op.returned);
    if(seh||!op.returned)return unknown("District original setter failed SEH="+std::to_string(seh));
    action::DistrictState after;action::Ledger after_money;witness::UndoWitness after_undo;
    if(!sample(*r,binding,target,after,after_money,after_undo,error))return unknown(error);
    if(undo!=after_undo)return unknown("District original text changed native Undo");
    op.evidence.after=after_money;op.evidence.district_after=after;
    if(!action::valid_district_effect(request,op.evidence)||(authority&&(authority->district_after!=after||authority->after!=after_money)))
        return unknown("District original complete after state differs from requested/Host result");
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error){
    auto* r=published.load(std::memory_order_acquire);const auto found=operations.find(request.action_uuid);
    if(!r||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed){error="District observation lacks exact confirmed operation";return false;}
    const auto& op=found->second;action::DistrictState actual;action::Ledger money;witness::UndoWitness undo;
    if(!sample(*r,binding,op.target,actual,money,undo,error))return false;
    if(actual!=op.evidence.district_after||money!=op.evidence.after||undo!=op.undo){error="District current identity/state/Undo/Ledger differs from confirmed result";return false;}
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& error){
    action::Evidence actual;if(!observe(binding,request,actual,error))return false;
    auto& op=operations.at(request.action_uuid);if(op.retired){error="District operation already retired";return false;}op.retired=true;return true;
}
json status(){
    json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"attempted",op.attempted},{"returned",op.returned},
        {"confirmed",op.confirmed},{"retired",op.retired},{"local_uid",op.target.row.uid},{"key",op.target.key},{"cause",op.cause}};
    return {{"enabled",installed},{"pending",pending()},{"captures",captures},{"submitted",submitted},{"rejected",rejections},{"cause",cause},{"operations",std::move(rows)}};
}
}
