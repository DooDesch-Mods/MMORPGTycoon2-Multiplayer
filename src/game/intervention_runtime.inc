#include "intervention_sample.inc"
namespace {
struct InterventionRoute {uintptr_t rva;size_t bytes,stolen;const char* sha;intervention::native::Original original=nullptr;release_ui_owner::Owner owner;};
struct InterventionPending {a::Context context;a::Binding binding;a::InterventionIntent intent;std::string id;};
struct InterventionRuntime {
    std::array<InterventionRoute,2> routes{{
        {0x39f9f0,336,7,"5379a117ed2e285af583ceb59f2cd667d70bded7a9e963bd5eba5b478a92bdb2"},
        {0x3eb9f0,896,10,"4652b983637842fe3f594a7b1dc43be13baf700bdcea99482388c9aeee6f5972"}}};
    std::optional<InterventionPending> pending;std::map<std::string,Receipt> operations;
    bool original_active=false;
} interventions_runtime;
bool intervention_routes(Runtime& r,std::string& error){for(const auto& route:interventions_runtime.routes){const auto& owner=route.owner;MT2FW_MT2_RouteReceipt actual{};actual.size=sizeof actual;
    if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),actual))return fail(error,"Intervention route read failed original="+hooks::framework::process_backend().error());
    if(actual.state!=MT2FW_MT2_ROUTE_ENABLED||actual.generation!=owner.generation||actual.entry!=owner.entry||actual.trampoline!=owner.trampoline||actual.detour!=owner.detour||std::strcmp(actual.owner,"doodesch.multiplayer"))return fail(error,"Intervention route owner/generation/trampoline changed");
    if(!release_ui_owner::verify(owner,[&](auto address,auto out,auto bytes,auto& cause){return r.memory.copy(address,out,bytes,cause);},[](const auto& bytes){return mt2mp::sha256(bytes);},route.sha,error)||!hooks::framework::process_backend().verify_code(owner.entry,owner.pristine.data(),owner.pristine.size(),error))return false;}
    std::vector<unsigned char> compare(96);if(!r.memory.copy(r.image+0x11e3aa0,compare.data(),compare.size(),error))return false;
    return mt2mp::sha256(compare)=="0e61786467a8750678d39484e6d505ebadce3a61c3ea992e786c93f0d0732939"||fail(error,"Intervention original GNU string comparison helper differs");
}
bool intervention_pending(){return interventions_runtime.pending.has_value();}
void intervention_reset(){interventions_runtime.pending.reset();interventions_runtime.operations.clear();interventions_runtime.original_active=false;}
bool intervention_active(Runtime& r){return !r.replay&&net::session_wire::connected()&&typed_action_runtime::controller_status().value("started",false);}
bool intervention_queue(Runtime& r,uintptr_t receiver,intervention::Command command,std::string& error){
    const auto controller=typed_action_runtime::controller_status();a::Context context;a::Binding binding;
    if(!r.ready||r.replay||GetCurrentThreadId()!=r.thread||r.pending||policy_pending()||rules_pending()||intervention_pending()||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||!a::selected_session().current(context,error)||!typed_action_runtime::network_binding(context.snapshot_uuid,binding,error)||!intervention_routes(r,error))return fail(error,error.empty()?"Intervention normal Command requires idle bound Mainthread Controller":error);
    intervention::Census census;if(!intervention::census(context,binding,census,error))return false;const auto actor=std::find_if(census.actors.begin(),census.actors.end(),[&](const auto& value){return value.object==receiver;});
    if(actor==census.actors.end())return fail(error,"Intervention original Command receiver absent from actual current Actor census");InterventionSample before;
    if(!intervention_sample(r,context,binding,actor->target,before,error))return false;a::InterventionIntent intent{actor->target,command,before.proof.actor.sha};
    if(!intervention::valid(intent))return fail(error,"Intervention original Command is unavailable for its actual concrete family");
    auto id=uuid(error);if(id.empty())return false;interventions_runtime.pending=InterventionPending{context,binding,intent,id};++r.captures;return true;
}
bool intervention_entry(size_t route,void* receiver,const void* command){auto* r=published.load(std::memory_order_acquire);const auto original=interventions_runtime.routes[route].original;
    if(!r||!original)throw std::runtime_error("Intervention original Command route was not published");if(!intervention_active(*r))return original(receiver,command);
    std::string error;for(auto candidate:{intervention::Command::Heal,intervention::Command::Kill,intervention::Command::Warn,intervention::Command::Ban,intervention::Command::Respawn,intervention::Command::RaiseLevel,intervention::Command::LowerLevel}){
        if(!route&&candidate!=intervention::Command::Heal&&candidate!=intervention::Command::Kill)continue;bool matches=false;const auto exception=intervention::native::matches(r->image,command,candidate,matches);
        if(exception){reject(*r,"Intervention original command comparison SEH="+std::to_string(exception));return true;}if(!matches)continue;
        if(!intervention_queue(*r,uintptr_t(receiver),candidate,error))reject(*r,error);return true;}
    if(!route){bool control=false;const auto exception=intervention::native::matches_name(r->image,command,"Control",control);
        if(exception){reject(*r,"Intervention original Control comparison SEH="+std::to_string(exception));return true;}
        if(control){if(!control_input::runtime::capture_acquire(uintptr_t(receiver),error))reject(*r,error);return true;}}
    return original(receiver,command);
}
bool intervention_character_entry(void* receiver,const void* command){return intervention_entry(0,receiver,command);}
bool intervention_toon_entry(void* receiver,const void* command){return intervention_entry(1,receiver,command);}
bool intervention_enable(Runtime& r,std::string& error){const intervention::native::Original entries[]{&intervention_character_entry,&intervention_toon_entry};
    for(size_t i=0;i<interventions_runtime.routes.size();++i){auto& route=interventions_runtime.routes[i];auto& owner=route.owner;owner.entry=r.image+route.rva;owner.detour=uintptr_t(entries[i]);owner.stolen=route.stolen;owner.pristine.resize(route.bytes);
        if(!r.memory.copy(owner.entry,owner.pristine.data(),owner.pristine.size(),error)||mt2mp::sha256(owner.pristine)!=route.sha)return fail(error,error.empty()?"Intervention original complete command body differs":error);
        const auto prepared=MH_CreateHook(reinterpret_cast<void*>(owner.entry),reinterpret_cast<void*>(entries[i]),reinterpret_cast<void**>(&route.original));if(prepared!=MH_OK||!route.original)return fail(error,"Intervention named route prepare failed status="+std::to_string(prepared)+" original="+hook_cause());owner.trampoline=uintptr_t(route.original);
        const auto enabled=MH_EnableHook(reinterpret_cast<void*>(owner.entry));if(enabled!=MH_OK)return fail(error,"Intervention named route enable failed status="+std::to_string(enabled)+" original="+hook_cause());MT2FW_MT2_RouteReceipt actual{};actual.size=sizeof actual;
        if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),actual))return fail(error,"Intervention named route receipt failed original="+hook_cause());owner.generation=actual.generation;}
    return intervention_routes(r,error);
}
void intervention_tick(Runtime& r){if(!interventions_runtime.pending)return;const auto pending=*interventions_runtime.pending;interventions_runtime.pending.reset();a::Context context;a::Binding binding;std::string error;
    if(!a::selected_session().current(context,error)||context!=pending.context||!typed_action_runtime::network_binding(context.snapshot_uuid,binding,error)||binding!=pending.binding||!typed_action_runtime::submit_intervention(pending.id,pending.intent,error)){reject(r,error.empty()?"Intervention captured normal Command binding changed":error);return;}++r.submitted;
}
a::Outcome intervention_perform(const a::Binding& binding,const a::Request& request,const a::Evidence* host,std::string& error){
    auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||request.action!=a::Action::Intervention||!intervention::valid(request.intervention)){error="Intervention runtime/action unavailable";return a::Outcome::RejectedNoEffect;}
    if(interventions_runtime.operations.contains(request.action_uuid)){error="Intervention original token already attempted; no replay";return a::Outcome::Unknown;}
    if(interventions_runtime.operations.size()>=a::history_limit){auto oldest=interventions_runtime.operations.end();for(auto it=interventions_runtime.operations.begin();it!=interventions_runtime.operations.end();++it)if(it->second.retired&&(oldest==interventions_runtime.operations.end()||it->second.request.host_revision<oldest->second.request.host_revision))oldest=it;
        if(oldest==interventions_runtime.operations.end()){error="Intervention unretired original receipt capacity reached";return a::Outcome::RejectedNoEffect;}interventions_runtime.operations.erase(oldest);}
    const auto controller=typed_action_runtime::controller_status();a::Context context;
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid||!a::selected_session().current(context,error)||context.snapshot_uuid!=request.snapshot_uuid||(context.role==a::Role::Client)!=(host!=nullptr)||!intervention_routes(*r,error)){if(error.empty())error="Intervention original requires exact pending Controller/Context/role";return a::Outcome::RejectedNoEffect;}
    InterventionSample before;if(!intervention_sample(*r,context,binding,request.intervention.target,before,error))return a::Outcome::RejectedNoEffect;
    if(before.proof.actor.sha!=request.intervention.before_sha){error="Intervention current complete before Actor record differs from captured intent";return a::Outcome::RejectedNoEffect;}
    if(host&&(!host->intervention_before||!host->intervention_after||*host->intervention_before!=before.proof||host->before!=before.money)){error="Intervention Client complete before original record/cascade/full Ledger differs from Host";return a::Outcome::RejectedNoEffect;}
    live::NativePort port(context,binding,nullptr);if(!port.current_owners(before.local.pin,error))return a::Outcome::RejectedNoEffect;
    auto& receipt=interventions_runtime.operations.emplace(request.action_uuid,Receipt{binding,request}).first->second;receipt.evidence.before=before.money;receipt.evidence.intervention_before=before.proof;
    r->replay=true;interventions_runtime.original_active=true;const auto native=intervention::native::execute(interventions_runtime.routes[request.intervention.target.family==intervention::Family::Toon?1:0].original,before.selected.object,request.intervention.command);interventions_runtime.original_active=false;r->replay=false;receipt.attempted=native.attempted;receipt.returned=native.returned;
    const auto unknown=[&](const std::string& cause){error=receipt.cause=cause;return a::Outcome::Unknown;};
    if(native.exception||!native.attempted||!native.returned||!native.result)return unknown("Intervention original Command failed or returned false SEH="+std::to_string(native.exception)+"; no replay");
    InterventionSample after;if(!intervention_sample(*r,context,binding,request.intervention.target,after,error))return unknown(error);
    receipt.evidence.after=after.money;receipt.evidence.intervention_after=after.proof;
    if(before.selected!=after.selected||!intervention::effect(request.intervention,before.proof,after.proof)||(host&&(*host->intervention_after!=after.proof||host->after!=after.money)))return unknown("Intervention original complete Actor/Subscriber/cascade/full Ledger readback differs from Host");
    receipt.confirmed=true;error.clear();return a::Outcome::Confirmed;
}
bool intervention_observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& error){auto* r=published.load(std::memory_order_acquire);if(!r)return fail(error,"Intervention receipt owner absent");const auto found=interventions_runtime.operations.find(request.action_uuid);
    if(found==interventions_runtime.operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)return fail(error,"Intervention exact confirmed original receipt absent");a::Context context;if(!a::selected_session().current(context,error))return false;
    if(context.snapshot_uuid!=request.snapshot_uuid)return fail(error,"Intervention receipt current snapshot differs");InterventionSample actual;if(!intervention_sample(*r,context,binding,request.intervention.target,actual,error))return false;
    if(actual.proof!=*found->second.evidence.intervention_after||actual.money!=found->second.evidence.after)return fail(error,"Intervention complete actual native record/cascade/full Ledger differs before retirement");out=found->second.evidence;out.retirement_confirmed=found->second.retired;error.clear();return true;
}
bool intervention_retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence actual;if(!intervention_observe(binding,request,actual,error))return false;interventions_runtime.operations.at(request.action_uuid).retired=true;error.clear();return true;}
nlohmann::json intervention_status(){nlohmann::json rows=nlohmann::json::object();
    for(const auto& [key,receipt]:interventions_runtime.operations)rows[key]={{"attempted",receipt.attempted},{"returned",receipt.returned},{"confirmed",receipt.confirmed},{"retired",receipt.retired},{"unknown",receipt.attempted&&!receipt.confirmed},{"cause",receipt.cause},
        {"intent",intervention::encode(receipt.request.intervention)},
        {"before",receipt.evidence.intervention_before?intervention::encode(*receipt.evidence.intervention_before):nlohmann::json(nullptr)},
        {"after",receipt.evidence.intervention_after?intervention::encode(*receipt.evidence.intervention_after):nlohmann::json(nullptr)}};
    return {{"pending",interventions_runtime.pending.has_value()},{"original_active",interventions_runtime.original_active},{"operations",rows}};
}
}
