namespace {
namespace gp=gm_policy;
struct PolicyRoute {uintptr_t rva;size_t bytes,stolen;const char* sha;void* original=nullptr;release_ui_owner::Owner owner;};
struct PolicyPending {a::Context context;a::Binding binding;a::GmPolicyIntent intent;std::string id;};
struct PolicyRuntime {
    std::array<PolicyRoute,8> routes{{
#include "gm_policy_route_pins.inc"
    }};
    std::optional<PolicyPending> pending;std::map<std::string,Receipt> operations;
};
PolicyRuntime policy;
bool policy_values(Runtime& r,gp::State& values,uintptr_t& queue,std::string& error){
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return at(r,object,offset,value,cause);};
    if(!at(r,r.image,0x13df360,queue,error)||!gp::read(read,r.image,queue,values,error))return false;
    uintptr_t final_queue=0;gp::State checked;
    return at(r,r.image,0x13df360,final_queue,error)&&gp::read(read,r.image,final_queue,checked,error)&&
        ((final_queue==queue&&checked==values)||fail(error,"GM policy current owner/scalars changed during double read"));
}
bool policy_route_code(Runtime& r,std::string& error){
    for(const auto& route:policy.routes){const auto& owner=route.owner;MT2FW_MT2_RouteReceipt actual{};actual.size=sizeof actual;
        if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),actual))return fail(error,"GM policy route read original="+hooks::framework::process_backend().error());
        if(actual.state!=MT2FW_MT2_ROUTE_ENABLED||actual.generation!=owner.generation||actual.entry!=owner.entry||actual.trampoline!=owner.trampoline||actual.detour!=owner.detour||std::strcmp(actual.owner,"doodesch.multiplayer"))return fail(error,"GM policy physical route owner/generation differs");
        if(!release_ui_owner::verify(owner,[&](auto address,auto out,auto bytes,auto& cause){return r.memory.copy(address,out,bytes,cause);},[](const auto& bytes){return mt2mp::sha256(bytes);},route.sha,error)||
           !hooks::framework::process_backend().verify_code(owner.entry,owner.pristine.data(),owner.pristine.size(),error))return false;
    }
    struct Code{uintptr_t rva;size_t bytes;const char* sha;};const Code code[]{
#include "gm_policy_helper_pins.inc"
    };
    for(const auto& pin:code){std::vector<unsigned char> bytes(pin.bytes);if(!r.memory.copy(r.image+pin.rva,bytes.data(),bytes.size(),error)||mt2mp::sha256(bytes)!=pin.sha)return fail(error,error.empty()?"GM policy original cascade/helper body differs RVA="+std::to_string(pin.rva):error);}
    return true;
}
bool policy_proof(Runtime& r,const a::Context& context,const a::Binding& binding,Local& local,a::GmPolicyProof& proof,a::Ledger& ledger,uintptr_t& queue,std::string& error){
    management_simulation::LiveState current;if(!primary_read(r,context,binding,local,current,error)||!policy_values(r,proof.state,queue,error))return false;
    gm_ticket::Publication value;if(!gm_capture(r,context,binding,local,value,error))return false;
    const auto encoded=gm_ticket::encode(value).dump();proof.gm_sha256=mt2mp::sha256(std::vector<unsigned char>(encoded.begin(),encoded.end()));ledger=live::ledger(local.pin.values);
    gp::State checked;uintptr_t final_queue=0;if(!policy_values(r,checked,final_queue,error)||checked!=proof.state||final_queue!=queue)return fail(error,error.empty()?"GM policy owner/scalars changed during complete GM capture":error);
    return gp::valid(proof)||fail(error,"GM policy complete proof invalid");
}
bool policy_capture(Runtime& r,uintptr_t receiver,bool boolean,gp::Intent& intent,std::string& error,bool& ours){
    ours=false;uintptr_t value=0,owner=0,queue=0;
    if(!at(r,receiver,boolean?0x3c8:0x420,value,error))return false;if(!value)return true;
    if(!at(r,value,8,owner,error)||!at(r,r.image,0x13df360,queue,error))return false;
    if(!queue||(owner!=queue+0x48&&owner!=queue+0x70))return true;ours=true;
    if(!ap(r,receiver,boolean?0x16d98c0:0x16e7070,error)||!ap(r,value,boolean?0x1749610:0x17496b0,error))return false;
    std::string name;uintptr_t scalar=0;if(!text(r,value+0x18,name,32,error)||!at(r,value,0x38,scalar,error))return false;
    size_t field=0;while(field<gp::names.size()&&name!=gp::names[field])++field;
    if(field>=7||gp::integer(gp::Field(field))==boolean||scalar!=owner+gp::offsets[field])return fail(error,"GM policy native bound property name/type/address differs");
    gp::State state;uintptr_t actual_queue=0;if(!policy_values(r,state,actual_queue,error))return false;if(actual_queue!=queue)return fail(error,"GM policy bound UI Queue owner changed during capture");
    intent.policy=uint8_t(owner==queue+0x70);intent.field=gp::Field(field);intent.before=state.policies[intent.policy][field];
    if(!boolean){int32_t min=0,max=0;if(!at(r,receiver,0x320,min,error)||!at(r,receiver,0x324,max,error)||min||max!=gp::maximum(intent.field))return fail(error,"GM policy original spinner range differs");}
    return true;
}
bool policy_queue(Runtime& r,const gp::Intent& intent,std::string& error){
    if(!gp::valid(intent))return fail(error,"GM policy requested native property change is invalid");
    const auto controller=typed_action_runtime::controller_status();a::Context context;a::Binding binding;
    if(!r.ready||r.replay||GetCurrentThreadId()!=r.thread||r.pending||policy.pending||rules_pending()||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||
       !a::selected_session().current(context,error)||!typed_action_runtime::network_binding(context.snapshot_uuid,binding,error)||!policy_route_code(r,error))return fail(error,error.empty()?"GM policy normal UI requires idle bound Controller":error);
    Local local;a::GmPolicyProof before;a::Ledger ledger;uintptr_t queue=0;if(!policy_proof(r,context,binding,local,before,ledger,queue,error)||before.state.policies[intent.policy][size_t(intent.field)]!=intent.before)return fail(error,error.empty()?"GM policy captured original before value differs":error);
    auto id=uuid(error);if(id.empty())return false;policy.pending=PolicyPending{context,binding,intent,id};++r.captures;return true;
}
void policy_boolean(size_t index,void* receiver,bool desired){auto* r=published.load(std::memory_order_acquire);auto original=reinterpret_cast<void(*)(void*,bool)>(policy.routes[index].original);
    if(!r||!original)throw std::runtime_error("GM policy original checkbox was not published");
    if(r->replay||!net::session_wire::connected()||!typed_action_runtime::controller_status().value("started",false)){original(receiver,desired);return;}
    gp::Intent intent;bool ours=false;std::string error;if(!policy_capture(*r,uintptr_t(receiver),true,intent,error,ours)){reject(*r,error);return;}
    if(!ours){bool handled=false;if(!rules_checkbox(*r,uintptr_t(receiver),desired,handled,error)){reject(*r,error);return;}if(!handled)original(receiver,desired);return;}intent.value=int32_t(desired);if(intent.value==intent.before){original(receiver,desired);return;}if(!policy_queue(*r,intent,error))reject(*r,error);
}
void policy_checkbox(void* p,bool v){policy_boolean(0,p,v);}void policy_toggled(void* p,bool v){policy_boolean(1,p,v);}void policy_instant(void* p,bool v){policy_boolean(2,p,v);}
void policy_integer(size_t index,void* receiver,int32_t desired){auto* r=published.load(std::memory_order_acquire);auto original=reinterpret_cast<void(*)(void*,int32_t)>(policy.routes[index].original);
    if(!r||!original)throw std::runtime_error("GM policy original spinner was not published");
    if(r->replay||!net::session_wire::connected()||!typed_action_runtime::controller_status().value("started",false)){original(receiver,desired);return;}
    gp::Intent intent;bool ours=false;std::string error;if(!policy_capture(*r,uintptr_t(receiver),false,intent,error,ours)){reject(*r,error);return;}
    if(!ours){original(receiver,desired);return;}
    int64_t target=desired;if(index==5||index==6){int32_t step=0,current=0;if(!at(*r,uintptr_t(receiver),index==5?0x32c:0x330,step,error)||!at(*r,uintptr_t(receiver),0x428,current,error)){reject(*r,error);return;}target=int64_t(current)+int64_t(step)*desired;}
    intent.value=int32_t(std::clamp<int64_t>(target,0,gp::maximum(intent.field)));if(intent.value==intent.before){original(receiver,desired);return;}if(!policy_queue(*r,intent,error))reject(*r,error);
}
void policy_internal(void* p,int32_t v){policy_integer(3,p,v);}void policy_set(void* p,int32_t v){policy_integer(4,p,v);}void policy_click(void* p,int32_t v){policy_integer(5,p,v);}void policy_drag(void* p,int32_t v){policy_integer(6,p,v);}
void policy_wheel(void* receiver,void* cursor,const void* delta,float time){auto* r=published.load(std::memory_order_acquire);auto original=reinterpret_cast<void(*)(void*,void*,const void*,float)>(policy.routes[7].original);
    if(!r||!original)throw std::runtime_error("GM policy original wheel was not published");
    if(r->replay||!net::session_wire::connected()||!typed_action_runtime::controller_status().value("started",false)){original(receiver,cursor,delta,time);return;}
    gp::Intent intent;bool ours=false;std::string error;if(!policy_capture(*r,uintptr_t(receiver),false,intent,error,ours)){reject(*r,error);return;}
    if(!ours){original(receiver,cursor,delta,time);return;}
    uint8_t enabled=0;float accumulated=0;int32_t step=0,current=0;
    if(!at(*r,uintptr_t(receiver),0x3f1,enabled,error)||!at(*r,uintptr_t(receiver),0x440,accumulated,error)||!at(*r,uintptr_t(receiver),0x32c,step,error)||!at(*r,uintptr_t(receiver),0x428,current,error)){reject(*r,error);return;}
    if(!enabled)return;if(!std::isfinite(accumulated)||std::abs(accumulated)>=8388608.f){reject(*r,"GM policy native wheel accumulator is outside finite input domain");return;}
    const float consumed=std::floor(accumulated),remainder=accumulated-consumed;const double target=double(float(current)+float(step)*consumed);
    if(target<INT32_MIN||target>INT32_MAX){reject(*r,"GM policy native wheel result exceeds int32");return;}
    intent.value=std::clamp(int32_t(target),0,gp::maximum(intent.field));bool stored=false;
    const auto exception=gn::compare(uintptr_t(receiver)+0x440,4,std::bit_cast<uint32_t>(accumulated),std::bit_cast<uint32_t>(remainder),stored);
    if(exception||!stored){reject(*r,"GM policy original UI wheel consumption differs SEH="+std::to_string(exception));return;}
    if(intent.value!=intent.before&&!policy_queue(*r,intent,error))reject(*r,error);
}
bool policy_enable(Runtime& r,std::string& error){void* detours[]{reinterpret_cast<void*>(&policy_checkbox),reinterpret_cast<void*>(&policy_toggled),reinterpret_cast<void*>(&policy_instant),reinterpret_cast<void*>(&policy_internal),reinterpret_cast<void*>(&policy_set),reinterpret_cast<void*>(&policy_click),reinterpret_cast<void*>(&policy_drag),reinterpret_cast<void*>(&policy_wheel)};
    for(size_t i=0;i<policy.routes.size();++i){auto& route=policy.routes[i];auto& owner=route.owner;owner.entry=r.image+route.rva;owner.detour=uintptr_t(detours[i]);owner.stolen=route.stolen;owner.pristine.resize(route.bytes);
        if(!r.memory.copy(owner.entry,owner.pristine.data(),owner.pristine.size(),error)||mt2mp::sha256(owner.pristine)!=route.sha)return fail(error,error.empty()?"GM policy complete original setter body differs":error);
        const auto created=MH_CreateHook(reinterpret_cast<void*>(owner.entry),detours[i],&route.original);if(created!=MH_OK||!route.original)return fail(error,"GM policy named route prepare failed status="+std::to_string(created)+" original="+hook_cause());owner.trampoline=uintptr_t(route.original);
        const auto enabled=MH_EnableHook(reinterpret_cast<void*>(owner.entry));if(enabled!=MH_OK)return fail(error,"GM policy named route enable failed status="+std::to_string(enabled)+" original="+hook_cause());
        MT2FW_MT2_RouteReceipt actual{};actual.size=sizeof actual;if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),actual))return fail(error,"GM policy named route receipt failed original="+hook_cause());owner.generation=actual.generation;
    }return policy_route_code(r,error);
}
bool policy_pending(){return policy.pending.has_value();}
void policy_reset(){policy.pending.reset();policy.operations.clear();}
void policy_tick(Runtime& r){if(!policy.pending)return;const auto pending=*policy.pending;policy.pending.reset();std::string error;a::Context context;a::Binding binding;
    if(!a::selected_session().current(context,error)||context!=pending.context||!typed_action_runtime::network_binding(context.snapshot_uuid,binding,error)||binding!=pending.binding||!typed_action_runtime::submit_gm_policy(pending.id,pending.intent,error)){reject(r,error.empty()?"GM policy saved UI binding changed":error);return;}++r.submitted;
}
a::Outcome policy_perform(const a::Binding& binding,const a::Request& request,const a::Evidence* host,std::string& error){
    auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||request.action!=a::Action::GmPolicy||!gp::valid(request.gm_policy)){error="GM policy runtime/action unavailable";return a::Outcome::RejectedNoEffect;}
    if(policy.operations.contains(request.action_uuid)){error="GM policy original token already attempted; no replay";return a::Outcome::Unknown;}
    if(policy.operations.size()>=a::history_limit){auto oldest=policy.operations.end();for(auto it=policy.operations.begin();it!=policy.operations.end();++it)if(it->second.retired&&(oldest==policy.operations.end()||it->second.request.host_revision<oldest->second.request.host_revision))oldest=it;
        if(oldest==policy.operations.end()){error="GM policy unretired receipt capacity reached";return a::Outcome::RejectedNoEffect;}policy.operations.erase(oldest);}
    const auto controller=typed_action_runtime::controller_status();a::Context context;
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid||!a::selected_session().current(context,error)||context.snapshot_uuid!=request.snapshot_uuid||(context.role==a::Role::Client)!=(host!=nullptr)||!policy_route_code(*r,error)){if(error.empty())error="GM policy requires exact Controller/saved Context/role";return a::Outcome::RejectedNoEffect;}
    Local local;a::GmPolicyProof before;a::Ledger ledger;uintptr_t queue=0;
    if(!policy_proof(*r,context,binding,local,before,ledger,queue,error))return a::Outcome::RejectedNoEffect;
    if(before.state.policies[request.gm_policy.policy][size_t(request.gm_policy.field)]!=request.gm_policy.before){error="GM policy actual current member differs from captured before value";return a::Outcome::RejectedNoEffect;}
    if(host&&(!host->gm_policy_before||!host->gm_policy_after||*host->gm_policy_before!=before||host->before!=ledger||host->after!=ledger)){error="GM policy Client complete before GM/policy/Ledger differs from Host";return a::Outcome::RejectedNoEffect;}
    live::NativePort port(context,binding,nullptr);if(!port.current_owners(local.pin,error))return a::Outcome::RejectedNoEffect;
    auto& receipt=policy.operations.emplace(request.action_uuid,Receipt{binding,request}).first->second;receipt.evidence.before=ledger;receipt.evidence.gm_policy_before=before;
    r->replay=true;receipt.attempted=true;const auto native=gp::native::change(r->image,queue+0x48+size_t(request.gm_policy.policy)*40,request.gm_policy);r->replay=false;receipt.returned=native.notified;
    auto unknown=[&](const std::string& cause){error=receipt.cause=cause;return a::Outcome::Unknown;};
    if(native.exception||!native.stored||!native.notified)return unknown("GM policy original member store/Notify/ApplyPolicy failed SEH="+std::to_string(native.exception)+"; no replay");
    Local final;a::GmPolicyProof after;uintptr_t after_queue=0;if(!policy_proof(*r,context,binding,final,after,receipt.evidence.after,after_queue,error))return unknown(error);
    receipt.evidence.gm_policy_after=after;
    if(final.pin!=local.pin||after_queue!=queue||final.gm_rows!=local.gm_rows||final.gm_uids!=local.gm_uids||!gp::effect(request.gm_policy,before,after)||receipt.evidence.after!=ledger||(host&&*host->gm_policy_after!=after))return unknown("GM policy complete actual cascade/owner/Clock/Ledger readback differs from intent/Host");
    receipt.confirmed=true;error.clear();return a::Outcome::Confirmed;
}
bool policy_observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& error){auto* r=published.load(std::memory_order_acquire);if(!r)return fail(error,"GM policy owner absent");const auto found=policy.operations.find(request.action_uuid);
    if(found==policy.operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)return fail(error,"GM policy exact confirmed original receipt absent");
    Local local;a::Context context;a::GmPolicyProof actual;a::Ledger ledger;uintptr_t queue=0;if(!a::selected_session().current(context,error))return false;
    if(context.snapshot_uuid!=request.snapshot_uuid)return fail(error,"GM policy receipt current saved snapshot differs");if(!policy_proof(*r,context,binding,local,actual,ledger,queue,error))return false;
    if(actual!=*found->second.evidence.gm_policy_after||ledger!=found->second.evidence.after)return fail(error,"GM policy actual full GM/policy/Ledger differs before receipt retirement");out=found->second.evidence;out.retirement_confirmed=found->second.retired;error.clear();return true;
}
bool policy_retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence actual;if(!policy_observe(binding,request,actual,error))return false;policy.operations.at(request.action_uuid).retired=true;error.clear();return true;}
bool policy_adopt(Runtime& r,const gp::State& desired,live::ManagementReceipt& receipt,std::string& error){gp::State before;uintptr_t queue=0;
    if(!gp::valid(desired)||!policy_values(r,before,queue,error))return fail(error,error.empty()?"GM policy Host values invalid":error);
    for(size_t p=0;p<2;++p)for(size_t f=0;f<7;++f)if(before.policies[p][f]!=desired.policies[p][f]){gp::State actual;uintptr_t current_queue=0;if(!policy_values(r,actual,current_queue,error)||current_queue!=queue||actual!=before)return fail(error,error.empty()?"GM policy authoritative adoption concurrent owner/value differs":error);
        bool stored=false;receipt.attempted=true;++receipt.attempted_stores;const auto exception=gn::compare(queue+0x48+p*40+gp::offsets[f],gp::integer(gp::Field(f))?4:1,uint32_t(before.policies[p][f]),uint32_t(desired.policies[p][f]),stored);
        if(exception||!stored)return fail(error,"GM policy authoritative scalar CAS failed SEH="+std::to_string(exception));++receipt.stores;before.policies[p][f]=desired.policies[p][f];}
    return true;
}
}
