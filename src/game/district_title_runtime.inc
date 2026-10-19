namespace district_title {
namespace {
using Setter=void(__fastcall*)(void*,const void*);
Setter set_position=nullptr;
bool installed=false,install_attempted=false;std::string install_cause,cause;
struct Pending {action::Binding binding;std::string snapshot,key;action::DistrictTitleIntent intent;uintptr_t behaviour=0;
    district_saved_identity::Current target;action::DistrictState before;action::Ledger money;witness::UndoWitness undo;};
struct Operation {
    action::Binding binding;action::Request request;district_saved_identity::Current target;
    action::Evidence evidence;witness::UndoWitness undo;
    bool attempted=false,returned=false,confirmed=false,retired=false;uint32_t original_steps=0;std::string cause;
};
std::optional<Pending> captured;std::map<std::string,Operation> operations;
uint64_t captures=0,submitted=0,rejections=0;
void reject(const std::string& e){++rejections;if(cause!=e){cause=e;LOG_ERROR("[DistrictTitle] {}",e);}}
bool inside(Runtime& r,uintptr_t object,const action::DistrictTitleIntent& intent,std::string& e){
    bool contained=false;const auto code=district_text::contains_call(r.image,object+0x220,uintptr_t(intent.position.data()),contained);
    if(code||!contained){e="District title original Contains rejected point SEH="+std::to_string(code)+" inside="+std::to_string(contained);return false;}return true;
}
bool capture(Runtime& r,uintptr_t object,uintptr_t point,std::string& e){
    const auto controller=typed_action_runtime::controller_status();
    if(!installed||!r.ready||GetCurrentThreadId()!=r.thread||captured||r.pending||district_text::pending()||
       !controller.value("started",false)||controller.value("pending",true)||controller.value("locked",true)){
        e="District title capture requires ready idle Mainthread Controller";return false;}
    action::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!typed_action_runtime::network_binding(snapshot,binding,e))return false;
    action::DistrictTitleIntent intent;uintptr_t behaviour=0;uint64_t uid=0;std::string key;
    if(!district_title_reader::capture(r.memory,r.image,object,point,behaviour,intent,e)||!action::valid_district_title(intent)||
       !district_saved_identity::at(r.memory,object,0x18,uid,e)||!district_saved_identity_runtime::bind_local(object,uid,key,e)){
        if(e.empty())e="District title original point/full UID invalid";return false;}
    district_saved_identity::Current target;action::DistrictState before;action::Ledger money;witness::UndoWitness undo;
    if(!district_saved_identity_runtime::resolve(key,target,e)||!district_text::sample(r,binding,target,before,money,undo,e)||!inside(r,object,intent,e))return false;
    captured=Pending{binding,snapshot,key,intent,behaviour,target,before,money,undo};++captures;return true;
}
uint32_t restore_flag_call(uintptr_t address,bool& exchanged){
    __try{exchanged=_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address),1,0)==0;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool restore_source(Runtime& r,const Pending& value,std::string& e){
    action::DistrictState actual;action::Ledger money;witness::UndoWitness undo;
    if(!district_text::sample(r,value.binding,value.target,actual,money,undo,e))return false;
    if(actual==value.before&&money==value.money&&undo==value.undo)return true;
    auto expected=value.before;
    if(!expected.manual_title_inside){expected.flags[11]=0;expected.manual_title_inside=true;}
    if(actual!=expected||money!=value.money||undo!=value.undo){e="District title original cleanup changed complete owner/state/Undo/Ledger";return false;}
    if(value.before.manual_title_inside)return true;
    // Deactivate restores presentation through GetDesiredTitlePosition. Restore
    // its incidental invalid-manual-position flag reset before ordering the edit.
    bool exchanged=false;const auto code=restore_flag_call(value.target.row.pointer+0x350,exchanged);
    if(code||!exchanged){e="District title source flag restore CAS failed SEH="+std::to_string(code)+" exchanged="+std::to_string(exchanged);return false;}
    if(!district_text::sample(r,value.binding,value.target,actual,money,undo,e))return false;
    if(actual!=value.before||money!=value.money||undo!=value.undo){e="District title source flag restore actual readback differs";return false;}
    return true;
}
void position_entry(void* object,const void* point){
    auto* r=published.load(std::memory_order_acquire);
    if(!net::session_wire::connected()||!r||uintptr_t(_ReturnAddress())!=r->image+0x5b040b){set_position(object,point);return;}
    try{std::string e;if(!capture(*r,uintptr_t(object),uintptr_t(point),e))reject(e);}
    catch(const std::exception& x){reject(std::string("District title capture: ")+x.what());}
}
uint32_t invoke(uintptr_t image,uintptr_t object,const action::DistrictTitleIntent& intent,uint32_t& steps){
    __try{set_position(reinterpret_cast<void*>(object),intent.position.data());steps|=1;
        reinterpret_cast<void(__fastcall*)(void*)>(image+0x418470)(reinterpret_cast<void*>(object));steps|=2;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool enable(std::string& e){
    if(installed)return true;if(install_attempted){e="District title prior hook install failed: "+install_cause;return false;}install_attempted=true;
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->image||r->thread!=GetCurrentThreadId()){e=install_cause="District title requires bound Mainthread";return false;}
    constexpr std::array<uint8_t,16> expected{0x56,0x53,0x48,0x83,0xec,0x38,0x48,0x8b,0x99,0x78,0x03,0x00,0x00,0x48,0x85,0xdb};
    std::array<uint8_t,16> actual{};
    if(!r->memory.read(r->image+0x418470,actual.data(),actual.size(),e)||actual!=expected){e=install_cause=e.empty()?"District title original presentation prefix differs":e;return false;}
    const auto target=reinterpret_cast<void*>(r->image+0x4173c0);
    const auto created=MH_CreateHook(target,reinterpret_cast<void*>(&position_entry),reinterpret_cast<void**>(&set_position));
    if(created!=MH_OK||!set_position){e=install_cause="District title hook create failed status="+std::to_string(created)+" "+mt2mp::hooks::framework::process_backend().error();return false;}
    const auto enabled=MH_EnableHook(target);
    if(enabled!=MH_OK){e=install_cause="District title hook enable failed status="+std::to_string(enabled)+" "+mt2mp::hooks::framework::process_backend().error();return false;}
    installed=true;return true;
}
bool pending(){return bool(captured);}
void retire_session(){captured.reset();operations.clear();cause.clear();}
void tick(){
    if(!net::session_wire::connected()){if(captured)reject("District title source cancelled after session retirement");retire_session();return;}
    if(!captured)return;auto value=std::move(*captured);captured.reset();std::string e;action::Binding binding;auto* r=published.load(std::memory_order_acquire);
    if(!r||!district_title_reader::cleaned(r->memory,r->image,value.behaviour,e)||!typed_action_runtime::network_binding(value.snapshot,binding,e)||binding!=value.binding){reject(e.empty()?"District title source binding changed":e);return;}
    if(!restore_source(*r,value,e)){live_host_state::economic_fence().journal_failed(e);reject(e);return;}
    const auto id=uuid(e);if(id.empty()||!typed_action_runtime::submit_district_title(id,value.key,value.intent,e)){reject(e);return;}++submitted;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||!installed||r->thread!=GetCurrentThreadId()||request.action!=action::Action::MoveDistrictTitle||!action::valid_district_title(request.district_title)){
        e="District title runtime/request unavailable";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){e="District title already attempted; no replay";return action::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||
       controller.value("action_uuid",std::string{})!=request.action_uuid||bool(authority)==Config::instance().is_host||request.snapshot_uuid!=net::current_snapshot_id()){
        e="District title exact pending Controller/role/snapshot differs";return action::Outcome::RejectedNoEffect;}
    district_saved_identity::Current target;action::DistrictState before;action::Ledger money;witness::UndoWitness undo;
    if(!district_saved_identity_runtime::resolve(request.canonical_record_key,target,e)||
       !district_text::sample(*r,binding,target,before,money,undo,e)||!inside(*r,target.row.pointer,request.district_title,e))return action::Outcome::RejectedNoEffect;
    if(authority&&(!action::valid_district_title_effect(request,*authority)||authority->district_before!=before||authority->before!=money)){
        e="District title Client complete before state/Ledger differs from Host";return action::Outcome::RejectedNoEffect;}
    if(!action::district_title_wire_fits(request,before,money,e))return action::Outcome::RejectedNoEffect;
    auto& op=operations.emplace(request.action_uuid,Operation{binding,request,target}).first->second;
    op.evidence.before=money;op.evidence.district_before=before;op.undo=undo;
    auto unknown=[&](const std::string& why){e=op.cause=why;return action::Outcome::Unknown;};
    op.attempted=true;const auto code=invoke(r->image,target.row.pointer,request.district_title,op.original_steps);op.returned=op.original_steps==3;
    if(code||!op.returned)return unknown("District title original setter/presentation failed SEH="+std::to_string(code)+" returned_steps="+std::to_string(op.original_steps));
    action::DistrictState after;action::Ledger after_money;witness::UndoWitness after_undo;
    if(!district_text::sample(*r,binding,target,after,after_money,after_undo,e))return unknown(e);
    if(undo!=after_undo)return unknown("District title original changed native Undo");
    op.evidence.after=after_money;op.evidence.district_after=after;
    if(!action::valid_district_title_effect(request,op.evidence)||(authority&&(authority->district_after!=after||authority->after!=after_money)))
        return unknown("District title original complete after state differs from requested/Host result");
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);const auto found=operations.find(request.action_uuid);
    if(!r||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed){e="District title observation lacks exact confirmed operation";return false;}
    const auto& op=found->second;action::DistrictState actual;action::Ledger money;witness::UndoWitness undo;
    if(!district_text::sample(*r,binding,op.target,actual,money,undo,e))return false;
    if(actual!=op.evidence.district_after||money!=op.evidence.after||undo!=op.undo){e="District title current identity/state/Undo/Ledger differs from confirmed result";return false;}
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence actual;if(!observe(binding,request,actual,e))return false;
    auto& op=operations.at(request.action_uuid);if(op.retired){e="District title operation already retired";return false;}op.retired=true;return true;
}
json status(){
    json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"attempted",op.attempted},{"returned",op.returned},{"original_steps",op.original_steps},
        {"confirmed",op.confirmed},{"retired",op.retired},{"local_uid",op.target.row.uid},{"key",op.target.key},{"cause",op.cause}};
    return {{"enabled",installed},{"pending",pending()},{"captures",captures},{"submitted",submitted},{"rejected",rejections},{"cause",cause},{"operations",std::move(rows)}};
}
}
