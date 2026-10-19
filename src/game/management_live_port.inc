namespace {uint32_t scalar_cas(uintptr_t,bool,int32_t,int32_t,bool&)noexcept;}
#include "service_live_native.inc"
namespace {
namespace management=management_simulation;
bool primary_once(Runtime& r,const a::Context& context,const a::Binding& binding,Local& local,management::LiveState& out,std::string& e){
    local={};out={};a::Context actual;if(GetCurrentThreadId()!=r.thread||!a::selected_session().current(actual,e)||actual!=context)return fail(e,e.empty()?"Management live Session/Mainthread differs":e);
    live::NativePort port(context,binding,nullptr);if(!port.read(local.pin,e)||local.pin.image!=r.image)return false;
    if(!at(r,r.image,0x13df500,local.software,e)||!ap(r,local.software,0x1715450,e)||!at(r,r.image,0x13df4f0,local.release,e)||!ap(r,local.release,0x1714ed0,e)||
       !at(r,local.pin.business,0x50,out.game_masters,e)||!at(r,local.release,0xb8,out.release_funds,e)||
       !at(r,local.software,0x37c,out.free_points,e)||!at(r,local.software,0x378,out.purchased_points,e)||
       !at(r,local.software,0x31c,out.statistics,e)||!at(r,local.software,0x350,out.spent,e)||!at(r,local.software,0x344,out.purchased,e)||
       !at(r,local.software,0x88,out.software_version,e)||!at(r,local.release,0x38,local.current_release,e))return false;
    out.release_present=local.current_release!=0;
    if(out.release_present&&(!ap(r,local.current_release,0x16c12b0,e)||!at(r,local.current_release,0x50,out.planned_version,e)||!at(r,local.current_release,0x60,out.release_type,e)))return false;
    if(!read_slots(r,local.software,out.character_slots,e)||!read_employment(r,local,out.lounge_employed,out.gm_employment,e))return false;
    return services_read(r,context,binding,local,out.services,e)&&general_project_runtime::capture_progress(context,binding,out.project,e)&&management::valid(out,e);
}
bool primary_read(Runtime& r,const a::Context& context,const a::Binding& binding,Local& local,management::LiveState& out,std::string& e){
    Local first;management::LiveState before;if(!primary_once(r,context,binding,first,before,e)||!primary_once(r,context,binding,local,out,e))return false;
    if(first.pin!=local.pin||first.software!=local.software||first.release!=local.release||first.current_release!=local.current_release||first.lounge!=local.lounge||first.gm_rows!=local.gm_rows||first.gm_uids!=local.gm_uids||first.services!=local.services||first.service_destinations!=local.service_destinations||before!=out)return fail(e,"Management live roots/scalars changed during double read");return true;
}
#include "gm_ticket_live.inc"
bool gm_prepare_pool(Runtime& r,const a::Context& context,const a::Binding& binding,Local& local,management::LiveState& before,const management::LiveState& desired,live::ManagementReceipt& receipt,std::string& error){
    if(before.gm_employment.size()==desired.gm_employment.size())return true;
    if(!desired.gm||!before.gm_employment.empty()||desired.gm_employment.size()!=50)return fail(error,"GM fixed pool transition requires original empty-to50 creation");
    live::NativePort port(context,binding,nullptr);if(!port.current_owners(local.pin,error)||!gm_route_code(r,error))return false;
    const auto original=local;receipt.attempted=true;++receipt.attempted_stores;
    const auto exception=gm_ticket::native::pool(r.image,local.lounge,desired.lounge_employed);
    if(exception)return fail(error,"GM original fixed pool creation SEH="+std::to_string(exception)+"; no replay");++receipt.stores;
    if(!primary_read(r,context,binding,local,before,error))return false;
    if(local.pin!=original.pin||local.lounge!=original.lounge||local.software!=original.software||local.release!=original.release||local.current_release!=original.current_release||local.gm_rows.size()!=50)return fail(error,"GM original fixed pool creation changed current owner/Clock/Ledger");
    return true;
}
uintptr_t scalar_address(const Local& local,size_t slot){
    if(slot==0)return local.pin.business+0x50;if(slot==1)return local.release+0xb8;
    if(slot==2)return local.software+0x37c;if(slot==3)return local.software+0x378;
    if(slot<14)return local.software+0x31c+(slot-4)*4;if(slot<24)return local.software+0x350+(slot-14)*4;
    if(slot<34)return local.software+0x344+(slot-24);if(slot<37)return local.software+0x88+(slot-34)*4;
    if(slot<40)return local.current_release?local.current_release+0x50+(slot-37)*4:0;
    if(slot==40)return local.current_release?local.current_release+0x60:0;
    if(slot==44)return local.lounge+0xd0;
    if(slot>=45&&slot-45<local.gm_rows.size()*2)return local.gm_rows[(slot-45)/2]+0x4c4+(slot-45)%2;
    return 0;
}
uint32_t scalar_cas(uintptr_t address,bool byte,int32_t expected,int32_t desired,bool& stored)noexcept{
    __try{if(byte)stored=uint8_t(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address),char(desired),char(expected)))==uint8_t(expected);
        else stored=InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address),LONG(desired),LONG(expected))==LONG(expected);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t slots_once(uintptr_t image,uintptr_t collection,int32_t type,int32_t desired)noexcept{
    __try{reinterpret_cast<void(*)(void*,int32_t,int32_t)>(image+0x9af00)(reinterpret_cast<void*>(collection),type,desired);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool LivePort::bind(const a::Context& context,const a::Binding& binding,uint64_t epoch,std::string& e){
    auto* r=published.load(std::memory_order_acquire);Local local;management::LiveState state;
    if(!r||!r->ready||!epoch||locked_||!primary_read(*r,context,binding,local,state,e))return fail(e,e.empty()?"Management live bind requires current native owner":e);
    std::vector<unsigned char> code(224);if(!r->memory.copy(r->image+0x9af00,code.data(),code.size(),e)||mt2mp::sha256(code)!="7ba261b1a85d1de5ab024d27a87eef5533238a344ac196c94d9ede3a0f937dcc")return fail(e,e.empty()?"Management original SetMaxSlotCount body differs":e);
    code.resize(32);if(!r->memory.copy(r->image+0x3abb30,code.data(),code.size(),e)||mt2mp::sha256(code)!="689331e018209973cd1f61946fbff9b7da29fc36c8407be2c5122ad3b6fc01d0")return fail(e,e.empty()?"Management original employment scalar body differs":e);
    if(bound_&&(context.session!=context_.session||context.role!=context_.role||context.local_peer!=context_.local_peer||context.peer_mask!=context_.peer_mask))return fail(e,"Management live immutable Session changed");
    building_mixed_identity::opened_file::CurrentWorld world;if(!gm_world(*r,context,binding,world,e)||!gm_route_code(*r,e)||!gm_helper_code(*r,e)||!policy_route_code(*r,e)||!rules_routes(*r,e))return false;
    if(!interventions::actors::live::bind(context,binding,epoch,e)||!control_input::runtime::bind(context,binding,epoch,e))return false;
    r->gm_context=context;r->gm_binding=binding;r->gm_root=uintptr_t(world.map)+0x1c0;r->gm_client=context.role==a::Role::Client;r->gm_suspended=false;
    context_=context;binding_=binding;epoch_=epoch;bound_=true;suspended_=false;last_=nullptr;cause_.clear();e.clear();return true;
}
bool LivePort::suspend(std::string& e){if(locked_)return fail(e,cause_);if(!control_input::runtime::suspend(e))return false;suspended_=true;if(auto* r=published.load(std::memory_order_acquire))r->gm_suspended=true;return interventions::actors::live::suspend(e);}
bool LivePort::capture(const a::Context& context,const a::Binding& binding,management::LiveState& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);if(!r||!bound_||suspended_||locked_||context!=context_||binding!=binding_)return fail(e,"Management live capture exact saved context/binding unavailable");Local local;if(!primary_read(*r,context,binding,local,out,e))return false;
    gm_ticket::Publication values;if(!gm_capture(*r,context,binding,local,values,e))return false;out.gm=std::move(values);gm_policy::State policies;uintptr_t queue=0;if(!policy_values(*r,policies,queue,e))return false;out.policies=policies;game_rules::State rules;game_rules::Pin owners;if(!rules_values(*r,rules,owners,e)||owners.software!=local.software)return fail(e,e.empty()?"Management Design current Software owner differs":e);out.rules=rules;subscriber_intervention::State subscribers;subscriber_intervention::Owners subscriber_owners;if(!intervention_subscribers(*r,subscribers,subscriber_owners,e))return false;out.subscriber_interventions=std::move(subscribers);
    interventions::actors::State actors;interventions::actors::lifecycle::Batch journal;if(!interventions::actors::live::capture(context,binding,actors,journal,e))return false;out.intervention_actors=std::move(actors);out.actor_lifecycle=std::move(journal);return management::valid(out,e);
}
bool LivePort::finish_capture(const a::Context& context,const a::Binding& binding,live::Frame& frame,std::string& error){
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!bound_||suspended_||locked_||context!=context_||binding!=binding_||!frame.management||!frame.management->subscriber_interventions||frame.session!=context.session||frame.snapshot!=context.snapshot_uuid||frame.epoch!=epoch_)return fail(error,"Management late capture exact saved Context/Binding/Frame unavailable");
    live::NativePort port(context,binding,nullptr);live::Pin pin;if(!port.read(pin,error))return false;
    if(pin.values!=frame.values||pin.common_epoch!=frame.epoch)return fail(error,"Management late capture actual Clock/full Ledger changed during original captures");
    subscriber_intervention::State values;subscriber_intervention::Owners owners;if(!intervention_subscribers(*r,values,owners,error))return false;
    frame.management->subscriber_interventions=std::move(values);
    interventions::actors::State actors;interventions::actors::lifecycle::Batch journal;
    if(!interventions::actors::live::capture(context,binding,actors,journal,error))return false;
    frame.management->intervention_actors=std::move(actors);frame.management->actor_lifecycle=std::move(journal);
    control_input::ReplicaState control;if(!control_input::runtime::capture(context,binding,control,error))return false;frame.control=std::move(control);
    return management::valid(*frame.management,error)&&port.current_owners(pin,error);
}
bool LivePort::apply(const a::Context& context,const a::Binding& binding,const live::Frame& frame,live::ManagementReceipt& receipt,std::string& e){
    receipt={};auto* r=published.load(std::memory_order_acquire);
    if(!r||!bound_||suspended_||locked_||context!=context_||binding!=binding_||context.role!=a::Role::Client||frame.session!=context.session||frame.snapshot!=context.snapshot_uuid||frame.epoch!=epoch_||!frame.management)return fail(e,"Management live Apply saved Context/Binding/Frame unavailable");
    if(!management::valid(*frame.management,e))return false;
    Local local;management::LiveState before;if(!primary_read(*r,context,binding,local,before,e)||local.pin.selected!=0)return fail(e,e.empty()?"Management Client actual Clock must be0":e);
    auto failed=[&](const std::string& cause){const auto original=cause;e=cause_.empty()?original:cause_;cause_=e;receipt.cause=e;receipt.unknown=receipt.attempted;locked_=receipt.unknown;last_={{"revision",frame.revision},{"confirmed",false},{"unknown",receipt.unknown},{"cause",e}};const auto services=services_difference(local,before.services,frame.management->services);if(!services.is_null())last_["service_difference"]=services;return false;};
    if(!gm_prepare_pool(*r,context,binding,local,before,*frame.management,receipt,e))return failed(e);
    std::vector<management::Change> changes;if(!management::plan(before,*frame.management,changes,e))return failed(e);
    live::NativePort port(context,binding,nullptr);
    for(const auto& change:changes){
        if(!port.current_owners(local.pin,e))return failed(e);
        if(change.slot>=41&&change.slot<44){
            const auto type=change.slot-41;uintptr_t values=0;int32_t actual=0;
            if(!at(*r,local.software+0xb8,0x140,values,e)||!at(*r,values,type*4,actual,e)||actual!=change.before)return failed(e.empty()?"Management Character slot concurrent value differs":e);
            receipt.attempted=true;++receipt.attempted_stores;const auto seh=slots_once(r->image,local.software+0xb8,int32_t(type),change.after);
            if(seh)return failed("Management original SetMaxSlotCount SEH="+std::to_string(seh)+"; no replay");++receipt.stores;
        }else{
            if(change.slot>=45){uintptr_t data=0,current=0;uint64_t uid=0;int32_t count=0,slot=-1;const auto index=(change.slot-45)/2;
                if(!at(*r,local.lounge,0x40,data,e)||!at(*r,local.lounge,0x48,count,e)||count!=int32_t(local.gm_rows.size())||!at(*r,data,index*sizeof(uintptr_t),current,e)||current!=local.gm_rows[index]||!ap(*r,current,0x16e5070,e)||!at(*r,current,0x18,uid,e)||uid!=local.gm_uids[index]||!at(*r,current,0x4ac,slot,e)||slot!=int32_t(index))return failed(e.empty()?"Management GameMaster current full-UID/fixed-slot membership changed":e);
                uintptr_t registered=0;const auto seh=uid_lookup(r->image,uid,registered);if(seh||registered!=current)return failed("Management GameMaster current FindUID differs SEH="+std::to_string(seh));
            }
            const auto address=scalar_address(local,change.slot);if(!address)return failed("Management scalar target has no current native owner");
            bool stored=false;receipt.attempted=true;++receipt.attempted_stores;const auto seh=scalar_cas(address,(change.slot>=24&&change.slot<34)||change.slot>=45,change.before,change.after,stored);
            if(seh||!stored)return failed("Management scalar CAS changed concurrently slot="+std::to_string(change.slot)+" SEH="+std::to_string(seh)+"; no replay");++receipt.stores;
        }
    }
    if(!services_apply(*r,context,binding,local,before.services,frame.management->services,receipt,e))return failed(e);
    bool project_attempted=false;uint32_t project_attempts=0,project_stores=0;
    const bool project_applied=general_project_runtime::apply_progress(context,binding,frame.management->project,project_attempted,project_attempts,project_stores,e);
    receipt.attempted|=project_attempted;receipt.attempted_stores+=project_attempts;receipt.stores+=project_stores;
    if(!project_applied)return failed(e);
    if(frame.management->subscriber_interventions&&!intervention_adopt_subscribers(*r,context,binding,*frame.management->subscriber_interventions,receipt,e))return failed(e);
    if(frame.management->rules&&!rules_adopt(*r,*frame.management->rules,receipt,e))return failed(e);
    if(frame.management->policies&&!policy_adopt(*r,*frame.management->policies,receipt,e))return failed(e);
    if(frame.management->gm&&!gm_apply(*r,context,binding,local,*frame.management->gm,receipt,e))return failed(e);
    if(frame.management->intervention_actors||frame.management->actor_lifecycle){
        interventions::actors::Receipt actors;const auto okay=interventions::actors::live::apply(context,binding,frame,actors,e);
        receipt.attempted|=actors.attempted;receipt.attempted_stores+=actors.attempted_stores;receipt.stores+=actors.stores;if(!okay)return failed(e);
    }
    if(!frame.control)return failed("Control authoritative Frame state is absent");
    bool control_attempted=false;const bool control_okay=control_input::runtime::apply(context,binding,*frame.control,control_attempted,e);receipt.attempted|=control_attempted;if(!control_okay)return failed(e);
    Local final;management::LiveState actual;if(!primary_read(*r,context,binding,final,actual,e))return failed(e);
    if(frame.management->gm){gm_ticket::Publication values;if(!gm_capture(*r,context,binding,final,values,e))return failed(e);actual.gm=std::move(values);}
    if(frame.management->policies){gm_policy::State values;uintptr_t queue=0;if(!policy_values(*r,values,queue,e))return failed(e);actual.policies=values;}
    if(frame.management->rules){game_rules::State values;game_rules::Pin owners;if(!rules_values(*r,values,owners,e)||owners.software!=final.software)return failed(e.empty()?"Management final Design Software owner differs":e);actual.rules=values;}
    if(frame.management->subscriber_interventions){subscriber_intervention::State values;subscriber_intervention::Owners owners;if(!intervention_subscribers(*r,values,owners,e))return failed(e);actual.subscriber_interventions=std::move(values);}
    if(frame.management->intervention_actors){interventions::actors::State values;interventions::actors::Locals owners;if(!interventions::actors::read_current(context,binding,values,owners,e))return failed(e);actual.intervention_actors=std::move(values);}
    if(frame.management->actor_lifecycle)actual.actor_lifecycle=frame.management->actor_lifecycle;
    if(actual!=*frame.management||final.pin!=local.pin||final.software!=local.software||final.release!=local.release||final.current_release!=local.current_release||final.lounge!=local.lounge||final.gm_rows!=local.gm_rows||final.gm_uids!=local.gm_uids)return failed("Management actual complete scalar/GM/slot/Clock/Ledger readback differs from Host");
    receipt.confirmed=true;last_={{"revision",frame.revision},{"confirmed",true},{"unknown",false},{"native_readback_present",true},{"before",management::encode(before)},{"after",management::encode(actual)},{"host_state",management::encode(*frame.management)},{"stores",receipt.stores}};e.clear();return true;
}
bool LivePort::sent(const live::Frame& frame,std::string& error){return interventions::actors::live::sent(frame,error);}
bool LivePort::acknowledge(uint64_t peer,uint64_t revision,std::string& error){return interventions::actors::live::acknowledge(peer,revision,error);}
bool LivePort::admit(const live::Frame& frame,const live::Frame* prior,std::string& error){if(!frame.control)return fail(error,"Control authoritative Frame state is absent");return control_input::valid(*frame.control,error)&&interventions::actors::live::admit(frame,prior,error);}
nlohmann::json LivePort::status()const{return {{"bound",bound_},{"suspended",suspended_},{"locked",locked_},{"cause",cause_},{"last",last_},{"actors",interventions::actors::live::status()},{"control",control_input::runtime::status()}};}
