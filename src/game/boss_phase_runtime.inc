namespace boss_phase {
namespace {
using Reorder=void(__fastcall*)(void*,void*,int32_t);
using Command=bool(__fastcall*)(void*,void*);using Menu=void(__fastcall*)(void*,const void*);using Create=void(__fastcall*)(void*);
Reorder original=nullptr;Command original_command=nullptr;Menu original_menu=nullptr;Create original_create=nullptr;bool installed=false,install_attempted=false;
std::string install_cause,cause;
struct Sample {
    host::WorldState world;uintptr_t manager=0,data=0;int32_t capacity=0;
    std::vector<boss_phase_reader::LocalType> local;
    action::BossPhaseState value;action::Ledger money;witness::UndoWitness undo;
    std::array<uint64_t,4> random{};
};
struct Pending {action::Binding binding;std::string snapshot;action::BossPhaseIntent intent;};
struct Operation {action::Binding binding;action::Request request;Sample before,after;action::Evidence evidence;
    bool attempted=false,returned=false,confirmed=false,retired=false;std::string cause;};
std::optional<Pending> captured;std::map<std::string,Operation> operations;
uint64_t captures=0,submitted=0,rejected=0;
void reject(const std::string& error){++rejected;if(cause!=error){cause=error;LOG_ERROR("[BossPhase] {}",error);}}
uint32_t named_window(Runtime& r,uintptr_t& window){
    __try{const prop_text::Text label{"info_charactertype",18,{}};
        window=reinterpret_cast<uintptr_t(*)(const prop_text::Text&)>(r.image+0x81aa90)(label);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool live_list(Runtime& r,uintptr_t selected,uintptr_t& window,uintptr_t& list,std::string& e){
    list=0;const auto seh=named_window(r,window);if(seh){e="Boss phase named Window query SEH="+std::to_string(seh);return false;}
    if(!window)return true;
    auto at=[&](uintptr_t base,size_t offset,auto& value){return base&&base<=UINTPTR_MAX-offset&&r.memory.read(base+offset,&value,sizeof value,e);};
    uintptr_t ap=0,view=0,owner=0,phases=0;int32_t queued=0;
    if(!at(window,0,ap)||ap!=r.image+0x173a7f0||!at(window,0x4e8,view)||!at(view,0,ap)||ap!=r.image+0x1736770||!at(view,0x460,owner)){
        if(e.empty())e="Boss phase native Character Window/View class differs";return false;}
    if(owner!=selected)return true;
    if(!at(view,0x3c8,phases)||!at(phases,0,ap)||ap!=r.image+0x1712800||!at(phases,0x318,owner)||owner!=selected||
       !at(phases,0x320,list)||!list||!at(list,0,ap)||ap!=r.image+0x1721a00||!at(list,0x318,owner)||owner!=selected||!at(list,0x348,queued)||queued!=0){
        if(e.empty())e="Boss phase selected native PhaseView/List owner differs";return false;}
    return true;
}
bool sample(Runtime& r,const action::Binding& binding,const std::string& snapshot,Sample& out,std::string& e){
    out={};if(!world(r,out.world,e)||out.world.origin.world_epoch!=binding.native_epoch||out.world.origin.generation!=binding.native_generation||snapshot!=net::current_snapshot_id()){
        if(e.empty())e="Boss phase current native World/snapshot differs";return false;}
    auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& error){
        if(!base||base>UINTPTR_MAX-offset){error="Boss phase address invalid";return false;}return r.memory.read(base+offset,&value,sizeof value,error);};
    auto text=[&](uintptr_t address,std::string& value,std::string& error){return prop_text::text(r,address,value,error);};
    uintptr_t owned=0,ap=0,random=0,release_manager=0,release=0,color=0;int32_t count=0;
    if(!read(r.image,0x13df7a0,out.manager,e)||!read(uintptr_t(out.world.origin.state),0x168,owned,e)||owned!=out.manager||
       !read(out.manager,0,ap,e)||ap!=r.image+0x1732430||
       !boss_phase_reader::array(read,out.manager,0x170,out.data,count,out.capacity,e)){
        if(e.empty())e="Boss phase current Character manager/Boss collection differs";return false;}
    std::set<uintptr_t> pointers;std::set<uint64_t> uids;
    for(int32_t id=0;id<count;++id){uintptr_t object=0;action::BossTypePhases row;boss_phase_reader::LocalType local;
        if(!read(out.data,size_t(id)*8,object,e)||!pointers.insert(object).second||
           !boss_phase_reader::type(read,text,r.image,object,id,row,local,e)||!uids.insert(local.uid).second){
            if(e.empty())e="Boss phase duplicated Character pointer/full UID";return false;}
        out.value.bosses.push_back(std::move(row));out.local.push_back(std::move(local));}
    uintptr_t after_data=0;int32_t after_count=0,after_capacity=0;
    if(!boss_phase_reader::array(read,out.manager,0x170,after_data,after_count,after_capacity,e)||
       after_data!=out.data||after_count!=count||after_capacity!=out.capacity||
       !ledger(out.money,e)||!witness::read_undo(r.memory.process,r.image,out.undo,e)||out.undo.active_graph||
       !read(r.image,0x164b250,random,e)||!read(random,0,out.random,e)||
       !read(r.image,0x13df4f0,release_manager,e)||!read(release_manager,0,ap,e)||ap!=r.image+0x1714ed0||
       !read(release_manager,0x38,release,e)||!read(release,0,ap,e)||ap!=r.image+0x16c12b0||
       !read(release,0x120,out.value.release_edits,e)||!read(r.image,0x164c5c0,color,e)||!read(color,0,out.value.default_color,e)){
        if(e.empty())e="Boss phase collection/Undo/RNG/release ownership differs";return false;}
    host::WorldState after;if(!world(r,after,e)||!host::same_world(out.world.origin,after.origin)){
        if(e.empty())e="Boss phase World changed during census";return false;}return true;
}
bool same_local(const Sample& before,const Sample& after,const action::BossPhaseIntent* intent=nullptr){
    if(!host::same_world(before.world.origin,after.world.origin)||before.manager!=after.manager||before.data!=after.data||before.capacity!=after.capacity||
       before.money!=after.money||before.undo!=after.undo||before.random!=after.random||before.local.size()!=after.local.size())return false;
    for(size_t i=0;i<before.local.size();++i){
        if(intent&&intent->kind==action::BossPhaseKind::Reorder&&int32_t(i)==intent->character_id){if(!boss_phase_reader::reordered_local(before.local[i],after.local[i],intent->phase,intent->boundary))return false;}
        else if(intent&&int32_t(i)==intent->character_id&&(intent->kind==action::BossPhaseKind::RemoveAbility||intent->kind==action::BossPhaseKind::AddAbility)){
            if(!boss_phase_reader::membership_local(before.local[i],after.local[i],*intent))return false;}
        else if(intent&&int32_t(i)==intent->character_id&&(intent->kind==action::BossPhaseKind::Create||intent->kind==action::BossPhaseKind::Destroy)){
            if(!boss_phase_reader::phase_set_local(before.local[i],after.local[i],*intent))return false;}
        else if(before.local[i]!=after.local[i])return false;
    }return true;
}
bool capture(Runtime& r,uintptr_t object,uintptr_t phase,int32_t boundary,std::string& e){
    const auto controller=typed_action_runtime::controller_status();action::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!installed||!r.ready||GetCurrentThreadId()!=r.thread||captured||r.pending||prop_text::pending()||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||
       !typed_action_runtime::network_binding(snapshot,binding,e)){
        if(e.empty())e="Boss phase capture requires idle bound Mainthread Controller";return false;}
    Sample before;if(!sample(r,binding,snapshot,before,e))return false;
    uintptr_t window=0,list=0,item=0,actual_phase=0;int32_t actual_boundary=-1;
    if(!live_list(r,object,window,list,e)||!list||
       !r.memory.read(list+0x350,&item,sizeof item,e)||!item||
       !r.memory.read(item+0x348,&actual_phase,sizeof actual_phase,e)||actual_phase!=phase||
       !r.memory.read(list+0x358,&actual_boundary,sizeof actual_boundary,e)||actual_boundary!=boundary){
        if(e.empty())e="Boss phase actual native drag receiver/phase/boundary differs";return false;}
    action::BossPhaseIntent intent;intent.boundary=boundary;
    for(size_t i=0;i<before.local.size();++i)if(before.local[i].object==object){intent.character_id=int32_t(i);
        for(size_t j=0;j<before.local[i].phases.size();++j)if(before.local[i].phases[j].pointer==phase)intent.phase=int32_t(j);}
    if(!action::valid_boss_phase(intent)||before.value.bosses[size_t(intent.character_id)].deleted||size_t(boundary)>before.local[size_t(intent.character_id)].phases.size()){
        e="Boss phase exact current Character/phase/boundary ownership differs";return false;}
    if(boundary==intent.phase||boundary==intent.phase+1)return true;
    captured=Pending{binding,snapshot,intent};++captures;return true;
}
#include "boss_phase_property_runtime.inc"
void entry(void* object,void* phase,int32_t boundary){
    auto* r=published.load(std::memory_order_acquire);
    if(!net::session_wire::connected()||!r||uintptr_t(_ReturnAddress())!=r->image+0x696aa3){original(object,phase,boundary);return;}
    try{std::string error;if(!capture(*r,uintptr_t(object),uintptr_t(phase),boundary,error))reject(error);}
    catch(const std::exception& x){reject(std::string("Boss phase capture: ")+x.what());}
}
uint32_t invoke(const boss_phase_reader::LocalType& owner,const action::BossPhaseIntent& intent,bool& returned){
    if(intent.kind!=action::BossPhaseKind::Reorder){auto* r=published.load(std::memory_order_acquire);
        NativePropertyCalls calls;
        if(intent.kind==action::BossPhaseKind::Create)return calls.create(r->image,owner.object,intent.text,returned);
        if(intent.kind==action::BossPhaseKind::Destroy)return calls.destroy(r->image,owner.object,owner.phases[size_t(intent.phase)].pointer,returned);
        if(intent.kind==action::BossPhaseKind::AddAbility)return boss_phase_property::add(calls,r->image,owner.phases[size_t(intent.phase)].pointer,owner.abilities,intent,returned);
        return boss_phase_property::apply(calls,r->image,owner.phases[size_t(intent.phase)].pointer,intent,returned);}
    __try{original(reinterpret_cast<void*>(owner.object),reinterpret_cast<void*>(owner.phases[size_t(intent.phase)].pointer),intent.boundary);returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t refresh(Runtime& r,uintptr_t window,uintptr_t list){
    __try{reinterpret_cast<void(*)(void*)>(r.image+0x696970)(reinterpret_cast<void*>(list));
        reinterpret_cast<void(*)(void*)>(r.image+0x81a330)(reinterpret_cast<void*>(window));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool enable(std::string& e){
    if(installed)return true;if(install_attempted){e="Boss phase previous hook failure: "+install_cause;return false;}install_attempted=true;
    auto* r=published.load(std::memory_order_acquire);if(!r||GetCurrentThreadId()!=r->thread){e=install_cause="Boss phase requires bound Mainthread";return false;}
    auto result=MH_CreateHook(reinterpret_cast<void*>(r->image+0x93900),reinterpret_cast<void*>(&entry),reinterpret_cast<void**>(&original));
    if(result!=MH_OK||!original){e=install_cause="Boss phase hook create status="+std::to_string(result)+" "+mt2mp::hooks::framework::process_backend().error();return false;}
    result=MH_EnableHook(reinterpret_cast<void*>(r->image+0x93900));
    if(result!=MH_OK){e=install_cause="Boss phase hook enable status="+std::to_string(result)+" "+mt2mp::hooks::framework::process_backend().error();return false;}
    result=MH_CreateHook(reinterpret_cast<void*>(r->image+0x698960),reinterpret_cast<void*>(&property_entry),reinterpret_cast<void**>(&original_command));
    if(result!=MH_OK||!original_command){e=install_cause="Boss phase command hook create status="+std::to_string(result)+" "+mt2mp::hooks::framework::process_backend().error();return false;}
    result=MH_EnableHook(reinterpret_cast<void*>(r->image+0x698960));
    if(result!=MH_OK){e=install_cause="Boss phase command hook enable status="+std::to_string(result)+" "+mt2mp::hooks::framework::process_backend().error();return false;}
    result=MH_CreateHook(reinterpret_cast<void*>(r->image+0x697bf0),reinterpret_cast<void*>(&menu_entry),reinterpret_cast<void**>(&original_menu));
    if(result!=MH_OK||!original_menu){e=install_cause="Boss phase menu hook create status="+std::to_string(result)+" "+mt2mp::hooks::framework::process_backend().error();return false;}
    result=MH_EnableHook(reinterpret_cast<void*>(r->image+0x697bf0));
    if(result!=MH_OK){e=install_cause="Boss phase menu hook enable status="+std::to_string(result)+" "+mt2mp::hooks::framework::process_backend().error();return false;}
    result=MH_CreateHook(reinterpret_cast<void*>(r->image+0x6961b0),reinterpret_cast<void*>(&create_entry),reinterpret_cast<void**>(&original_create));
    if(result!=MH_OK||!original_create){e=install_cause="Boss phase create hook status="+std::to_string(result)+" "+mt2mp::hooks::framework::process_backend().error();return false;}
    result=MH_EnableHook(reinterpret_cast<void*>(r->image+0x6961b0));
    if(result!=MH_OK){e=install_cause="Boss phase create hook enable status="+std::to_string(result)+" "+mt2mp::hooks::framework::process_backend().error();return false;}
    installed=true;return true;
}
bool pending(){return bool(captured);}
void tick(){if(!captured)return;auto value=std::move(*captured);captured.reset();std::string e;action::Binding binding;
    if(!typed_action_runtime::network_binding(value.snapshot,binding,e)||binding!=value.binding){reject(e.empty()?"Boss phase captured world changed":e);return;}
    const auto id=uuid(e);if(id.empty()||!typed_action_runtime::submit_boss_phase(id,value.intent,e)){reject(e);return;}++submitted;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||!installed||GetCurrentThreadId()!=r->thread||request.action!=action::Action::ReorderBossPhase||!action::valid_boss_phase(request.boss_phase)){
        e="Boss phase runtime/request unavailable";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){e="Boss phase action already attempted; no replay";return action::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||
       controller.value("action_uuid",std::string{})!=request.action_uuid||bool(authority)==Config::instance().is_host){e="Boss phase exact pending Controller/role differs";return action::Outcome::RejectedNoEffect;}
    Sample before;if(!sample(*r,binding,request.snapshot_uuid,before,e))return action::Outcome::RejectedNoEffect;
    const auto& intent=request.boss_phase;
    if(size_t(intent.character_id)>=before.local.size()||before.value.bosses[size_t(intent.character_id)].deleted||
       (intent.kind!=action::BossPhaseKind::Create&&size_t(intent.phase)>=before.local[size_t(intent.character_id)].phases.size())||
       (intent.kind==action::BossPhaseKind::Reorder&&(size_t(intent.boundary)>before.local[size_t(intent.character_id)].phases.size()||intent.boundary==intent.phase||intent.boundary==intent.phase+1))||
       (intent.kind!=action::BossPhaseKind::Reorder&&intent.kind!=action::BossPhaseKind::Create&&property_same(before.value.bosses[size_t(intent.character_id)].phases[size_t(intent.phase)],intent))){e="Boss phase current type/phase/boundary is absent or unchanged";return action::Outcome::RejectedNoEffect;}
    if(!property_membership_valid(before.value.bosses[size_t(intent.character_id)],intent)){e="Boss phase current ability membership/name/index differs";return action::Outcome::RejectedNoEffect;}
    if(authority&&(!action::valid_boss_phase_effect(request,*authority)||authority->boss_phase_before!=before.value||authority->before!=before.money)){
        e="Boss phase Client complete current types/phases/abilities/Ledger differ from Host";return action::Outcome::RejectedNoEffect;}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;op.before=std::move(before);
    op.evidence.before=op.before.money;op.evidence.boss_phase_before=op.before.value;
    const auto unknown=[&](const std::string& why){e=op.cause=why;return action::Outcome::Unknown;};
    op.attempted=true;const auto seh=invoke(op.before.local[size_t(intent.character_id)],intent,op.returned);
    if(seh||!op.returned)return unknown("Boss phase original edit failed SEH="+std::to_string(seh));
    uintptr_t window=0,list=0;if(!live_list(*r,op.before.local[size_t(intent.character_id)].object,window,list,e))return unknown(e);
    if(list){const auto presentation=refresh(*r,window,list);if(presentation)return unknown("Boss phase original list refresh failed SEH="+std::to_string(presentation));}
    if(!sample(*r,binding,request.snapshot_uuid,op.after,e))return unknown(e);
    op.evidence.after=op.after.money;op.evidence.boss_phase_after=op.after.value;
    if(!same_local(op.before,op.after,&intent)||!action::valid_boss_phase_effect(request,op.evidence)||
       (authority&&(authority->boss_phase_after!=op.after.value||authority->after!=op.after.money)))return unknown("Boss phase original pointer/UID/order/ability/event result, Undo/RNG/Ledger differs");
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    const auto found=operations.find(request.action_uuid);auto* r=published.load(std::memory_order_acquire);
    if(!r||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed){e="Boss phase exact confirmed operation absent";return false;}
    const auto& op=found->second;Sample current;if(!sample(*r,binding,request.snapshot_uuid,current,e))return false;
    if(!same_local(op.after,current)||op.after.value!=current.value){e="Boss phase current full result changed before ACK";return false;}
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence current;if(!observe(binding,request,current,e))return false;auto& op=operations.at(request.action_uuid);
    if(op.retired){e="Boss phase action already retired";return false;}op.retired=true;return true;
}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"attempted",op.attempted},{"returned",op.returned},{"confirmed",op.confirmed},{"retired",op.retired},{"cause",op.cause}};
    return {{"enabled",installed},{"pending",pending()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},{"cause",cause},{"operations",std::move(rows)}};
}
}
