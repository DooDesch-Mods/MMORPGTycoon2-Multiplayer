namespace service_membership {
namespace {
using Menu=void(__fastcall*)(void*,const void*);using Remove=void(__fastcall*)(void*,void*);
Menu original_menu=nullptr;Remove original_remove=nullptr;
bool installed=false,install_attempted=false;std::string install_cause,cause;
struct Sample {
    host::WorldState world;remove_diagnostics::LocalTarget target;
    action::PropTextState object;action::ServiceMembershipState value;
    service_membership_reader::Local local;action::Ledger money;witness::UndoWitness undo;
    uintptr_t random_owner=0;std::array<uint64_t,4> random{};
};
struct Pending {action::Binding binding;std::string snapshot,key;action::ServiceMembershipIntent intent;};
struct Operation {action::Binding binding;action::Request request;Sample before,after;action::Evidence evidence;
    bool attempted=false,returned=false,confirmed=false,retired=false;std::string cause;};
std::optional<Pending> captured;std::map<std::string,Operation> operations;uint64_t captures=0,submitted=0,rejected=0;
void reject(const std::string& e){++rejected;if(cause!=e){cause=e;LOG_ERROR("[ServiceMembership] {}",e);}}
bool sample(Runtime& r,const action::Binding& binding,const std::string& snapshot,const std::string& key,Sample& out,std::string& e){
    out={};if(snapshot!=net::current_snapshot_id()||!remove_diagnostics::resolve_record_target(snapshot,key,out.target,e)||!out.target.normal_building||
       out.target.world_epoch!=binding.native_epoch||out.target.world_generation!=binding.native_generation){if(e.empty())e="Service membership current Building/key/full UID/native generation differs";return false;}
    if(!prop_text::read(r,out.target,out.world,out.object,out.money,out.undo,e))return false;
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& error){if(!base||base>UINTPTR_MAX-offset){error="Service membership address invalid";return false;}return r.memory.read(base+offset,&value,sizeof value,error);};
    auto text=[&](uintptr_t address,std::string& value,std::string& error){return prop_text::text(r,address,value,error);};
    auto destination=[&](uintptr_t object,std::string& key_out,std::string& error){remove_diagnostics::LocalTarget target;
        if(!remove_diagnostics::resolve_local_target(out.world.selection,object,target,error)||!target.normal_building||target.snapshot_id!=snapshot||target.world_epoch!=binding.native_epoch||target.world_generation!=binding.native_generation){
            if(error.empty())error="Service transit destination lacks current canonical Building/full UID";return false;}key_out=target.record_key;return true;};
    uintptr_t character_owner=0;
    if(!service_membership_reader::sample(at,text,destination,r.image,out.target.object,out.value,out.local,e)||
       !at(uintptr_t(out.world.origin.state),0x168,character_owner,e)||character_owner!=out.local.manager||
       !at(r.image,0x164b250,out.random_owner,e)||!at(out.random_owner,0,out.random,e)){
        if(e.empty())e="Service membership current NativeState Character owner/RNG receiver differs";return false;}
    host::WorldState after;if(!world(r,after,e)||!host::same_world(out.world.origin,after.origin)){if(e.empty())e="Service membership World changed during census";return false;}
    return true;
}
bool capture(Runtime& r,uintptr_t object,uintptr_t window,uintptr_t service,uintptr_t character,action::ServiceMembershipKind kind,std::string& e){
    const auto controller=typed_action_runtime::controller_status();action::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!installed||!r.ready||GetCurrentThreadId()!=r.thread||captured||r.pending||prop_text::pending()||boss_phase::pending()||
       !controller.value("started",false)||controller.value("pending",true)||controller.value("locked",true)||!typed_action_runtime::network_binding(snapshot,binding,e)){
        if(e.empty())e="Service membership capture requires idle bound Mainthread Controller";return false;}
    host::WorldState world_value;if(!world(r,world_value,e))return false;remove_diagnostics::LocalTarget target;
    if(!remove_diagnostics::resolve_local_target(world_value.selection,object,target,e))return false;
    Sample before;if(!sample(r,binding,snapshot,target.record_key,before,e))return false;
    prop_text::UiMemory memory(r);action::ui_diagnostic::Window current;
    action::ui_diagnostic::Context context{r.image,before.world.selection,object,target.uid,target.world_epoch,target.world_generation,
        "bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",action::ui_diagnostic::Family::Building};
    if(!action::ui_diagnostic::read_live_window(memory,context,current,e)||(window&&current.window!=window)){if(e.empty())e="Service membership source window differs";return false;}
    action::ServiceMembershipIntent intent;intent.kind=kind;
    if(kind==action::ServiceMembershipKind::Remove){
        for(size_t i=0;i<before.local.rows.size();++i)if(before.local.rows[i].object==service)intent.ordinal=int32_t(i);
        if(intent.ordinal<0){e="Service membership source service is not owned by selected Building";return false;}
    }else if(character){const auto found=std::find(before.local.characters.begin(),before.local.characters.end(),character);
        if(found==before.local.characters.end()){e="Service membership source spawn type is not current kind0 Character";return false;}intent.character=int32_t(found-before.local.characters.begin());}
    action::ServiceMembershipState after;if(!action::service_membership_expected(intent,before.value,after)){e="Service membership source target/bounds invalid";return false;}
    captured=Pending{binding,snapshot,target.record_key,intent};++captures;return true;
}
uint32_t record_construct(uintptr_t image,void* record,const void* text,bool& constructed){
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x967510)(record,text);constructed=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t record_read(uintptr_t image,const void* record,bool argument,uintptr_t& value){
    __try{const auto token=argument?reinterpret_cast<uintptr_t(*)(const void*,int32_t)>(image+0x967740)(record,0):uintptr_t(record);
        value=reinterpret_cast<uintptr_t(*)(const void*)>(image+0x96d880)(reinterpret_cast<const void*>(token));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t record_destroy(uintptr_t image,void* record){
    __try{reinterpret_cast<void(*)(void*)>(image+0x9676a0)(record);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t character_by_name(uintptr_t image,const std::string& name,uintptr_t manager,uintptr_t& character){
    __try{const prop_text::Text text{name.data(),name.size(),{}};
        character=reinterpret_cast<uintptr_t(*)(void*,int32_t,const void*)>(image+0x9e640)(reinterpret_cast<void*>(manager),0,&text);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool menu_values(Runtime& r,const void* native_text,std::string& label,std::string& name,std::string& e){
    alignas(8) unsigned char record[0x78]{};bool constructed=false;uintptr_t label_ptr=0,value_ptr=0;
    auto fault=record_construct(r.image,record,native_text,constructed);bool ok=false;
    try{if(!fault){fault=record_read(r.image,record,false,label_ptr);if(!fault)ok=prop_text::text(r,label_ptr,label,e);
            if(ok&&label=="Spawn"){fault=record_read(r.image,record,true,value_ptr);ok=!fault&&prop_text::text(r,value_ptr,name,e);}}}
    catch(const std::exception& x){e=std::string("Service membership menu text: ")+x.what();}
    if(constructed){const auto cleanup=record_destroy(r.image,record);if(cleanup){e+="; Service membership Record cleanup SEH="+std::to_string(cleanup);return false;}}
    if(fault){e="Service membership original Record read SEH="+std::to_string(fault);return false;}return ok;
}
void menu_entry(void* window,const void* text){
    auto* r=published.load(std::memory_order_acquire);if(!net::session_wire::connected()||!r){original_menu(window,text);return;}
    try{std::string label,name,e;if(!menu_values(*r,text,label,name,e)){reject(e);return;}
        if(label!="Spawn"){original_menu(window,text);return;}
        uintptr_t object=0,ap=0,character=0,manager=0;
        if(!r->memory.read(uintptr_t(window),&ap,sizeof ap,e)||ap!=r->image+0x172af10||!r->memory.read(uintptr_t(window)+0x4f8,&object,sizeof object,e)){
            reject(e.empty()?"Service membership original Building window class differs":e);return;}
        if(name!="Anyone"){
            if(!r->memory.read(r->image+0x13df7a0,&manager,sizeof manager,e)||!manager){reject(e.empty()?"Service membership Character manager absent":e);return;}
            const auto fault=character_by_name(r->image,name,manager,character);if(fault){reject("Service membership original Character lookup SEH="+std::to_string(fault));return;}
            if(!character)return;
        }
        if(!capture(*r,object,uintptr_t(window),0,character,action::ServiceMembershipKind::AddSpawn,e))reject(e);
    }catch(const std::exception& x){reject(std::string("Service membership menu capture: ")+x.what());}
}
void remove_entry(void* building,void* service){
    auto* r=published.load(std::memory_order_acquire);
    if(!net::session_wire::connected()||!r||uintptr_t(_ReturnAddress())!=r->image+0x66e387){original_remove(building,service);return;}
    try{std::string e;if(!capture(*r,uintptr_t(building),0,uintptr_t(service),0,action::ServiceMembershipKind::Remove,e))reject(e);}
    catch(const std::exception& x){reject(std::string("Service membership remove capture: ")+x.what());}
}
uint32_t invoke(Runtime& r,const action::ServiceMembershipIntent& intent,const Sample& before,bool& returned){
    __try{
        if(intent.kind==action::ServiceMembershipKind::Remove)original_remove(reinterpret_cast<void*>(before.target.object),reinterpret_cast<void*>(before.local.rows[size_t(intent.ordinal)].object));
        else{
            const auto service=reinterpret_cast<uintptr_t(*)(size_t)>(r.image+0xa02670)(0x180);
            reinterpret_cast<void(*)(void*)>(r.image+0x2442d0)(reinterpret_cast<void*>(service));
            *reinterpret_cast<int32_t*>(service+0x20)=2;*reinterpret_cast<uintptr_t*>(service+0x138)=intent.character<0?0:before.local.characters[size_t(intent.character)];
            *reinterpret_cast<uint8_t*>(service+0x4a)=1;
            reinterpret_cast<void(*)(void*,void*)>(r.image+0x407b10)(reinterpret_cast<void*>(before.target.object),reinterpret_cast<void*>(service));
        }
        returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool unchanged(const Sample& a,const Sample& b){return host::same_world(a.world.origin,b.world.origin)&&a.target.object==b.target.object&&a.target.uid==b.target.uid&&a.object==b.object&&a.money==b.money&&a.undo==b.undo&&a.random_owner==b.random_owner&&a.random==b.random;}
}
bool enable(std::string& e){
    if(installed)return true;if(install_attempted){e="Service membership previous hook failure: "+install_cause;return false;}install_attempted=true;
    auto* r=published.load(std::memory_order_acquire);if(!r||GetCurrentThreadId()!=r->thread){e=install_cause="Service membership requires bound Mainthread";return false;}
    struct Hook {uintptr_t rva;void* detour;void** original;};const Hook hooks[]{
        {0x66a790,reinterpret_cast<void*>(&menu_entry),reinterpret_cast<void**>(&original_menu)},
        {0x405510,reinterpret_cast<void*>(&remove_entry),reinterpret_cast<void**>(&original_remove)}};
    for(const auto& h:hooks){auto result=MH_CreateHook(reinterpret_cast<void*>(r->image+h.rva),h.detour,h.original);
        if(result!=MH_OK||!*h.original){e=install_cause="Service membership hook create RVA="+std::to_string(h.rva)+" status="+std::to_string(result)+" "+mt2mp::hooks::framework::process_backend().error();return false;}
        result=MH_EnableHook(reinterpret_cast<void*>(r->image+h.rva));if(result!=MH_OK){e=install_cause="Service membership hook enable RVA="+std::to_string(h.rva)+" status="+std::to_string(result)+" "+mt2mp::hooks::framework::process_backend().error();return false;}}
    installed=true;return true;
}
bool pending(){return bool(captured);}
void tick(){if(!captured)return;auto value=std::move(*captured);captured.reset();std::string e;action::Binding binding;
    if(!typed_action_runtime::network_binding(value.snapshot,binding,e)||binding!=value.binding){reject(e.empty()?"Service membership captured World changed":e);return;}
    const auto id=uuid(e);if(id.empty()||!typed_action_runtime::submit_service_membership(id,value.key,value.intent,e)){reject(e);return;}++submitted;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||!installed||GetCurrentThreadId()!=r->thread||request.action!=action::Action::EditServiceMembership||!action::service_membership_intent_valid(request.service_membership)){
        e="Service membership runtime/request unavailable";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){e="Service membership already attempted; no replay";return action::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid||bool(authority)==Config::instance().is_host){e="Service membership pending Controller/role differs";return action::Outcome::RejectedNoEffect;}
    Sample before;if(!sample(*r,binding,request.snapshot_uuid,request.canonical_record_key,before,e))return action::Outcome::RejectedNoEffect;
    action::ServiceMembershipState expected;if(!action::service_membership_expected(request.service_membership,before.value,expected)){e="Service membership exact current service/type/bounds differs";return action::Outcome::RejectedNoEffect;}
    if(authority&&(!action::valid_service_membership_effect(request,*authority)||authority->service_membership_before!=before.value||authority->before!=before.money)){
        e="Service membership Client complete before rows/configs/Characters/Ledger differs from Host";return action::Outcome::RejectedNoEffect;}
    if(!action::service_membership_wire_fits(request,before.value,before.money,e))return action::Outcome::RejectedNoEffect;
    auto& op=operations.emplace(request.action_uuid,Operation{binding,request,before}).first->second;
    op.evidence.before=before.money;op.evidence.service_membership_before=before.value;
    auto unknown=[&](const std::string& why){e=op.cause=why;return action::Outcome::Unknown;};
    op.attempted=true;const auto fault=invoke(*r,request.service_membership,before,op.returned);
    if(fault||!op.returned)return unknown("Service membership original mutation SEH="+std::to_string(fault));
    if(!sample(*r,binding,request.snapshot_uuid,request.canonical_record_key,op.after,e))return unknown(e);
    op.evidence.after=op.after.money;op.evidence.service_membership_after=op.after.value;
    if(!unchanged(before,op.after)||!service_membership_reader::transition(request.service_membership,before.value,before.local,op.after.local)||
       !action::valid_service_membership_effect(request,op.evidence)||(authority&&(authority->service_membership_after!=op.after.value||authority->after!=op.after.money)))return unknown("Service membership original complete effect/owner/survivors differs");
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);const auto found=operations.find(request.action_uuid);
    if(!r||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed){e="Service membership observation lacks exact confirmed operation";return false;}
    const auto& op=found->second;Sample current;if(!sample(*r,binding,request.snapshot_uuid,request.canonical_record_key,current,e))return false;
    if(!unchanged(op.after,current)||op.after.value!=current.value||op.after.local!=current.local){e="Service membership current complete state/UID/owners differs before ACK";return false;}
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence current;if(!observe(binding,request,current,e))return false;auto& op=operations.at(request.action_uuid);
    if(op.retired){e="Service membership operation already retired";return false;}op.retired=true;return true;
}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"attempted",op.attempted},{"returned",op.returned},{"confirmed",op.confirmed},{"retired",op.retired},{"local_uid",op.before.target.uid},{"cause",op.cause}};
    return {{"enabled",installed},{"pending",pending()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},{"cause",cause},{"operations",std::move(rows)}};
}
}
